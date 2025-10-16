#include "HeartRateProcessor.h"
#include <cmath>

namespace
{
    constexpr float kMinValidBpm = 30.0f;
    constexpr float kMaxValidBpm = 220.0f;
    constexpr float kBaselineMin = 10.0f;
    constexpr float kBaselineMax = 90.0f;
    constexpr float kUpdateIntervalSeconds = HeartRateProcessor::historyTimeDeltaSeconds;
    constexpr int   kVariabilityWindow = 10;
}

HeartRateProcessor::HeartRateProcessor() = default;

void HeartRateProcessor::setSmoothingFactor(float factor)
{
    smoothingParam.store(juce::jlimit(0.1f, 10.0f, factor));
}

float HeartRateProcessor::getSmoothingFactor() const
{
    return smoothingParam.load();
}

void HeartRateProcessor::setHeartRateOffset(float bpmOffset)
{
    heartRateOffset.store(juce::jlimit(-100.0f, 100.0f, bpmOffset));
}

float HeartRateProcessor::getHeartRateOffset() const
{
    return heartRateOffset.load();
}

void HeartRateProcessor::setWetDryOffset(float percentOffset)
{
    wetDryOffset.store(juce::jlimit(-100.0f, 100.0f, percentOffset));
}

float HeartRateProcessor::getWetDryOffset() const
{
    return wetDryOffset.load();
}

void HeartRateProcessor::setWetDrySource(WetDrySource source)
{
    wetDrySourceFlag.store(source == WetDrySource::Raw ? 1 : 0);
}

HeartRateProcessor::WetDrySource HeartRateProcessor::getWetDrySource() const
{
    return wetDrySourceFlag.load() == 1 ? WetDrySource::Raw : WetDrySource::Smoothed;
}

void HeartRateProcessor::setBpmRange(float minBpm, float maxBpm)
{
    juce::ScopedLock lock(stateLock);
    bpmMin = juce::jmin(minBpm, maxBpm);
    bpmMax = juce::jmax(minBpm + 1.0f, maxBpm);
}

void HeartRateProcessor::pushNewMeasurement(float bpm, const std::vector<float>& rrMs)
{
    if (!std::isfinite(bpm))
        return;

    const float adjusted = juce::jlimit(kMinValidBpm, kMaxValidBpm, bpm + heartRateOffset.load());

    juce::ScopedLock lock(stateLock);
    rawValue.store(adjusted);
    latestRr = rrMs;

    updateSmoothed(adjusted);
    updateWetDry(adjusted, rrMs);
    updateInverted();

    addToHistory(rawHistory, adjusted);
    addToHistory(smoothedHistory, smoothedValue.load());
    addToHistory(wetDryHistory, wetDryValue.load());
}

void HeartRateProcessor::process()
{
    juce::ScopedLock lock(stateLock);
    updateInverted();
}

float HeartRateProcessor::getRawBpm() const
{
    return rawValue.load();
}

float HeartRateProcessor::getSmoothedBpm() const
{
    return smoothedValue.load();
}

float HeartRateProcessor::getWetDryValue() const
{
    return wetDryValue.load();
}

float HeartRateProcessor::getInvertedBpm() const
{
    return invertedValue.load();
}

HeartRateProcessor::SmoothingMetrics HeartRateProcessor::getSmoothingMetrics() const
{
    SmoothingMetrics metrics;
    const float alpha = smoothingAlpha.load();
    metrics.alpha = alpha;
    metrics.halfLifeSeconds = smoothingHalfLife.load();
    metrics.effectiveWindowSamples = smoothingEffectiveWindow.load();
    return metrics;
}

void HeartRateProcessor::getHistoryCopy(juce::Array<float>& rawOut,
                                        juce::Array<float>& smoothedOut,
                                        juce::Array<float>& wetDryOut) const
{
    juce::ScopedLock lock(stateLock);
    rawOut.clearQuick();
    smoothedOut.clearQuick();
    wetDryOut.clearQuick();

    rawOut.ensureStorageAllocated((int) rawHistory.size());
    smoothedOut.ensureStorageAllocated((int) smoothedHistory.size());
    wetDryOut.ensureStorageAllocated((int) wetDryHistory.size());

    for (auto value : rawHistory)
        rawOut.add(value);
    for (auto value : smoothedHistory)
        smoothedOut.add(value);
    for (auto value : wetDryHistory)
        wetDryOut.add(value);
}

void HeartRateProcessor::attachToValueTree(juce::AudioProcessorValueTreeState& apvts)
{
    if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(HeartRateParams::SMOOTHING_FACTOR)))
        smoothingParam.store(param->get());
}

void HeartRateProcessor::updateSmoothed(float adjustedBpm)
{
    const float uiFactor = juce::jlimit(0.1f, 10.0f, smoothingParam.load());
    const float alpha = 1.0f / (1.0f + uiFactor);

    const float previous = smoothedValue.load();
    const bool hasPrevious = std::isfinite(previous) && previous > 0.0f;
    const float newValue = hasPrevious ? (alpha * adjustedBpm + (1.0f - alpha) * previous)
                                       : adjustedBpm;

    smoothedValue.store(newValue);
    smoothingAlpha.store(alpha);

    const float oneMinusAlpha = juce::jlimit(0.0001f, 0.9999f, 1.0f - alpha);
    const float halfSamples = static_cast<float>(std::log(0.5f) / std::log(oneMinusAlpha));
    const float halfLife = juce::jlimit(0.0f, 60.0f, halfSamples * kUpdateIntervalSeconds);
    smoothingHalfLife.store(halfLife);
    smoothingEffectiveWindow.store(juce::jlimit(1, 2000, static_cast<int>(std::round(halfSamples * 5.0f))));
}

void HeartRateProcessor::updateWetDry(float adjustedBpm, const std::vector<float>& rrMs)
{
    const bool useRaw = wetDrySourceFlag.load() == 1;
    const float sourceHr = useRaw ? adjustedBpm : smoothedValue.load();

    float baseline = (sourceHr - 40.0f) / (180.0f - 40.0f);
    baseline = juce::jlimit(0.0f, 1.0f, baseline);
    float combined = kBaselineMin + baseline * (kBaselineMax - kBaselineMin);

    if (rrMs.size() > 1)
    {
        double rrMean = 0.0;
        for (auto rr : rrMs)
            rrMean += rr;
        rrMean /= rrMs.size();

        double variance = 0.0;
        for (auto rr : rrMs)
        {
            const double diff = rr - rrMean;
            variance += diff * diff;
        }
        variance /= rrMs.size();
        const double rrStd = std::sqrt(juce::jmax(0.0, variance));
        const float hrvComponent = juce::jlimit(0.0f, 100.0f, static_cast<float>(rrStd / 120.0) * 100.0f);
        combined = 0.6f * combined + 0.4f * hrvComponent;
    }
    else
    {
        const auto& sourceHistory = useRaw ? rawHistory : smoothedHistory;
        if (sourceHistory.size() >= 5)
        {
            const int sampleCount = (int) juce::jmin<size_t>(kVariabilityWindow, sourceHistory.size());
            double mean = 0.0;
            for (int i = 0; i < sampleCount; ++i)
                mean += sourceHistory[sourceHistory.size() - 1 - i];
            mean /= sampleCount;

            double variance = 0.0;
            for (int i = 0; i < sampleCount; ++i)
            {
                const double v = sourceHistory[sourceHistory.size() - 1 - i];
                variance += (v - mean) * (v - mean);
            }
            variance /= sampleCount;
            const double stdDev = std::sqrt(juce::jmax(0.0, variance));
            const float variability = juce::jlimit(0.0f, 1.0f, static_cast<float>(stdDev / 5.0));
            combined = 0.7f * combined + 0.3f * (variability * 100.0f);
        }
    }

    combined += wetDryOffset.load();
    wetDryValue.store(juce::jlimit(1.0f, 100.0f, combined));
}

void HeartRateProcessor::updateInverted()
{
    const float smooth = smoothedValue.load();
    const float normalised = juce::jlimit(0.0f, 1.0f, (smooth - bpmMin) / juce::jmax(1.0f, bpmMax - bpmMin));
    invertedValue.store(1.0f - normalised);
}

void HeartRateProcessor::addToHistory(std::deque<float>& buffer, float value)
{
    buffer.push_back(value);
    while (buffer.size() > historyCapacity)
        buffer.pop_front();
}

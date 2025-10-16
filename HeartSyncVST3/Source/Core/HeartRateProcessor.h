#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <deque>
#include <vector>
#include "HeartRateParams.h"

class HeartRateProcessor
{
public:
    enum class WetDrySource { Smoothed = 0, Raw = 1 };

    struct SmoothingMetrics
    {
        float alpha = 0.0f;
        float halfLifeSeconds = 0.0f;
        int effectiveWindowSamples = 0;
    };

    static constexpr float historyTimeDeltaSeconds = 0.1f;
    static constexpr size_t historyCapacity = 300;

    HeartRateProcessor();

    void setSmoothingFactor(float factor);      // Accepts 0.1 – 10.0 from UI
    float getSmoothingFactor() const;

    void setHeartRateOffset(float bpmOffset);   // -100 – +100 BPM
    float getHeartRateOffset() const;

    void setWetDryOffset(float percentOffset);  // -100 – +100 %
    float getWetDryOffset() const;

    void setWetDrySource(WetDrySource source);
    WetDrySource getWetDrySource() const;

    void setBpmRange(float minBpm, float maxBpm);

    void pushNewMeasurement(float bpm, const std::vector<float>& rrMs = {});
    void process();

    float getRawBpm() const;
    float getSmoothedBpm() const;
    float getWetDryValue() const;
    float getInvertedBpm() const;

    SmoothingMetrics getSmoothingMetrics() const;

    void getHistoryCopy(juce::Array<float>& rawOut,
                        juce::Array<float>& smoothedOut,
                        juce::Array<float>& wetDryOut) const;

    void attachToValueTree(juce::AudioProcessorValueTreeState& apvts);

private:
    void updateSmoothed(float adjustedBpm);
    void updateWetDry(float adjustedBpm, const std::vector<float>& rrMs);
    void updateInverted();
    void addToHistory(std::deque<float>& buffer, float value);

    mutable juce::CriticalSection stateLock;

    std::atomic<float> rawValue { 0.0f };
    std::atomic<float> smoothedValue { 0.0f };
    std::atomic<float> wetDryValue { 0.0f };
    std::atomic<float> invertedValue { 0.0f };

    std::atomic<float> smoothingParam { 0.1f };
    std::atomic<float> smoothingAlpha { 1.0f };
    std::atomic<float> smoothingHalfLife { 0.0f };
    std::atomic<int> smoothingEffectiveWindow { 0 };
    std::atomic<float> heartRateOffset { 0.0f };
    std::atomic<float> wetDryOffset { 0.0f };
    std::atomic<int> wetDrySourceFlag { 0 };

    std::vector<float> latestRr;

    std::deque<float> rawHistory;
    std::deque<float> smoothedHistory;
    std::deque<float> wetDryHistory;

    float bpmMin = 40.0f;
    float bpmMax = 180.0f;

};


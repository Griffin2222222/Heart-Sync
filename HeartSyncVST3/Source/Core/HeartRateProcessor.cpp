#include "HeartRateProcessor.h"
#include "HeartRateParams.h"

HeartRateProcessor::HeartRateProcessor()
{
}

void HeartRateProcessor::pushNewBpm(float bpm)
{
    rawBpm.store(bpm);
    process(); // Update smoothed values immediately
}

void HeartRateProcessor::setSmoothingFactor(float factor)
{
    smoothingFactor.store(juce::jlimit(0.0f, 1.0f, factor));
}

float HeartRateProcessor::getSmoothingFactor() const
{
    return smoothingFactor.load();
}

float HeartRateProcessor::getRawBpm() const
{
    return rawBpm.load();
}

float HeartRateProcessor::getSmoothedBpm() const
{
    return smoothedBpm.load();
}

float HeartRateProcessor::getInvertedBpm() const
{
    return invertedBpm.load();
}

void HeartRateProcessor::setBpmRange(float minBpm, float maxBpm)
{
    juce::ScopedLock lock(processingLock);
    bpmMin = minBpm;
    bpmMax = maxBpm;
}

void HeartRateProcessor::getInversionRange(float& minInverted, float& maxInverted) const
{
    minInverted = inversionMin;
    maxInverted = inversionMax;
}

void HeartRateProcessor::attachToValueTree(juce::AudioProcessorValueTreeState& apvts)
{
    // Get smoothing parameter from APVTS
    if (auto* param = apvts.getParameter(HeartRateParams::SMOOTHING_FACTOR))
    {
        smoothingParameter.reset(dynamic_cast<juce::AudioParameterFloat*>(param));
    }
}

void HeartRateProcessor::process()
{
    updateSmoothedBpm();
    updateInvertedBpm();
}

void HeartRateProcessor::updateSmoothedBpm()
{
    juce::ScopedLock lock(processingLock);
    
    float currentRaw = rawBpm.load();
    float currentSmoothed = smoothedBpm.load();
    float factor = smoothingFactor.load();
    
    // Exponential smoothing: smoothed = α * raw + (1-α) * previous_smoothed
    float newSmoothed = factor * currentRaw + (1.0f - factor) * currentSmoothed;
    
    smoothedBpm.store(newSmoothed);
}

void HeartRateProcessor::updateInvertedBpm()
{
    juce::ScopedLock lock(processingLock);
    
    float currentSmoothed = smoothedBpm.load();
    
    // Normalize to 0-1 range
    float normalized = (currentSmoothed - bpmMin) / (bpmMax - bpmMin);
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    
    // Invert and map to inversion range
    float inverted = 1.0f - normalized;
    float mappedInverted = inversionMin + inverted * (inversionMax - inversionMin);
    
    invertedBpm.store(mappedInverted);
}

#include "HeartRateProcessor.h"

HeartRateProcessor::HeartRateProcessor()
    : rawHeartRate(0.0f)
    , smoothedHeartRate(0.0f)
    , hrOffset(0.0f)
    , smoothingFactor(0.15f)  // Default from Python
    , wetDryOffset(0.0f)
    , isFirstSample(true)
{
}

float HeartRateProcessor::processHeartRate(int rawHR)
{
    // Update raw heart rate with offset
    rawHeartRate = static_cast<float>(rawHR) + hrOffset;
    
    // Clamp to valid range (40-180 BPM)
    rawHeartRate = juce::jlimit(40.0f, 180.0f, rawHeartRate);
    
    // EMA smoothing (matching Python algorithm exactly)
    if (isFirstSample)
    {
        smoothedHeartRate = rawHeartRate;
        isFirstSample = false;
    }
    else
    {
        // Python: smoothed = alpha * raw + (1 - alpha) * smoothed
        // where alpha = smoothingFactor
        float alpha = smoothingFactor;
        smoothedHeartRate = alpha * rawHeartRate + (1.0f - alpha) * smoothedHeartRate;
    }
    
    // Clamp smoothed to valid range
    smoothedHeartRate = juce::jlimit(40.0f, 180.0f, smoothedHeartRate);
    
    return smoothedHeartRate;
}

float HeartRateProcessor::calculateWetDryRatio() const
{
    // Python algorithm:
    // wet_dry_ratio = ((smoothed_hr - 40.0) / (180.0 - 40.0)) * 100.0
    // Then apply wet_dry_offset and clamp to 0-100
    
    float ratio = ((smoothedHeartRate - 40.0f) / (180.0f - 40.0f)) * 100.0f;
    ratio += wetDryOffset;
    
    // Clamp to 0-100
    return juce::jlimit(0.0f, 100.0f, ratio);
}

void HeartRateProcessor::reset()
{
    rawHeartRate = 0.0f;
    smoothedHeartRate = 0.0f;
    isFirstSample = true;
}

#pragma once

#include <JuceHeader.h>

/**
 * @brief Processes raw heart rate data with EMA smoothing and wet/dry mixing
 * 
 * Implements the exact algorithm from HeartSyncProfessional.py:
 * - Exponential Moving Average (EMA) smoothing
 * - HR offset adjustment
 * - Wet/dry ratio calculation with offset
 */
class HeartRateProcessor
{
public:
    HeartRateProcessor();
    
    /** Process new raw heart rate value, returns smoothed HR */
    float processHeartRate(int rawHR);
    
    /** Calculate wet/dry ratio (0-100) based on current HR values */
    float calculateWetDryRatio() const;
    
    /** Get current raw heart rate */
    float getRawHeartRate() const { return rawHeartRate; }
    
    /** Get current smoothed heart rate */
    float getSmoothedHeartRate() const { return smoothedHeartRate; }
    
    // --- Parameters (matching Python exactly) ---
    
    void setHROffset(float offset) { hrOffset = offset; }
    float getHROffset() const { return hrOffset; }
    
    void setSmoothingFactor(float factor) { smoothingFactor = juce::jlimit(0.01f, 10.0f, factor); }
    float getSmoothingFactor() const { return smoothingFactor; }
    
    void setWetDryOffset(float offset) { wetDryOffset = offset; }
    float getWetDryOffset() const { return wetDryOffset; }
    
    /** Reset smoothing state */
    void reset();
    
private:
    float rawHeartRate;
    float smoothedHeartRate;
    
    // Parameters (matching Python ranges exactly)
    float hrOffset;           // -100 to +100
    float smoothingFactor;    // 0.01 to 10.0 (default 0.15)
    float wetDryOffset;       // -100 to +100
    
    bool isFirstSample;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartRateProcessor)
};

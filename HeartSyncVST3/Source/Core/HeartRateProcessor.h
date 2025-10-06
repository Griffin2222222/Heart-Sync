#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include "HeartRateParams.h"

class HeartRateProcessor
{
public:
    HeartRateProcessor();
    ~HeartRateProcessor() = default;

    // Heart rate input
    void pushNewBpm(float bpm);
    
    // Parameter control
    void setSmoothingFactor(float factor); // 0.0 - 1.0
    float getSmoothingFactor() const;
    
    // Output values
    float getRawBpm() const;
    float getSmoothedBpm() const;
    float getInvertedBpm() const;
    
    // Range configuration
    void setBpmRange(float minBpm, float maxBpm);
    void getInversionRange(float& minInverted, float& maxInverted) const;
    
    // Parameter binding for APVTS
    void attachToValueTree(juce::AudioProcessorValueTreeState& apvts);
    
    // Processing
    void process(); // Call regularly to update smoothed values

private:
    void updateSmoothedBpm();
    void updateInvertedBpm();
    
    std::atomic<float> rawBpm { 0.0f };
    std::atomic<float> smoothedBpm { 0.0f };
    std::atomic<float> invertedBpm { 0.0f };
    std::atomic<float> smoothingFactor { 0.1f };
    
    float bpmMin = 50.0f;
    float bpmMax = 150.0f;
    float inversionMin = 0.0f;
    float inversionMax = 1.0f;
    
    std::unique_ptr<juce::AudioParameterFloat> smoothingParameter;
    
    juce::CriticalSection processingLock;
};

// Parameter IDs for APVTS
namespace HeartRateParams
{
    const juce::String SMOOTHING_FACTOR = "smoothing_factor";
}

#pragma once

#include <JuceHeader.h>

#ifdef HEARTSYNC_USE_BRIDGE
    #include "Core/HeartSyncBLEClient.h"
#else
    #include "Core/HeartSyncBLE.h"
#endif

#include "Core/HeartRateProcessor.h"

/**
 * @brief HeartSync VST3/AU Plugin Processor
 * 
 * Uses external BLE Helper app for Bluetooth communication (avoids TCC crashes).
 * Converts heart rate data to MIDI CC messages for DAW automation.
 * 
 * Parameters (matching Python exactly):
 * - Raw HR (40-180 BPM)
 * - Smoothed HR (40-180 BPM)
 * - Wet/Dry Ratio (0-100%)
 * - HR Offset (-100 to +100)
 * - Smoothing Factor (0.01-10.0)
 * - Wet/Dry Offset (-100 to +100)
 */
class HeartSyncProcessor : public juce::AudioProcessor
{
public:
    HeartSyncProcessor();
    ~HeartSyncProcessor() override;

    // --- AudioProcessor interface ---
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- BLE Control ---
    
    #ifdef HEARTSYNC_USE_BRIDGE
    /** Get BLE client for UI */
    HeartSyncBLEClient& getBLEClient() { return bleClient; }
    #else
    /** Get BLE manager for UI */
    HeartSyncBLE& getBLEManager() { return bleManager; }
    #endif
    
    /** Check if connected to BLE device */
    bool isBLEConnected() const { return bleConnected.load(); }
    
    /** Get current heart rate values */
    float getRawHeartRate() const { return hrProcessor.getRawHeartRate(); }
    float getSmoothedHeartRate() const { return hrProcessor.getSmoothedHeartRate(); }
    float getWetDryRatio() const { return hrProcessor.calculateWetDryRatio(); }

    // --- Parameters ---
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    // --- Core Components ---
    #ifdef HEARTSYNC_USE_BRIDGE
    HeartSyncBLEClient bleClient;
    #else
    HeartSyncBLE bleManager;
    #endif
    
    HeartRateProcessor hrProcessor;
    
    // --- Parameter Management ---
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // Parameter IDs (matching Python exactly)
    static constexpr const char* PARAM_RAW_HR = "raw_hr";
    static constexpr const char* PARAM_SMOOTHED_HR = "smoothed_hr";
    static constexpr const char* PARAM_WET_DRY_RATIO = "wet_dry_ratio";
    static constexpr const char* PARAM_HR_OFFSET = "hr_offset";
    static constexpr const char* PARAM_SMOOTHING_FACTOR = "smoothing_factor";
    static constexpr const char* PARAM_WET_DRY_OFFSET = "wet_dry_offset";
    
    // --- Callbacks ---
    void handleHeartRateUpdate(int rawHR, const std::vector<float>& rrIntervals);
    void handleBLEDisconnect();
    
    // --- MIDI Output ---
    void sendMIDICC(juce::MidiBuffer& midiMessages, int ccNumber, float normalizedValue);
    
    // --- State ---
    std::atomic<bool> bleConnected{false};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncProcessor)
};

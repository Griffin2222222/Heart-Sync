#pragma once
#include "RectPanel.h"
#include "WaveGraph.h"
#include "HSTheme.h"
#include <functional>

/**
 * @brief Single metric row: Value (200px) | Controls (200px) | Waveform (flex)
 * 
 * Matches Python's stacked row layout exactly:
 * - Left: value tile with large centered number and bottom title
 * - Middle: controls column (buildControlsFn populates this)
 * - Right: waveform graph with ECG grid
 */
class MetricRow : public juce::Component
{
public:
    MetricRow(const juce::String& title,
              const juce::String& unit,
              juce::Colour rowColour,
              std::function<void(juce::Component& controlsHost)> buildControlsFn)
        : titleText(title), unitText(unit), colour(rowColour), 
          buildControls(buildControlsFn)
    {
        // Value panel (left)
        valuePanel = std::make_unique<RectPanel>(colour);
        addAndMakeVisible(*valuePanel);
        
        // Value label (large centered monospaced)
        valueLabel.setFont(HSTheme::monoLarge());
        valueLabel.setColour(juce::Label::textColourId, colour);
        valueLabel.setText("0", juce::dontSendNotification);
        valueLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(valueLabel);
        
        // Title label at bottom of value panel
        titleLabel.setFont(HSTheme::caption());
        titleLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
        juce::String fullTitle = title;
        if (unit.isNotEmpty())
            fullTitle += " [" + unit + "]";
        titleLabel.setText(fullTitle, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);
        
        // Controls panel (middle)
        controlsPanel = std::make_unique<RectPanel>(colour);
        addAndMakeVisible(*controlsPanel);
        
        // Controls host (transparent container)
        addAndMakeVisible(controlsHost);
        
        // Let builder function populate controls
        if (buildControls)
            buildControls(controlsHost);
        
        // Waveform panel (right)
        waveformPanel = std::make_unique<RectPanel>(colour);
        addAndMakeVisible(*waveformPanel);
        
        // Waveform graph
        graph = std::make_unique<WaveGraph>(colour);
        graph->setLineColour(colour);
        addAndMakeVisible(*graph);
    }
    
    void setValueText(const juce::String& text)
    {
        valueLabel.setText(text, juce::dontSendNotification);
    }
    
    WaveGraph& getGraph() { return *graph; }
    
    void resized() override
    {
        auto r = getLocalBounds().reduced(HSTheme::grid);
        
        // Three columns: 200px | 200px | remaining
        constexpr int valueWidth = 200;
        constexpr int controlsWidth = 200;
        
        // Value column (left)
        auto valueCol = r.removeFromLeft(valueWidth);
        r.removeFromLeft(HSTheme::grid); // gap
        
        valuePanel->setBounds(valueCol);
        
        // Value label centered with space for title at bottom
        auto valueBounds = valueCol.reduced(HSTheme::grid);
        auto titleArea = valueBounds.removeFromBottom(24);
        titleLabel.setBounds(titleArea);
        valueLabel.setBounds(valueBounds);
        
        // Controls column (middle)
        auto controlsCol = r.removeFromLeft(controlsWidth);
        r.removeFromLeft(HSTheme::grid); // gap
        
        controlsPanel->setBounds(controlsCol);
        controlsHost.setBounds(controlsCol.reduced(HSTheme::grid));
        
        // Waveform column (right - remaining width)
        waveformPanel->setBounds(r);
        graph->setBounds(r);
    }
    
private:
    juce::String titleText, unitText;
    juce::Colour colour;
    std::function<void(juce::Component&)> buildControls;
    
    std::unique_ptr<RectPanel> valuePanel;
    juce::Label valueLabel;
    juce::Label titleLabel;
    
    std::unique_ptr<RectPanel> controlsPanel;
    juce::Component controlsHost;
    
    std::unique_ptr<RectPanel> waveformPanel;
    std::unique_ptr<WaveGraph> graph;
};

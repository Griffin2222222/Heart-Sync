#pragma once
#include "RectPanel.h"
#include "WaveGraph.h"
#include "HSTheme.h"
#include <functional>
#include <memory>

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
    class ControlsHost : public juce::Component
    {
    public:
        std::function<void(const juce::Rectangle<int>&)> onLayout;

        void resized() override
        {
            if (onLayout)
                onLayout(getLocalBounds());
        }
    };

    MetricRow(const juce::String& title,
              const juce::String& unit,
              juce::Colour rowColour,
              std::function<void(juce::Component& controlsHost)> buildControlsFn)
        : titleText(title), unitText(unit), colour(rowColour), 
          buildControls(buildControlsFn)
    {
        // Value panel (left)
        valuePanel = std::make_unique<RectPanel>(colour);
        valuePanel->setInterceptsMouseClicks(false, false); // Allow mouse events to pass through to parent
        addAndMakeVisible(*valuePanel);
        
        // Value label (large centered monospaced)
        valueLabel.setFont(HSTheme::monoLarge());
        valueLabel.setColour(juce::Label::textColourId, colour);
        valueLabel.setText("0", juce::dontSendNotification);
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setInterceptsMouseClicks(false, false); // Allow mouse events to pass through
        addAndMakeVisible(valueLabel);
        
        // Title label at bottom of value panel
        titleLabel.setFont(HSTheme::caption());
        titleLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
        juce::String fullTitle = title;
        if (unit.isNotEmpty())
            fullTitle += " [" + unit + "]";
        titleLabel.setText(fullTitle, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setInterceptsMouseClicks(false, false); // Allow mouse events to pass through
        addAndMakeVisible(titleLabel);
        
        // Controls panel (middle)
        controlsPanel = std::make_unique<RectPanel>(colour);
        addAndMakeVisible(*controlsPanel);
        
        // Controls host (transparent container)
        controlsHost = std::make_unique<ControlsHost>();
        addAndMakeVisible(*controlsHost);
        
        // Let builder function populate controls
        if (buildControls)
            buildControls(*controlsHost);
        
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
    
    void setTempoSyncActive(bool active) 
    { 
        isSyncedToTempo = active;
        repaint();
    }
    
    bool isTempoSyncActive() const { return isSyncedToTempo; }
    
    // Callback when user requests tempo sync from right-click menu
    std::function<void(bool enable)> onTempoSyncRequested;
    
    void mouseDown(const juce::MouseEvent& event) override
    {
        // Right-click anywhere on the left value area shows tempo sync menu
        auto valueArea = valuePanel->getBounds();
        
        if (event.mods.isPopupMenu() && valueArea.contains(event.getPosition()))
        {
            DBG("MetricRow: Right-click detected on value panel - isSyncedToTempo=" << isSyncedToTempo);
            
            juce::PopupMenu menu;
            
            menu.addSectionHeader("TEMPO SYNC");
            menu.addSeparator();
            
            if (isSyncedToTempo)
            {
                menu.addItem(1, juce::CharPointer_UTF8("✓ Currently Syncing"), false);
                menu.addSeparator();
                menu.addItem(2, "Disable Tempo Sync", true);
            }
            else
            {
                menu.addItem(1, "Sync Session Tempo to This Value", true);
            }
            
            // Use async menu to avoid blocking and modal loops
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                [this, wasSyncedBefore = isSyncedToTempo](int result) 
                { 
                    DBG("MetricRow: Menu result = " << result << ", wasSyncedBefore=" << wasSyncedBefore);
                    
                    if (result == 1 && !wasSyncedBefore && onTempoSyncRequested)
                    {
                        DBG("MetricRow: Enabling tempo sync");
                        onTempoSyncRequested(true);
                    }
                    else if (result == 2 && wasSyncedBefore && onTempoSyncRequested)
                    {
                        DBG("MetricRow: Disabling tempo sync");
                        onTempoSyncRequested(false);
                    }
                    repaint();
                });
            
            return;
        }
        
        juce::Component::mouseDown(event);
    }
    
    void paint(juce::Graphics& g) override
    {
        // Draw tempo sync indicator if active
        if (isSyncedToTempo)
        {
            g.setColour(HSTheme::ACCENT_TEAL);
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            
            auto indicatorBounds = valuePanel->getBounds().removeFromTop(20).reduced(4, 2);
            g.drawText(juce::CharPointer_UTF8("♩ TEMPO SYNC"), indicatorBounds, juce::Justification::centred);
        }
    }
    
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
        controlsHost->setBounds(controlsCol.reduced(HSTheme::grid));
        
        // Waveform column (right - remaining width)
        waveformPanel->setBounds(r);
        graph->setBounds(r);
    }
    
private:
    juce::String titleText, unitText;
    juce::Colour colour;
    std::function<void(juce::Component&)> buildControls;
    bool isSyncedToTempo = false;
    
    std::unique_ptr<RectPanel> valuePanel;
    juce::Label valueLabel;
    juce::Label titleLabel;
    
    std::unique_ptr<RectPanel> controlsPanel;
    std::unique_ptr<ControlsHost> controlsHost;
    
    std::unique_ptr<RectPanel> waveformPanel;
    std::unique_ptr<WaveGraph> graph;
};

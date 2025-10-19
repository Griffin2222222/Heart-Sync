#pragma once
#include "RectPanel.h"
#include <algorithm>
#include <deque>
#include <optional>
#include <vector>

class WaveGraph : public RectPanel, private juce::Timer
{
public:
    WaveGraph (juce::Colour border) : RectPanel (border), lineColour(border) { startTimerHz (40); }
    
    void push (float v)
    {
        if (buffer.size() >= maxPts) buffer.pop_front();
        buffer.push_back (v);
        recomputeStats();
    }
    
    void setLineColour(juce::Colour c) { lineColour = c; }
    void setYAxisLabel(const juce::String& label) { axisLabel = label; repaint(); }

    void setFixedRange(float minValue, float maxValue)
    {
        fixedRange = juce::Range<float>(minValue, maxValue);
        repaint();
    }

    void clearFixedRange()
    {
        fixedRange.reset();
        repaint();
    }

    void setSamples(const std::vector<float>& values)
    {
        if (values.empty())
        {
            buffer.clear();
            recomputeStats();
            repaint();
            return;
        }

        if (values.size() > maxPts)
            buffer.assign(values.end() - maxPts, values.end());
        else
            buffer.assign(values.begin(), values.end());

        recomputeStats();
        repaint();
    }

private:
    void timerCallback() override { repaint(); }

    void paint (juce::Graphics& g) override
    {
        RectPanel::paint (g);
        auto r = getLocalBounds().reduced (HSTheme::grid).toFloat();

        // Left margin for Y-axis label
        constexpr float leftMargin = 40.0f;
        auto plot = r.reduced(HSTheme::grid * 1.0f);
        plot.removeFromLeft(leftMargin);
        
        if (axisLabel.isNotEmpty())
        {
            g.saveState();
            g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                            r.getX() + 20, r.getCentreY()));
            g.setColour(HSTheme::TEXT_SECONDARY);
            g.setFont(HSTheme::label());
            g.drawText(axisLabel, juce::Rectangle<float>(r.getX(), r.getCentreY() - 40, 80, 20),
                       juce::Justification::centred);
            g.restoreState();
        }
        
        // Inner frame (ECG monitor style)
        g.setColour (juce::Colour (0xff003f3f)); // Major grid color
        g.drawRect (plot, 1.0f);

        // ECG-style grid lines
        // Major grid lines (darker teal)
        g.setColour (juce::Colour (0xff003f3f));
        for (int i = 1; i < 5; ++i)
        {
            auto y = plot.getY() + plot.getHeight() * i / 5.0f;
            g.drawLine (plot.getX(), y, plot.getRight(), y, 1.0f);
        }
        
        // Minor grid lines (very dark)
        g.setColour (juce::Colour (0xff001e1e));
        for (int i = 1; i < 25; ++i)
        {
            if (i % 5 == 0) continue; // Skip major lines
            auto y = plot.getY() + plot.getHeight() * i / 25.0f;
            g.drawLine (plot.getX(), y, plot.getRight(), y, 0.5f);
        }
        
        // Draw LAST / PEAK / MIN in top-right
        g.setColour(HSTheme::TEXT_SECONDARY);
        g.setFont(HSTheme::mono(10.0f, false));
        juce::String stats = juce::String::formatted("LAST %.0f  PEAK %.0f  MIN %.0f", 
                                                     lastValue, peakValue, minValue);
        g.drawText(stats, plot.removeFromTop(16).reduced(4, 0), juce::Justification::centredRight);

        if (buffer.size() < 2) return;

        // Scale with 12% headroom
        float lo = minValue;
        float hi = peakValue;

        if (fixedRange.has_value())
        {
            lo = fixedRange->getStart();
            hi = fixedRange->getEnd();
        }
        else
        {
            float range = hi - lo;
            if (range < 1.0f)
            {
                range = 10.0f;
                lo = lastValue - 5.0f;
                hi = lastValue + 5.0f;
            }

            const float headroom = range * 0.12f;
            lo -= headroom;
            hi += headroom;
        }
        
        // Draw waveform path (2px line width)
        juce::Path p; 
        int i = 0;
        for (auto v : buffer)
        {
            float x = plot.getX() + plot.getWidth() * (float) i / (float) (buffer.size() - 1);
            float y = juce::jmap (v, lo, hi, plot.getBottom(), plot.getY());
            if (i++ == 0) p.startNewSubPath (x, y); 
            else p.lineTo (x, y);
        }
        
        g.setColour (lineColour); 
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }

    static constexpr int maxPts = 300;
    std::deque<float> buffer;
    juce::Colour lineColour;
    float lastValue = 0.0f;
    float peakValue = 0.0f;
    float minValue = 0.0f;
    juce::String axisLabel{"BPM"};
    std::optional<juce::Range<float>> fixedRange;

    void recomputeStats()
    {
        if (buffer.empty())
        {
            lastValue = peakValue = minValue = 0.0f;
            return;
        }

        auto [mn, mx] = std::minmax_element(buffer.begin(), buffer.end());
        minValue = *mn;
        peakValue = *mx;
        lastValue = buffer.back();
    }
};

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "HSTheme.h"

/**
 * @brief Interactive parameter box matching Python's Entry widgets
 * 
 * Features:
 * - Boxed value display with 1-2px border (RIDGE-like)
 * - Mouse drag up/down to adjust (Shift=fine, Cmd=coarse)
 * - Double-click to type exact value
 * - Formatted display: "0 BPM", "0.1x", "0%"
 * - ~10 monospace chars wide, ~32px tall
 */
class ParamBox : public juce::Component
{
public:
    ParamBox(const juce::String& title, juce::Colour colour, 
             const juce::String& suffix, float minVal, float maxVal, float step)
        : titleText(title), rowColour(colour), valueSuffix(suffix),
          minValue(minVal), maxValue(maxVal), stepSize(step), currentValue(minVal)
    {
        setSize(120, 72); // Title + box
    }
    
    void setValue(float v)
    {
        currentValue = juce::jlimit(minValue, maxValue, v);
        repaint();
        if (onChange)
            onChange(currentValue);
    }
    
    float getValue() const { return currentValue; }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        
        // Title label at top
        auto titleArea = bounds.removeFromTop(20);
        g.setColour(rowColour);
        g.setFont(HSTheme::label());
        g.drawText(titleText, titleArea, juce::Justification::centredLeft);
        
        // Value box (RIDGE-like with double border)
        auto boxArea = bounds.reduced(0, 4);
        
        // Fill background
        g.setColour(HSTheme::SURFACE_BASE_START);
        g.fillRect(boxArea);
        
        // Outer darker border
        g.setColour(rowColour.darker(0.6f));
        g.drawRect(boxArea, 1.0f);
        
        // Inner bright border (inset)
        g.setColour(rowColour);
        g.drawRect(boxArea.reduced(2), 1.0f);
        
        // Value text (formatted)
        g.setColour(HSTheme::TEXT_PRIMARY);
        g.setFont(HSTheme::mono(13.0f, true));
        
        juce::String displayText;
        if (valueSuffix == "x")
            displayText = juce::String(currentValue, 1) + valueSuffix;
        else if (valueSuffix.contains("%"))
            displayText = juce::String(juce::roundToInt(currentValue)) + valueSuffix;
        else
            displayText = juce::String(juce::roundToInt(currentValue)) + " " + valueSuffix;
        
        g.drawText(displayText, boxArea.reduced(4), juce::Justification::centred);
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            if (e.getNumberOfClicks() == 2)
            {
                // Double-click: open text editor for exact value input
                startEdit();
            }
            else
            {
                // Single click: prepare for drag
                dragStartY = e.y;
                dragStartValue = currentValue;
            }
        }
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragStartY >= 0)
        {
            float delta = (dragStartY - e.y) * stepSize;
            
            // Modifiers
            if (e.mods.isShiftDown())
                delta *= 0.1f; // Fine adjustment
            if (e.mods.isCommandDown())
                delta *= 10.0f; // Coarse adjustment
            
            setValue(dragStartValue + delta);
        }
    }
    
    void mouseUp(const juce::MouseEvent&) override
    {
        dragStartY = -1;
    }
    
    std::function<void(float)> onChange;
    
private:
    void startEdit()
    {
        auto* editor = new juce::TextEditor();
        editor->setBounds(getLocalBounds().reduced(0, 24).reduced(4));
        editor->setFont(HSTheme::mono(13.0f, true));
        editor->setColour(juce::TextEditor::backgroundColourId, HSTheme::SURFACE_BASE_START);
        editor->setColour(juce::TextEditor::textColourId, HSTheme::TEXT_PRIMARY);
        editor->setColour(juce::TextEditor::outlineColourId, rowColour);
        editor->setText(juce::String(currentValue, 2));
        editor->selectAll();
        
        addAndMakeVisible(editor);
        editor->grabKeyboardFocus();
        
        editor->onReturnKey = [this, editor]() {
            float newVal = editor->getText().getFloatValue();
            setValue(newVal);
            removeChildComponent(editor);
            delete editor;
        };
        
        editor->onEscapeKey = [this, editor]() {
            removeChildComponent(editor);
            delete editor;
        };
        
        editor->onFocusLost = [this, editor]() {
            float newVal = editor->getText().getFloatValue();
            setValue(newVal);
            removeChildComponent(editor);
            delete editor;
        };
    }
    
    juce::String titleText;
    juce::Colour rowColour;
    juce::String valueSuffix;
    float minValue, maxValue, stepSize;
    float currentValue;
    
    int dragStartY = -1;
    float dragStartValue = 0.0f;
};

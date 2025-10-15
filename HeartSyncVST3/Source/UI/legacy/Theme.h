#pragma once

#include <JuceHeader.h>

/**
 * @brief HeartSync Theme Tokens
 * 
 * Centralized design system matching Python "Heart Sync System" aesthetic:
 * - Dark grid layout with neon quantum teal accents
 * - Medical red/gold/teal vital signs
 * - Monospaced UI font for technical precision
 */
namespace HeartSync
{

struct Colors
{
    // Surface & Background
    static inline const juce::Colour surfaceBase     = juce::Colour::fromString("FF000000"); // #000000
    static inline const juce::Colour surfacePanel    = juce::Colour::fromString("FF001111"); // #001111
    
    // Strokes & Borders
    static inline const juce::Colour panelStroke     = juce::Colour::fromString("FF00F5D4"); // Quantum teal
    static inline const juce::Colour panelStrokeAlt  = juce::Colour::fromString("FFFFD93D"); // Amber/gold
    static inline const juce::Colour panelStrokeWarn = juce::Colour::fromString("FFFF6B6B"); // Medical red
    
    // Text
    static inline const juce::Colour textPrimary     = juce::Colour::fromString("FFD6FFF5"); // #d6fff5
    static inline const juce::Colour textSecondary   = juce::Colour::fromString("FF00CCCC"); // #00cccc
    static inline const juce::Colour textMuted       = juce::Colour::fromString("FF7BAAA3"); // Muted teal
    
    // Accents & Vitals
    static inline const juce::Colour accentTeal      = juce::Colour::fromString("FF00F5D4"); // Quantum teal
    static inline const juce::Colour accentTealDark  = juce::Colour::fromString("FF00D4AA"); // Dark teal
    static inline const juce::Colour vitalHeartRate  = juce::Colour::fromString("FFFF6B6B"); // Medical red
    static inline const juce::Colour vitalSmoothed   = juce::Colour::fromString("FF00F5D4"); // Quantum teal
    static inline const juce::Colour vitalWetDry     = juce::Colour::fromString("FFFFD93D"); // Medical gold
    
    // Status
    static inline const juce::Colour statusConnected = juce::Colour::fromString("FF00F5D4"); // Quantum teal
    static inline const juce::Colour statusError     = juce::Colour::fromString("FFFF6B6B"); // Medical red
};

struct Metrics
{
    // Base unit (8px grid)
    static constexpr int unit = 8;
    
    // Border & Stroke
    static constexpr float borderRadius = 12.0f;
    static constexpr float strokeWidth = 2.0f;
    static constexpr float strokeWidthThick = 3.0f;
    
    // Spacing
    static constexpr int gap = 12;
    static constexpr int gapSmall = 6;
    static constexpr int gapLarge = 16;
    static constexpr int padding = 12;
    static constexpr int paddingLarge = 16;
    
    // Component sizes
    static constexpr int buttonHeight = 40;
    static constexpr int knobSize = 80;
    static constexpr int panelMinHeight = 120;
    
    // Font scaling
    static constexpr float fontScale = 1.0f;
};

struct Typography
{
    // Font sizes (scaled by Metrics::fontScale)
    static constexpr float sizeCaption = 9.0f;
    static constexpr float sizeSmall = 10.0f;
    static constexpr float sizeBody = 12.0f;
    static constexpr float sizeLabelPrimary = 13.0f;
    static constexpr float sizeLabelSecondary = 11.0f;
    static constexpr float sizeH1 = 20.0f;
    static constexpr float sizeDisplayVital = 48.0f;  // Large BPM display
    
    // Get scaled font
    static juce::Font getUIFont(float size = sizeBody, bool bold = false);
    static juce::Font getMonoFont(float size = sizeBody);
    static juce::Font getDisplayFont(float size = sizeH1, bool bold = true);
};

} // namespace HeartSync

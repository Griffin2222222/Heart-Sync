#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace HSTheme {
    // Colours (from Python HeartSyncProfessional.py)
    static inline const juce::Colour SURFACE_PANEL_LIGHT { 0xff001111 };
    static inline const juce::Colour SURFACE_BASE_START { 0xff000000 };
    static inline const juce::Colour TEXT_PRIMARY       { 0xffd6fff5 };
    static inline const juce::Colour TEXT_SECONDARY     { 0xff00cccc };
    static inline const juce::Colour STATUS_CONNECTED   { 0xff00f5d4 };
    static inline const juce::Colour STATUS_DISCONNECTED{ 0xff666666 };
    static inline const juce::Colour STATUS_SCANNING    { 0xffffd93d };
    static inline const juce::Colour STATUS_CONNECTING  { 0xff00cccc };
    static inline const juce::Colour STATUS_ERROR       { 0xffff6b6b };
    static inline const juce::Colour VITAL_HEART_RATE   { 0xffff6b6b };
    static inline const juce::Colour VITAL_SMOOTHED     { 0xff00f5d4 };
    static inline const juce::Colour VITAL_WET_DRY      { 0xffffd93d };
    static inline const juce::Colour ACCENT_TEAL        { 0xff00f5d4 };
    static inline const juce::Colour ACCENT_TEAL_DARK   { 0xff00d4aa };

    // Metrics (matching Python grid system)
    constexpr int   grid      = 12;     // base spacing (Python uses ~12px)
    constexpr float stroke    = 3.0f;   // outer border
    constexpr float inner     = 2.0f;   // inner border
    constexpr int   headerH   = 80;     // header bar height

    // Fonts
    inline juce::Font mono (float size, bool bold=true)
    {
        juce::Font f { juce::Font::getDefaultMonospacedFontName(), size,
                       bold ? juce::Font::bold : juce::Font::plain };
        // Prefer Menlo on macOS
       #if JUCE_MAC
        f.setTypefaceName ("Menlo");
       #endif
        return f;
    }

    inline juce::Font heading()   { return juce::Font (22.0f, juce::Font::bold); }
    inline juce::Font label()     { return juce::Font (14.0f, juce::Font::bold); }
    inline juce::Font caption()   { return juce::Font (11.0f); }
    inline juce::Font monoLarge() { return mono (32.0f, true); } // large centered value in metric tiles
}

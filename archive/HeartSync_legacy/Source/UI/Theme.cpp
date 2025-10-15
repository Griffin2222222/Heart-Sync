#include "Theme.h"

namespace HeartSync
{

juce::Font Typography::getUIFont(float size, bool bold)
{
    float scaledSize = size * Metrics::fontScale;
    
    // Try to use system monospace/geometric font
    auto font = juce::Font(juce::Font::getDefaultMonospacedFontName(), scaledSize, 
                           bold ? juce::Font::bold : juce::Font::plain);
    
    return font;
}

juce::Font Typography::getMonoFont(float size)
{
    float scaledSize = size * Metrics::fontScale;
    return juce::Font(juce::Font::getDefaultMonospacedFontName(), scaledSize, juce::Font::plain);
}

juce::Font Typography::getDisplayFont(float size, bool bold)
{
    float scaledSize = size * Metrics::fontScale;
    
    // Use sans-serif for large display numbers
    auto font = juce::Font(juce::Font::getDefaultSansSerifFontName(), scaledSize,
                           bold ? juce::Font::bold : juce::Font::plain);
    
    return font;
}

} // namespace HeartSync

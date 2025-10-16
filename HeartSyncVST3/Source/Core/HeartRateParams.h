#pragma once

#include <juce_core/juce_core.h>

namespace HeartRateParams
{
    constexpr auto SMOOTHING_FACTOR = "smoothingFactor";
    constexpr auto HEART_RATE_OFFSET = "heartRateOffset";
    constexpr auto WET_DRY_OFFSET = "wetDryOffset";
    constexpr auto WET_DRY_SOURCE = "wetDrySource";
    constexpr auto OSC_ENABLED = "oscEnabled";
    constexpr auto OSC_PORT = "oscPort";
    constexpr auto OSC_HOST = "oscHost";
    constexpr auto UI_LOCKED = "uiLocked";
}

cmake_minimum_required(VERSION 3.22)

project(HeartSync VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS deployment version")

# Option to use external BLE bridge process (default: ON)
option(HEARTSYNC_USE_BRIDGE "Use external BLE bridge process instead of direct CoreBluetooth" ON)

# Use FetchContent to get JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.9
)
FetchContent_MakeAvailable(JUCE)

# Add the plugin target
juce_add_plugin(HeartSync
    COMPANY_NAME "Quantum Bio Audio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE QBA0
    PLUGIN_CODE HS54
    FORMATS VST3 AU
    PRODUCT_NAME "HeartSync"
    BUNDLE_ID "com.quantumbioaudio.heartsync"
)

# Generate JuceHeader.h
juce_generate_juce_header(HeartSync)

# Source files - NO CoreBluetooth code when using helper
target_sources(HeartSync PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Core/HeartRateProcessor.cpp
)

# Add BLE client when using bridge mode
if(HEARTSYNC_USE_BRIDGE)
    target_sources(HeartSync PRIVATE Source/Core/HeartSyncBLEClient.cpp)
endif()

# Link JUCE modules
target_link_libraries(HeartSync PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
)

# Compiler definitions
target_compile_definitions(HeartSync PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_PLUGINHOST_VST3=0
    JUCE_PLUGINHOST_AU=0
)

# Define HEARTSYNC_USE_BRIDGE for plugin code
if(HEARTSYNC_USE_BRIDGE)
    target_compile_definitions(HeartSync PRIVATE HEARTSYNC_USE_BRIDGE=1)
    message(STATUS "HeartSync: Using external BLE bridge (no CoreBluetooth in plugin)")
else()
    # Legacy mode: direct CoreBluetooth (causes TCC crashes in DAWs)
    target_sources(HeartSync PRIVATE Source/Core/HeartSyncBLE.mm)
    target_link_libraries(HeartSync PRIVATE
        "-framework CoreBluetooth"
        "-framework Foundation"
    )
    message(STATUS "HeartSync: Using direct CoreBluetooth (NOT RECOMMENDED)")
endif()

# ============================================================================
# HeartSync BLE Bridge App (headless background worker for BLE communication)
# ============================================================================
if(HEARTSYNC_USE_BRIDGE)
    add_executable(HeartSyncBridge MACOSX_BUNDLE
        BridgeApp/HeartSyncBridge.m
    )
    
    set_target_properties(HeartSyncBridge PROPERTIES
        BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/BridgeApp/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "HeartSync Bridge"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.quantumbioaudio.heartsync.bridge"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        OUTPUT_NAME "HeartSync Bridge"
        COMPILE_FLAGS "-fobjc-arc"
    )
    
    target_link_libraries(HeartSyncBridge PRIVATE
        "-framework Foundation"
        "-framework CoreBluetooth"
        "-framework AppKit"
    )
    
    # Install bridge to ~/Applications for development
    install(TARGETS HeartSyncBridge
        BUNDLE DESTINATION "$ENV{HOME}/Applications"
    )
    
    message(STATUS "HeartSync Bridge: Will install to ~/Applications/HeartSync Bridge.app")
endif()

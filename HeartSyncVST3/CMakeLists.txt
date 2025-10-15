cmake_minimum_required(VERSION 3.22)

project(HeartSyncVST3 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect compiler and configure accordingly
if(MSVC)
    message(STATUS "Configuring for MSVC")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_definitions(WINRT_LEAN_AND_MEAN HEARTSYNC_USE_WINRT=1)
    set(HEARTSYNC_USE_WINRT ON)
elseif(MINGW)
    message(STATUS "Configuring for MinGW")
    # MinGW static linking
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
    add_compile_definitions(HEARTSYNC_USE_WINRT=0)
    set(HEARTSYNC_USE_WINRT OFF)
endif()

# Add JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.9
)
FetchContent_MakeAvailable(JUCE)

# Create the plugin target
juce_add_plugin(HeartSyncVST3
    COMPANY_NAME "Conscious Audio"
    BUNDLE_ID "com.consciousaudio.heartsync"
    PLUGIN_MANUFACTURER_CODE ConA
    PLUGIN_CODE HSyc
    FORMATS VST3 Standalone
    PRODUCT_NAME "HeartSync"
    PLUGIN_NAME "HeartSync"
    DESCRIPTION "Professional heart rate reactive modulation plugin with Bluetooth LE support"
    PLUGIN_VERSION "1.0.0"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE)

# Add source files (progressive professional build)
target_sources(HeartSyncVST3 PRIVATE
    Source/PluginProcessor_Professional.cpp
    Source/PluginProcessor_Professional.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Core/BluetoothManager.h
    Source/Core/BluetoothManager_Native.mm)

# Link JUCE modules to the plugin target
target_link_libraries(HeartSyncVST3 PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# Platform-specific libraries
if(WIN32)
    if(HEARTSYNC_USE_WINRT)
        # MSVC with full WinRT support
        target_link_libraries(HeartSyncVST3 PRIVATE 
            windowsapp
            runtimeobject
        )
        message(STATUS "Linking WinRT libraries for MSVC")
    else()
        # MinGW with Windows libraries but no WinRT
        target_link_libraries(HeartSyncVST3 PRIVATE 
            ws2_32
            winmm
        )
        message(STATUS "Linking standard Windows libraries for MinGW")
    endif()
elseif(APPLE)
    # macOS Core Bluetooth framework for Bluetooth LE support
    target_link_libraries(HeartSyncVST3 PRIVATE 
        "-framework CoreBluetooth"
        "-framework Foundation"
    )
    message(STATUS "Linking Core Bluetooth framework for macOS")
endif()

# Preprocessor definitions
target_compile_definitions(HeartSyncVST3 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

# Compiler-specific configurations
if(MSVC)
    target_compile_options(HeartSyncVST3 PRIVATE /MT$<$<CONFIG:Debug>:d>)
    message(STATUS "MSVC static runtime configured")
elseif(MINGW)
    target_compile_options(HeartSyncVST3 PRIVATE -O2)
    message(STATUS "MinGW optimization configured")
endif()


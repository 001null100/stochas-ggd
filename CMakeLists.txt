# This script has been adapted from the example script that comes with JUCE in the cmake example dir.
# Original closed-source Stochas code used the projucer
#
# To build stochas-ggd:
#   git submodule update --init --recursive
#   cmake -B build -A x64 -DSTOCHAS_VERSION=xxx
#   cmake --build build --config Release --target stochas_CLAP
#
cmake_minimum_required(VERSION 3.16)

# always statically link c runtime on windows
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# default to 10.11 (earlier versions will fail)
if (NOT DEFINED STOCHAS_MAC_SDK_VER)
  set(STOCHAS_MAC_SDK_VER 10.11)
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET ${STOCHAS_MAC_SDK_VER} CACHE STRING "Build for ${STOCHAS_MAC_SDK_VER}")

if (NOT DEFINED STOCHAS_VERSION)
  set (STOCHAS_VERSION 1.3.10)
endif()

# we don't want to mess with jack on mac
set(STOCHAS_JACK 0)
if (UNIX AND NOT APPLE)
  set(STOCHAS_JACK 1)
endif()

project(StochasGGD VERSION ${STOCHAS_VERSION})

option(STOCHAS_COPY_AFTER_BUILD FALSE "Copy after build")

set(CMAKE_CXX_STANDARD 17)

message( STATUS "Building Stochas GGD with version ${STOCHAS_VERSION}" )
if( APPLE )
  message( STATUS "Building using ${CMAKE_OSX_DEPLOYMENT_TARGET}" )
endif()

# synth defaults to true
if (NOT DEFINED STOCHAS_IS_SYNTH)
  set(STOCHAS_IS_SYNTH "TRUE")
endif()

# midi effect defaults to true
if (NOT DEFINED STOCHAS_IS_MIDI_EFFECT)
  set(STOCHAS_IS_MIDI_EFFECT "TRUE")
endif()
message( STATUS "Is Synth: ${STOCHAS_IS_SYNTH}, Is Midi Effect: ${STOCHAS_IS_MIDI_EFFECT}")
set(STOCHAS_CATEGORY "Effect")
add_subdirectory(lib/JUCE)

# Keep the JSON files under maps/ggd authoritative while making them available
# inside the plugin binary. This avoids relying on external map files at runtime.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/geninclude")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/maps/ggd/p-v.json" GGD_MAP_PV_JSON)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/maps/ggd/p-iv.json" GGD_MAP_PIV_JSON)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/maps/ggd/modern-and-massive.json" GGD_MAP_MM_JSON)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GgdBuiltInMaps.h.in"
  "${CMAKE_BINARY_DIR}/geninclude/GgdBuiltInMaps.h"
  @ONLY
)

add_custom_target( git-info BYPRODUCTS ${CMAKE_BINARY_DIR}/geninclude/version.cpp
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -D PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
                           -D PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
                           -D PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
                           -D STOCHASSRC=${CMAKE_SOURCE_DIR} -D STOCHASBLD=${CMAKE_BINARY_DIR}
                           -D AZURE_PIPELINE=${AZURE_PIPELINE}
                           -P ${CMAKE_SOURCE_DIR}/cmake/versiontools.cmake
                           )

# if you want vst2, you need to specify VST2_PATH in your environment
if (DEFINED VST2_PATH)
  message( STATUS "Building VST2. SDK at ${VST2_PATH}")
  juce_set_vst2_sdk_path(${VST2_PATH})
  set(VST2 "VST")
endif()

juce_add_plugin(stochas
    COMPANY_NAME "Stochas GGD"
    BUNDLE_ID "local.stochas-ggd"
    DESCRIPTION "GGD-focused drum pattern sequencer"
    ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/image/app_logo_512.png"
    IS_SYNTH ${STOCHAS_IS_SYNTH}
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT ${STOCHAS_IS_MIDI_EFFECT}
    COPY_PLUGIN_AFTER_BUILD ${STOCHAS_COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE AuVi
    PLUGIN_CODE Stoc
    FORMATS VST3 ${VST2} AU Standalone
    VST3_CATEGORIES ${STOCHAS_CATEGORY}
    USE_LEGACY_COMPATIBILITY_PLUGIN_CODE TRUE
    VST3_CAN_REPLACE_VST2 TRUE
    PRODUCT_NAME "Stochas GGD")

add_subdirectory(lib/clap-juce-extensions EXCLUDE_FROM_ALL)
clap_juce_extensions_plugin(TARGET stochas
        CLAP_ID "local.stochas-ggd"
        CLAP_FEATURES "note-effect" "sequencer")

add_dependencies(stochas git-info)
include_directories(src)
target_include_directories(stochas PRIVATE "${CMAKE_BINARY_DIR}/geninclude")

juce_generate_juce_header(stochas)

target_sources(stochas PRIVATE
    src/ChainDialog.cpp
    src/CommonComponents.cpp
    src/EditDialog.cpp
    src/EditorState.cpp
    src/FileDialog.cpp
    src/GgdDrumEditor.cpp
    src/GgdKitMap.cpp
    src/HelpBanner.cpp
    src/InfoDialog.cpp
    src/MidiDialog.cpp
    src/NotePanel.cpp
    src/OptionsPanel.cpp
    src/pcg_basic.c
    src/Persist.cpp
    src/PlayPanel.cpp
    src/PluginEditor.cpp
    src/PluginProcessor.cpp
    src/Scale.cpp
    src/SeqRandom.cpp
    src/SequenceData.cpp
    src/SettingsTab.cpp
    src/StepPanel.cpp
    src/StochaEngine.cpp

    ${CMAKE_BINARY_DIR}/geninclude/version.cpp
    )

target_compile_definitions(stochas
    PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_JACK=${STOCHAS_JACK}
    JUCE_ALSA=1
)

juce_add_binary_data(assets NAMESPACE SeqImageX SOURCES
    image/exclamation-64.png
    image/not.png
    image/play.png
    image/play2.png
    image/logo_no_bg.png
    image/x-mark-4-64.png
    text/infobox.txt
    )

set_target_properties(assets PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(stochas PRIVATE
  assets
  juce::juce_audio_utils
  juce::juce_audio_plugin_client
  )

if( APPLE )
  add_custom_target( install-au-local )
  add_dependencies( install-au-local stochas_AU )
  add_custom_command(
    TARGET install-au-local
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND echo "Installing audio unit locally"
    COMMAND rsync -r --delete "${ZIP_FROM_DIR}/AU/Stochas GGD.component/" "\${HOME}/Library/Audio/Plug-Ins/Components/Stochas GGD.component/"
    COMMAND auval -vt aumi AuVi
    )
endif()

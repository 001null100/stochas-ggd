# Stochas GGD - CLAP drum sequencer for GetGood Drums libraries.
#
# To build:
#   git submodule update --init --recursive
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --target stochas_CLAP
#
# Developer targets (engine/persistence tests and offscreen UI snapshots):
#   cmake -S . -B build -DSTOCHAS_BUILD_TESTS=ON
#   cmake --build build --target stochas_tests
#
cmake_minimum_required(VERSION 3.22)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT DEFINED STOCHAS_MAC_SDK_VER)
  set(STOCHAS_MAC_SDK_VER 10.13)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET ${STOCHAS_MAC_SDK_VER} CACHE STRING "Build for ${STOCHAS_MAC_SDK_VER}")

if (NOT DEFINED STOCHAS_VERSION)
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" STOCHAS_VERSION LIMIT_COUNT 1)
  # Re-run configuration when the version file changes.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
endif()

set(STOCHAS_JACK 0)
if (UNIX AND NOT APPLE)
  set(STOCHAS_JACK 1)
endif()

project(StochasGGD VERSION ${STOCHAS_VERSION})
option(STOCHAS_COPY_AFTER_BUILD "Copy plug-ins to the system folders after building" FALSE)
option(STOCHAS_BUILD_TESTS "Build engine tests and the UI snapshot tool" FALSE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "Building Stochas GGD with version ${STOCHAS_VERSION}")

if (NOT DEFINED STOCHAS_IS_SYNTH)
  set(STOCHAS_IS_SYNTH "TRUE")
endif()
if (NOT DEFINED STOCHAS_IS_MIDI_EFFECT)
  set(STOCHAS_IS_MIDI_EFFECT "TRUE")
endif()

set(STOCHAS_CATEGORY "Effect")
add_subdirectory(lib/JUCE)

# Built-in kit maps are compiled in from the authoritative JSON files.
set(GGD_MAP_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/maps/ggd/p-v.json"
  "${CMAKE_CURRENT_SOURCE_DIR}/maps/ggd/p-iv.json"
  "${CMAKE_CURRENT_SOURCE_DIR}/maps/ggd/modern-and-massive.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GGD_MAP_FILES})
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/geninclude")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/maps/ggd/p-v.json" GGD_MAP_PV_JSON)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/maps/ggd/p-iv.json" GGD_MAP_PIV_JSON)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/maps/ggd/modern-and-massive.json" GGD_MAP_MM_JSON)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GgdBuiltInMaps.h.in"
  "${CMAKE_BINARY_DIR}/geninclude/GgdBuiltInMaps.h"
  @ONLY
)

add_custom_target(git-info BYPRODUCTS ${CMAKE_BINARY_DIR}/geninclude/version.cpp
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -D PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
                           -D PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
                           -D PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
                           -D STOCHASSRC=${CMAKE_SOURCE_DIR} -D STOCHASBLD=${CMAKE_BINARY_DIR}
                           -D AZURE_PIPELINE=${AZURE_PIPELINE}
                           -P ${CMAKE_SOURCE_DIR}/cmake/versiontools.cmake
)

if (DEFINED VST2_PATH)
  juce_set_vst2_sdk_path(${VST2_PATH})
  set(VST2 "VST")
endif()

juce_add_plugin(stochas
    COMPANY_NAME "Stochas GGD"
    BUNDLE_ID "local.stochas-ggd"
    DESCRIPTION "GGD-focused drum pattern sequencer"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/image/app_logo_512.png"
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

# Engine, data model, persistence and file formats. No UI dependencies.
set(STOCHAS_CORE_SOURCES
    src/GgdEngine.cpp
    src/GgdEventModel.cpp
    src/GgdEventPersist.cpp
    src/GgdKitMap.cpp
    src/GgdMidiExporter.cpp
    src/GgdMidiImporter.cpp
    src/GgdPatternFile.cpp
    src/GgdPatternOps.cpp
    src/GgdProject.cpp
    src/GgdTimeline.cpp
    src/Persist.cpp
    src/Scale.cpp
    src/SequenceData.cpp
)

set(STOCHAS_UI_SOURCES
    src/GgdDrumEditor.cpp
    src/GgdDrumGrid.cpp
    src/GgdLibraryBrowser.cpp
    src/GgdPadEditor.cpp
    src/GgdSettingsPanel.cpp
    src/GgdUiTheme.cpp
)

target_sources(stochas PRIVATE
    ${STOCHAS_CORE_SOURCES}
    ${STOCHAS_UI_SOURCES}
    src/PluginProcessor.cpp
    ${CMAKE_BINARY_DIR}/geninclude/version.cpp
)

target_compile_definitions(stochas
    PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_JACK=${STOCHAS_JACK}
    JUCE_ALSA=1
)

# Embedded UI fonts (Inter, SIL OFL - see resources/fonts/OFL.txt).
juce_add_binary_data(stochas_assets
    HEADER_NAME GgdAssets.h
    NAMESPACE GgdAssets
    SOURCES
      resources/fonts/Inter-Regular.otf
      resources/fonts/Inter-Medium.otf
      resources/fonts/Inter-SemiBold.otf
      resources/fonts/Inter-Bold.otf)

target_link_libraries(stochas PRIVATE
    stochas_assets
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
)

if (STOCHAS_BUILD_TESTS)
  # Fast unit tests of the engine, timeline and persistence (no UI).
  juce_add_console_app(stochas_core_tests PRODUCT_NAME "Stochas GGD Core Tests")
  juce_generate_juce_header(stochas_core_tests)
  add_dependencies(stochas_core_tests git-info)
  target_include_directories(stochas_core_tests PRIVATE src "${CMAKE_BINARY_DIR}/geninclude")
  target_sources(stochas_core_tests PRIVATE
      tests/CoreTests.cpp
      tests/TestMain.cpp
      ${STOCHAS_CORE_SOURCES}
      ${CMAKE_BINARY_DIR}/geninclude/version.cpp)
  target_compile_definitions(stochas_core_tests PRIVATE
      JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 STOCHAS_CORE_TESTS_ONLY=1)
  target_link_libraries(stochas_core_tests PRIVATE
      juce::juce_core juce::juce_data_structures juce::juce_events juce::juce_audio_basics
      juce::juce_recommended_config_flags)

  # Processor-level tests and the offscreen UI snapshot tool link the
  # complete plug-in code.
  add_executable(stochas_tests tests/TestMain.cpp tests/CoreTests.cpp tests/PluginTests.cpp)
  target_link_libraries(stochas_tests PRIVATE stochas)
  target_include_directories(stochas_tests PRIVATE $<TARGET_PROPERTY:stochas,INCLUDE_DIRECTORIES>)
  target_compile_definitions(stochas_tests PRIVATE
      $<TARGET_PROPERTY:stochas,COMPILE_DEFINITIONS>)
endif()

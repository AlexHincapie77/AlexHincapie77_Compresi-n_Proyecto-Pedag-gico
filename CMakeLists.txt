cmake_minimum_required(VERSION 3.22)

project(PedagogicalCompressor VERSION 1.2.4 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ─── JUCE 8 ──────────────────────────────────────────────────────────────
option(PEDACOMP_USE_SYSTEM_JUCE "Use a system-provided JUCE checkout" OFF)

if(NOT PEDACOMP_USE_SYSTEM_JUCE)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ─── CLAP support ────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        6bde108329ff9fc24404b79b8b397261a75cf6a8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(clap_juce_extensions)
include(${clap_juce_extensions_SOURCE_DIR}/cmake/ClapTargetHelpers.cmake)

# ─── Formatos por plataforma ────────────────────────────────────────────
if(APPLE)
    set(PEDACOMP_FORMATS VST3 AU Standalone)
else()
    set(PEDACOMP_FORMATS VST3 LV2 Standalone)
endif()

# ─── Plugin target ───────────────────────────────────────────────────────
juce_add_plugin(PedagogicalCompressor
    COMPANY_NAME           "PedagogicalAudio"
    PLUGIN_MANUFACTURER_CODE Peda
    PLUGIN_CODE            Pdcm
    FORMATS                ${PEDACOMP_FORMATS}
    IS_SYNTH               FALSE
    NEEDS_MIDI_INPUT       FALSE
    NEEDS_MIDI_OUTPUT      FALSE
    IS_MIDI_EFFECT         FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PRODUCT_NAME           "Pedagogical Compressor"
    LV2URI                 "https://pedagogicalaudio.com/compressor"
)

clap_juce_extensions_plugin(TARGET PedagogicalCompressor
    CLAP_ID "com.pedagogicalaudio.compressor"
    CLAP_FEATURES "audio-effect" "compressor" "stereo")

target_sources(PedagogicalCompressor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# Necesario para #include "clap-juce-extensions/clap-juce-extensions.h"
# en PluginProcessor.h (heredar clap_properties). Sin esto, VST3/LV2/Standalone
# no encuentran el header y solo el target CLAP compilaría.
target_include_directories(PedagogicalCompressor PUBLIC
    ${clap_juce_extensions_SOURCE_DIR}/include
)

target_compile_definitions(PedagogicalCompressor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(PedagogicalCompressor PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
)

# Dependencias del sistema para ALSA (sonido nativo Linux en Standalone).
# JUCE pide símbolos ALSA (snd_*) al enlazar el Standalone y los wrappers.
# Solo aplica en Linux.  macOS y Windows no necesitan esto.
if(NOT APPLE AND NOT WIN32)
    find_library(ALSA_LIB asound)
    if(ALSA_LIB)
        target_link_libraries(PedagogicalCompressor PRIVATE ${ALSA_LIB})
    else()
        # Fallback: link directo al nombre (asume que está en PATH estándar)
        target_link_libraries(PedagogicalCompressor PRIVATE asound)
    endif()
endif()

# NOTA: juce_recommended_lto_flags ELIMINADO — LTO con GCC 13 corrompe RTTI
target_link_libraries(PedagogicalCompressor PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

include(GNUInstallDirs)
install(TARGETS PedagogicalCompressor
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/PedagogicalCompressor)

message(STATUS "")
message(STATUS "Pedagogical Compressor targets:")
message(STATUS "  VST3       -> $<TARGET_FILE:PedagogicalCompressor_VST3>")
if(NOT APPLE)
    message(STATUS "  LV2        -> $<TARGET_FILE:PedagogicalCompressor_LV2>")
else()
    message(STATUS "  AU         -> $<TARGET_FILE:PedagogicalCompressor_AU>")
endif()
message(STATUS "  CLAP       -> $<TARGET_FILE:PedagogicalCompressor_CLAP>")
message(STATUS "  Standalone -> $<TARGET_FILE:PedagogicalCompressor_Standalone>")
message(STATUS "")

# ─── Mini-host validador (opcional) ──────────────────────────────────────
option(PEDACOMP_BUILD_VALIDATOR "Compilar mini-host de validación VST3/LV2/CLAP" OFF)

if(PEDACOMP_BUILD_VALIDATOR)
    add_subdirectory(scripts/validator_host EXCLUDE_FROM_ALL)
endif()

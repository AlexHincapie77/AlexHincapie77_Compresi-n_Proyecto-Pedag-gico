cmake_minimum_required(VERSION 3.22)

# ─── Project (debe ir antes que FetchContent) ───────────────────────────
project(PedagogicalCompressor VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ─── JUCE ─────────────────────────────────────────────────────────────────
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

# ─── CLAP support (clap-juce-extensions) ────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        6bde108329ff9fc24404b79b8b397261a75cf6a8  # pin exacto (main probado) = builds reproducibles
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(clap_juce_extensions)

# El helper .cmake que define clap_juce_extensions_plugin no se carga
# automáticamente con FetchContent_MakeAvailable, hay que incluirlo explícitamente.
include(${clap_juce_extensions_SOURCE_DIR}/cmake/ClapTargetHelpers.cmake)

# El formato AU (Audio Unit) solo existe en macOS; en Linux/Windows se ignora
# automáticamente, así que se puede declarar siempre de forma segura.
if(APPLE)
    set(PEDACOMP_FORMATS VST3 AU Standalone)
else()
    set(PEDACOMP_FORMATS VST3 LV2 Standalone)
endif()

# ─── Plugin target ──────────────────────────────────────────────────────
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

# Add CLAP target using the helper from clap-juce-extensions
clap_juce_extensions_plugin(TARGET PedagogicalCompressor
    CLAP_ID "com.pedagogicalaudio.compressor"
    CLAP_FEATURES "audio-effect" "compressor" "stereo")

target_sources(PedagogicalCompressor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
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

target_link_libraries(PedagogicalCompressor PUBLIC
    juce::juce_recommended_config_flags
    # NOTA: juce_recommended_lto_flags ELIMINADO — LTO con GCC 13 (Mint 22 /
    # Ubuntu 24.04) corrompe los lambdas de XWindowSystem::initialiseXDisplay
    # en JUCE 8, causando SIGSEGV en __dynamic_cast al arrancar la GUI.
    juce::juce_recommended_warning_flags
)

# ─── Install (opcional — solo si se hace cmake --install) ───────────────
include(GNUInstallDirs)

install(TARGETS PedagogicalCompressor
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/PedagogicalCompressor)

# ─── Arnés de estrés del editor (solo desarrollo, OFF por defecto) ──────
option(PEDACOMP_BUILD_TEST_HARNESS "Compilar el arnés de estrés del editor (dev)" OFF)

if(PEDACOMP_BUILD_TEST_HARNESS)
    add_executable(StateDebug
        extras/StateDebug.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(StateDebug PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JucePlugin_Name="Pedagogical Compressor")
    target_link_libraries(StateDebug PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags)

    add_executable(EditorStressHarness
        extras/EditorStressHarness.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(EditorStressHarness PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Pedagogical Compressor")
    target_link_libraries(EditorStressHarness PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags)
endif()

# Helpful summary message after build
message(STATUS "")
message(STATUS "Pedagogical Compressor targets:")
message(STATUS "  • VST3       -> $<TARGET_FILE:PedagogicalCompressor_VST3>")
if(NOT APPLE)
    message(STATUS "  • LV2        -> $<TARGET_FILE:PedagogicalCompressor_LV2>")
else()
    message(STATUS "  • AU         -> $<TARGET_FILE:PedagogicalCompressor_AU>")
endif()
message(STATUS "  • CLAP       -> $<TARGET_FILE:PedagogicalCompressor_CLAP>")
message(STATUS "  • Standalone -> $<TARGET_FILE:PedagogicalCompressor_Standalone>")
message(STATUS "")

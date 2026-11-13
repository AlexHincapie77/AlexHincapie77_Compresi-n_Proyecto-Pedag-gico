juce_add_console_app(validate_all
    NEEDS_CURL FALSE)

target_sources(validate_all PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/validate_all.cpp)

target_compile_definitions(validate_all PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    # Habilitar el hosting de cada formato en el mini-host de validación
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_LV2=1
    JUCE_PLUGINHOST_CLAP=1
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_AAX=0
)

target_link_libraries(validate_all PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    # El soporte CLAP de JUCE requiere el wrapper estático de clap-juce-extensions
    $<TARGET_OBJECTS:clap_juce_extensions>
)
target_include_directories(validate_all PRIVATE
    ${clap_juce_extensions_SOURCE_DIR}/include
)

target_link_libraries(validate_all PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

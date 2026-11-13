#pragma once

#include "PluginProcessor.h"
#include "Gui/BandSplitVisualizer.h"
#include "Gui/CompressionCurveComponent.h"
#include "Gui/LevelMeterComponent.h"
#include "Gui/GainReductionMeter.h"
#include "Gui/EnvelopeVisualizer.h"

// ===========================================================================
//  PedagogicalCompressorAudioProcessorEditor  (v1.2.1 — UI pedagógica)
//
//  Principios de diseño:
//    1. FUENTES GRANDES: 14pt mínimo, 16-20pt para valores, 22-26pt títulos.
//    2. PRESET VISIBLE: ComboBox grande con nombre del preset actual.
//    3. GR METER CENTRAL: el GR meter es lo más visible de la pantalla.
//    4. COLORES POR BANDA: cada banda tiene su color consistente.
//    5. TOOLTIPS PEDAGÓGICOS en español: cada control explica QUÉ HACE.
//    6. AGRUPACIÓN LÓGICA: globales arriba, banda en su grupo.
// ===========================================================================

class PedagogicalCompressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                    private juce::ComboBox::Listener,
                                                    private juce::Slider::Listener,
                                                    private juce::Button::Listener,
                                                    private juce::Timer
{
public:
    explicit PedagogicalCompressorAudioProcessorEditor(PedagogicalCompressorAudioProcessor&);
    ~PedagogicalCompressorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void comboBoxChanged(juce::ComboBox* cb) override;
    void sliderValueChanged(juce::Slider* s) override;
    void buttonClicked(juce::Button* b) override;
    void buttonStateChanged(juce::Button*) override {}

    void rebuildBandControls();
    void refreshComboBoxSelections();
    void refreshMeterValues();

    PedagogicalCompressorAudioProcessor& processor;

    // ─── Globales: header ────────────────────────────────────────────
    juce::Label titleLabel;       // "PEDAGOGICAL COMPRESSOR" (grande)
    juce::Label subtitleLabel;    // "Compresor multibanda pedagógico"
    juce::Label presetNameLabel;  // Muestra el nombre del preset actual

    juce::ComboBox numBandsCombo;
    juce::ComboBox presetCombo;
    juce::Label numBandsLabel;
    juce::Label presetLabel;

    // Globales
    juce::Slider inputGainSlider, outputGainSlider, limiterCeilingSlider, globalMixSlider;
    juce::Label inputGainLabel, outputGainLabel, limiterCeilingLabel, globalMixLabel;
    juce::Label inputGainValue, outputGainValue, limiterCeilingValue, globalMixValue;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboBoxAttachment> numBandsAttachment;
    std::unique_ptr<ComboBoxAttachment> presetAttachment;
    std::unique_ptr<SliderAttachment> inputGainAttachment;
    std::unique_ptr<SliderAttachment> outputGainAttachment;
    std::unique_ptr<SliderAttachment> limiterCeilingAttachment;
    std::unique_ptr<SliderAttachment> globalMixAttachment;

    juce::TextButton bypassButton;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    // Auto-Gain global (v1.2)
    juce::TextButton autoGainButton;
    std::unique_ptr<ButtonAttachment> autoGainAttachment;
    juce::Label autoGainReadout;

    // ─── Por banda ───────────────────────────────────────────────────
    struct BandControls {
        juce::GroupComponent group;
        juce::Slider threshold, ratio, attack, release, knee, makeup, mix;
        juce::Slider scHpf, lookahead, stereoLink, saturation;
        juce::ComboBox type, detector;
        juce::TextButton bypass, extSC;
        juce::TextButton autoMakeup;
        // Labels de parámetros (pequeños pero legibles)
        juce::Label thrLabel, ratLabel, attLabel, relLabel, kneeLabel, mkLabel, mixLabel;
        juce::Label typeLabel, detLabel, scHpfLabel, lookaheadLabel, slLabel;
        juce::Label satLabel;
        // Valores actuales en dB (legibles, grandes)
        juce::Label thrValue, attValue, relValue, mkValue;
        std::unique_ptr<SliderAttachment> thrAtt, ratAtt, attAtt, relAtt, kneeAtt, mkAtt, mixAtt;
        std::unique_ptr<SliderAttachment> scHpfAtt, laAtt, slAtt, satAtt;
        std::unique_ptr<ComboBoxAttachment> typeAtt, detAtt;
        std::unique_ptr<ButtonAttachment> bypassAtt, extScAtt, amkAtt;
        // Mini GR meter por banda
        std::unique_ptr<GainReductionMeter> grMiniMeter;
    };
    BandControls bands[MultibandProcessor::kMaxBands];

    // ─── Visualizadores ──────────────────────────────────────────────
    BandSplitVisualizer bandSplitVisualizer;
    CompressionCurveComponent compressionCurve;
    LevelMeterComponent inputMeter { LevelMeterComponent::Type::Input };
    LevelMeterComponent outputMeter { LevelMeterComponent::Type::Output };
    GainReductionMeter grMeter;  // GR meter grande central
    EnvelopeVisualizer envelopeVisualizer;

    // Para saber qué banda está seleccionada
    int selectedBandForCurve = 0;

    // Helper: slider con tooltip pedagógico
    void setupSliderPedagogical(juce::Slider& s, juce::Label& l, juce::Label& v,
                                 const juce::String& name, const juce::String& tooltip,
                                 const juce::Colour& color);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedagogicalCompressorAudioProcessorEditor)
};

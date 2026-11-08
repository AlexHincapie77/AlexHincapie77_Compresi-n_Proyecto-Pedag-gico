/*
 * PluginEditor.h — versión simplificada
 */

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <array>
#include "Gui/CompressionCurveComponent.h"
#include "Gui/LevelMeterComponent.h"
#include "Gui/BandSplitVisualizer.h"
#include "PluginProcessor.h"

class PedagogicalCompressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                     private juce::Timer
{
public:
    explicit PedagogicalCompressorAudioProcessorEditor (PedagogicalCompressorAudioProcessor&);
    ~PedagogicalCompressorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshFromProcessor();
    void updateTooltips();

    PedagogicalCompressorAudioProcessor& processorRef;

    BandSplitVisualizer bandSplit;
    CompressionCurveComponent curve;
    LevelMeterComponent inputMeter  { LevelMeterComponent::Type::Level };
    LevelMeterComponent outputMeter { LevelMeterComponent::Type::Level };
    LevelMeterComponent grMeter     { LevelMeterComponent::Type::GainReduction };

    juce::ComboBox presetCombo;
    juce::ComboBox bandCountCombo;

    juce::Slider inputGainSlider, outputGainSlider, mixSlider;
    juce::Label  inputGainLabel,  outputGainLabel,  mixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAtt, outputGainAtt, mixAtt;

    juce::TextButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;

    juce::TextButton limiterButton { "Limiter ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limiterAtt;

    struct BandControls
    {
        juce::GroupComponent group;
        juce::ComboBox typeCombo;
        juce::Slider threshold, ratio, attack, release, knee, makeup, mix, scHpf, lookahead;
        juce::Label  thresholdL, ratioL, attackL, releaseL, kneeL, makeupL, mixL, scHpfL, lookaheadL;
        juce::ComboBox detectionCombo, oversamplingCombo;
        juce::Label  detectionL, oversamplingL;
        juce::ToggleButton enableToggle, bypassToggle, extScToggle;
        juce::Label  enableL, bypassL, extScL;
        LevelMeterComponent inputMeter { LevelMeterComponent::Type::Level };
        LevelMeterComponent grMeter    { LevelMeterComponent::Type::GainReduction };
        juce::Label infoText;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt, detAtt, osAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thrAtt, ratAtt, attAtt, relAtt,
            knAtt, mkAtt, mixAtt, scHpfAtt, lkAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enAtt, bpAtt, extScAtt;
    };

    std::array<std::unique_ptr<BandControls>, 4> bandControls;

    juce::Label pedagogicalLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedagogicalCompressorAudioProcessorEditor)
};

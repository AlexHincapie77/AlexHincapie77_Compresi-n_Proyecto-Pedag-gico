/*
 * PluginEditor.cpp — versión simplificada
 */

#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    void styleVerticalSlider (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
        s.setColour (juce::Slider::trackColourId, juce::Colour (0xff2c2c3a));
        s.setColour (juce::Slider::thumbColourId, juce::Colour (0xff4a9eff));
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffc8c8d8));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff2c2c3a));
    }

    void styleHorizontalSlider (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 14);
        s.setColour (juce::Slider::trackColourId, juce::Colour (0xff2c2c3a));
        s.setColour (juce::Slider::thumbColourId, juce::Colour (0xff4a9eff));
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffc8c8d8));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff2c2c3a));
    }

    void styleLabel (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, juce::Colour (0xffc8c8d8));
        l.setFont (juce::Font (juce::FontOptions (10.0f)));
        l.setJustificationType (juce::Justification::centred);
    }

    void styleComboBox (juce::ComboBox& cb)
    {
        cb.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff26262e));
        cb.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff4a9eff));
        cb.setColour (juce::ComboBox::textColourId, juce::Colour (0xffe5e5e5));
        cb.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff4a9eff));
        cb.setJustificationType (juce::Justification::centred);
    }

    void styleButton (juce::Button& b)
    {
        b.setColour (juce::TextButton::buttonColourId,    juce::Colour (0xff26262e));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4a9eff));
        b.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffe5e5e5));
        b.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffffffff));
    }

    juce::Colour bandAccentColor (int b)
    {
        static const juce::Colour colors[4] =
        {
            juce::Colour (0xff4a9eff),
            juce::Colour (0xff3acc5a),
            juce::Colour (0xfff5a623),
            juce::Colour (0xffff5050)
        };
        return colors[juce::jlimit (0, 3, b)];
    }

    juce::String pidFor (int b, const juce::String& suffix)
    {
        return "Band" + juce::String (b) + suffix;
    }
}

// ─────────────────────────────────────────────────────────────────────────
PedagogicalCompressorAudioProcessorEditor::PedagogicalCompressorAudioProcessorEditor (
    PedagogicalCompressorAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    addAndMakeVisible (bandSplit);
    bandSplit.onCrossoverChanged = [this] (int idx, float freq) {
        processorRef.setCrossoverFrequency (idx, freq);
    };
    bandSplit.setNumBands (processorRef.getBandCount());

    addAndMakeVisible (curve);
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);
    addAndMakeVisible (grMeter);

    addAndMakeVisible (presetCombo);
    {
        const auto presets = pedacomp::getFactoryPresets();
        for (size_t i = 0; i < presets.size(); ++i)
            presetCombo.addItem (presets[i].name, (int) i + 1);
        presetCombo.setSelectedItemIndex (0, juce::dontSendNotification);
    }
    styleComboBox (presetCombo);
    presetCombo.onChange = [this] {
        const int idx = presetCombo.getSelectedItemIndex();
        if (idx >= 0)
            processorRef.applyFactoryPreset (idx);
    };

    addAndMakeVisible (bandCountCombo);
    bandCountCombo.addItem ("1 Band", 1);
    bandCountCombo.addItem ("2 Bands", 2);
    bandCountCombo.addItem ("3 Bands", 3);
    bandCountCombo.addItem ("4 Bands", 4);
    styleComboBox (bandCountCombo);
    bandCountCombo.setSelectedItemIndex (processorRef.getBandCount() - 1, juce::dontSendNotification);
    bandCountCombo.onChange = [this] {
        processorRef.setBandCount (bandCountCombo.getSelectedItemIndex() + 1);
        bandSplit.setNumBands (processorRef.getBandCount());
        resized();
    };

    addAndMakeVisible (inputGainSlider);  styleHorizontalSlider (inputGainSlider);
    addAndMakeVisible (inputGainLabel);   styleLabel (inputGainLabel, "Input");
    inputGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getAPVTS(), "InputGain", inputGainSlider);

    addAndMakeVisible (outputGainSlider); styleHorizontalSlider (outputGainSlider);
    addAndMakeVisible (outputGainLabel);  styleLabel (outputGainLabel, "Output");
    outputGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getAPVTS(), "OutputGain", outputGainSlider);

    addAndMakeVisible (mixSlider);        styleHorizontalSlider (mixSlider);
    addAndMakeVisible (mixLabel);         styleLabel (mixLabel, "Mix");
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getAPVTS(), "GlobalMix", mixSlider);

    addAndMakeVisible (bypassButton);     styleButton (bypassButton);
    bypassButton.setClickingTogglesState (true);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.getAPVTS(), "GlobalBypass", bypassButton);

    addAndMakeVisible (limiterButton);    styleButton (limiterButton);
    limiterButton.setClickingTogglesState (true);
    limiterAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.getAPVTS(), "LimiterOn", limiterButton);

    using namespace juce;
    for (int b = 0; b < 4; ++b)
    {
        auto bc = std::make_unique<BandControls>();
        bc->group.setText ("Band " + String (b + 1));
        bc->group.setColour (GroupComponent::outlineColourId, bandAccentColor (b));
        bc->group.setColour (GroupComponent::textColourId, bandAccentColor (b));

        addAndMakeVisible (bc->group);

        addAndMakeVisible (bc->typeCombo);
        bc->typeCombo.addItemList ({ "Digital", "VCA", "FET", "Optical", "Vari-Mu" }, 1);
        styleComboBox (bc->typeCombo);
        bc->typeAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (
            processorRef.getAPVTS(), pidFor (b, "CompType"), bc->typeCombo);

        addAndMakeVisible (bc->threshold); styleVerticalSlider (bc->threshold);
        addAndMakeVisible (bc->thresholdL); bc->thresholdL.setText ("Threshold", dontSendNotification);

        addAndMakeVisible (bc->ratio);    styleVerticalSlider (bc->ratio);
        addAndMakeVisible (bc->ratioL);   bc->ratioL.setText ("Ratio", dontSendNotification);

        addAndMakeVisible (bc->attack);   styleVerticalSlider (bc->attack);
        addAndMakeVisible (bc->attackL);  bc->attackL.setText ("Attack", dontSendNotification);

        addAndMakeVisible (bc->release);   styleVerticalSlider (bc->release);
        addAndMakeVisible (bc->releaseL); bc->releaseL.setText ("Release", dontSendNotification);

        addAndMakeVisible (bc->knee);     styleVerticalSlider (bc->knee);
        addAndMakeVisible (bc->kneeL);    bc->kneeL.setText ("Knee", dontSendNotification);

        addAndMakeVisible (bc->makeup);   styleVerticalSlider (bc->makeup);
        addAndMakeVisible (bc->makeupL);  bc->makeupL.setText ("Makeup", dontSendNotification);

        addAndMakeVisible (bc->mix);      styleVerticalSlider (bc->mix);
        addAndMakeVisible (bc->mixL);     bc->mixL.setText ("Mix", dontSendNotification);

        addAndMakeVisible (bc->scHpf);    styleVerticalSlider (bc->scHpf);
        addAndMakeVisible (bc->scHpfL);   bc->scHpfL.setText ("SC HPF", dontSendNotification);

        addAndMakeVisible (bc->lookahead); styleVerticalSlider (bc->lookahead);
        addAndMakeVisible (bc->lookaheadL); bc->lookaheadL.setText ("Lookahead", dontSendNotification);

        bc->thrAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.getAPVTS(), pidFor (b, "Threshold"), bc->threshold);
        bc->ratAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.getAPVTS(), pidFor (b, "Ratio"),     bc->ratio);
        bc->attAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.getAPVTS(), pidFor (b, "Attack"),    bc->attack);
        bc->relAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.getAPVTS(), pidFor (b, "Release"),   bc->release);
        bc->knAtt  = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.getAPVTS(), pidFor (b, "Knee"),      bc->knee);
        bc->mkAtt  = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.getAPVTS(), pidFor (b, "Makeup"),    bc->makeup);
        bc->mixAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.getAPVTS(), pidFor (b, "Mix"),       bc->mix);
        bc->scHpfAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.getAPVTS(), pidFor (b, "ScHpf"),     bc->scHpf);
        bc->lkAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.getAPVTS(), pidFor (b, "Lookahead"), bc->lookahead);

        addAndMakeVisible (bc->detectionCombo);
        bc->detectionCombo.addItemList ({ "Peak", "RMS" }, 1);
        styleComboBox (bc->detectionCombo);
        bc->detAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (
            processorRef.getAPVTS(), pidFor (b, "Detection"), bc->detectionCombo);
        addAndMakeVisible (bc->detectionL); bc->detectionL.setText ("Detect", dontSendNotification);

        addAndMakeVisible (bc->oversamplingCombo);
        bc->oversamplingCombo.addItemList ({ "1x", "2x", "4x" }, 1);
        styleComboBox (bc->oversamplingCombo);
        bc->osAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (
            processorRef.getAPVTS(), pidFor (b, "Oversampling"), bc->oversamplingCombo);
        addAndMakeVisible (bc->oversamplingL); bc->oversamplingL.setText ("OS", dontSendNotification);

        addAndMakeVisible (bc->enableToggle); bc->enableToggle.setClickingTogglesState (true);
        addAndMakeVisible (bc->enableL); bc->enableL.setText ("ON", dontSendNotification);
        bc->enAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (
            processorRef.getAPVTS(), pidFor (b, "Enabled"), bc->enableToggle);

        addAndMakeVisible (bc->bypassToggle); bc->bypassToggle.setClickingTogglesState (true);
        addAndMakeVisible (bc->bypassL); bc->bypassL.setText ("BYP", dontSendNotification);
        bc->bpAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (
            processorRef.getAPVTS(), pidFor (b, "Bypass"), bc->bypassToggle);

        addAndMakeVisible (bc->extScToggle); bc->extScToggle.setClickingTogglesState (true);
        addAndMakeVisible (bc->extScL); bc->extScL.setText ("ExtSC", dontSendNotification);
        bc->extScAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (
            processorRef.getAPVTS(), pidFor (b, "ExtSc"), bc->extScToggle);

        addAndMakeVisible (bc->inputMeter);
        addAndMakeVisible (bc->grMeter);

        addAndMakeVisible (bc->infoText);
        bc->infoText.setColour (Label::textColourId, juce::Colour (0xffa8a8c0));
        bc->infoText.setFont (Font (juce::FontOptions (9.0f)));
        bc->infoText.setJustificationType (Justification::centredLeft);

        bandControls[b] = std::move (bc);
    }

    addAndMakeVisible (pedagogicalLabel);
    pedagogicalLabel.setColour (Label::backgroundColourId, juce::Colour (0xff1a1a22));
    pedagogicalLabel.setColour (Label::textColourId, juce::Colour (0xffe5e5e5));
    pedagogicalLabel.setColour (Label::outlineColourId, juce::Colour (0xff4a9eff));
    pedagogicalLabel.setFont (Font (juce::FontOptions (12.0f)));
    pedagogicalLabel.setJustificationType (Justification::centredLeft);

    updateTooltips();

    setSize (1200, 760);
    setResizable (true, true);
    setResizeLimits (1000, 640, 1800, 1200);

    startTimerHz (15);  // Reducido de 30 a 15 para evitar issues de threading
}

PedagogicalCompressorAudioProcessorEditor::~PedagogicalCompressorAudioProcessorEditor()
{
    // Detener el timer del editor primero
    stopTimer();
}

void PedagogicalCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff11111a));
    g.setColour (juce::Colour (0xff4a9eff));
    g.setFont (juce::Font (juce::FontOptions (15.0f)));
    g.drawText ("PEDAGOGICAL COMPRESSOR  |  Multiband (1-4)  |  Digital/VCA/FET/Optical/Vari-Mu",
                getLocalBounds().reduced (8, 0).removeFromTop (16), juce::Justification::centred);
}

void PedagogicalCompressorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8, 8).withTrimmedTop (18);

    auto topRow = area.removeFromTop (190);
    auto metersStrip = topRow.removeFromRight (90);
    bandSplit.setBounds (topRow.removeFromLeft (topRow.getWidth() * 0.55));
    curve.setBounds (topRow);

    inputMeter.setBounds  (metersStrip.removeFromLeft (28).reduced (2));
    outputMeter.setBounds (metersStrip.removeFromLeft (28).reduced (2));
    grMeter.setBounds     (metersStrip.removeFromLeft (28).reduced (2));

    auto globalRow = area.removeFromTop (50);
    presetCombo.setBounds (globalRow.removeFromLeft (140).reduced (4));
    bandCountCombo.setBounds (globalRow.removeFromLeft (110).reduced (4));

    auto inputArea = globalRow.removeFromLeft (160).reduced (4);
    inputGainSlider.setBounds (inputArea.removeFromBottom (24));
    inputGainLabel.setBounds (inputArea);

    auto outputArea = globalRow.removeFromLeft (160).reduced (4);
    outputGainSlider.setBounds (outputArea.removeFromBottom (24));
    outputGainLabel.setBounds (outputArea);

    auto mixArea = globalRow.removeFromLeft (130).reduced (4);
    mixSlider.setBounds (mixArea.removeFromBottom (24));
    mixLabel.setBounds (mixArea);

    bypassButton.setBounds (globalRow.removeFromLeft (70).reduced (4));
    limiterButton.setBounds (globalRow.removeFromLeft (90).reduced (4));

    const int numBands = processorRef.getBandCount();
    const int bandWidth = area.getWidth() / numBands;

    for (int b = 0; b < numBands; ++b)
    {
        auto& bc = *bandControls[b];
        auto bandArea = area.removeFromLeft (bandWidth).reduced (4);

        bc.group.setBounds (bandArea);
        auto inner = bandArea.reduced (8, 16).withTrimmedTop (10);

        bc.typeCombo.setBounds (inner.removeFromTop (20));

        auto metersRow = inner.removeFromLeft (50).reduced (2);
        bc.inputMeter.setBounds (metersRow.removeFromTop (metersRow.getHeight() / 2).reduced (2));
        bc.grMeter.setBounds (metersRow.reduced (2));

        const int sliderCount = 9;
        const int sliderW = juce::jmax (38, inner.getWidth() / sliderCount);

        // Array de punteros a sliders
        juce::Slider* sliders[9] = {
            &bc.threshold, &bc.ratio, &bc.attack, &bc.release, &bc.knee,
            &bc.makeup, &bc.mix, &bc.scHpf, &bc.lookahead
        };
        juce::Label* labels[9] = {
            &bc.thresholdL, &bc.ratioL, &bc.attackL, &bc.releaseL, &bc.kneeL,
            &bc.makeupL, &bc.mixL, &bc.scHpfL, &bc.lookaheadL
        };
        for (int i = 0; i < sliderCount; ++i)
        {
            auto col = inner.removeFromLeft (sliderW);
            auto lblArea = col.removeFromBottom (14);
            sliders[i]->setBounds (col);
            labels[i]->setBounds (lblArea);
        }

        auto comboRow = inner.removeFromTop (22);
        bc.detectionCombo.setBounds (comboRow.removeFromLeft (comboRow.getWidth() / 2 - 4));
        bc.oversamplingCombo.setBounds (comboRow.removeFromLeft (comboRow.getWidth() - 4));
        auto comboLabelRow = inner.removeFromTop (12);
        bc.detectionL.setBounds (comboLabelRow.removeFromLeft (comboLabelRow.getWidth() / 2));
        bc.oversamplingL.setBounds (comboLabelRow);

        auto toggleRow = inner.removeFromTop (20);
        bc.enableToggle.setBounds (toggleRow.removeFromLeft (toggleRow.getWidth() / 3).reduced (2));
        bc.bypassToggle.setBounds (toggleRow.removeFromLeft (toggleRow.getWidth() / 2).reduced (2));
        bc.extScToggle.setBounds (toggleRow.reduced (2));
        auto toggleLRow = inner.removeFromTop (12);
        bc.enableL.setBounds (toggleLRow.removeFromLeft (toggleLRow.getWidth() / 3));
        bc.bypassL.setBounds (toggleLRow.removeFromLeft (toggleLRow.getWidth() / 2));
        bc.extScL.setBounds (toggleLRow);

        bc.infoText.setBounds (inner);
    }

    for (int b = numBands; b < 4; ++b)
        bandControls[b]->group.setVisible (false);
    for (int b = 0; b < numBands; ++b)
        bandControls[b]->group.setVisible (true);

    pedagogicalLabel.setBounds (area.reduced (4));
}

void PedagogicalCompressorAudioProcessorEditor::timerCallback()
{
    // Solo refrescar si el componente está visible y tiene tamaño
    if (! isVisible() || getWidth() <= 0 || getHeight() <= 0) return;

    refreshFromProcessor();

    // Hacer tick en los meters (peak-hold decay) y repintar
    inputMeter.tick();
    outputMeter.tick();
    grMeter.tick();
    curve.repaint();
    bandSplit.repaint();
    for (auto& bc : bandControls)
    {
        if (bc)
        {
            bc->inputMeter.tick();
            bc->grMeter.tick();
        }
    }
}

void PedagogicalCompressorAudioProcessorEditor::refreshFromProcessor()
{
    // Guard: si los bandControls no están inicializados, salir
    for (int i = 0; i < 4; ++i)
        if (! bandControls[i]) return;

    const int nb = processorRef.getBandCount();
    if (nb <= 0 || nb > 4) return;

    // Sincronizar combos con el estado real (los presets pueden cambiarlos)
    if (bandCountCombo.getSelectedItemIndex() != nb - 1)
        bandCountCombo.setSelectedItemIndex (nb - 1, juce::dontSendNotification);
    const int prog = processorRef.getCurrentProgramIndex();
    if (prog >= 0 && presetCombo.getSelectedItemIndex() != prog)
        presetCombo.setSelectedItemIndex (prog, juce::dontSendNotification);

    bandSplit.setNumBands (nb);
    for (int i = 0; i < 3; ++i)
        bandSplit.setCrossover (i, processorRef.getCrossoverFrequency (i));

    for (int b = 0; b < nb && b < 4; ++b)
    {
        if (! bandControls[b]) continue;

        bandSplit.setBandGainReduction (b, processorRef.getBandGainReductionDb (b));
        bandSplit.setBandColor (b, bandAccentColor (b));

        bandControls[b]->inputMeter.setLevelDb (processorRef.getBandPeakInputDb (b));
        bandControls[b]->grMeter.setLevelDb    (processorRef.getBandGainReductionDb (b));

        const auto& bp = processorRef.getMultiband().getBand (b).getParameters();
        const auto type = (pedacomp::CompressorType) juce::jlimit (0, 4, bp.compType);

        if (b == 0)
        {
            curve.setThreshold (bp.thresholdDb);
            curve.setRatio     (bp.ratio);
            curve.setKnee      (bp.kneeDb);
            curve.setMakeup    (bp.makeupDb);
            curve.setCompType  (juce::jlimit (0, 4, bp.compType));
            curve.setCurveColor (bandAccentColor (b));
            curve.setInputEnv  (processorRef.getBandPeakInputDb (b));

            pedagogicalLabel.setText (
                "BANDA 1 (" + pedacomp::nameForType (type) + "): " + pedacomp::descriptionForType (type),
                juce::dontSendNotification);

            bandControls[b]->infoText.setText (
                pedacomp::descriptionForType (type), juce::dontSendNotification);
        }
        else
        {
            bandControls[b]->infoText.setText (
                pedacomp::nameForType (type) + ": " + pedacomp::descriptionForType (type).substring (0, 100) + "...",
                juce::dontSendNotification);
        }
    }

    inputMeter.setLevelDb  (processorRef.getOverallInputDb());
    outputMeter.setLevelDb (processorRef.getOverallOutputDb());
    grMeter.setLevelDb     (processorRef.getOverallGrDb());
}

void PedagogicalCompressorAudioProcessorEditor::updateTooltips()
{
    presetCombo.setTooltip ("Presets factory.");
    bandCountCombo.setTooltip ("Numero de bandas.");
    inputGainSlider.setTooltip  ("Ganancia de entrada.");
    outputGainSlider.setTooltip ("Ganancia de salida.");
    mixSlider.setTooltip        ("Mezcla Dry/Wet global.");
    bypassButton.setTooltip     ("Bypass global.");
    limiterButton.setTooltip    ("Limiter brick-wall.");

    for (int b = 0; b < 4; ++b)
    {
        auto& bc = *bandControls[b];
        bc.typeCombo.setTooltip ("Tipo de compresion.");
        bc.threshold.setTooltip ("Threshold (umbral).");
        bc.ratio.setTooltip     ("Ratio.");
        bc.attack.setTooltip    ("Attack.");
        bc.release.setTooltip   ("Release.");
        bc.knee.setTooltip      ("Knee.");
        bc.makeup.setTooltip    ("Makeup gain.");
        bc.mix.setTooltip       ("Mix dry/wet.");
        bc.scHpf.setTooltip     ("HPF en sidechain.");
        bc.lookahead.setTooltip  ("Lookahead.");
        bc.detectionCombo.setTooltip ("Detector Peak/RMS.");
        bc.oversamplingCombo.setTooltip ("Oversampling.");
        bc.enableToggle.setTooltip ("ON: la banda suena. OFF: la banda queda MUTE.");
        bc.bypassToggle.setTooltip ("Bypass: la banda suena SIN comprimir.");
        bc.extScToggle.setTooltip  ("Sidechain externo.");
    }
}

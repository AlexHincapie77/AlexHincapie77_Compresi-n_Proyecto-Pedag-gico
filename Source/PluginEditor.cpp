#include "PluginEditor.h"

namespace Palette
{
    static const juce::Colour bg         { 0xff14141a };
    static const juce::Colour bgLight    { 0xff1c1c22 };
    static const juce::Colour panel      { 0xff22222a };
    static const juce::Colour panelLight { 0xff2e2e36 };
    static const juce::Colour accent     { 0xfff5a623 };   // naranja-ámbar
    static const juce::Colour accent2     { 0xff4ec9ff };   // azul pedagógico
    static const juce::Colour text       { 0xfff0f0f0 };
    static const juce::Colour textDim    { 0xffa0a0a8 };
    static const juce::Colour bandColors[4] = {
        juce::Colour(0xff4a9eff), juce::Colour(0xff50ff80),
        juce::Colour(0xffffc060), juce::Colour(0xffff5050)
    };
    static const char* bandColorsEs[4] = { "Azul (Bajos)", "Verde (Medios graves)",
                                            "Naranja (Medios agudos)", "Rojo (Agudos)" };
}

// Helper: setup slider con tooltip pedagógico en español
void PedagogicalCompressorAudioProcessorEditor::setupSliderPedagogical(
    juce::Slider& s, juce::Label& l, juce::Label& v,
    const juce::String& name, const juce::String& tooltip,
    const juce::Colour& color)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setColour(juce::Slider::rotarySliderFillColourId, color);
    s.setColour(juce::Slider::rotarySliderOutlineColourId, Palette::panelLight);
    s.setColour(juce::Slider::thumbColourId, color.brighter(0.2f));
    s.setTooltip(tooltip);

    l.setText(name, juce::dontSendNotification);
    l.setFont(juce::Font(juce::FontOptions(13.0f)));
    l.setColour(juce::Label::textColourId, Palette::textDim);
    l.setJustificationType(juce::Justification::centred);

    v.setFont(juce::Font(juce::FontOptions(13.0f)));
    v.setColour(juce::Label::textColourId, Palette::text);
    v.setJustificationType(juce::Justification::centred);
}

// =========================================================================
//  Constructor
// =========================================================================
PedagogicalCompressorAudioProcessorEditor::PedagogicalCompressorAudioProcessorEditor(
    PedagogicalCompressorAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p)
{
    // LookAndFeel global con fuentes más grandes
    setSize(1500, 950);
    setResizable(true, true);
    setResizeLimits(1280, 820, 1920, 1200);

    // ─── TÍTULO GRANDE Y VISIBLE ────────────────────────────────────
    titleLabel.setText("PEDAGOGICAL COMPRESSOR", juce::dontSendNotification);
    juce::Font titleFont(juce::FontOptions(28.0f));
    titleFont.setBold(true);
    titleLabel.setFont(titleFont);
    titleLabel.setColour(juce::Label::textColourId, Palette::accent);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Aprende compresión mientras la usas · v1.2.2", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    subtitleLabel.setColour(juce::Label::textColourId, Palette::textDim);
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(subtitleLabel);

    // ─── PRESET SELECTOR GRANDE Y VISIBLE ───────────────────────────
    presetLabel.setText("PRESET:", juce::dontSendNotification);
    presetLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    presetLabel.setColour(juce::Label::textColourId, Palette::accent2);
    presetLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(presetLabel);

    for (int i = 0; i < Presets::getNumPresets(); ++i)
        presetCombo.addItem(Presets::getPresetName(i), i + 1);
    presetCombo.setTooltip("Selecciona un preset para empezar a aprender. Cada preset "
                            "demuestra un uso típico: mastering, de-esser, drum bus, etc.");
    presetCombo.addListener(this);
    // Hacer el combo más grande
    presetCombo.setColour(juce::ComboBox::backgroundColourId, Palette::panel);
    presetCombo.setColour(juce::ComboBox::outlineColourId, Palette::accent2);
    presetCombo.setColour(juce::ComboBox::textColourId, Palette::text);
    presetCombo.setColour(juce::ComboBox::arrowColourId, Palette::accent2);
    addAndMakeVisible(presetCombo);
    presetAttachment = std::make_unique<ComboBoxAttachment>(
        processor.getAPVTS(), "preset", presetCombo);

    // ─── NUM BANDS SELECTOR ─────────────────────────────────────────
    numBandsLabel.setText("BANDAS:", juce::dontSendNotification);
    numBandsLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    numBandsLabel.setColour(juce::Label::textColourId, Palette::accent);
    numBandsLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(numBandsLabel);

    numBandsCombo.addItem("1 banda", 1);
    numBandsCombo.addItem("2 bandas", 2);
    numBandsCombo.addItem("3 bandas", 3);
    numBandsCombo.addItem("4 bandas", 4);
    numBandsCombo.setTooltip("Cantidad de bandas de compresión. Una sola banda es "
                              "más simple y suena natural. Más bandas permiten controlar "
                              "rangos de frecuencia por separado (bajos, medios, agudos) "
                              "pero añaden complejidad. Para empezar, usa 1 banda.");
    numBandsCombo.addListener(this);
    numBandsCombo.setColour(juce::ComboBox::backgroundColourId, Palette::panel);
    numBandsCombo.setColour(juce::ComboBox::outlineColourId, Palette::accent);
    numBandsCombo.setColour(juce::ComboBox::textColourId, Palette::text);
    numBandsCombo.setColour(juce::ComboBox::arrowColourId, Palette::accent);
    addAndMakeVisible(numBandsCombo);
    numBandsAttachment = std::make_unique<ComboBoxAttachment>(
        processor.getAPVTS(), "numBands", numBandsCombo);

    // ─── GLOBALES — SLIDERS GRANDES ──────────────────────────────────
    auto setupGlobal = [this](juce::Slider& s, juce::Label& l, juce::Label& v,
                                const juce::String& name, const juce::String& tooltip,
                                const juce::String& paramId) {
        setupSliderPedagogical(s, l, v, name, tooltip, Palette::accent);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        s.setColour(juce::Slider::textBoxTextColourId, Palette::text);
        s.setColour(juce::Slider::textBoxOutlineColourId, Palette::panelLight);
        s.setColour(juce::Slider::textBoxHighlightColourId, Palette::accent.withAlpha(0.3f));
        addAndMakeVisible(s);
        addAndMakeVisible(l);
        addAndMakeVisible(v);
        return std::make_unique<SliderAttachment>(processor.getAPVTS(), paramId, s);
    };

    inputGainAttachment = setupGlobal(inputGainSlider, inputGainLabel, inputGainValue,
        "Input", "Ganancia a la ENTRADA del compresor. Súbelo si la señal llega muy baja, "
                  "bájalo si distorsiona antes de entrar.", "inputGain");
    outputGainAttachment = setupGlobal(outputGainSlider, outputGainLabel, outputGainValue,
        "Output", "Ganancia a la SALIDA. Compensa el cambio de volumen causado por la "
                   "compresión. Úsalo junto con Auto-Gain para mantener loudness.", "outputGain");
    limiterCeilingAttachment = setupGlobal(limiterCeilingSlider, limiterCeilingLabel, limiterCeilingValue,
        "Ceiling", "Techo del limitador final. La señal nunca pasará de este nivel. "
                    "Típicamente -1 dB para evitar clipping digital en el export.", "limiterCeiling");
    globalMixAttachment = setupGlobal(globalMixSlider, globalMixLabel, globalMixValue,
        "Mix", "Mezcla entre señal original (Dry) y comprimida (Wet). 100% = todo comprimido. "
                "Menos de 100% = compresión en paralelo (más transparente).", "globalMix");

    // ─── BYPASS Y AUTO-GAIN ─────────────────────────────────────────
    bypassButton.setButtonText("BYPASS");
    bypassButton.setClickingTogglesState(true);
    bypassButton.setTooltip("Compara la señal original con la comprimida. Útil para "
                             "verificar que la compresión mejora el sonido.");
    bypassButton.setColour(juce::TextButton::buttonColourId, Palette::panel);
    bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffff5050));
    bypassButton.setColour(juce::TextButton::textColourOffId, Palette::text);
    bypassButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    bypassButton.setColour(juce::ComboBox::textColourId, Palette::text);
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(
        processor.getAPVTS(), "bypass", bypassButton);

    autoGainButton.setButtonText("AUTO GAIN");
    autoGainButton.setClickingTogglesState(true);
    autoGainButton.setTooltip("Compensación automática de volumen: mantiene el nivel "
                              "de salida igual al de entrada tras la compresión. Útil "
                              "para comparar antes/después sin que el cambio de loudness "
                              "engañe al oído.");
    autoGainButton.setColour(juce::TextButton::buttonColourId, Palette::panel);
    autoGainButton.setColour(juce::TextButton::buttonOnColourId, Palette::accent);
    autoGainButton.setColour(juce::TextButton::textColourOffId, Palette::text);
    autoGainButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    addAndMakeVisible(autoGainButton);
    autoGainAttachment = std::make_unique<ButtonAttachment>(
        processor.getAPVTS(), "autoGain", autoGainButton);

    autoGainReadout.setText("OFF", juce::dontSendNotification);
    autoGainReadout.setFont(juce::Font(juce::FontOptions(13.0f)));
    autoGainReadout.setColour(juce::Label::textColourId, Palette::accent);
    autoGainReadout.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(autoGainReadout);

    // ─── CONTROLES POR BANDA ────────────────────────────────────────
    rebuildBandControls();

    // ─── VISUALIZADORES ──────────────────────────────────────────────
    addAndMakeVisible(bandSplitVisualizer);
    addAndMakeVisible(compressionCurve);
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(grMeter);          // GR meter GRANDE y central
    addAndMakeVisible(envelopeVisualizer);

    bandSplitVisualizer.onCrossoverChanged = [this](int idx, float freq) {
        if (idx + 1 < MultibandProcessor::kMaxBands)
        {
            auto& apvts = processor.getAPVTS();
            auto pId = PID::bandParam(idx + 1, "cross");
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(pId)))
                p->setValueNotifyingHost(p->convertTo0to1(freq));
        }
    };

    startTimerHz(30);
    refreshComboBoxSelections();
}

PedagogicalCompressorAudioProcessorEditor::~PedagogicalCompressorAudioProcessorEditor()
{
    stopTimer();
}

// =========================================================================
//  rebuildBandControls — sliders por banda con tooltips pedagógicos
// =========================================================================
void PedagogicalCompressorAudioProcessorEditor::rebuildBandControls()
{
    auto& apvts = processor.getAPVTS();
    static const char* bandNames[4] = { "Banda 1 — Bajos", "Banda 2 — Medios graves",
                                          "Banda 3 — Medios agudos", "Banda 4 — Agudos" };
    static const char* bandDescs[4] = {
        "Frecuencias bajas (kick, bajo, graves)",
        "Rango medio-bajo (cuerpos, voz fundamental)",
        "Rango medio-alto (presencia, ataque)",
        "Frecuencias altas (aire, brillo, sibilancias)"
    };

    for (int b = 0; b < MultibandProcessor::kMaxBands; ++b)
    {
        auto& bc = bands[b];
        bc.group.setText(bandNames[b]);
        bc.group.setTextLabelPosition(juce::Justification::centred);
        bc.group.setColour(juce::GroupComponent::textColourId, Palette::bandColors[b]);
        bc.group.setColour(juce::GroupComponent::outlineColourId, Palette::bandColors[b].withAlpha(0.5f));
        addAndMakeVisible(bc.group);

        auto setupBandSlider = [this, b](juce::Slider& s, juce::Label& l,
                                          const juce::String& name, const juce::String& tooltip) {
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 16);
            s.setColour(juce::Slider::textBoxTextColourId, Palette::text);
            s.setColour(juce::Slider::textBoxOutlineColourId, Palette::panelLight);
            s.setColour(juce::Slider::textBoxHighlightColourId,
                        Palette::bandColors[b].withAlpha(0.3f));
            s.setColour(juce::Slider::rotarySliderFillColourId, Palette::bandColors[b]);
            s.setColour(juce::Slider::rotarySliderOutlineColourId, Palette::panelLight);
            s.setColour(juce::Slider::thumbColourId, Palette::bandColors[b].brighter(0.2f));
            s.setTooltip(tooltip);
            bands[b].group.addAndMakeVisible(s);

            l.setText(name, juce::dontSendNotification);
            l.setFont(juce::Font(juce::FontOptions(13.0f)));
            l.setColour(juce::Label::textColourId, Palette::textDim);
            l.setJustificationType(juce::Justification::centred);
            bands[b].group.addAndMakeVisible(l);
        };

        setupBandSlider(bc.threshold, bc.thrLabel, "Threshold",
            "Umbral: por encima de este nivel (en dB) el compresor EMPIEZA a actuar. "
            "Si la señal está por debajo, no comprime nada.");
        setupBandSlider(bc.ratio, bc.ratLabel, "Ratio",
            "Cuánto se reduce la señal por encima del threshold. Ratio 4:1 significa que "
            "por cada 4 dB que entre por encima del threshold, sale 1 dB. Más alto = más "
            "compresión. Para mastering usa 2-3; para limitar usa 10+.");
        setupBandSlider(bc.attack, bc.attLabel, "Attack",
            "Tiempo que tarda el compresor en reaccionar cuando la señal supera el "
            "threshold. Rápido (1-10 ms) atrapa transitorios (percusión). Lento (30+ ms) "
            "los deja pasar (más natural).");
        setupBandSlider(bc.release, bc.relLabel, "Release",
            "Tiempo que tarda el compresor en dejar de comprimir cuando la señal baja "
            "del threshold. Corto (50-100 ms) suena más 'pegajoso'. Largo (300+ ms) más "
            "natural. Si bombea, alarga el release.");
        setupBandSlider(bc.knee, bc.kneeLabel, "Knee",
            "Suavidad de la curva cerca del threshold. Knee 0 = brusco (hard knee). "
            "Knee alto = suave (soft knee), entra gradualmente. Más suave = más natural.");
        setupBandSlider(bc.makeup, bc.mkLabel, "Makeup",
            "Ganancia aplicada DESPUÉS de comprimir para compensar la pérdida de "
            "volumen. Si activas Auto-Makeup, este control se ignora.");
        setupBandSlider(bc.mix, bc.mixLabel, "Mix",
            "Mezcla Dry/Wet por banda. 100% = banda totalmente comprimida. Menos = "
            "compresión en paralelo (más transparente).");
        setupBandSlider(bc.scHpf, bc.scHpfLabel, "SC HPF",
            "Filtro pasa-altos en el detector interno. Hace que el compresor ignore "
            "las frecuencias graves al decidir cuánto comprimir. Útil para que un kick "
            "no haga bombeo excesivo en el resto del espectro.");
        setupBandSlider(bc.lookahead, bc.lookaheadLabel, "Look",
            "Mirar hacia adelante: retrasa la señal para que el compresor pueda reaccionar "
            "ANTES de que el pico la alcance. Más transparente para picos rápidos. "
            "Añade latencia equivalente.");
        setupBandSlider(bc.stereoLink, bc.slLabel, "SLink",
            "Enlaza L y R: 100% = ambos canales se comprimen igual (preserva imagen "
            "estéreo). 0% = cada canal independiente (puede descentrar).");
        setupBandSlider(bc.saturation, bc.satLabel, "Sat",
            "Saturación analógica: añade armónicos al estilo de un compresor de tubo "
            "o cinta. 0 = limpio. Sube para dar 'color' y calidez, especialmente con "
            "tipos FET o Vari-Mu.");

        // Combos
        bc.type.addItem("Digital", 1);
        bc.type.addItem("VCA (SSL)", 2);
        bc.type.addItem("FET (1176)", 3);
        bc.type.addItem("Optical (LA-2A)", 4);
        bc.type.addItem("Vari-Mu (Fairchild)", 5);
        bc.type.setTooltip("Tipo de compresor emulado:\n"
                            "• Digital: limpio, sin color\n"
                            "• VCA: SSL-style, transparente, glue de bus\n"
                            "• FET: 1176-style, rápido y agresivo (drums, voz)\n"
                            "• Optical: LA-2A-style, suave y musical (voz, bajo)\n"
                            "• Vari-Mu: Fairchild-style, tubo, mastering");
        bc.type.setColour(juce::ComboBox::backgroundColourId, Palette::panel);
        bc.type.setColour(juce::ComboBox::outlineColourId, Palette::bandColors[b].withAlpha(0.5f));
        bc.type.setColour(juce::ComboBox::textColourId, Palette::text);
        bc.type.addListener(this);
        bc.group.addAndMakeVisible(bc.type);

        bc.typeLabel.setText("Tipo", juce::dontSendNotification);
        bc.typeLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
        bc.typeLabel.setColour(juce::Label::textColourId, Palette::textDim);
        bc.typeLabel.setJustificationType(juce::Justification::centred);
        bc.group.addAndMakeVisible(bc.typeLabel);

        bc.detector.addItem("Peak", 1);
        bc.detector.addItem("RMS", 2);
        bc.detector.setTooltip("Cómo el compresor 'escucha' la señal:\n"
                                "• Peak: reacciona a los picos instantáneos. Más agresivo, "
                                "atrapa transitorios pero puede bombeo.\n"
                                "• RMS: reacciona al promedio de energía. Más musical y "
                                "suave, respeta el carácter.");
        bc.detector.setColour(juce::ComboBox::backgroundColourId, Palette::panel);
        bc.detector.setColour(juce::ComboBox::outlineColourId, Palette::bandColors[b].withAlpha(0.5f));
        bc.detector.setColour(juce::ComboBox::textColourId, Palette::text);
        bc.detector.addListener(this);
        bc.group.addAndMakeVisible(bc.detector);

        bc.detLabel.setText("Det", juce::dontSendNotification);
        bc.detLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
        bc.detLabel.setColour(juce::Label::textColourId, Palette::textDim);
        bc.detLabel.setJustificationType(juce::Justification::centred);
        bc.group.addAndMakeVisible(bc.detLabel);

        // Buttons
        bc.bypass.setButtonText("BYP");
        bc.bypass.setClickingTogglesState(true);
        bc.bypass.setTooltip("Bypass de esta banda (pasa la señal sin comprimir).");
        bc.bypass.setColour(juce::TextButton::buttonColourId, Palette::panel);
        bc.bypass.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffff5050));
        bc.bypass.setColour(juce::TextButton::textColourOffId, Palette::text);
        bc.group.addAndMakeVisible(bc.bypass);

        bc.extSC.setButtonText("Ext SC");
        bc.extSC.setClickingTogglesState(true);
        bc.extSC.setTooltip("SIDECHAIN EXTERNO: usa la señal del bus 3/4 (Sidechain) "
                             "como detector en lugar de la señal principal. En REAPER: "
                             "pincha en el pin del plugin → rutea la señal externa al "
                             "bus 3/4. Útil para ducking (que la voz baje cuando suena "
                             "el kick, o para de-essing con banda dividida).");
        bc.extSC.setColour(juce::TextButton::buttonColourId, Palette::panel);
        bc.extSC.setColour(juce::TextButton::buttonOnColourId, Palette::accent2);
        bc.extSC.setColour(juce::TextButton::textColourOffId, Palette::text);
        bc.extSC.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        bc.group.addAndMakeVisible(bc.extSC);

        bc.autoMakeup.setButtonText("Auto MK");
        bc.autoMakeup.setClickingTogglesState(true);
        bc.autoMakeup.setTooltip("AUTO-MAKEUP: compensa automáticamente la pérdida de "
                                  "volumen causada por la compresión en esta banda. "
                                  "Calcula el running-average del GR y aplica un "
                                  "makeup opuesto.");
        bc.autoMakeup.setColour(juce::TextButton::buttonColourId, Palette::panel);
        bc.autoMakeup.setColour(juce::TextButton::buttonOnColourId, Palette::bandColors[b]);
        bc.autoMakeup.setColour(juce::TextButton::textColourOffId, Palette::text);
        bc.autoMakeup.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        bc.group.addAndMakeVisible(bc.autoMakeup);

        // Mini GR meter por banda
        bc.grMiniMeter = std::make_unique<GainReductionMeter>();
        bc.group.addAndMakeVisible(*bc.grMiniMeter);

        // Attachments
        const juce::String prefix = "b" + juce::String(b) + "_";
        bc.thrAtt = std::make_unique<SliderAttachment>(apvts, prefix + "thr", bc.threshold);
        bc.ratAtt = std::make_unique<SliderAttachment>(apvts, prefix + "rat", bc.ratio);
        bc.attAtt = std::make_unique<SliderAttachment>(apvts, prefix + "att", bc.attack);
        bc.relAtt = std::make_unique<SliderAttachment>(apvts, prefix + "rel", bc.release);
        bc.kneeAtt = std::make_unique<SliderAttachment>(apvts, prefix + "knee", bc.knee);
        bc.mkAtt = std::make_unique<SliderAttachment>(apvts, prefix + "mk", bc.makeup);
        bc.mixAtt = std::make_unique<SliderAttachment>(apvts, prefix + "mix", bc.mix);
        bc.scHpfAtt = std::make_unique<SliderAttachment>(apvts, prefix + "schpf", bc.scHpf);
        bc.laAtt = std::make_unique<SliderAttachment>(apvts, prefix + "la", bc.lookahead);
        bc.slAtt = std::make_unique<SliderAttachment>(apvts, prefix + "sl", bc.stereoLink);
        bc.satAtt = std::make_unique<SliderAttachment>(apvts, prefix + "sat", bc.saturation);
        bc.typeAtt = std::make_unique<ComboBoxAttachment>(apvts, prefix + "type", bc.type);
        bc.detAtt = std::make_unique<ComboBoxAttachment>(apvts, prefix + "det", bc.detector);
        bc.bypassAtt = std::make_unique<ButtonAttachment>(apvts, prefix + "bbyp", bc.bypass);
        bc.extScAtt = std::make_unique<ButtonAttachment>(apvts, prefix + "extsc", bc.extSC);
        bc.amkAtt = std::make_unique<ButtonAttachment>(apvts, prefix + "amk", bc.autoMakeup);
    }
}

// =========================================================================
//  refreshComboBoxSelections
// =========================================================================
void PedagogicalCompressorAudioProcessorEditor::refreshComboBoxSelections()
{
    auto& apvts = processor.getAPVTS();
    int nb = processor.getNumBands();
    bandSplitVisualizer.setNumBands(nb);
    for (int i = 0; i < nb - 1; ++i)
    {
        float freq = static_cast<float>(apvts.getRawParameterValue(
            PID::bandParam(i + 1, "cross"))->load());
        bandSplitVisualizer.setCrossover(i, freq);
    }

    int curveBand = juce::jlimit(0, nb - 1, selectedBandForCurve);
    CompressionCurveComponent::CurveParams p;
    p.threshold = apvts.getRawParameterValue(PID::bandParam(curveBand, "thr"))->load();
    p.ratio     = apvts.getRawParameterValue(PID::bandParam(curveBand, "rat"))->load();
    p.knee      = apvts.getRawParameterValue(PID::bandParam(curveBand, "knee"))->load();
    p.makeup    = apvts.getRawParameterValue(PID::bandParam(curveBand, "mk"))->load();
    p.type      = static_cast<int>(apvts.getRawParameterValue(PID::bandParam(curveBand, "type"))->load());
    p.threshold = juce::jlimit(-60.0f, 0.0f, p.threshold);
    p.ratio     = juce::jlimit(1.0f, 20.0f, p.ratio);
    p.knee      = juce::jlimit(0.0f, 24.0f, p.knee);
    p.makeup    = juce::jlimit(0.0f, 24.0f, p.makeup);
    compressionCurve.setBandIndex(curveBand);
    compressionCurve.setParams(p);
    compressionCurve.setInputLevelDb(processor.getBandInputLevel(curveBand));
    compressionCurve.setGainReductionDb(processor.getBandGainReduction(curveBand));

    float aMs = apvts.getRawParameterValue(PID::bandParam(curveBand, "att"))->load();
    float rMs = apvts.getRawParameterValue(PID::bandParam(curveBand, "rel"))->load();
    envelopeVisualizer.setAttackMs(aMs);
    envelopeVisualizer.setReleaseMs(rMs);
    envelopeVisualizer.setInputLevelDb(processor.getBandInputLevel(curveBand));
    envelopeVisualizer.setEnvelopeDb(processor.getBandEnvelopeDb(curveBand));
    envelopeVisualizer.setGainReductionDb(processor.getBandGainReduction(curveBand));

    // Valores numéricos de los sliders globales (legibles)
    auto fmtDb = [](float v) {
        if (std::abs(v) < 0.05f) return juce::String("0.0 dB");
        return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB";
    };
    auto fmtPct = [](float v) {
        return juce::String(static_cast<int>(v * 100.0f)) + " %";
    };
    inputGainValue.setText(fmtDb(apvts.getRawParameterValue(PID::InputGain)->load()), juce::dontSendNotification);
    outputGainValue.setText(fmtDb(apvts.getRawParameterValue(PID::OutputGain)->load()), juce::dontSendNotification);
    limiterCeilingValue.setText(fmtDb(apvts.getRawParameterValue(PID::LimiterCeiling)->load()), juce::dontSendNotification);
    globalMixValue.setText(fmtPct(apvts.getRawParameterValue(PID::GlobalMix)->load()), juce::dontSendNotification);
}

// =========================================================================
//  ComboBox::Listener
// =========================================================================
void PedagogicalCompressorAudioProcessorEditor::comboBoxChanged(juce::ComboBox* cb)
{
    if (cb == &numBandsCombo)
    {
        juce::MessageManager::callAsync([this] {
            refreshComboBoxSelections();
            resized();
        });
    }
    else if (cb == &presetCombo)
    {
        int idx = presetCombo.getSelectedItemIndex();
        if (idx >= 0 && idx < Presets::getNumPresets())
        {
            processor.setPreset(idx);
            juce::MessageManager::callAsync([this] {
                refreshComboBoxSelections();
                resized();
            });
        }
    }
    else
    {
        juce::MessageManager::callAsync([this] {
            refreshComboBoxSelections();
        });
    }
}

void PedagogicalCompressorAudioProcessorEditor::sliderValueChanged(juce::Slider*)
{
    refreshComboBoxSelections();
}

void PedagogicalCompressorAudioProcessorEditor::buttonClicked(juce::Button*) {}

// =========================================================================
//  Timer
// =========================================================================
void PedagogicalCompressorAudioProcessorEditor::timerCallback()
{
    refreshMeterValues();
    refreshComboBoxSelections();

    // Auto-Gain readout con valor real
    if (processor.isAutoGainEnabled())
    {
        float db = processor.getAutoGainDb();
        juce::String txt = (db >= 0 ? "+" : "") + juce::String(db, 1) + " dB";
        autoGainReadout.setText(txt, juce::dontSendNotification);
    }
    else
        autoGainReadout.setText("OFF", juce::dontSendNotification);
}

void PedagogicalCompressorAudioProcessorEditor::refreshMeterValues()
{
    inputMeter.setLevelDb(processor.getInputLevelDb());
    outputMeter.setLevelDb(processor.getOutputLevelDb());
    grMeter.setGainReductionDb(processor.getGainReductionDb());

    int nb = processor.getNumBands();
    for (int b = 0; b < nb; ++b)
    {
        bandSplitVisualizer.setBandGainReduction(b, processor.getBandGainReduction(b));
        if (bands[b].grMiniMeter)
            bands[b].grMiniMeter->setGainReductionDb(processor.getBandGainReduction(b));
    }
}

// =========================================================================
//  Paint — fondo con degradado y leyenda pedagógica
// =========================================================================
void PedagogicalCompressorAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient grad;
    grad.isRadial = false;
    grad.point1 = { 0, 0 };
    grad.point2 = { 0, (float)getHeight() };
    grad.addColour(0.0, Palette::bg.brighter(0.05f));
    grad.addColour(1.0, Palette::bg.darker(0.10f));
    g.setGradientFill(grad);
    g.fillAll();

    // Banda lateral pedagógica: explica qué hacer al principio
    // (sólo visible si no hay mensaje; aquí siempre dibujamos ayuda mínima)
}

// =========================================================================
//  Resized — Layout pedagógico completo
// =========================================================================
void PedagogicalCompressorAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(10);

    // ─── HEADER (70 px): título grande + preset + bands + bypass ─────
    auto header = r.removeFromTop(70);
    // Título y subtítulo ocupan la izquierda
    auto titleArea = header.removeFromLeft(500);
    titleLabel.setBounds(titleArea.removeFromTop(38).reduced(2));
    subtitleLabel.setBounds(titleArea.removeFromTop(20).reduced(2));

    // Botones Bypass y Auto-Gain a la derecha
    auto topRight = header.removeFromRight(300);
    auto bypassArea = topRight.removeFromRight(85).reduced(2);
    bypassButton.setBounds(bypassArea.removeFromTop(36));

    auto agArea = topRight.removeFromRight(110).reduced(2);
    autoGainButton.setBounds(agArea.removeFromTop(36));
    autoGainReadout.setBounds(agArea.removeFromTop(16));

    // Preset y NumBands en el centro
    auto centerRow = header.reduced(2);
    auto presetRow = centerRow.removeFromTop(34);
    presetLabel.setBounds(presetRow.removeFromLeft(70).reduced(2));
    presetCombo.setBounds(presetRow.removeFromLeft(280).reduced(2));
    numBandsLabel.setBounds(presetRow.removeFromLeft(70).reduced(2));
    numBandsCombo.setBounds(presetRow.removeFromLeft(170).reduced(2));

    // ─── GLOBALES (90 px): 4 sliders grandes ───────────────────────
    auto globalsRow = r.removeFromTop(95).reduced(4);
    auto globalsLayout = [&globalsRow](juce::Component& s, juce::Component& l, juce::Component& v, int w) {
        auto col = globalsRow.removeFromLeft(w);
        l.setBounds(col.removeFromTop(18));
        s.setBounds(col.removeFromTop(56));
        v.setBounds(col.removeFromTop(16));
    };
    globalsLayout(inputGainSlider, inputGainLabel, inputGainValue, 110);
    globalsLayout(outputGainSlider, outputGainLabel, outputGainValue, 110);
    globalsLayout(limiterCeilingSlider, limiterCeilingLabel, limiterCeilingValue, 110);
    globalsLayout(globalMixSlider, globalMixLabel, globalMixValue, 110);

    // ─── VISUALIZADORES (320 px): layout 4 columnas ────────────────
    // [BandSplit] [CompressionCurve + Envelope] [GR Meter grande] [I/O meters]
    auto visRow = r.removeFromTop(330).reduced(4);

    // Izquierda: band split visualizer (350)
    auto visLeft = visRow.removeFromLeft(350);
    bandSplitVisualizer.setBounds(visLeft);

    // Centro: curva arriba (220), envelope abajo (110)
    auto visCenter = visRow.removeFromLeft(560);
    compressionCurve.setBounds(visCenter.removeFromTop(220).reduced(2));
    envelopeVisualizer.setBounds(visCenter.removeFromTop(105).reduced(2));

    // Derecha-lejana: GR meter GRANDE y CENTRAL (140)
    auto visGR = visRow.removeFromRight(140);
    grMeter.setBounds(visGR.reduced(2));

    // Derecha-cercana: Input/Output meters (50)
    auto visMeters = visRow.removeFromRight(50);
    inputMeter.setBounds(visMeters.removeFromTop(160).reduced(2));
    outputMeter.setBounds(visMeters.removeFromTop(160).reduced(2));

    // ─── CONTROLES POR BANDA ──────────────────────────────────────
    int nb = processor.getNumBands();
    int bandWidth = r.getWidth() / juce::jmax(1, nb);

    for (int b = 0; b < MultibandProcessor::kMaxBands; ++b)
    {
        auto& bc = bands[b];
        if (b < nb)
        {
            bc.group.setVisible(true);
            auto bandBounds = r.removeFromLeft(bandWidth).reduced(4);
            bc.group.setBounds(bandBounds);
            auto inner = bc.group.getLocalBounds().reduced(12, 22);

            // Layout grid 4 columnas × 3 filas (11 sliders + 1 meter slot)
            const int cols = 4;
            const int rows = 3;
            int cellW = inner.getWidth() / cols;
            int cellH = inner.getHeight() / rows;

            auto placeSlider = [&](juce::Component& s, juce::Component& l, int col, int row) {
                auto cell = juce::Rectangle<int>(
                    inner.getX() + col * cellW,
                    inner.getY() + row * cellH,
                    cellW, cellH).reduced(2);
                l.setBounds(cell.removeFromTop(16));
                s.setBounds(cell);
            };

            placeSlider(bc.threshold, bc.thrLabel, 0, 0);
            placeSlider(bc.ratio, bc.ratLabel, 1, 0);
            placeSlider(bc.attack, bc.attLabel, 2, 0);
            placeSlider(bc.release, bc.relLabel, 3, 0);
            placeSlider(bc.knee, bc.kneeLabel, 0, 1);
            placeSlider(bc.makeup, bc.mkLabel, 1, 1);
            placeSlider(bc.mix, bc.mixLabel, 2, 1);
            placeSlider(bc.saturation, bc.satLabel, 3, 1);
            placeSlider(bc.scHpf, bc.scHpfLabel, 0, 2);
            placeSlider(bc.lookahead, bc.lookaheadLabel, 1, 2);
            placeSlider(bc.stereoLink, bc.slLabel, 2, 2);

            // Mini GR meter en celda (3,2)
            if (bc.grMiniMeter)
            {
                auto meterCell = juce::Rectangle<int>(
                    inner.getX() + 3 * cellW,
                    inner.getY() + 2 * cellH,
                    cellW, cellH).reduced(2);
                bc.grMiniMeter->setBounds(meterCell);
            }

            // Combos y buttons en una fila abajo
            auto bottomRow = inner.withY(inner.getY() + 3 * cellH + 4).withHeight(28);
            bc.typeLabel.setBounds(bottomRow.removeFromLeft(40).reduced(2));
            bc.type.setBounds(bottomRow.removeFromLeft(105).reduced(2));
            bc.detLabel.setBounds(bottomRow.removeFromLeft(34).reduced(2));
            bc.detector.setBounds(bottomRow.removeFromLeft(75).reduced(2));
            bc.bypass.setBounds(bottomRow.removeFromLeft(50).reduced(2));
            bc.extSC.setBounds(bottomRow.removeFromLeft(60).reduced(2));
            bc.autoMakeup.setBounds(bottomRow.removeFromLeft(70).reduced(2));
        }
        else
        {
            bc.group.setVisible(false);
        }
    }
}

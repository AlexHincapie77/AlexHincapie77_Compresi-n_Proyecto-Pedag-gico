#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

// Los IDs de parámetros (PID) están en PluginProcessor.h

// =========================================================================
//  Layout APVTS
// =========================================================================
static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Globales
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(PID::InputGain, 1), "Input Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(PID::OutputGain, 1), "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(PID::GlobalMix, 1), "Global Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(PID::LimiterCeiling, 1), "Limiter Ceiling",
        juce::NormalisableRange<float>(-12.0f, 0.0f, 0.1f), -1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(PID::NumBands, 1), "Num Bands",
        juce::StringArray({ "1", "2", "3", "4" }), 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(PID::Bypass, 1), "Bypass", false));
    // v1.2: Auto-Gain global (compensación de volumen output)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(PID::AutoGain, 1), "Auto Gain", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(PID::Preset, 1), "Preset",
        juce::StringArray({ "Default", "Master Bus Glue", "Vocal De-Esser",
                              "Drum Bus Punch (1176)", "Bass Tight (Fairchild)",
                              "4-Band Stereo Master", "Broadcast Limiter" }), 0));

    // Por banda (4)
    for (int b = 0; b < MultibandProcessor::kMaxBands; ++b)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(PID::bandParam(b, PID::Threshold), 1),
            "B" + juce::String(b) + " Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(PID::bandParam(b, PID::Ratio), 1),
            "B" + juce::String(b) + " Ratio",
            juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 4.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(PID::bandParam(b, PID::Attack), 1),
            "B" + juce::String(b) + " Attack",
            juce::NormalisableRange<float>(0.1f, 200.0f, 0.1f), 10.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(PID::bandParam(b, PID::Release), 1),
            "B" + juce::String(b) + " Release",
            juce::NormalisableRange<float>(1.0f, 1000.0f, 1.0f), 100.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(PID::bandParam(b, PID::Knee), 1),
            "B" + juce::String(b) + " Knee",
            juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 3.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(PID::bandParam(b, PID::Makeup), 1),
            "B" + juce::String(b) + " Makeup",
            juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(PID::bandParam(b, PID::Mix), 1),
            "B" + juce::String(b) + " Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(PID::bandParam(b, PID::Type), 1),
            "B" + juce::String(b) + " Type",
            juce::StringArray({ "Digital", "VCA", "FET", "Optical", "Vari-Mu" }), 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(PID::bandParam(b, PID::Detector), 1),
            "B" + juce::String(b) + " Detector",
            juce::StringArray({ "Peak", "RMS" }), 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(PID::bandParam(b, PID::ScHpf), 1),
            "B" + juce::String(b) + " SC HPF",
            juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f), 80.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(PID::bandParam(b, PID::Lookahead), 1),
            "B" + juce::String(b) + " Lookahead",
            juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(PID::bandParam(b, PID::StereoLink), 1),
            "B" + juce::String(b) + " Stereo Link",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(PID::bandParam(b, PID::BandBypass), 1),
            "B" + juce::String(b) + " Bypass", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(PID::bandParam(b, PID::ExtSc), 1),
            "B" + juce::String(b) + " Ext SC", false));
        // v1.2: Saturación analógica por banda (0..1)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(PID::bandParam(b, PID::Saturation), 1),
            "B" + juce::String(b) + " Saturation",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        // v1.2: Auto-Makeup por banda
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(PID::bandParam(b, PID::AutoMakeup), 1),
            "B" + juce::String(b) + " Auto Makeup", false));

        // Crossover: solo 3 (bandas 1, 2, 3 tienen crossover inferior)
        if (b > 0)
        {
            float def = (b == 1) ? 120.0f : (b == 2 ? 1200.0f : 6000.0f);
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(PID::bandParam(b, PID::Crossover), 1),
                "B" + juce::String(b) + " Crossover",
                juce::NormalisableRange<float>(20.0f, 18000.0f, 1.0f), def));
        }
    }

    return { params.begin(), params.end() };
}

// =========================================================================
//  Constructor / Destructor
// =========================================================================
PedagogicalCompressorAudioProcessor::PedagogicalCompressorAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
        .withInput("Main",     juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
        .withOutput("Main",    juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    apvts.addParameterListener(PID::InputGain, this);
    apvts.addParameterListener(PID::OutputGain, this);
    apvts.addParameterListener(PID::GlobalMix, this);
    apvts.addParameterListener(PID::LimiterCeiling, this);
    apvts.addParameterListener(PID::Bypass, this);
    apvts.addParameterListener(PID::AutoGain, this);
    apvts.addParameterListener(PID::NumBands, this);
    apvts.addParameterListener(PID::Preset, this);

    for (int b = 0; b < MultibandProcessor::kMaxBands; ++b)
    {
        apvts.addParameterListener(PID::bandParam(b, PID::Threshold), this);
        apvts.addParameterListener(PID::bandParam(b, PID::Ratio), this);
        apvts.addParameterListener(PID::bandParam(b, PID::Attack), this);
        apvts.addParameterListener(PID::bandParam(b, PID::Release), this);
        apvts.addParameterListener(PID::bandParam(b, PID::Knee), this);
        apvts.addParameterListener(PID::bandParam(b, PID::Makeup), this);
        apvts.addParameterListener(PID::bandParam(b, PID::Mix), this);
        apvts.addParameterListener(PID::bandParam(b, PID::Type), this);
        apvts.addParameterListener(PID::bandParam(b, PID::Detector), this);
        apvts.addParameterListener(PID::bandParam(b, PID::ScHpf), this);
        apvts.addParameterListener(PID::bandParam(b, PID::Lookahead), this);
        apvts.addParameterListener(PID::bandParam(b, PID::StereoLink), this);
        apvts.addParameterListener(PID::bandParam(b, PID::BandBypass), this);
        apvts.addParameterListener(PID::bandParam(b, PID::ExtSc), this);
        apvts.addParameterListener(PID::bandParam(b, PID::Saturation), this);
        apvts.addParameterListener(PID::bandParam(b, PID::AutoMakeup), this);
        if (b > 0)
            apvts.addParameterListener(PID::bandParam(b, PID::Crossover), this);
    }

    // Aplica los valores iniciales al DSP
    syncAllParametersToDSP();
}

PedagogicalCompressorAudioProcessor::~PedagogicalCompressorAudioProcessor()
{
    apvts.removeParameterListener(PID::InputGain, this);
    apvts.removeParameterListener(PID::OutputGain, this);
    apvts.removeParameterListener(PID::GlobalMix, this);
    apvts.removeParameterListener(PID::LimiterCeiling, this);
    apvts.removeParameterListener(PID::Bypass, this);
    apvts.removeParameterListener(PID::AutoGain, this);
    apvts.removeParameterListener(PID::NumBands, this);
    apvts.removeParameterListener(PID::Preset, this);

    for (int b = 0; b < MultibandProcessor::kMaxBands; ++b)
    {
        apvts.removeParameterListener(PID::bandParam(b, PID::Threshold), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::Ratio), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::Attack), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::Release), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::Knee), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::Makeup), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::Mix), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::Type), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::Detector), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::ScHpf), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::Lookahead), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::StereoLink), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::BandBypass), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::ExtSc), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::Saturation), this);
        apvts.removeParameterListener(PID::bandParam(b, PID::AutoMakeup), this);
        if (b > 0)
            apvts.removeParameterListener(PID::bandParam(b, PID::Crossover), this);
    }
}

// =========================================================================
//  prepareToPlay
// =========================================================================
void PedagogicalCompressorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;

    multiband.prepare(spec);

    limiterCeilingLinear = std::pow(10.0f, limiterCeilingDb.load() * 0.05f);

    // Auto-Gain global: reset y smoothed value lento
    autoGainSmoothed.reset(sampleRate, 0.5);
    autoGainSmoothed.setCurrentAndTargetValue(1.0f);
    runningInLevelDb = -60.0f;
    runningOutLevelDb = -60.0f;

    syncAllParametersToDSP();
}

void PedagogicalCompressorAudioProcessor::releaseResources()
{
    multiband.reset();
}

bool PedagogicalCompressorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Requiere al menos main stereo output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Input main debe ser stereo
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Sidechain opcional: si está, debe ser stereo
    const auto sc = layouts.getChannelSet(true, 1);
    if (sc != juce::AudioChannelSet::disabled() && sc != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

// =========================================================================
//  processBlock
// =========================================================================
void PedagogicalCompressorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();

    if (numSamples == 0) return;

    // Limpia canales de salida extra
    for (int ch = 2; ch < numOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    auto* inL = buffer.getWritePointer(0);
    auto* inR = buffer.getWritePointer(1);

    // Sidechain externo (canales 3/4 si el host los manda).
    // DEFENSA v1.2.2: comprobar también buffer.getNumChannels() — algunos
    // hosts activan el bus sidechain en el layout pero no conectan los
    // pines; getWritePointer() sobre un canal inexistente devuelve basura
    // y eso dispara el compresor con señal aleatoria (= saturación).
    float* scL = inL;
    float* scR = inR;
    const int bufferChannels = buffer.getNumChannels();
    if (numInputChannels >= 4 && bufferChannels >= 4)
    {
        scL = buffer.getWritePointer(2);
        scR = buffer.getWritePointer(3);
    }

    // 1) Input gain
    const float inGain = std::pow(10.0f, inputGainDb.load() * 0.05f);
    if (inGain != 1.0f)
    {
        juce::FloatVectorOperations::multiply(inL, inGain, numSamples);
        juce::FloatVectorOperations::multiply(inR, inGain, numSamples);
    }

    // Mide nivel de entrada
    float maxIn = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float m = std::max(std::abs(inL[i]), std::abs(inR[i]));
        if (m > maxIn) maxIn = m;
    }
    const float measuredInputDb = maxIn > 1e-6f ? 20.0f * std::log10(maxIn) : -100.0f;
    inputLevelDb.store(measuredInputDb);

    // Bypass?
    if (globalBypass.load())
    {
        outputLevelDb.store(inputLevelDb.load());
        totalGrDb.store(0.0f);
        autoGainGainDb.store(0.0f);
        return;
    }

    // Guarda copia seca para Global Mix (preserva fase respecto a la entrada)
    // y para el Auto-Gain (compara dry vs wet RMS).
    juce::AudioBuffer<float> dryCopy;
    const bool needDry = (globalMix.load() < 0.999f) || autoGainEnabled.load();
    if (needDry)
    {
        dryCopy.setSize(2, numSamples, false, false, true);
        dryCopy.copyFrom(0, 0, inL, numSamples);
        dryCopy.copyFrom(1, 0, inR, numSamples);
    }

    // 2) Multiband compression
    multiband.process(inL, inR, scL, scR, numSamples);

    // 3) Global Mix (dry/wet) — con coherencia de fase porque dryCopy es pre-multiband
    const float gMix = globalMix.load();
    if (gMix < 0.999f && needDry)
    {
        auto* dryL = dryCopy.getReadPointer(0);
        auto* dryR = dryCopy.getReadPointer(1);
        const float oneMinusMix = 1.0f - gMix;
        for (int i = 0; i < numSamples; ++i)
        {
            inL[i] = dryL[i] * oneMinusMix + inL[i] * gMix;
            inR[i] = dryR[i] * oneMinusMix + inR[i] * gMix;
        }
    }

    // 4) Output gain
    const float outGain = std::pow(10.0f, outputGainDb.load() * 0.05f);
    if (outGain != 1.0f)
    {
        juce::FloatVectorOperations::multiply(inL, outGain, numSamples);
        juce::FloatVectorOperations::multiply(inR, outGain, numSamples);
    }

    // 4b) Auto-Gain global: compensa la diferencia de loudness entre
    //     el dryCopy (señal de entrada pre-compresión) y la salida post-output-gain.
    //     Si el compresor bajó el nivel en X dB, Auto-Gain sube X dB.
    if (autoGainEnabled.load() && needDry)
    {
        // RMS del dry
        float sumDrySq = 0.0f;
        auto* dryL = dryCopy.getReadPointer(0);
        auto* dryR = dryCopy.getReadPointer(1);
        for (int i = 0; i < numSamples; ++i)
        {
            sumDrySq += dryL[i] * dryL[i] + dryR[i] * dryR[i];
        }
        const float dryRms = std::sqrt(sumDrySq / (2.0f * numSamples) + 1e-12f);
        const float dryDb = 20.0f * std::log10(dryRms);

        // RMS del wet
        float sumWetSq = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            sumWetSq += inL[i] * inL[i] + inR[i] * inR[i];
        }
        const float wetRms = std::sqrt(sumWetSq / (2.0f * numSamples) + 1e-12f);
        const float wetDb = 20.0f * std::log10(wetRms);

        // Suavizado tipo leaky integrator (FIX v1.2.2: más lento para que
        // no bombee)
        const float alpha = 0.002f;
        runningInLevelDb = runningInLevelDb * (1.0f - alpha) + dryDb * alpha;
        runningOutLevelDb = runningOutLevelDb * (1.0f - alpha) + wetDb * alpha;

        // Target: outputLevel ≈ inputLevel → compensar la diferencia.
        // FIX v1.2.2: límite máximo +12 dB (antes +18 dB: empujaba la señal
        // directo contra el limiter y sonaba saturado).
        float targetGainDb = juce::jlimit(-12.0f, 12.0f, runningInLevelDb - runningOutLevelDb);
        autoGainSmoothed.setTargetValue(std::pow(10.0f, targetGainDb * 0.05f));
        autoGainSmoothed.skip(numSamples);
        autoGainGainDb.store(targetGainDb);

        const float ag = autoGainSmoothed.getCurrentValue();
        juce::FloatVectorOperations::multiply(inL, ag, numSamples);
        juce::FloatVectorOperations::multiply(inR, ag, numSamples);
    }
    else
    {
        autoGainGainDb.store(0.0f);
    }

    // 5) Limiter de salida — red de seguridad ANTI-SATURACIÓN:
    //    a) NaN/Inf → 0 (protege los parlantes ante cualquier patología
    //       numérica del DSP — un solo NaN puede "congelar" un DAC en
    //       nivel máximo y dañar los tweeters).
    //    b) Soft-clip cúbico que sólo actúa por encima del 95% del ceiling.
    //    c) Hard brick-wall en el ceiling como tope absoluto.
    const float ceiling = limiterCeilingLinear;
    const float softStart = ceiling * 0.95f;  // soft clip empieza al 95% del ceiling
    for (int i = 0; i < numSamples; ++i)
    {
        // (a) Guard NaN/Inf
        if (!std::isfinite(inL[i])) inL[i] = 0.0f;
        if (!std::isfinite(inR[i])) inR[i] = 0.0f;

        auto softClip = [&](float s) {
            float abs_s = std::abs(s);
            if (abs_s <= softStart)
                return s;  // sin tocar la señal por debajo del 95%
            // Zona de soft-clip: 95%..100% del ceiling.
            // Curva cúbica: dentro de la zona suave, el output crece pero
            // se aplasta al acercarse al ceiling.
            float sign = (s >= 0.0f) ? 1.0f : -1.0f;
            float over = (abs_s - softStart) / (ceiling - softStart);  // 0..N
            // Función de transferencia: y = softStart + (ceiling - softStart) * (1 - (1 - x)^3)
            // donde x es over clampado a [0,1].  Para over > 1 (señal por encima
            // del ceiling en la entrada), el hard brick-wall lo captura.
            float x = juce::jlimit(0.0f, 1.0f, over);
            float y = softStart + (ceiling - softStart) * (1.0f - std::pow(1.0f - x, 3.0f));
            return sign * y;
        };
        inL[i] = softClip(inL[i]);
        inR[i] = softClip(inR[i]);
        // Hard brick-wall como red de seguridad final
        inL[i] = juce::jlimit(-ceiling, ceiling, inL[i]);
        inR[i] = juce::jlimit(-ceiling, ceiling, inR[i]);
    }

    // Mide nivel de salida
    float maxOut = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float m = std::max(std::abs(inL[i]), std::abs(inR[i]));
        if (m > maxOut) maxOut = m;
    }
    outputLevelDb.store(maxOut > 1e-6f ? 20.0f * std::log10(maxOut) : -100.0f);

    // GR total (suma de GRs absolutos de cada banda)
    float grSum = 0.0f;
    int nb = multiband.getNumBands();
    for (int b = 0; b < nb; ++b)
        grSum += multiband.getBand(b).getGainReductionDb();
    totalGrDb.store(grSum / juce::jmax(1, nb));
}

// =========================================================================
//  parameterChanged  (hilo de mensaje → DSP)
// =========================================================================
void PedagogicalCompressorAudioProcessor::parameterChanged(const juce::String& paramID, float newValue)
{
    // Globales
    if (paramID == PID::InputGain)       { inputGainDb.store(newValue); return; }
    if (paramID == PID::OutputGain)      { outputGainDb.store(newValue); return; }
    if (paramID == PID::GlobalMix)        { globalMix.store(newValue); return; }
    if (paramID == PID::LimiterCeiling)
    {
        limiterCeilingDb.store(newValue);
        limiterCeilingLinear = std::pow(10.0f, newValue * 0.05f);
        return;
    }
    if (paramID == PID::Bypass)          { globalBypass.store(newValue > 0.5f); return; }
    if (paramID == PID::AutoGain)        { autoGainEnabled.store(newValue > 0.5f); return; }
    if (paramID == PID::NumBands)
    {
        int n = static_cast<int>(newValue) + 1;  // Choice 0..3 → 1..4 bandas
        multiband.setNumBands(n);
        return;
    }
    if (paramID == PID::Preset)
    {
        // Aplica preset
        int idx = static_cast<int>(newValue);
        if (idx >= 0 && idx < Presets::getNumPresets())
        {
            currentPreset.store(idx);
            applyPresetInternal(Presets::getPresets().getChild(idx));
        }
        return;
    }

    // Por banda: parsear "b<idx>_<suffix>"
    if (paramID.startsWith("b"))
    {
        int band = paramID.substring(1).getIntValue();
        juce::String suffix = paramID.fromFirstOccurrenceOf("_", false, false);

        if (band < 0 || band >= MultibandProcessor::kMaxBands) return;

        auto& b = multiband.getBand(band);

        if (suffix == PID::Threshold) b.setThreshold(newValue);
        else if (suffix == PID::Ratio)     b.setRatio(newValue);
        else if (suffix == PID::Attack)    b.setAttack(newValue);
        else if (suffix == PID::Release)   b.setRelease(newValue);
        else if (suffix == PID::Knee)       b.setKnee(newValue);
        else if (suffix == PID::Makeup)     b.setMakeup(newValue);
        else if (suffix == PID::Mix)        b.setMix(newValue);
        else if (suffix == PID::Type)       b.setType(static_cast<CompressorType>(static_cast<int>(newValue)));
        else if (suffix == PID::Detector)   b.setDetector(static_cast<DetectorMode>(static_cast<int>(newValue)));
        else if (suffix == PID::ScHpf)      b.setSidechainHPF(newValue);
        else if (suffix == PID::Lookahead)  b.setLookahead(newValue);
        else if (suffix == PID::StereoLink) b.setStereoLink(newValue);
        else if (suffix == PID::BandBypass) b.setBypass(newValue > 0.5f);
        else if (suffix == PID::ExtSc)      b.setExtSidechain(newValue > 0.5f);
        else if (suffix == PID::Saturation) b.setSaturation(newValue);
        else if (suffix == PID::AutoMakeup) b.setAutoMakeup(newValue > 0.5f);
        else if (suffix == PID::Crossover && band > 0)
            multiband.setCrossover(band - 1, newValue);
    }
}

void PedagogicalCompressorAudioProcessor::syncAllParametersToDSP()
{
    parameterChanged(PID::InputGain, apvts.getRawParameterValue(PID::InputGain)->load());
    parameterChanged(PID::OutputGain, apvts.getRawParameterValue(PID::OutputGain)->load());
    parameterChanged(PID::GlobalMix, apvts.getRawParameterValue(PID::GlobalMix)->load());
    parameterChanged(PID::LimiterCeiling, apvts.getRawParameterValue(PID::LimiterCeiling)->load());
    parameterChanged(PID::Bypass, apvts.getRawParameterValue(PID::Bypass)->load());
    parameterChanged(PID::AutoGain, apvts.getRawParameterValue(PID::AutoGain)->load());
    parameterChanged(PID::NumBands, apvts.getRawParameterValue(PID::NumBands)->load());
    parameterChanged(PID::Preset, apvts.getRawParameterValue(PID::Preset)->load());

    for (int b = 0; b < MultibandProcessor::kMaxBands; ++b)
    {
        parameterChanged(PID::bandParam(b, PID::Threshold), apvts.getRawParameterValue(PID::bandParam(b, PID::Threshold))->load());
        parameterChanged(PID::bandParam(b, PID::Ratio),     apvts.getRawParameterValue(PID::bandParam(b, PID::Ratio))->load());
        parameterChanged(PID::bandParam(b, PID::Attack),    apvts.getRawParameterValue(PID::bandParam(b, PID::Attack))->load());
        parameterChanged(PID::bandParam(b, PID::Release),   apvts.getRawParameterValue(PID::bandParam(b, PID::Release))->load());
        parameterChanged(PID::bandParam(b, PID::Knee),      apvts.getRawParameterValue(PID::bandParam(b, PID::Knee))->load());
        parameterChanged(PID::bandParam(b, PID::Makeup),    apvts.getRawParameterValue(PID::bandParam(b, PID::Makeup))->load());
        parameterChanged(PID::bandParam(b, PID::Mix),       apvts.getRawParameterValue(PID::bandParam(b, PID::Mix))->load());
        parameterChanged(PID::bandParam(b, PID::Type),      apvts.getRawParameterValue(PID::bandParam(b, PID::Type))->load());
        parameterChanged(PID::bandParam(b, PID::Detector),   apvts.getRawParameterValue(PID::bandParam(b, PID::Detector))->load());
        parameterChanged(PID::bandParam(b, PID::ScHpf),      apvts.getRawParameterValue(PID::bandParam(b, PID::ScHpf))->load());
        parameterChanged(PID::bandParam(b, PID::Lookahead), apvts.getRawParameterValue(PID::bandParam(b, PID::Lookahead))->load());
        parameterChanged(PID::bandParam(b, PID::StereoLink),apvts.getRawParameterValue(PID::bandParam(b, PID::StereoLink))->load());
        parameterChanged(PID::bandParam(b, PID::BandBypass),apvts.getRawParameterValue(PID::bandParam(b, PID::BandBypass))->load());
        parameterChanged(PID::bandParam(b, PID::ExtSc),     apvts.getRawParameterValue(PID::bandParam(b, PID::ExtSc))->load());
        parameterChanged(PID::bandParam(b, PID::Saturation), apvts.getRawParameterValue(PID::bandParam(b, PID::Saturation))->load());
        parameterChanged(PID::bandParam(b, PID::AutoMakeup), apvts.getRawParameterValue(PID::bandParam(b, PID::AutoMakeup))->load());
        if (b > 0)
            parameterChanged(PID::bandParam(b, PID::Crossover),
                              apvts.getRawParameterValue(PID::bandParam(b, PID::Crossover))->load());
    }
}

// =========================================================================
//  Aplicar preset (silenciosamente, sin disparar listeners de vuelta)
// =========================================================================
void PedagogicalCompressorAudioProcessor::applyPresetInternal(const juce::ValueTree& preset)
{
    if (!preset.isValid()) return;

    // NOTA: no se suspenden los listeners a propósito — cada setValueNotifyingHost
    // dispara parameterChanged, que es justamente lo que sincroniza el DSP.
    // No hay recursión porque parameterChanged nunca escribe de vuelta en el APVTS.

    // Num bands
    int nb = preset.getProperty("numBands", 1);
    nb = juce::jlimit(1, MultibandProcessor::kMaxBands, nb);
    apvts.getParameter(PID::NumBands)->setValueNotifyingHost(
        static_cast<float>(nb - 1) / 3.0f);

    multiband.setNumBands(nb);

    // Crossovers
    for (int i = 0; i < nb - 1; ++i)
    {
        float freq = static_cast<float>(preset.getProperty("cross" + juce::String(i), 1000.0));
        if (i + 1 < MultibandProcessor::kMaxBands)
        {
            auto* p = dynamic_cast<juce::AudioParameterFloat*>(
                apvts.getParameter(PID::bandParam(i + 1, PID::Crossover)));
            if (p)
            {
                p->setValueNotifyingHost(p->convertTo0to1(freq));
            }
        }
    }

    // Parámetros por banda
    for (int b = 0; b < nb; ++b)
    {
        auto setType = [&](const char* suffix, float val)
        {
            auto* p = dynamic_cast<juce::AudioParameterFloat*>(
                apvts.getParameter(PID::bandParam(b, suffix)));
            if (p) p->setValueNotifyingHost(p->convertTo0to1(val));
        };
        auto setChoice = [&](const char* suffix, int idx, int maxIdx)
        {
            auto* p = dynamic_cast<juce::AudioParameterChoice*>(
                apvts.getParameter(PID::bandParam(b, suffix)));
            if (p)
                p->setValueNotifyingHost(static_cast<float>(idx) / juce::jmax(1, maxIdx));
        };
        auto setBool = [&](const char* suffix, bool val)
        {
            auto* p = dynamic_cast<juce::AudioParameterBool*>(
                apvts.getParameter(PID::bandParam(b, suffix)));
            if (p) p->setValueNotifyingHost(val ? 1.0f : 0.0f);
        };

        setType(PID::Threshold, static_cast<float>(preset.getProperty("thr" + juce::String(b), 0.0)));
        setType(PID::Ratio,     static_cast<float>(preset.getProperty("rat" + juce::String(b), 4.0)));
        setType(PID::Attack,    static_cast<float>(preset.getProperty("att" + juce::String(b), 10.0)));
        setType(PID::Release,   static_cast<float>(preset.getProperty("rel" + juce::String(b), 100.0)));
        setType(PID::Knee,      static_cast<float>(preset.getProperty("knee" + juce::String(b), 3.0)));
        setType(PID::Makeup,    static_cast<float>(preset.getProperty("mk"  + juce::String(b), 0.0)));
        setType(PID::Mix,       static_cast<float>(preset.getProperty("mix" + juce::String(b), 1.0)));
        setChoice(PID::Type,    static_cast<int>(preset.getProperty("type" + juce::String(b), 0)), 4);
        setChoice(PID::Detector,static_cast<int>(preset.getProperty("det"  + juce::String(b), 1)), 1);
        setType(PID::ScHpf,      static_cast<float>(preset.getProperty("schpf" + juce::String(b), 80.0)));
        setType(PID::Lookahead,  static_cast<float>(preset.getProperty("la" + juce::String(b), 0.0)));
        setType(PID::StereoLink, static_cast<float>(preset.getProperty("sl" + juce::String(b), 1.0)));
        setType(PID::Saturation, static_cast<float>(preset.getProperty("sat" + juce::String(b), 0.0)));
        // Auto-Makeup: defaults to off unless preset overrides
        setBool(PID::AutoMakeup, static_cast<int>(preset.getProperty("amk" + juce::String(b), 0)) != 0);
    }
    // NOTA: NO llamar a syncAllParametersToDSP() aquí — cada
    // setValueNotifyingHost ya dispara parameterChanged que sincroniza el DSP.
    // Llamarlo aquí causaría recursión infinita (preset → sync → preset → …)
    // y un stack overflow que crashea el host al cargar el plugin.
}

// =========================================================================
//  Program/Preset API
// =========================================================================
void PedagogicalCompressorAudioProcessor::setCurrentProgram(int idx)
{
    setPreset(idx);
}

void PedagogicalCompressorAudioProcessor::setPreset(int idx)
{
    if (idx < 0 || idx >= Presets::getNumPresets()) return;
    if (currentPreset.load() == idx) return;
    currentPreset.store(idx);

    // Al cambiar el parámetro "preset", parameterChanged() aplica el preset
    // al DSP (ver applyPresetInternal). No lo duplicamos aquí para evitar
    // doble aplicación y recursion.
    auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(PID::Preset));
    if (p)
        p->setValueNotifyingHost(p->convertTo0to1(idx));
}

void PedagogicalCompressorAudioProcessor::setNumBands(int n)
{
    n = juce::jlimit(1, MultibandProcessor::kMaxBands, n);
    auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(PID::NumBands));
    if (p)
        p->setValueNotifyingHost(static_cast<float>(n - 1) / 3.0f);
    multiband.setNumBands(n);
}

// =========================================================================
//  State save/load
// =========================================================================
void PedagogicalCompressorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PedagogicalCompressorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        syncAllParametersToDSP();
    }
}

// =========================================================================
//  Editor
// =========================================================================
juce::AudioProcessorEditor* PedagogicalCompressorAudioProcessor::createEditor()
{
    return new PedagogicalCompressorAudioProcessorEditor(*this);
}

// =========================================================================
//  Create
// =========================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PedagogicalCompressorAudioProcessor();
}

/*
 * PluginProcessor.cpp — versión profesional (rev. 3)
 * ─────────────────────────────────────────────────────────────────────────
 * Rev. 3 — correcciones de estabilidad y calidad:
 *   • TODOS los cambios de parámetros se aplican en el AUDIO THREAD vía
 *     flags atómicos (antes la GUI escribía el DSP en plena reproducción
 *     → clicks y crashes).
 *   • El sidechain externo (entrada 3/4 del plugin) se cablea al DSP.
 *   • Bypass global = passthrough puro + protección anti-clip.
 *   • Limiter de salida CONTINUO (sin la discontinuidad que hacía POPS).
 *   • Vista del bus principal: los canales del sidechain ya no se
 *     procesan como si fueran audio de mezcla.
 *   • Todas las bandas activas por defecto.
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    float measurePeak (const juce::AudioBuffer<float>& buf) noexcept
    {
        float peak = 0.0f;
        for (int c = 0; c < buf.getNumChannels(); ++c)
        {
            const float* d = buf.getReadPointer (c);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const float a = std::abs (d[i]);
                if (a > peak) peak = a;
            }
        }
        return peak;
    }

    // ── Fix de exactitud del estado (rev. 4) ─────────────────────────
    // Problema detectado con pluginval rigor 10 y extras/StateDebug.cpp:
    //
    //  1. Un host puede cambiar un parámetro con setValue() crudo (sin
    //     setValueNotifyingHost). La caché interna del APVTS no se entera.
    //  2. RangedAudioParameter::convertTo0to1/From0to1 aplican
    //     snapToLegalValue: los bools (rango {0,1,intervalo 1}) cuantizan
    //     0.8 → 1.0 al pasar por la caché.
    //
    // Consecuencias sin fix: copyState() guarda valores viejos o con snap,
    // y replaceState() omite parámetros desincronizados.
    //
    // Solución: NUNCA confiar en el árbol interno del APVTS para el estado
    // I/O — al guardar se corrige la copia con el valor crudo real de cada
    // parámetro; al restaurar se fuerza el valor EXACTO leído del XML
    // guardado (conversiones sin snap).
    // ───────────────────────────────────────────────────────────────────

    // Valor normalizado crudo (SIN snapToLegalValue) de un child "value"
    float treeChildToRawNormalized (const juce::RangedAudioParameter& ranged,
                                    const juce::ValueTree& child)
    {
        const auto& range = ranged.getNormalisableRange();
        const float defaultDenorm = range.convertFrom0to1 (ranged.getDefaultValue());
        const float denorm = (float) (double) child.getProperty ("value", (double) defaultDenorm);
        return range.convertTo0to1 (denorm);
    }
}

namespace ids
{
    #define P(name, str) static const juce::String name { str };
    P(BandCount,        "BandCount")
    P(Crossover1,       "Crossover1")
    P(Crossover2,       "Crossover2")
    P(Crossover3,       "Crossover3")
    P(GlobalBypass,     "GlobalBypass")
    P(GlobalMix,        "GlobalMix")
    P(OutputGain,       "OutputGain")
    P(InputGain,        "InputGain")
    P(LimiterOn,        "LimiterOn")
    P(CeilingDb,        "CeilingDb")

    P(BandEnabledSuffix,  "Enabled")
    P(BandBypassSuffix,  "Bypass")
    P(ThresholdSuffix,    "Threshold")
    P(RatioSuffix,        "Ratio")
    P(AttackSuffix,       "Attack")
    P(ReleaseSuffix,      "Release")
    P(KneeSuffix,         "Knee")
    P(MakeupSuffix,       "Makeup")
    P(MixSuffix,          "Mix")
    P(CompTypeSuffix,     "CompType")
    P(DetectionSuffix,    "Detection")
    P(StereoLinkSuffix,   "StereoLink")
    P(ScHpfSuffix,        "ScHpf")
    P(ExternalScSuffix,   "ExtSc")
    P(LookaheadSuffix,    "Lookahead")
    P(OversamplingSuffix, "Oversampling")
    #undef P

    inline juce::String bandId (int band, const juce::String& suffix)
    {
        return "Band" + juce::String (band) + suffix;
    }
}

// ─────────────────────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
PedagogicalCompressorAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterChoice>(
        ids::BandCount, "Band Count",
        StringArray { "1 Band", "2 Bands", "3 Bands", "4 Bands" }, 0));

    params.push_back (std::make_unique<AudioParameterFloat>(
        ids::Crossover1, "Crossover 1",
        NormalisableRange<float>(40.0f, 4000.0f, 1.0f, 0.4f), 150.0f));
    params.push_back (std::make_unique<AudioParameterFloat>(
        ids::Crossover2, "Crossover 2",
        NormalisableRange<float>(200.0f, 12000.0f, 1.0f, 0.4f), 1200.0f));
    params.push_back (std::make_unique<AudioParameterFloat>(
        ids::Crossover3, "Crossover 3",
        NormalisableRange<float>(1000.0f, 18000.0f, 1.0f, 0.4f), 6000.0f));

    params.push_back (std::make_unique<AudioParameterBool>(ids::GlobalBypass, "Global Bypass", false));
    params.push_back (std::make_unique<AudioParameterFloat>(
        ids::GlobalMix, "Global Mix", NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back (std::make_unique<AudioParameterFloat>(
        ids::InputGain, "Input Gain", NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<AudioParameterFloat>(
        ids::OutputGain, "Output Gain", NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back (std::make_unique<AudioParameterBool>(ids::LimiterOn, "Output Limiter", true));
    params.push_back (std::make_unique<AudioParameterFloat>(
        ids::CeilingDb, "Output Ceiling", NormalisableRange<float>(-6.0f, 0.0f, 0.1f), -0.3f));

    for (int b = 0; b < 4; ++b)
    {
        const juce::String prefix = "Band" + juce::String (b);
        // Todas las bandas activas por defecto (rev. 3)
        params.push_back (std::make_unique<AudioParameterBool>(prefix + ids::BandEnabledSuffix, prefix + " Enabled", true));
        params.push_back (std::make_unique<AudioParameterFloat>(prefix + ids::ThresholdSuffix, prefix + " Threshold", NormalisableRange<float>(-60.0f, 0.0f, 0.1f, 0.4f), -20.0f));
        params.push_back (std::make_unique<AudioParameterFloat>(prefix + ids::RatioSuffix, prefix + " Ratio", NormalisableRange<float>(1.0f, 20.0f, 0.01f, 0.4f), 4.0f));
        params.push_back (std::make_unique<AudioParameterFloat>(prefix + ids::AttackSuffix, prefix + " Attack", NormalisableRange<float>(0.1f, 200.0f, 0.1f, 0.4f), 10.0f));
        params.push_back (std::make_unique<AudioParameterFloat>(prefix + ids::ReleaseSuffix, prefix + " Release", NormalisableRange<float>(10.0f, 5000.0f, 1.0f, 0.4f), 100.0f));
        params.push_back (std::make_unique<AudioParameterFloat>(prefix + ids::KneeSuffix, prefix + " Knee", NormalisableRange<float>(0.0f, 24.0f, 0.1f), 6.0f));
        params.push_back (std::make_unique<AudioParameterFloat>(prefix + ids::MakeupSuffix, prefix + " Makeup", NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f));
        params.push_back (std::make_unique<AudioParameterFloat>(prefix + ids::MixSuffix, prefix + " Mix", NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
        params.push_back (std::make_unique<AudioParameterChoice>(prefix + ids::CompTypeSuffix, prefix + " Type", juce::StringArray { "Digital", "VCA", "FET", "Optical", "Vari-Mu" }, 0));
        params.push_back (std::make_unique<AudioParameterChoice>(prefix + ids::DetectionSuffix, prefix + " Detection", juce::StringArray { "Peak", "RMS" }, 0));
        params.push_back (std::make_unique<AudioParameterFloat>(prefix + ids::StereoLinkSuffix, prefix + " Stereo Link", NormalisableRange<float>(0.0f, 1.0f), 1.0f));
        params.push_back (std::make_unique<AudioParameterFloat>(prefix + ids::ScHpfSuffix, prefix + " SC HPF", NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 0.4f), 20.0f));
        params.push_back (std::make_unique<AudioParameterBool>(prefix + ids::ExternalScSuffix, prefix + " Ext Sidechain", false));
        params.push_back (std::make_unique<AudioParameterFloat>(prefix + ids::LookaheadSuffix, prefix + " Lookahead", NormalisableRange<float>(0.0f, 20.0f, 0.1f), 0.0f));
        params.push_back (std::make_unique<AudioParameterChoice>(prefix + ids::OversamplingSuffix, prefix + " Oversampling", juce::StringArray { "1x", "2x", "4x" }, 0));
        params.push_back (std::make_unique<AudioParameterBool>(prefix + ids::BandBypassSuffix, prefix + " Bypass", false));
    }

    return { params.begin(), params.end() };
}

// ─────────────────────────────────────────────────────────────────────────
PedagogicalCompressorAudioProcessor::PedagogicalCompressorAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Main",       juce::AudioChannelSet::stereo(), true)
        .withInput  ("Sidechain",  juce::AudioChannelSet::stereo(), false)
        .withOutput ("Main",       juce::AudioChannelSet::stereo(), true))
    , apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Listener para todos los parámetros
    apvts.addParameterListener (ids::BandCount, this);
    apvts.addParameterListener (ids::Crossover1, this);
    apvts.addParameterListener (ids::Crossover2, this);
    apvts.addParameterListener (ids::Crossover3, this);
    apvts.addParameterListener (ids::GlobalBypass, this);
    apvts.addParameterListener (ids::GlobalMix, this);
    apvts.addParameterListener (ids::InputGain, this);
    apvts.addParameterListener (ids::OutputGain, this);
    apvts.addParameterListener (ids::LimiterOn, this);
    apvts.addParameterListener (ids::CeilingDb, this);

    static const juce::StringArray bandSuffixes {
        ids::ThresholdSuffix, ids::RatioSuffix, ids::AttackSuffix, ids::ReleaseSuffix,
        ids::KneeSuffix, ids::MakeupSuffix, ids::MixSuffix, ids::CompTypeSuffix,
        ids::DetectionSuffix, ids::StereoLinkSuffix, ids::ScHpfSuffix,
        ids::ExternalScSuffix, ids::LookaheadSuffix, ids::OversamplingSuffix,
        ids::BandEnabledSuffix, ids::BandBypassSuffix
    };

    for (int b = 0; b < 4; ++b)
        for (const auto& suffix : bandSuffixes)
            apvts.addParameterListener (ids::bandId (b, suffix), this);

    // Antes de que exista audio, los updates directos son seguros
    updateBandCountFromParams();
    updateBandParameters();
    updateCrossoversFromParams();
}

void PedagogicalCompressorAudioProcessor::parameterChanged (const juce::String& paramID, float)
{
    // Solo marcar flags: el AUDIO THREAD aplica los cambios en su próximo
    // bloque (patrón thread-safe, sin locks en tiempo real)
    needsParameterUpdate.store (true);

    if (paramID == ids::BandCount)
        needsBandCountUpdate.store (true);
    if (paramID == ids::Crossover1 || paramID == ids::Crossover2 || paramID == ids::Crossover3)
        needsCrossoverUpdate.store (true);
}

void PedagogicalCompressorAudioProcessor::setBandCount (int count)
{
    // Thread-safe (llamado desde la GUI): publica el valor y marca el flag;
    // el audio thread llama a multiband.setBandCount + updates.
    const int c = juce::jlimit (1, 4, count);
    currentBandCount.store (c);
    pendingBandCount.store (c);
    needsBandCountUpdate.store (true);
    needsParameterUpdate.store (true);
}

void PedagogicalCompressorAudioProcessor::setCrossoverFrequency (int index, float freqHz)
{
    // Thread-safe: solo se notifica al host; el cambio llega al DSP vía
    // parameterChanged → flag → audio thread (nada de escribir filtros aquí)
    const auto pid = index == 0 ? ids::Crossover1 : (index == 1 ? ids::Crossover2 : ids::Crossover3);
    if (auto* param = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (pid)))
        param->setValueNotifyingHost (param->convertTo0to1 (freqHz));
}

// ─────────────────────────────────────────────────────────────────────────
void PedagogicalCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int mainIn = getMainBusNumInputChannels();
    const int numCh = juce::jlimit (1, 2, mainIn > 0 ? mainIn : 2);

    multiband.prepare (sampleRate, juce::jmax (1, samplesPerBlock), numCh);
    multiband.setBandCount (currentBandCount.load());

    updateBandParameters();       // escribe targets — el audio aún no corre
    updateCrossoversFromParams();

    preGain.prepare   ({ sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), (juce::uint32) numCh });
    postGain.prepare  ({ sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), (juce::uint32) numCh });

    preGain.setRampDurationSeconds (0.02);
    postGain.setRampDurationSeconds (0.02);

    preGain.setGainDecibels (apvts.getRawParameterValue (ids::InputGain)->load());
    postGain.setGainDecibels (apvts.getRawParameterValue (ids::OutputGain)->load());

    dryBuffer.setSize (numCh, juce::jmax (samplesPerBlock * 2, 4096), false, false, true);
    dryBuffer.clear();

    isPrepared.store (true);
}

void PedagogicalCompressorAudioProcessor::releaseResources()
{
    isPrepared.store (false);
    multiband.reset();
}

bool PedagogicalCompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;

    return true;
}

// ─────────────────────────────────────────────────────────────────────────
void PedagogicalCompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                         juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalCh = buffer.getNumChannels();
    const int numSamp = buffer.getNumSamples();
    if (totalCh <= 0 || numSamp <= 0) return;

    // Vista SOLO del bus principal (los canales del sidechain no se
    // procesan como audio de mezcla — antes sí y era un bug)
    const int mainCh = juce::jlimit (1, totalCh, juce::jmax (1, getMainBusNumInputChannels()));
    juce::AudioBuffer<float> main (buffer.getArrayOfWritePointers(), mainCh, numSamp);

    // Sidechain externo = canales que van después del bus principal
    juce::AudioBuffer<float> scView;
    const juce::AudioBuffer<float>* scPtr = nullptr;
    if (totalCh > mainCh)
    {
        scView = juce::AudioBuffer<float> (buffer.getArrayOfWritePointers() + mainCh,
                                           totalCh - mainCh, numSamp);
        scPtr = &scView;
    }

    if (! isPrepared.load())
    {
        main.clear();
        return;
    }

    // ── Aplicar cambios pendientes EN EL AUDIO THREAD (sin locks) ──
    if (needsParameterUpdate.exchange (false))
    {
        updateBandParameters();
        preGain.setGainDecibels (apvts.getRawParameterValue (ids::InputGain)->load());
        postGain.setGainDecibels (apvts.getRawParameterValue (ids::OutputGain)->load());
    }
    if (needsBandCountUpdate.exchange (false))
    {
        multiband.setBandCount (pendingBandCount.load());
        multiband.resetCrossovers();   // sin transitorios al reconstruir el split
        updateBandParameters();
    }
    if (needsCrossoverUpdate.exchange (false))
        updateCrossoversFromParams();

    const bool globalBypass = apvts.getRawParameterValue (ids::GlobalBypass)->load() > 0.5f;

    // ── BYPASS global: passthrough puro + protección de parlantes ──
    if (globalBypass)
    {
        overallInputDb.store (juce::Decibels::gainToDecibels (measurePeak (main)));
        overallGrDb.store (0.0f);
        applyOutputSafety (main);
        overallOutputDb.store (juce::Decibels::gainToDecibels (measurePeak (main)));
        return;
    }

    // ── Ganancia de entrada (ramp 20 ms anti-click) ──
    preGain.setGainDecibels (apvts.getRawParameterValue (ids::InputGain)->load());
    {
        juce::dsp::AudioBlock<float> block (main);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        preGain.process (ctx);
    }

    // dry copiado DESPUÉS del preGain (dry/wet al mismo nivel)
    if (dryBuffer.getNumChannels() < mainCh || dryBuffer.getNumSamples() < numSamp)
        dryBuffer.setSize (mainCh, juce::jmax (numSamp, dryBuffer.getNumSamples()), false, false, true);
    for (int c = 0; c < mainCh; ++c)
        dryBuffer.copyFrom (c, 0, main, c, 0, numSamp);

    overallInputDb.store (juce::Decibels::gainToDecibels (measurePeak (main)));

    // ── Compresión multibanda (con sidechain externo si el host lo cablea) ──
    multiband.process (main, scPtr);

    // ── Mix global dry/wet ──
    const float gMix = apvts.getRawParameterValue (ids::GlobalMix)->load();
    if (gMix < 0.999f)
    {
        for (int c = 0; c < mainCh; ++c)
        {
            const float* dry = dryBuffer.getReadPointer (c);
            float* wet = main.getWritePointer (c);
            for (int i = 0; i < numSamp; ++i)
                wet[i] = dry[i] * (1.0f - gMix) + wet[i] * gMix;
        }
    }

    // ── Ganancia de salida (ramp suave) ──
    postGain.setGainDecibels (apvts.getRawParameterValue (ids::OutputGain)->load());
    {
        juce::dsp::AudioBlock<float> block (main);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        postGain.process (ctx);
    }

    // ── Protección final anti-clip (limiter continuo, sin saltos) ──
    applyOutputSafety (main);

    overallOutputDb.store (juce::Decibels::gainToDecibels (measurePeak (main)));

    // GR promedio de bandas activas
    {
        float sumGr = 0.0f;
        int active = 0;
        const int nb = currentBandCount.load();
        for (int b = 0; b < nb; ++b)
        {
            const auto& bp = multiband.getBand (b).getParameters();
            if (bp.enabled && ! bp.bypass)
            {
                sumGr += multiband.getBand (b).getGainReductionDb();
                ++active;
            }
        }
        overallGrDb.store (active > 0 ? sumGr / (float) active : 0.0f);
    }
}

void PedagogicalCompressorAudioProcessor::applyOutputSafety (juce::AudioBuffer<float>& buffer)
{
    const bool limiterOn = apvts.getRawParameterValue (ids::LimiterOn)->load() > 0.5f;
    const float ceilingDb = limiterOn
        ? juce::jlimit (-6.0f, 0.0f, apvts.getRawParameterValue (ids::CeilingDb)->load())
        : 0.0f;
    const float ceilingLin = juce::Decibels::decibelsToGain (ceilingDb);
    const float knee = ceilingLin * 0.90f;
    const float range = juce::jmax (1.0e-6f, ceilingLin - knee);

    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        float* d = buffer.getWritePointer (c);
        const int n = buffer.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            const float ax = std::abs (d[i]);
            if (ax > knee)
            {
                // Continuo en el knee, asíntota = ceiling, clamp duro final.
                // (El limiter antiguo saltaba de 0.9c a 0.68c → POPS.)
                const float over = (ax - knee) / range;
                const float soft = knee + range * std::tanh (over);
                d[i] = std::copysign (juce::jmin (soft, ceilingLin), d[i]);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────
float PedagogicalCompressorAudioProcessor::getBandGainReductionDb (int b) const
{
    if (b < 0 || b >= currentBandCount.load()) return 0.0f;
    return multiband.getBand (b).getGainReductionDb();
}
float PedagogicalCompressorAudioProcessor::getBandPeakInputDb (int b) const
{
    if (b < 0 || b >= currentBandCount.load()) return -100.0f;
    return multiband.getBand (b).getPeakInputDb();
}
float PedagogicalCompressorAudioProcessor::getBandPeakOutputDb (int b) const
{
    if (b < 0 || b >= currentBandCount.load()) return -100.0f;
    return multiband.getBand (b).getPeakOutputDb();
}

// ─────────────────────────────────────────────────────────────────────────
void PedagogicalCompressorAudioProcessor::updateBandParameters()
{
    for (int b = 0; b < 4; ++b)
    {
        pedacomp::BandParameters p;
        p.thresholdDb  = apvts.getRawParameterValue (ids::bandId (b, ids::ThresholdSuffix))->load();
        p.ratio        = apvts.getRawParameterValue (ids::bandId (b, ids::RatioSuffix))->load();
        p.attackMs     = apvts.getRawParameterValue (ids::bandId (b, ids::AttackSuffix))->load();
        p.releaseMs    = apvts.getRawParameterValue (ids::bandId (b, ids::ReleaseSuffix))->load();
        p.kneeDb       = apvts.getRawParameterValue (ids::bandId (b, ids::KneeSuffix))->load();
        p.makeupDb     = apvts.getRawParameterValue (ids::bandId (b, ids::MakeupSuffix))->load();
        p.mix          = apvts.getRawParameterValue (ids::bandId (b, ids::MixSuffix))->load();
        p.compType     = (int) apvts.getRawParameterValue (ids::bandId (b, ids::CompTypeSuffix))->load();
        p.detection    = (int) apvts.getRawParameterValue (ids::bandId (b, ids::DetectionSuffix))->load();
        p.stereoLink   = apvts.getRawParameterValue (ids::bandId (b, ids::StereoLinkSuffix))->load();
        p.scHpfHz      = apvts.getRawParameterValue (ids::bandId (b, ids::ScHpfSuffix))->load();
        p.externalSidechain = apvts.getRawParameterValue (ids::bandId (b, ids::ExternalScSuffix))->load() > 0.5f;
        p.lookaheadMs  = apvts.getRawParameterValue (ids::bandId (b, ids::LookaheadSuffix))->load();
        const int osIdx = (int) apvts.getRawParameterValue (ids::bandId (b, ids::OversamplingSuffix))->load();
        p.oversampling = (osIdx == 0) ? 1 : (osIdx == 1 ? 2 : 4);
        p.enabled      = apvts.getRawParameterValue (ids::bandId (b, ids::BandEnabledSuffix))->load() > 0.5f;
        p.bypass       = apvts.getRawParameterValue (ids::bandId (b, ids::BandBypassSuffix))->load() > 0.5f;

        multiband.getBand (b).setParameters (p);
    }
}

void PedagogicalCompressorAudioProcessor::updateBandCountFromParams()
{
    const int newCount = 1 + (int) apvts.getRawParameterValue (ids::BandCount)->load();
    setBandCount (newCount);   // thread-safe (atómico + flag)
}

void PedagogicalCompressorAudioProcessor::updateCrossoversFromParams()
{
    multiband.setCrossover (0, apvts.getRawParameterValue (ids::Crossover1)->load());
    multiband.setCrossover (1, apvts.getRawParameterValue (ids::Crossover2)->load());
    multiband.setCrossover (2, apvts.getRawParameterValue (ids::Crossover3)->load());
}

// ─────────────────────────────────────────────────────────────────────────
int PedagogicalCompressorAudioProcessor::getNumPrograms()
{
    return (int) pedacomp::getFactoryPresets().size();
}

const juce::String PedagogicalCompressorAudioProcessor::getProgramName (int index)
{
    const auto presets = pedacomp::getFactoryPresets();
    if (index >= 0 && index < (int) presets.size())
        return presets[(size_t) index].name;
    return "Init";
}

void PedagogicalCompressorAudioProcessor::setCurrentProgram (int index)
{
    currentProgram = index;
    applyFactoryPreset (index);
}

void PedagogicalCompressorAudioProcessor::applyFactoryPreset (int presetIndex)
{
    const auto presets = pedacomp::getFactoryPresets();
    if (presetIndex < 0 || presetIndex >= (int) presets.size()) return;

    const auto& p = presets[(size_t) presetIndex];

    auto setFloat = [this] (const juce::String& pid, float value) {
        if (auto* param = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (pid)))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    apvts.getParameter (ids::BandCount)->setValueNotifyingHost ((float) (p.bandCount - 1) / 3.0f);
    setFloat (ids::Crossover1, p.crossovers[0]);
    setFloat (ids::Crossover2, p.crossovers[1]);
    setFloat (ids::Crossover3, p.crossovers[2]);
    setFloat (ids::InputGain,  p.inputGain);
    setFloat (ids::OutputGain, p.outputGain);
    setFloat (ids::GlobalMix,  p.globalMix);
    setFloat (ids::CeilingDb,  p.ceiling);
    apvts.getParameter (ids::GlobalBypass)->setValueNotifyingHost (p.globalBypass ? 1.0f : 0.0f);
    apvts.getParameter (ids::LimiterOn)->setValueNotifyingHost (p.limiterOn ? 1.0f : 0.0f);

    for (int b = 0; b < 4; ++b)
    {
        const auto& bs = p.bands[b];
        apvts.getParameter (ids::bandId (b, ids::BandEnabledSuffix))->setValueNotifyingHost (bs.enabled ? 1.0f : 0.0f);
        apvts.getParameter (ids::bandId (b, ids::BandBypassSuffix))->setValueNotifyingHost (bs.bypass ? 1.0f : 0.0f);
        setFloat (ids::bandId (b, ids::ThresholdSuffix), bs.threshold);
        setFloat (ids::bandId (b, ids::RatioSuffix),     bs.ratio);
        setFloat (ids::bandId (b, ids::AttackSuffix),    bs.attack);
        setFloat (ids::bandId (b, ids::ReleaseSuffix),   bs.release);
        setFloat (ids::bandId (b, ids::KneeSuffix),      bs.knee);
        setFloat (ids::bandId (b, ids::MakeupSuffix),    bs.makeup);
        setFloat (ids::bandId (b, ids::MixSuffix),       bs.mix);
        setFloat (ids::bandId (b, ids::StereoLinkSuffix), bs.stereoLink);
        setFloat (ids::bandId (b, ids::ScHpfSuffix),     bs.scHpf);
        setFloat (ids::bandId (b, ids::LookaheadSuffix), bs.lookahead);
        apvts.getParameter (ids::bandId (b, ids::CompTypeSuffix))->setValueNotifyingHost ((float) bs.compType / 4.0f);
        apvts.getParameter (ids::bandId (b, ids::DetectionSuffix))->setValueNotifyingHost ((float) bs.detection);
        apvts.getParameter (ids::bandId (b, ids::OversamplingSuffix))->setValueNotifyingHost ((float) bs.oversampling / 2.0f);
        apvts.getParameter (ids::bandId (b, ids::ExternalScSuffix))->setValueNotifyingHost (bs.extSc ? 1.0f : 0.0f);
    }
}

// ─────────────────────────────────────────────────────────────────────────
void PedagogicalCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Corregir la copia con el valor CRUDO REAL de cada parámetro: la caché
    // interna del APVTS puede estar desincronizada (host con setValue()
    // crudo) o contener valores con snap. El estado guardado debe reflejar
    // exactamente lo que el parámetro vale ahora mismo.
    for (auto* param : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr) continue;

        auto child = state.getChildWithProperty ("id", ranged->getParameterID());
        if (! child.isValid()) continue;

        const auto& range = ranged->getNormalisableRange();
        const float currentNorm = ranged->getValue();
        const float currentDenorm = range.convertFrom0to1 (currentNorm);   // sin snap
        const float treeNorm = treeChildToRawNormalized (*ranged, child);

        if (std::abs (treeNorm - currentNorm) > 0.00005f)
            child.setProperty ("value", (double) currentDenorm, nullptr);
    }

    state.setProperty ("currentProgram", currentProgram, nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PedagogicalCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
    {
        auto savedTree = juce::ValueTree::fromXml (*xml);

        // ¡OJO! replaceState() comparte el objeto ValueTree (handle con
        // referencia): su caché interna MUTARÍA nuestro árbol vía flush.
        // Copia profunda para que los valores del estado queden intactos.
        const auto savedTreeCopy = savedTree.createCopy();

        apvts.replaceState (savedTreeCopy);

        // Forzar el valor EXACTO del estado guardado en cada parámetro,
        // leído del árbol guardado ORIGINAL: replaceState() omite los
        // parámetros desincronizados y su caché interna aplica snap.
        for (auto* param : getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
            if (ranged == nullptr) continue;

            const auto child = savedTree.getChildWithProperty ("id", ranged->getParameterID());
            if (! child.isValid()) continue;

            const float treeNorm = treeChildToRawNormalized (*ranged, child);

            if (std::abs (ranged->getValue() - treeNorm) > 0.00005f)
                ranged->setValueNotifyingHost (treeNorm);
        }

        currentProgram = savedTree.getProperty ("currentProgram", 0);

        // Publicar TODO vía flags — el audio thread aplica (thread-safe,
        // incluso si REAPER carga el estado con el motor corriendo)
        updateBandCountFromParams();
        needsParameterUpdate.store (true);
        needsCrossoverUpdate.store (true);
    }
}

juce::AudioProcessorEditor* PedagogicalCompressorAudioProcessor::createEditor()
{
    return new PedagogicalCompressorAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PedagogicalCompressorAudioProcessor();
}

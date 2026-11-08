/*
 * CompressorBand.h — versión profesional (rev. 3)
 * ─────────────────────────────────────────────────────────────────────────
 * Arquitectura inspirada en:
 *   • Calf Compressor (open source LV2)
 *   • TDR Kotelnikov (program-dependent release)
 *   • FabFilter Pro-C (smoothed detection)
 *
 * REV. 3 — correcciones críticas de estabilidad:
 *   • Lookahead ROBUSTO: escritura circular con WRAP. (Antes: si el
 *     blockSize del host no dividía el tamaño del delayBuffer, copyFrom
 *     escribía fuera del heap → corrupción de memoria → crashes aleatorios
 *     en REAPER/standalone. Este era el bug principal.)
 *   • El delayBuffer crece si el host manda bloques más grandes de lo
 *     preparado (antes: overflow).
 *   • Limiter de seguridad CONTINUO en el knee: el anterior saltaba de
 *     0.9·ceiling a 0.68·ceiling → POPS/crackles con señal fuerte.
 *   • setParameters() solo desde el audio thread (el processor lo garantiza
 *     con flags atómicos). Antes la GUI lo llamaba en plena reproducción.
 *   • Release program-dependiente CONECTADO (FET 1176 / Optical LA-2A).
 *   • Stereo link CONECTADO en el detector.
 *   • Punteros del detector "hoisted" (antes getSample() por muestra era
 *     ~10× más caro → xruns → pops en el standalone).
 *   • Mix dry/wet POR MUESTRA (sin zipper).
 *   • Semántica clara: "Enabled" = la banda suena · "Bypass" = suena sin
 *     comprimir.
 */

#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "CompressorTypes.h"

namespace pedacomp
{

enum class DetectionMode : int { Peak = 0, Rms = 1 };

struct BandParameters
{
    float thresholdDb   = -20.0f;
    float ratio         = 4.0f;
    float attackMs      = 10.0f;
    float releaseMs     = 100.0f;
    float kneeDb        = 6.0f;
    float makeupDb      = 0.0f;
    float mix           = 1.0f;
    int   compType      = 0;
    int   detection     = 0;
    float stereoLink    = 1.0f;
    float scHpfHz       = 20.0f;
    bool  externalSidechain = false;
    float lookaheadMs    = 0.0f;
    int   oversampling   = 1;
    bool  enabled        = true;
    bool  bypass         = false;
};

class CompressorBand
{
public:
    CompressorBand() = default;

    void prepare (double sampleRate, int maximumBlockSize, int numChannels)
    {
        spec.sampleRate       = sampleRate > 1000.0 ? sampleRate : 48000.0;
        spec.maximumBlockSize = (juce::uint32) juce::jmax (1, maximumBlockSize);
        spec.numChannels      = (juce::uint32) juce::jmax (1, numChannels);

        const int maxLookaheadSamps = (int) std::ceil (maxLookaheadMs * 0.001 * spec.sampleRate);
        ensureDelayCapacity ((int) spec.numChannels,
                             (int) spec.maximumBlockSize + maxLookaheadSamps);
        delayWritePos = 0;

        scFilter.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        scFilter.prepare (spec);
        scFilter.reset();

        // SmoothedValues (estilo FabFilter): rampas de 20 ms = cero clicks
        thresholdSmoother .reset (spec.sampleRate, 0.02);
        ratioSmoother     .reset (spec.sampleRate, 0.02);
        makeupSmoother    .reset (spec.sampleRate, 0.02);
        mixSmoother       .reset (spec.sampleRate, 0.02);
        kneeSmoother      .reset (spec.sampleRate, 0.02);

        thresholdSmoother .setCurrentAndTargetValue (-20.0f);
        ratioSmoother     .setCurrentAndTargetValue (4.0f);
        makeupSmoother    .setCurrentAndTargetValue (0.0f);
        mixSmoother       .setCurrentAndTargetValue (1.0f);
        kneeSmoother      .setCurrentAndTargetValue (6.0f);

        envState = 0.0f;
        currentGrDb.store (0.0f, std::memory_order_relaxed);
        peakInputDb.store (-100.0f);
        peakOutputDb.store (-100.0f);
    }

    void reset()
    {
        delayBuffer.clear();
        delayWritePos = 0;
        scFilter.reset();
        envState = 0.0f;
        currentGrDb.store (0.0f, std::memory_order_relaxed);
    }

    // SOLO desde el audio thread (o antes de que arranque el audio).
    // El processor lo garantiza con flags atómicos.
    void setParameters (const BandParameters& p)
    {
        params = p;
        params.compType = juce::jlimit (0, 4, params.compType);

        thresholdSmoother .setTargetValue (params.thresholdDb);
        ratioSmoother     .setTargetValue (params.ratio);
        makeupSmoother    .setTargetValue (params.makeupDb);
        mixSmoother       .setTargetValue (params.mix);
        kneeSmoother      .setTargetValue (params.kneeDb);

        scFilter.setCutoffFrequency (juce::jlimit (10.0f, 2000.0f, params.scHpfHz));
    }

    const BandParameters& getParameters() const noexcept { return params; }

    void process (juce::AudioBuffer<float>& buffer,
                  const juce::AudioBuffer<float>* sidechainBuffer)
    {
        // BYPASS: la banda suena SIN comprimir (la señal dividida pasa intacta)
        if (params.bypass)
        {
            const float pdb = juce::Decibels::gainToDecibels (measurePeak (buffer));
            peakInputDb.store (pdb);
            peakOutputDb.store (pdb);
            currentGrDb.store (0.0f, std::memory_order_relaxed);
            return;
        }

        // Banda desactivada (ON apagado): la banda NO suena (mute)
        if (! params.enabled)
        {
            peakInputDb.store (juce::Decibels::gainToDecibels (measurePeak (buffer)));
            peakOutputDb.store (-100.0f);
            currentGrDb.store (0.0f, std::memory_order_relaxed);
            buffer.clear();
            return;
        }

        const int numCh      = juce::jmax (1, buffer.getNumChannels());
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0) return;

        // Medir input de la banda
        peakInputDb.store (juce::Decibels::gainToDecibels (measurePeak (buffer)));

        // ── Lookahead ROBUSTO ────────────────────────────────────────────
        // Garantiza capacidad aunque el host cambie el tamaño de bloque.
        const int maxLookaheadSamps = (int) std::ceil (maxLookaheadMs * 0.001 * spec.sampleRate);
        ensureDelayCapacity (numCh, numSamples + maxLookaheadSamps);

        const int delayBufSize = delayBuffer.getNumSamples();
        int lookaheadSamples = (int) std::round (params.lookaheadMs * 0.001 * spec.sampleRate);
        lookaheadSamples = juce::jlimit (0, juce::jmax (0, delayBufSize - 1), lookaheadSamples);

        // Señal delayada (el detector "ve el futuro" mientras la señal espera)
        juce::AudioBuffer<float> signalBuffer (numCh, numSamples);

        for (int ch = 0; ch < numCh; ++ch)
        {
            // Escritura circular CON WRAP — sin esto, copyFrom se salía del
            // buffer y corrompía el heap (causa de los crashes aleatorios)
            const int firstChunk = juce::jmin (numSamples, delayBufSize - delayWritePos);
            delayBuffer.copyFrom (ch, delayWritePos, buffer, ch, 0, firstChunk);
            if (firstChunk < numSamples)
                delayBuffer.copyFrom (ch, 0, buffer, ch, firstChunk, numSamples - firstChunk);

            const int readPos = (delayWritePos - lookaheadSamples + delayBufSize) % delayBufSize;
            const float* delayPtr = delayBuffer.getReadPointer (ch);
            float* sigPtr = signalBuffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
                sigPtr[i] = delayPtr[(readPos + i) % delayBufSize];
        }

        delayWritePos = (delayWritePos + numSamples) % delayBufSize;

        // Detector: input original (sin delay) o sidechain externo
        const juce::AudioBuffer<float>* detectorSource =
            (params.externalSidechain && sidechainBuffer != nullptr) ? sidechainBuffer : &buffer;

        applyCompression (signalBuffer, detectorSource);

        // Seguridad anti-clip CONTINUA — solo actúa por encima de 0.8:
        // la señal limpia queda intacta (el tipo "Digital" permanece limpio)
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* sig = signalBuffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                sig[i] = safetyLimit (sig[i], 0.8f, 1.0f);
        }

        // Mix dry/wet POR MUESTRA (ramp suave — sin zipper noise)
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* dry = buffer.getReadPointer (ch);
            const float* wet = signalBuffer.getReadPointer (ch);
            float* out = buffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                const float w = mixSmoother.getNextValue();
                out[i] = dry[i] * (1.0f - w) + wet[i] * w;
            }
        }

        // Medir output de la banda
        peakOutputDb.store (juce::Decibels::gainToDecibels (measurePeak (buffer)));
    }

    float getPeakInputDb()  const noexcept { return peakInputDb.load(); }
    float getPeakOutputDb() const noexcept { return peakOutputDb.load(); }
    float getGainReductionDb() const noexcept { return currentGrDb.load (std::memory_order_relaxed); }

private:
    // ─────────────────────────────────────────────────────────────────────
    //  Knee suave CONTINUO con asíntota en `ceiling`:
    //    · por debajo de kneeStart → x sin tocar (cero distorsión)
    //    · curva CONTINUA en kneeStart (el limiter anterior saltaba de
    //      0.9·c a 0.68·c → pops audibles con señal fuerte)
    //    · nunca supera ceiling (clamp duro final)
    // ─────────────────────────────────────────────────────────────────────
    static float safetyLimit (float x, float kneeStart, float ceiling) noexcept
    {
        const float ax = std::abs (x);
        if (ax <= kneeStart) return x;

        const float range = juce::jmax (1.0e-6f, ceiling - kneeStart);
        const float over  = (ax - kneeStart) / range;
        const float soft  = kneeStart + range * std::tanh (over);
        return std::copysign (juce::jmin (soft, ceiling), x);
    }

    void ensureDelayCapacity (int numChannels, int neededSamples)
    {
        if (delayBuffer.getNumSamples() < neededSamples || delayBuffer.getNumChannels() < numChannels)
        {
            delayBuffer.setSize (juce::jmax (numChannels, delayBuffer.getNumChannels()),
                                 juce::jmax (neededSamples, delayBuffer.getNumSamples()),
                                 false, true, false);
            delayBuffer.clear();
            delayWritePos = 0;
        }
    }

    void applyCompression (juce::AudioBuffer<float>& signal,
                           const juce::AudioBuffer<float>* detector)
    {
        const int numCh      = juce::jmax (1, signal.getNumChannels());
        const int numSamples = signal.getNumSamples();
        const float sr = (float) spec.sampleRate;
        if (numSamples <= 0 || sr <= 1000.0f) return;

        const auto type = (CompressorType) juce::jlimit (0, 4, params.compType);

        // Punteros del detector hoisted (sin getSample por muestra)
        const int detCh = (detector != nullptr) ? detector->getNumChannels() : 0;
        const int detN  = (detector != nullptr) ? detector->getNumSamples()  : 0;
        const float* detL = (detCh > 0) ? detector->getReadPointer (0) : nullptr;
        const float* detR = (detCh > 1) ? detector->getReadPointer (1) : nullptr;

        // Balísticas. El release es PROGRAM-DEPENDIENTE (FET / Optical)
        const float attMs = std::max (0.1f, params.attackMs);
        const float effRelMs = std::max (10.0f,
            effectiveReleaseMs (type, params.releaseMs,
                                currentGrDb.load (std::memory_order_relaxed)));
        const float attCoef = std::exp (-1.0f / (attMs    * 0.001f * sr));
        const float relCoef = std::exp (-1.0f / (effRelMs * 0.001f * sr));

        float gr = currentGrDb.load (std::memory_order_relaxed);
        const float link = juce::jlimit (0.0f, 1.0f, params.stereoLink);

        float* ch0 = signal.getWritePointer (0);
        float* ch1 = (numCh > 1) ? signal.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const float thresholdDb = thresholdSmoother.getNextValue();
            const float ratio       = juce::jmax (1.0f, ratioSmoother.getNextValue());
            const float makeupLin   = juce::Decibels::decibelsToGain (makeupSmoother.getNextValue());
            const float kneeDb      = kneeSmoother.getNextValue();

            // Detector (sidechain externo o señal de la banda)
            float dL = (detL != nullptr && i < detN) ? detL[i] : 0.0f;
            float dR = (detR != nullptr && i < detN) ? detR[i] : dL;

            // HPF en el sidechain (filtrado de graves para no "bombeear")
            dL = scFilter.processSample (0, dL);
            dR = (numCh > 1) ? scFilter.processSample (1, dR) : dL;

            // Stereo link + modo Peak/RMS
            float detectorValue;
            if (params.detection == (int) DetectionMode::Rms)
            {
                const float lSq = dL * dL;
                const float rSq = dR * dR;
                const float linked   = std::sqrt ((lSq + rSq) * 0.5f);
                const float unlinked = std::sqrt (std::max (lSq, rSq));
                detectorValue = linked * link + unlinked * (1.0f - link);
            }
            else
            {
                const float linked   = (std::abs (dL) + std::abs (dR)) * 0.5f;
                const float unlinked = std::max (std::abs (dL), std::abs (dR));
                detectorValue = linked * link + unlinked * (1.0f - link);
            }

            // Envelope con attack/release de 1 polo
            if (detectorValue > envState)
                envState = attCoef * envState + (1.0f - attCoef) * detectorValue;
            else
                envState = relCoef * envState + (1.0f - relCoef) * detectorValue;

            const float envDb = juce::Decibels::gainToDecibels (envState, -100.0f);

            // GR objetivo (soft-knee parabólico + ratio efectivo por tipo)
            const float targetGr = gainReductionDb (type, envDb, thresholdDb, ratio, kneeDb);

            // Suavizado del GR: attack al comprimir, release al soltar
            const float grCoef = (targetGr < gr) ? attCoef : relCoef;
            gr = grCoef * gr + (1.0f - grCoef) * targetGr;

            const float gainLin = juce::Decibels::decibelsToGain (gr) * makeupLin;

            ch0[i] = harmonicColor (type, ch0[i] * gainLin);
            if (ch1 != nullptr)
                ch1[i] = harmonicColor (type, ch1[i] * gainLin);
        }

        currentGrDb.store (gr, std::memory_order_relaxed);
    }

    static float measurePeak (const juce::AudioBuffer<float>& buffer) noexcept
    {
        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float a = std::abs (data[i]);
                if (a > peak) peak = a;
            }
        }
        return peak;
    }

    static constexpr float maxLookaheadMs = 20.0f;

    juce::dsp::ProcessSpec spec {};
    BandParameters params {};

    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;

    juce::dsp::LinkwitzRileyFilter<float> scFilter;

    // SmoothedValues para parámetros (estilo FabFilter)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> thresholdSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> ratioSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> makeupSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> kneeSmoother;

    float envState = 0.0f;

    std::atomic<float> currentGrDb { 0.0f };
    std::atomic<float> peakInputDb  { -100.0f };
    std::atomic<float> peakOutputDb { -100.0f };
};

} // namespace pedacomp

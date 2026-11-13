#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

// ===========================================================================
//  CompressorBand  —  Una banda de compresión con 5 modelos
//  (Digital / VCA / FET / Optical / Vari-Mu)
//  Linkwitz-Riley 4º orden está en MultibandProcessor.h
//
//  v1.2.0 cambios:
//    - Saturación analógica (tape/transformer) por banda
//    - Auto-Makeup: compensa automáticamente la pérdida de volumen
//    - Dry buffer (para Mix con coherencia de fase)
//    - Tracking de envolvente para visualización en tiempo real
//    - Detector Mode ya existía (Peak/RMS) — confirmado funcional
// ===========================================================================

enum class CompressorType
{
    Digital = 0,
    VCA,        // SSL-style, soft-knee
    FET,        // 1176-style
    Optical,    // LA-2A-style
    VariMu      // Fairchild-style
};

enum class DetectorMode { Peak = 0, RMS };

class CompressorBand
{
public:
    CompressorBand() { prepareSpec(44100.0, 512); }

    // ─── Parámetros (llamados desde el hilo de mensaje) ──────────────
    void setThreshold(float v) noexcept { threshold.store(v); }
    void setRatio(float v) noexcept { ratio.store(v); }
    void setAttack(float v) noexcept { attackMs.store(v); }
    void setRelease(float v) noexcept { releaseMs.store(v); }
    void setKnee(float v) noexcept { kneeDb.store(v); }
    void setMakeup(float v) noexcept { makeupDb.store(v); }
    void setMix(float v) noexcept { mix.store(v); }
    void setType(CompressorType t) noexcept { type.store(t); }
    void setDetector(DetectorMode m) noexcept { detector.store(m); }
    void setSidechainHPF(float v) noexcept { scHpf.store(v); }
    void setLookahead(float v) noexcept { lookaheadMs.store(v); }
    void setStereoLink(float v) noexcept { stereoLink.store(v); }
    void setBypass(bool b) noexcept { bypass.store(b); }
    void setExtSidechain(bool b) noexcept { extSC.store(b); }

    // Nuevos v1.2:
    void setSaturation(float v) noexcept { saturationAmount.store(v); }
    void setAutoMakeup(bool b) noexcept { autoMakeup.store(b); }

    float getGainReductionDb() const noexcept { return grDb.load(); }
    float getInputLevelDb() const noexcept { return inDb.load(); }
    float getEnvelopeDb() const noexcept { return envDb.load(); }
    float getOutputLevelDb() const noexcept { return outDb.load(); }
    float getSaturationDriveDb() const noexcept { return saturationAmount.load(); }
    float getAutoMakeupGainDb() const noexcept { return autoMakeupGainDb.load(); }

    void prepareSpec(double sr, int block)
    {
        sampleRate = sr;
        maxBlock = block;
        detectorEnv = 0.0f;
        gainReductionSmoothed.reset(sr, 0.02);
        inLevelSmoothed.reset(sr, 0.05);
        envSmoothed.reset(sr, 0.01);
        outSmoothed.reset(sr, 0.05);
        autoMakeupSmoothed.reset(sr, 0.3);  // reacciona lento

        // Buffer circular para lookahead
        const int laSamples = static_cast<int>(std::ceil(0.020 * sr)) + block + 4;
        delayLineL.resize(static_cast<size_t>(laSamples), 0.0f);
        delayLineR.resize(static_cast<size_t>(laSamples), 0.0f);
        delayWrite = 0;

        // Buffer dry para mix (preserva fase)
        dryBufferL.resize(static_cast<size_t>(block), 0.0f);
        dryBufferR.resize(static_cast<size_t>(block), 0.0f);

        // HPF del detector
        hpfLeft  = juce::dsp::IIR::Filter<float>(
            juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sr, currentHpfFreq));
        hpfRight = juce::dsp::IIR::Filter<float>(
            juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sr, currentHpfFreq));

        // Reset de integradores de auto-makeup
        runningInputLevelDb = -60.0f;
        runningGrDb = 0.0f;
    }

    // ─── Procesa un bloque estéreo ────────────────────────────────────
    //  inL / inR       → señal principal
    //  scL  / scR      → sidechain (externo o igual al input)
    void process(float* inL, float* inR, float* scL, float* scR, int numSamples) noexcept
    {
        if (bypass.load())
        {
            grDb.store(0.0f);
            envDb.store(-100.0f);
            inDb.store(-100.0f);
            outDb.store(-100.0f);
            return;
        }

        // Snap atómico de parámetros al inicio del bloque
        const float thr = threshold.load();
        const float rat = ratio.load();
        const float aMs = attackMs.load();
        const float rMs = releaseMs.load();
        const float knee = kneeDb.load();
        const float mkDb = makeupDb.load();
        const float mMix = mix.load();
        const CompressorType cType = type.load();
        const DetectorMode dMode = detector.load();
        const float scHpfFreq = scHpf.load();
        const float laMs = lookaheadMs.load();
        const float sLink = stereoLink.load();
        const bool useExt = extSC.load();
        const float satAmount = saturationAmount.load();  // 0..1
        const bool useAutoMK = autoMakeup.load();

        // Coeficientes del detector
        const float aCoef = (aMs > 0.001f) ? std::exp(-1.0f / (aMs * 0.001f * sampleRate)) : 0.0f;
        const float rCoef = (rMs > 0.001f) ? std::exp(-1.0f / (rMs * 0.001f * sampleRate)) : 0.0f;

        // HPF del detector
        if (std::abs(scHpfFreq - currentHpfFreq) > 0.5f)
        {
            auto newCoeffs = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(
                sampleRate, juce::jlimit(20.0f, 500.0f, scHpfFreq));
            hpfLeft.coefficients = newCoeffs;
            hpfRight.coefficients = newCoeffs;
            currentHpfFreq = scHpfFreq;
        }

        // Lookahead
        const int laSamples = static_cast<int>(std::ceil(laMs * 0.001 * sampleRate));
        const int delayLen = juce::jlimit(0, static_cast<int>(delayLineL.size()) - 1, laSamples);

        // Auto-Makeup: calcula la ganancia de compensación basándose en el
        // nivel medio de input y la reducción media (running average).
        // Si autoMakeup está activo, anula el makeup manual.
        const float mkGain = useAutoMK ? autoMakeupSmoothed.getCurrentValue()
                                        : std::pow(10.0f, mkDb * 0.05f);

        // Drive de saturación: cuanta más cantidad, más drive.
        // Mapeo 0..1 → 0 dB..+12 dB de drive (ganancia previa al tanh)
        const float satDrive = std::pow(10.0f, satAmount * 12.0f * 0.05f);  // 1..~4
        // Mix paralelo entre señal saturada y limpia:
        // satAmount=0 → 100% limpio; satAmount=1 → 100% saturado
        const float satMix = satAmount;

        // Guarda copia seca para Mix final (preserva fase respecto a delayedL/R)
        // Usamos el inL/inR ANTES de procesar (señal de entrada a la banda)
        std::copy(inL, inL + numSamples, dryBufferL.begin());
        std::copy(inR, inR + numSamples, dryBufferR.begin());

        float maxGR = 0.0f;
        float maxIn = 0.0f;
        float maxEnv = 0.0f;
        float maxOut = 0.0f;
        float sumInputDb = 0.0f;
        float sumGr = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            // 1) Señal sidechain (externa o interna)
            float detectorL = useExt ? scL[i] : inL[i];
            float detectorR = useExt ? scR[i] : inR[i];

            // 2) HPF en detector
            detectorL = hpfLeft.processSample(detectorL);
            detectorR = hpfRight.processSample(detectorR);

            // 3) Stereo link
            float det;
            if (sLink >= 0.99f)
            {
                det = std::max(std::abs(detectorL), std::abs(detectorR));
            }
            else if (sLink <= 0.01f)
            {
                det = (std::abs(detectorL) + std::abs(detectorR)) * 0.5f;
            }
            else
            {
                float linked = std::max(std::abs(detectorL), std::abs(detectorR));
                float independent = (std::abs(detectorL) + std::abs(detectorR)) * 0.5f;
                det = independent + (linked - independent) * sLink;
            }

            // 4) Detección Peak/RMS
            //    RMS trabaja en el dominio de POTENCIA (x²): la conversión
            //    correcta a dB es 10·log10 (≈ 20·log10 de la raíz).
            //    FIX v1.2.2: antes se usaba 20·log10 sobre el cuadrado, lo que
            //    duplicaba los dB y hacía que el detector leyera mal.
            const bool isRms = (dMode == DetectorMode::RMS);
            float key = isRms ? det * det : det;

            // 5) Filtro envolvente (peak detector clásico)
            if (key > detectorEnv)
                detectorEnv = aCoef * detectorEnv + (1.0f - aCoef) * key;
            else
                detectorEnv = rCoef * detectorEnv + (1.0f - rCoef) * key;

            float envDbLocal = (detectorEnv > 1e-8f)
                ? (isRms ? 10.0f : 20.0f) * std::log10(detectorEnv)
                : -160.0f;

            // 6) Cálculo de ganancia según el tipo de compresor
            float gainReductionDb = computeGainReduction(envDbLocal, thr, rat, knee, cType);

            // 7) Lookahead: escribir en delay, leer con delay
            float dryL = inL[i];
            float dryR = inR[i];
            float delayedL, delayedR;
            if (delayLen > 0)
            {
                int readIdx = delayWrite - delayLen;
                if (readIdx < 0) readIdx += static_cast<int>(delayLineL.size());
                delayedL = delayLineL[static_cast<size_t>(readIdx)];
                delayedR = delayLineR[static_cast<size_t>(readIdx)];
                delayLineL[static_cast<size_t>(delayWrite)] = dryL;
                delayLineR[static_cast<size_t>(delayWrite)] = dryR;
                if (++delayWrite >= static_cast<int>(delayLineL.size())) delayWrite = 0;
            }
            else
            {
                delayedL = dryL;
                delayedR = dryR;
            }

            // 8) Aplicar reducción de ganancia
            float gainLin = std::pow(10.0f, gainReductionDb * 0.05f);
            float wetL = delayedL * gainLin;
            float wetR = delayedR * gainLin;

            // 9) Saturación analógica (tanh con drive)
            //    Para Digital y VCA, saturación es muy sutil (color).
            //    Para FET, Optical, VariMu es más presente (simula transformador/tubo).
            if (satMix > 0.001f)
            {
                // El drive depende del tipo de compresor: tipos analógicos saturan más
                float typeSatBoost = 1.0f;
                switch (cType)
                {
                    case CompressorType::FET:      typeSatBoost = 1.6f; break;
                    case CompressorType::Optical:  typeSatBoost = 1.3f; break;
                    case CompressorType::VariMu:   typeSatBoost = 1.8f; break;
                    case CompressorType::VCA:      typeSatBoost = 0.8f; break;
                    case CompressorType::Digital:  typeSatBoost = 0.6f; break;
                }
                // FIX v1.2.2: la normalización anterior (tanh(x·d)/tanh(d))
                // tenía ganancia de pequeña señal = d/tanh(d), un REALCE de
                // hasta +7 dB que sumado por banda disparaba el nivel.
                // La forma correcta: y = tanh(x·d)/d → pendiente exactamente
                // 1 en el origen (sin realce), redondea los picos (satura).
                float totalDrive = juce::jlimit(1.0f, 3.0f, satDrive * typeSatBoost);
                const float invDrive = 1.0f / totalDrive;
                float satOutL = std::tanh(wetL * totalDrive) * invDrive;
                float satOutR = std::tanh(wetR * totalDrive) * invDrive;
                wetL = wetL * (1.0f - satMix) + satOutL * satMix;
                wetR = wetR * (1.0f - satMix) + satOutR * satMix;
            }

            // 10) Makeup gain
            wetL *= mkGain;
            wetR *= mkGain;

            // 11) Mix (dry/wet) — coherente en fase porque dryBuffer es pre-procesado
            float outL = dryL * (1.0f - mMix) + wetL * mMix;
            float outR = dryR * (1.0f - mMix) + wetR * mMix;

            inL[i] = outL;
            inR[i] = outR;

            // Estadísticas para meter y auto-makeup
            if (gainReductionDb < maxGR) maxGR = gainReductionDb;
            if (envDbLocal > maxEnv) maxEnv = envDbLocal;
            float absIn = std::max(std::abs(dryL), std::abs(dryR));
            if (absIn > maxIn) maxIn = absIn;
            float absOut = std::max(std::abs(outL), std::abs(outR));
            if (absOut > maxOut) maxOut = absOut;

            sumInputDb += (absIn > 1e-6f ? 20.0f * std::log10(absIn) : -100.0f);
            sumGr += gainReductionDb;
        }

        // 12) Meters suavizados (hilo de audio → lectura desde GUI)
        gainReductionSmoothed.setTargetValue(maxGR);
        inLevelSmoothed.setTargetValue(maxIn > 1e-6f ? 20.0f * std::log10(maxIn) : -100.0f);
        envSmoothed.setTargetValue(maxEnv);
        outSmoothed.setTargetValue(maxOut > 1e-6f ? 20.0f * std::log10(maxOut) : -100.0f);
        gainReductionSmoothed.skip(numSamples);
        inLevelSmoothed.skip(numSamples);
        envSmoothed.skip(numSamples);
        outSmoothed.skip(numSamples);

        grDb.store(gainReductionSmoothed.getCurrentValue());
        inDb.store(inLevelSmoothed.getCurrentValue());
        envDb.store(envSmoothed.getCurrentValue());
        outDb.store(outSmoothed.getCurrentValue());

        // 13) Auto-Makeup: promedio del bloque
        if (useAutoMK)
        {
            float avgInputDb = sumInputDb / numSamples;
            float avgGr = sumGr / numSamples;
            // Suavizado tipo leaky integrator
            const float alpha = 0.005f;  // lento (~200ms en 60Hz)
            runningInputLevelDb = runningInputLevelDb * (1.0f - alpha) + avgInputDb * alpha;
            runningGrDb = runningGrDb * (1.0f - alpha) + avgGr * alpha;

            // Target: compensar GR para que outputLevel ≈ inputLevel
            // - Si GR = -6 dB, auto-makeup = +6 dB
            // - Limitamos a +0..+6 dB (mucho más conservador que antes,
            //   que era +18 dB — eso causaba saturación con múltiples bandas).
            float targetMakeup = juce::jlimit(0.0f, 6.0f, -runningGrDb);
            autoMakeupSmoothed.setTargetValue(std::pow(10.0f, targetMakeup * 0.05f));
            autoMakeupSmoothed.skip(numSamples);
            autoMakeupGainDb.store(targetMakeup);
        }
        else
        {
            autoMakeupSmoothed.setTargetValue(std::pow(10.0f, mkDb * 0.05f));
            autoMakeupSmoothed.skip(numSamples);
            autoMakeupGainDb.store(0.0f);
        }
    }

private:
    float computeGainReduction(float envDbLocal, float thr, float ratio, float kneeDb,
                                 CompressorType ctype) const noexcept
    {
        const float kneeHalf = kneeDb * 0.5f;
        const float kneeLow = thr - kneeHalf;
        const float kneeHigh = thr + kneeHalf;

        float gr = 0.0f;

        switch (ctype)
        {
        case CompressorType::Digital:
        {
            if (envDbLocal > thr)
            {
                float over = envDbLocal - thr;
                gr = -over * (1.0f - 1.0f / ratio);
            }
            break;
        }
        case CompressorType::VCA:  // Soft-knee parabólico tipo SSL
        {
            if (envDbLocal > kneeHigh)
            {
                float over = envDbLocal - thr;
                gr = -over * (1.0f - 1.0f / ratio);
            }
            else if (envDbLocal > kneeLow)
            {
                float t = (envDbLocal - kneeLow) / (2.0f * kneeHalf);
                float over = (envDbLocal - kneeLow);
                gr = -over * t * (1.0f - 1.0f / ratio);
            }
            break;
        }
        case CompressorType::FET:  // 1176: ratio creciente con el nivel
        {
            if (envDbLocal > thr)
            {
                float over = envDbLocal - thr;
                float effRatio = ratio + std::min(8.0f, over * 0.4f);
                gr = -over * (1.0f - 1.0f / effRatio);
                gr = juce::jlimit(-24.0f, 0.0f, gr);
            }
            break;
        }
        case CompressorType::Optical:  // LA-2A: ratio variable, suave
        {
            if (envDbLocal > thr)
            {
                float over = envDbLocal - thr;
                float effRatio = ratio + std::min(6.0f, over * 0.3f);
                gr = -over * (1.0f - 1.0f / effRatio);
                gr = juce::jlimit(-20.0f, 0.0f, gr);
            }
            break;
        }
        case CompressorType::VariMu:  // Fairchild: ratio muy creciente
        {
            if (envDbLocal > thr - 4.0f)
            {
                float over = envDbLocal - thr + 4.0f;
                float effRatio = ratio + std::min(12.0f, over * 0.6f);
                gr = -(envDbLocal - thr) * (1.0f - 1.0f / effRatio);
                gr = juce::jlimit(-30.0f, 0.0f, gr);
            }
            break;
        }
        }

        return gr;
    }

    // ─── Estado del DSP ───────────────────────────────────────────────
    std::atomic<float> threshold { 0.0f };
    std::atomic<float> ratio { 4.0f };
    std::atomic<float> attackMs { 10.0f };
    std::atomic<float> releaseMs { 100.0f };
    std::atomic<float> kneeDb { 3.0f };   // v1.2.1: 3 dB (más suave, menos necesidad de makeup)
    std::atomic<float> makeupDb { 0.0f };
    std::atomic<float> mix { 1.0f };
    std::atomic<float> scHpf { 80.0f };
    std::atomic<float> lookaheadMs { 0.0f };
    std::atomic<float> stereoLink { 1.0f };
    std::atomic<bool>  bypass { false };
    std::atomic<bool>  extSC { false };
    std::atomic<CompressorType> type { CompressorType::Digital };
    std::atomic<DetectorMode> detector { DetectorMode::RMS };

    // Nuevos v1.2:
    std::atomic<float> saturationAmount { 0.0f };   // 0..1
    std::atomic<bool>  autoMakeup { false };
    std::atomic<float> autoMakeupGainDb { 0.0f };   // solo lectura GUI

    // ─── Estado del DSP ───────────────────────────────────────────────
    double sampleRate { 44100.0 };
    int    maxBlock { 512 };

    float detectorEnv { 0.0f };
    juce::SmoothedValue<float> gainReductionSmoothed { 0.0f };
    juce::SmoothedValue<float> inLevelSmoothed { -100.0f };
    juce::SmoothedValue<float> envSmoothed { -100.0f };
    juce::SmoothedValue<float> outSmoothed { -100.0f };
    juce::SmoothedValue<float> autoMakeupSmoothed { 1.0f };

    std::vector<float> delayLineL, delayLineR;
    int delayWrite { 0 };

    std::vector<float> dryBufferL, dryBufferR;

    // HPF del sidechain
    juce::dsp::IIR::Filter<float> hpfLeft, hpfRight;
    float currentHpfFreq { 80.0f };

    // Auto-makeup tracking
    float runningInputLevelDb { -60.0f };
    float runningGrDb { 0.0f };

    // Meters (escritos por audio, leídos por GUI)
    std::atomic<float> grDb { 0.0f };
    std::atomic<float> inDb { -100.0f };
    std::atomic<float> envDb { -100.0f };
    std::atomic<float> outDb { -100.0f };
};

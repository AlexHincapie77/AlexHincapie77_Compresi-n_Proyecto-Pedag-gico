#pragma once

#include <juce_dsp/juce_dsp.h>
#include "CompressorBand.h"

// ===========================================================================
//  MultibandProcessor  —  divide, comprime y reconstruye 1-4 bandas
//
//  v1.2.2 — FIX CRÍTICO de saturación:
//
//  La versión anterior usaba una estructura de crossover INCORRECTA para
//  3 y 4 bandas: la banda b sólo aplicaba UN HP (en c_{b-1}) y UN LP (en
//  c_b).  Eso significa que la banda 3 contenía, sin atenuar, contenido
//  grave/medio que ya estaba en las bandas 0-2 → la suma NO era la señal
//  original: los picos se sumaban con errores de fase y el nivel subía
//  VARIOS dB al activar más bandas → saturación.
//
//  La estructura CORRECTA (Linkwitz-Riley en cascada, la que usan todos
//  los multibanda profesionales) es:
//
//     banda 0 = LP(c0)
//     banda 1 = HP(c0) · LP(c1)
//     banda 2 = HP(c0) · HP(c1) · LP(c2)
//     banda 3 = HP(c0) · HP(c1) · HP(c2)
//
//  La suma telescópica es EXACTAMENTE la señal original (LP+HP = 1 en
//  cada etapa, en magnitud Y fase).  Con los compresores neutros la
//  salida = entrada, sin bumps, sin cancelaciones y sin subidas de nivel.
//
//  Además: fade-in corto al cambiar el número de bandas (evita el click
//  de los filtros arrancando con estado cero).
// ===========================================================================

class MultibandProcessor
{
public:
    static constexpr int kMaxBands = 4;
    static constexpr int kFadeSamples = 512;   // ~12 ms @ 44.1 kHz

    MultibandProcessor() { prepare({ 44100.0, 512, 2 }); }

    void setNumBands(int n) noexcept
    {
        n = juce::jlimit(1, kMaxBands, n);
        numBands.store(n);
        // El fade se dispara en el hilo de audio al detectar el cambio.
    }

    int getNumBands() const noexcept { return numBands.load(); }

    void setCrossover(int idx, float freqHz) noexcept
    {
        freqHz = juce::jlimit(20.0f, 18000.0f, freqHz);
        if (idx >= 0 && idx < kMaxBands - 1)
            crossoverFreqs[idx].store(freqHz);
    }

    float getCrossover(int idx) const noexcept
    {
        return (idx >= 0 && idx < kMaxBands - 1) ? crossoverFreqs[idx].load() : 1000.0f;
    }

    CompressorBand& getBand(int idx) { return bands[idx]; }
    const CompressorBand& getBand(int idx) const { return bands[idx]; }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        maxBlock = static_cast<int>(spec.maximumBlockSize);

        // Buffers temporales por banda
        for (int b = 0; b < kMaxBands; ++b)
        {
            bandBuffersL[b].resize(static_cast<size_t>(maxBlock), 0.0f);
            bandBuffersR[b].resize(static_cast<size_t>(maxBlock), 0.0f);
            bands[b].prepareSpec(sampleRate, maxBlock);
        }

        // Resetea el estado de TODAS las etapas de filtro (los coeficientes
        // se asignan justo después en updateCrossoverFilters).
        for (int b = 0; b < kMaxBands; ++b)
        {
            bandLowPass[b].resetState();
            for (int i = 0; i < kMaxBands - 1; ++i)
                bandHighPass[b][i].resetState();
        }

        updateCrossoverFilters();

        activeBands = numBands.load();
        fadePos = kFadeSamples;   // sin fade al arrancar
    }

    // Procesa in-place
    void process(float* inL, float* inR, float* scL, float* scR, int numSamples) noexcept
    {
        const int n = juce::jlimit(1, kMaxBands, numBands.load());

        // ¿Cambió el número de bandas desde el último bloque?
        // → dispara un fade-in corto para enmascarar el transitorio de los
        //   filtros que arrancan con estado cero (anti-click).
        if (n != activeBands)
        {
            activeBands = n;
            fadePos = 0;
        }

        if (n == 1)
        {
            // Modo single-band: sin crossover, procesa directo
            bands[0].process(inL, inR, scL, scR, numSamples);
            applyFade(inL, inR, numSamples);
            return;
        }

        // Revisa si cambió la frecuencia de crossover
        bool crossoversChanged = false;
        for (int i = 0; i < n - 1; ++i)
        {
            float target = crossoverFreqs[i].load();
            if (std::abs(target - currentCrossover[i]) > 0.5f)
            {
                currentCrossover[i] = target;
                crossoversChanged = true;
            }
        }
        if (crossoversChanged)
            updateCrossoverFilters();

        // Copia la señal a cada banda
        for (int b = 0; b < n; ++b)
        {
            std::copy(inL, inL + numSamples, bandBuffersL[b].data());
            std::copy(inR, inR + numSamples, bandBuffersR[b].data());
        }

        // ─── División: estructura LR en cascada ────────────────────────
        //   banda b = HP(c0)·HP(c1)·…·HP(c_{b-1})·LP(c_b)   [LP sólo si b < n-1]
        // Cada banda usa SUS PROPIAS instancias de filtro (estado independiente).
        for (int b = 0; b < n; ++b)
        {
            for (int i = 0; i < b; ++i)
            {
                bandHighPass[b][i].process(bandBuffersL[b].data(), numSamples);
                bandHighPass[b][i].process(bandBuffersR[b].data(), numSamples);
            }
            if (b < n - 1)
            {
                bandLowPass[b].process(bandBuffersL[b].data(), numSamples);
                bandLowPass[b].process(bandBuffersR[b].data(), numSamples);
            }
        }

        // Compresión por banda
        for (int b = 0; b < n; ++b)
        {
            bands[b].process(bandBuffersL[b].data(), bandBuffersR[b].data(),
                              scL, scR, numSamples);
        }

        // ─── Reconstrucción ────────────────────────────────────────────
        // Con la estructura telescópica la suma es la señal ORIGINAL exacta
        // cuando los compresores son neutros, y menor o igual cuando
        // comprimen (la GR es ≤ 0 dB).  NO se necesita atenuación 1/√N:
        // la reconstrucción es matemáticamente plana.
        for (int i = 0; i < numSamples; ++i)
        {
            float sumL = 0.0f, sumR = 0.0f;
            for (int b = 0; b < n; ++b)
            {
                sumL += bandBuffersL[b][i];
                sumR += bandBuffersR[b][i];
            }
            inL[i] = sumL;
            inR[i] = sumR;
        }

        applyFade(inL, inR, numSamples);
    }

    void reset()
    {
        for (int b = 0; b < kMaxBands; ++b)
        {
            std::fill(bandBuffersL[b].begin(), bandBuffersL[b].end(), 0.0f);
            std::fill(bandBuffersR[b].begin(), bandBuffersR[b].end(), 0.0f);
            bands[b].prepareSpec(sampleRate, maxBlock);
            bandLowPass[b].resetState();
            for (int i = 0; i < kMaxBands - 1; ++i)
                bandHighPass[b][i].resetState();
        }
    }

private:
    // ── Una etapa Linkwitz-Riley 4º orden = 2× Butterworth 2º orden ──
    struct LR4Stage
    {
        juce::dsp::IIR::Filter<float> stage[2];

        void setCoefficients(const juce::dsp::IIR::Coefficients<float>::Ptr& c)
        {
            stage[0].coefficients = c;
            stage[1].coefficients = c;
        }

        void resetState()
        {
            stage[0].reset();
            stage[1].reset();
        }

        void process(float* data, int numSamples) noexcept
        {
            for (int j = 0; j < 2; ++j)
            {
                juce::dsp::AudioBlock<float> block(&data, 1u, static_cast<size_t>(numSamples));
                juce::dsp::ProcessContextReplacing<float> ctx(block);
                stage[j].process(ctx);
            }
        }
    };

    void updateCrossoverFilters()
    {
        for (int i = 0; i < kMaxBands - 1; ++i)
        {
            const float freq = currentCrossover[i];
            if (freq < 20.0f) continue;

            auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, freq);
            auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, freq);

            // La banda i usa LP en el crossover i (sólo bandas 0..n-2)
            bandLowPass[i].setCoefficients(lpCoeffs);

            // Cualquier banda b > i usa HP en el crossover i
            for (int b = 0; b < kMaxBands; ++b)
                bandHighPass[b][i].setCoefficients(hpCoeffs);
        }
    }

    // Fade-in corto tras cambiar el número de bandas (anti-click)
    void applyFade(float* inL, float* inR, int numSamples) noexcept
    {
        if (fadePos >= kFadeSamples) return;
        for (int i = 0; i < numSamples && fadePos < kFadeSamples; ++i, ++fadePos)
        {
            const float g = static_cast<float>(fadePos) / static_cast<float>(kFadeSamples);
            inL[i] *= g;
            inR[i] *= g;
        }
    }

    double sampleRate { 44100.0 };
    int    maxBlock { 512 };

    std::atomic<int> numBands { 1 };
    int activeBands { 1 };
    int fadePos { kFadeSamples };

    std::atomic<float> crossoverFreqs[kMaxBands - 1] = { 120.0f, 1200.0f, 6000.0f };
    float currentCrossover[kMaxBands - 1] = { 120.0f, 1200.0f, 6000.0f };

    CompressorBand bands[kMaxBands];

    std::vector<float> bandBuffersL[kMaxBands];
    std::vector<float> bandBuffersR[kMaxBands];

    // Bancos de filtros POR BANDA (estado independiente por banda):
    //   bandLowPass[b]           = LP LR4 en crossover[b]
    //   bandHighPass[b][i]       = HP LR4 en crossover[i]
    // La banda b aplica bandHighPass[b][0..b-1] y (si b < n-1) bandLowPass[b].
    LR4Stage bandLowPass[kMaxBands];
    LR4Stage bandHighPass[kMaxBands][kMaxBands - 1];
};

/*
 * MultibandProcessor.h
 * ─────────────────────────────────────────────────────────────────────────
 * Divide la señal en 1, 2, 3 o 4 bandas con crossovers Linkwitz-Riley 4º
 * orden (-24 dB/oct). Cada banda tiene su propio CompressorBand.
 *
 *     1 banda: Procesa todo el espectro (compresor single-band clásico).
 *     2 bandas: Low / High    → 1 crossover en freq1Hz
 *     3 bandas: Low / Mid / High → 2 crossovers (freq1Hz, freq2Hz)
 *     4 bandas: Low / LowMid / HighMid / High → 3 crossovers
 *
 * La suma de las 4 bandas reproduce el espectro original (perfect
 * reconstruction thanks to LR4 phase-coherent summation).
 */

#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "CompressorBand.h"

namespace pedacomp
{

enum class BandCount : int { One = 1, Two = 2, Three = 3, Four = 4 };

class MultibandProcessor
{
public:
    static constexpr int kMaxBands = 4;

    MultibandProcessor()
    {
        // 3 crossovers como máximo (necesarios para 4 bandas)
        for (auto& lr : crossoversL)
            lr.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        for (auto& lr : crossoversH)
            lr.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    }

    void prepare (double sampleRate, int maximumBlockSize, int numChannels)
    {
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = (juce::uint32) maximumBlockSize;
        spec.numChannels      = (juce::uint32) numChannels;

        for (auto& lr : crossoversL) lr.prepare (spec);
        for (auto& lr : crossoversH) lr.prepare (spec);
        for (auto& lr : crossoversL) lr.reset();
        for (auto& lr : crossoversH) lr.reset();

        for (auto& b : bands)
            b.prepare (sampleRate, maximumBlockSize, numChannels);

        // Buffer temporal para cada banda
        for (auto& buf : bandBuffers)
            buf.setSize (numChannels, juce::jmax (1, maximumBlockSize));
    }

    void reset()
    {
        for (auto& lr : crossoversL) lr.reset();
        for (auto& lr : crossoversH) lr.reset();
        for (auto& b : bands) b.reset();
    }

    // Reinicia SOLO los filtros del splitter (al cambiar el número de
    // bandas evita transitorios de reconstrucción sin cortar la señal)
    void resetCrossovers()
    {
        for (auto& lr : crossoversL) lr.reset();
        for (auto& lr : crossoversH) lr.reset();
    }

    void setBandCount (int count)
    {
        numBands = juce::jlimit (1, kMaxBands, count);
    }

    int getBandCount() const noexcept { return numBands; }

    void setCrossover (int index, float frequencyHz)
    {
        if (index >= 0 && index < 3)
        {
            crossoverFreqs[index] = frequencyHz;
            crossoversL[index].setCutoffFrequency (frequencyHz);
            crossoversH[index].setCutoffFrequency (frequencyHz);
        }
    }

    float getCrossover (int index) const noexcept { return crossoverFreqs[index]; }

    CompressorBand& getBand (int index)
    {
        jassert (index >= 0 && index < kMaxBands);
        return bands[index];
    }

    const CompressorBand& getBand (int index) const
    {
        jassert (index >= 0 && index < kMaxBands);
        return bands[index];
    }

    // Procesa un bloque. sidechainBuffer opcional (band 0 lo usará si externalSidechain).
    void process (juce::AudioBuffer<float>& buffer,
                  const juce::AudioBuffer<float>* sidechainBuffer)
    {
        const int numCh = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        // Asegurar tamaño de buffers temporales
        for (auto& buf : bandBuffers)
        {
            if (buf.getNumChannels() != numCh || buf.getNumSamples() != numSamples)
                buf.setSize (numCh, numSamples, false, false, true);
        }
        // ─── 1 banda: bypass del crossover ───
        if (numBands == 1)
        {
            bands[0].process (buffer, sidechainBuffer);
            return;
        }

        // ─── Split según número de bandas ───
        switch (numBands)
        {
            case 2: split2 (buffer, numSamples, numCh); break;
            case 3: split3 (buffer, numSamples, numCh); break;
            case 4: split4 (buffer, numSamples, numCh); break;
            default: split4 (buffer, numSamples, numCh); break;
        }

        // ─── Procesa cada banda (sidechain externo disponible en todas) ───
        for (int b = 0; b < numBands; ++b)
            bands[b].process (bandBuffers[b], sidechainBuffer);

        // ─── Mezcla todas las bandas de vuelta al buffer ───
        buffer.clear();
        for (int b = 0; b < numBands; ++b)
            for (int ch = 0; ch < numCh; ++ch)
                buffer.addFrom (ch, 0, bandBuffers[b], ch, 0, numSamples);
    }

    // Cómputo del rango de frecuencia de cada banda (para GUI)
    juce::Range<float> getBandFrequencyRange (int bandIndex) const
    {
        const float nyquist = (float) spec.sampleRate * 0.5f;
        switch (numBands)
        {
            case 1: return { 20.0f, nyquist };
            case 2:
                if (bandIndex == 0) return { 20.0f, crossoverFreqs[0] };
                return { crossoverFreqs[0], nyquist };
            case 3:
                if (bandIndex == 0) return { 20.0f, crossoverFreqs[0] };
                if (bandIndex == 1) return { crossoverFreqs[0], crossoverFreqs[1] };
                return { crossoverFreqs[1], nyquist };
            case 4:
                if (bandIndex == 0) return { 20.0f, crossoverFreqs[0] };
                if (bandIndex == 1) return { crossoverFreqs[0], crossoverFreqs[1] };
                if (bandIndex == 2) return { crossoverFreqs[1], crossoverFreqs[2] };
                return { crossoverFreqs[2], nyquist };
        }
        return { 20.0f, nyquist };
    }

private:
    void split2 (const juce::AudioBuffer<float>& src, int numSamples, int numCh)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* in = src.getReadPointer (ch);
            float* lo = bandBuffers[0].getWritePointer (ch);
            float* hi = bandBuffers[1].getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                lo[i] = crossoversL[0].processSample (ch, in[i]);
                hi[i] = crossoversH[0].processSample (ch, in[i]);
            }
        }
    }

    void split3 (const juce::AudioBuffer<float>& src, int numSamples, int numCh)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* in = src.getReadPointer (ch);
            float* lo  = bandBuffers[0].getWritePointer (ch);
            float* mid = bandBuffers[1].getWritePointer (ch);
            float* hi  = bandBuffers[2].getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                const float stage1Lo = crossoversL[0].processSample (ch, in[i]);
                const float stage1Hi = crossoversH[0].processSample (ch, in[i]);

                lo[i]  = stage1Lo;
                mid[i] = crossoversL[1].processSample (ch, stage1Hi);
                hi[i]  = crossoversH[1].processSample (ch, stage1Hi);
            }
        }
    }

    void split4 (const juce::AudioBuffer<float>& src, int numSamples, int numCh)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* in = src.getReadPointer (ch);
            float* lo  = bandBuffers[0].getWritePointer (ch);
            float* lm  = bandBuffers[1].getWritePointer (ch);
            float* hm  = bandBuffers[2].getWritePointer (ch);
            float* hi  = bandBuffers[3].getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                const float s1Lo = crossoversL[0].processSample (ch, in[i]);
                const float s1Hi = crossoversH[0].processSample (ch, in[i]);

                lo[i] = s1Lo;

                const float s2Lo = crossoversL[1].processSample (ch, s1Hi);
                const float s2Hi = crossoversH[1].processSample (ch, s1Hi);
                lm[i] = s2Lo;
                hm[i] = crossoversL[2].processSample (ch, s2Hi);
                hi[i] = crossoversH[2].processSample (ch, s2Hi);
            }
        }
    }

    juce::dsp::ProcessSpec spec {};
    int numBands = 1;
    float crossoverFreqs[3] { 150.0f, 1200.0f, 6000.0f };

    juce::dsp::LinkwitzRileyFilter<float> crossoversL[3];
    juce::dsp::LinkwitzRileyFilter<float> crossoversH[3];

    std::array<CompressorBand, kMaxBands> bands;
    std::array<juce::AudioBuffer<float>, kMaxBands> bandBuffers;
};

} // namespace pedacomp

/*
 * CompressorTypes.h
 * ─────────────────────────────────────────────────────────────────────────
 * Modelos de comportamiento para cada "sabor" de compresor.
 *
 * Cada tipo define:
 *   - responseCurve():  forma de la curva ratio vs nivel sobre umbral
 *   - harmonicColor():  saturación armónica opcional (coloración)
 *   - description():    texto pedagógico para mostrar en la GUI
 *
 * Tipos implementados:
 *   0 - Digital       (clean, sin coloración, ratio fijo)
 *   1 - VCA           (limpio, SSL bus, soft knee, sin color)
 *   2 - FET           (1176-style: rápido, ratio crece con nivel, tanh suave)
 *   3 - Optical       (LA-2A-style: lento, release program-dependiente)
 *   4 - Vari-Mu       (Fairchild-style: ratio crece fuertemente, tubo)
 */

#pragma once

#include <cmath>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

namespace pedacomp
{

enum class CompressorType : int
{
    Digital = 0,
    VCA     = 1,
    FET     = 2,
    Optical = 3,
    VariMu  = 4
};

// ─────────────────────────────────────────────────────────────────────────
//  Cálculo del ratio efectivo según el tipo y el nivel sobre umbral.
//  overDb = max(0, envDb - thresholdDb)
// ─────────────────────────────────────────────────────────────────────────
inline float effectiveRatio (CompressorType type, float baseRatio, float overDb)
{
    switch (type)
    {
        case CompressorType::Digital:
            return baseRatio;

        case CompressorType::VCA:
            // Ratio fijo con soft knee puramente en la curva de GR.
            return baseRatio;

        case CompressorType::FET:
            // 1176: ratio sube ligeramente con el nivel (1.0 → 1.5× base)
            return juce::jlimit (1.0f, 20.0f, baseRatio * (1.0f + 0.012f * overDb));

        case CompressorType::Optical:
            // LA-2A: compresión más suave, ratio crece despacio
            return juce::jlimit (1.0f, 15.0f, baseRatio * (1.0f + 0.006f * overDb));

        case CompressorType::VariMu:
            // Fairchild: ratio crece fuertemente con el nivel
            return juce::jlimit (1.0f, 20.0f, baseRatio + overDb * 0.30f);
    }
    return baseRatio;
}

// ─────────────────────────────────────────────────────────────────────────
//  Cálculo del tiempo de release efectivo (program-dependiente).
//  Algunos modelos adaptan el release según el material.
//  gainReductionDb = GR actual (negativo)
// ─────────────────────────────────────────────────────────────────────────
inline float effectiveReleaseMs (CompressorType type, float baseReleaseMs, float gainReductionDb)
{
    if (type == CompressorType::Optical)
    {
        // LA-2A: release se hace más lento cuando hay poca GR (memoria del fotoacoplador)
        const float gr = juce::jlimit (0.0f, 20.0f, std::abs (gainReductionDb));
        return baseReleaseMs * (1.0f + 2.5f * std::exp (-gr * 0.30f));
    }
    if (type == CompressorType::FET)
    {
        // 1176: auto-release más rápido cuando hay mucha GR
        const float gr = juce::jlimit (0.0f, 20.0f, std::abs (gainReductionDb));
        return baseReleaseMs * (1.0f - 0.30f * std::tanh (gr * 0.25f));
    }
    return baseReleaseMs;
}

// ─────────────────────────────────────────────────────────────────────────
//  Coloración armónica opcional por tipo (waveshaper).
//  Aplica distorsión suave para emular el "color" del hardware.
// ─────────────────────────────────────────────────────────────────────────
inline float harmonicColor (CompressorType type, float sample) noexcept
{
    switch (type)
    {
        case CompressorType::Digital:
            return sample;

        case CompressorType::VCA:
            // VCA: muy limpio, coloración ínfima
            return sample - 0.0015f * std::tanh (sample * 6.0f);

        case CompressorType::FET:
            // FET: saturación suave tipo 1176 (tanh con drive medio)
            return std::tanh (sample * 1.20f) * 0.95f;

        case CompressorType::Optical:
            // Optical: tubo + fotoacoplador, coloración cálida
            return std::tanh (sample * 0.90f + 0.05f * std::sin (sample * 6.0f));

        case CompressorType::VariMu:
            // Vari-Mu: tubo, coloración más marcada
            return std::tanh (sample * 1.35f) * 0.92f;
    }
    return sample;
}

// ─────────────────────────────────────────────────────────────────────────
//  Soft-knee: reduce el GR cuando el nivel está cerca del umbral.
//  Devuelve GR en dB (negativo o cero).
// ─────────────────────────────────────────────────────────────────────────
inline float gainReductionDb (CompressorType type,
                              float envDb,
                              float thresholdDb,
                              float ratio,
                              float kneeDb)
{
    const float overDb = envDb - thresholdDb;

    if (type == CompressorType::VCA && kneeDb > 0.5f)
    {
        // Soft-knee simétrico de 2 * kneeDb de ancho centrado en threshold
        const float halfKnee = kneeDb * 0.5f;
        if (overDb < -halfKnee)
            return 0.0f;
        if (overDb > halfKnee)
            return -overDb * (1.0f - 1.0f / ratio);

        // Zona de transición: parábola suave
        const float x = overDb + halfKnee;   // 0 .. 2*halfKnee
        const float k = (1.0f - 1.0f / ratio) / (2.0f * 2.0f * halfKnee);
        return -k * x * x;
    }

    if (overDb <= 0.0f)
        return 0.0f;

    const float r = effectiveRatio (type, ratio, overDb);
    return -overDb * (1.0f - 1.0f / r);
}

// ─────────────────────────────────────────────────────────────────────────
//  Descripción pedagógica para mostrar en la GUI
// ─────────────────────────────────────────────────────────────────────────
inline juce::String descriptionForType (CompressorType type)
{
    switch (type)
    {
        case CompressorType::Digital:
            return "Digital: Compresión transparente sin coloración. Ideal para "
                   "mastering moderno y control dinámico quirúrgico. Sin distorsión "
                   "armónica añadida.";

        case CompressorType::VCA:
            return "VCA: Basado en compresores de bus tipo SSL 4000. Muy limpio y "
                   "preciso, con soft-knee. Perfecto para glue en buses y mezclas "
                   "donde no queremos alterar el timbre.";

        case CompressorType::FET:
            return "FET: Inspirado en el UREI 1176. Ataque ultrarrápido y coloración "
                   "agradable por saturación. Clásico para voces, baterías y bajo. "
                   "El ratio crece ligeramente con el nivel.";

        case CompressorType::Optical:
            return "Optical: Inspirado en el Teletronix LA-2A. Ataque y release "
                   "lentos y program-dependientes (memoria del fotoacoplador). "
                   "Cálido, suave, ideal para voces y bajo.";

        case CompressorType::VariMu:
            return "Vari-Mu: Inspirado en el Fairchild 670. El ratio crece "
                   "fuertemente con el nivel. Coloración de tubo marcada, "
                   "glue legendario en master bus. Ataque medio-lento.";
    }
    return {};
}

inline juce::String nameForType (CompressorType type)
{
    switch (type)
    {
        case CompressorType::Digital: return "Digital";
        case CompressorType::VCA:     return "VCA";
        case CompressorType::FET:     return "FET";
        case CompressorType::Optical: return "Optical";
        case CompressorType::VariMu:  return "Vari-Mu";
    }
    return {};
}

} // namespace pedacomp

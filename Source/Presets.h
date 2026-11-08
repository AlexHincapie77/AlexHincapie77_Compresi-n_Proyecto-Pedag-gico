/*
 * Presets.h
 * ─────────────────────────────────────────────────────────────────────────
 * Presets factory para el Pedagogical Compressor.
 *
 * Cada preset devuelve un mapa (paramID → valor) que la GUI/processor puede
 * aplicar masivamente.
 *
 * Presets incluidos:
 *   0  Init           — todo a defaults
 *   1  Vocal Glue     — 1 banda FET, control suave
 *   2  Master Bus      — 1 banda VCA, glue de mezcla
 *   3  Drum Punch     — 2 bandas, FET para graves
 *   4  Bass Tight     — 1 banda FET, control de picos
 *   5  Bus Glue        — 1 banda Vari-Mu, compresión suave
 *   6  De-Esser MB     — 3 bandas, banda aguda comprimida
 */

#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <unordered_map>

namespace pedacomp
{

struct Preset
{
    juce::String name;
    int bandCount;       // 1..4
    float crossovers[3]; // solo relevantes si bandCount > 1
    bool globalBypass = false;
    float globalMix = 1.0f;
    float inputGain = 0.0f;
    float outputGain = 0.0f;
    bool limiterOn = true;
    float ceiling = -0.3f;

    // Por banda: 16 parámetros. Índice 0..3 = banda, valores por defecto.
    struct BandState
    {
        bool enabled = true;
        bool bypass = false;
        float threshold = -20.0f;
        float ratio = 4.0f;
        float attack = 10.0f;
        float release = 100.0f;
        float knee = 6.0f;
        float makeup = 0.0f;
        float mix = 1.0f;
        int compType = 0;     // Digital
        int detection = 0;    // Peak
        float stereoLink = 1.0f;
        float scHpf = 20.0f;
        bool extSc = false;
        float lookahead = 0.0f;
        int oversampling = 0; // index: 0=1x, 1=2x, 2=4x
    };

    BandState bands[4];
};

inline std::vector<Preset> getFactoryPresets()
{
    std::vector<Preset> presets;

    // 0: Init
    {
        Preset p;
        p.name = "Init";
        p.bandCount = 1;
        presets.push_back (p);
    }

    // 1: Vocal Glue — FET suave para voces
    {
        Preset p;
        p.name = "Vocal Glue";
        p.bandCount = 1;
        auto& b = p.bands[0];
        b.threshold = -18.0f; b.ratio = 3.0f;
        b.attack = 8.0f;      b.release = 120.0f;
        b.knee = 6.0f;        b.makeup = 4.0f;
        b.compType = 2;       // FET
        b.detection = 1;      // RMS
        presets.push_back (p);
    }

    // 2: Master Bus — VCA glue para mezcla
    {
        Preset p;
        p.name = "Master Bus";
        p.bandCount = 1;
        auto& b = p.bands[0];
        b.threshold = -14.0f; b.ratio = 2.0f;
        b.attack = 30.0f;     b.release = 250.0f;
        b.knee = 12.0f;       b.makeup = 2.5f;
        b.compType = 1;        // VCA
        b.detection = 1;       // RMS
        b.oversampling = 1;    // 2x
        presets.push_back (p);
    }

    // 3: Drum Punch — 2 bandas, FET para graves
    {
        Preset p;
        p.name = "Drum Punch";
        p.bandCount = 2;
        p.crossovers[0] = 200.0f;
        // Banda baja: FET agresivo
        auto& lo = p.bands[0];
        lo.threshold = -22.0f; lo.ratio = 6.0f;
        lo.attack = 5.0f;      lo.release = 80.0f;
        lo.knee = 2.0f;        lo.makeup = 3.0f;
        lo.compType = 2;       // FET
        // Banda alta: VCA suave
        auto& hi = p.bands[1];
        hi.threshold = -18.0f; hi.ratio = 3.0f;
        hi.attack = 15.0f;     hi.release = 150.0f;
        hi.knee = 8.0f;        hi.makeup = 2.0f;
        hi.compType = 1;        // VCA
        presets.push_back (p);
    }

    // 4: Bass Tight — FET para bajo
    {
        Preset p;
        p.name = "Bass Tight";
        p.bandCount = 1;
        auto& b = p.bands[0];
        b.threshold = -20.0f; b.ratio = 5.0f;
        b.attack = 12.0f;      b.release = 90.0f;
        b.knee = 3.0f;         b.makeup = 3.5f;
        b.compType = 2;         // FET
        b.scHpf = 80.0f;
        presets.push_back (p);
    }

    // 5: Bus Glue — Vari-Mu suave
    {
        Preset p;
        p.name = "Bus Glue";
        p.bandCount = 1;
        auto& b = p.bands[0];
        b.threshold = -12.0f; b.ratio = 2.5f;
        b.attack = 25.0f;     b.release = 300.0f;
        b.knee = 8.0f;        b.makeup = 2.0f;
        b.compType = 4;        // Vari-Mu
        b.detection = 1;       // RMS
        b.oversampling = 1;    // 2x
        presets.push_back (p);
    }

    // 6: De-Esser MB — 3 bandas, banda aguda comprimida
    {
        Preset p;
        p.name = "De-Esser MB";
        p.bandCount = 3;
        p.crossovers[0] = 200.0f;
        p.crossovers[1] = 4500.0f;
        // Banda baja: sin tocar
        auto& lo = p.bands[0];
        lo.threshold = -10.0f; lo.ratio = 1.5f;
        lo.attack = 20.0f;     lo.release = 200.0f;
        // Banda media: control suave
        auto& mid = p.bands[1];
        mid.threshold = -18.0f; mid.ratio = 2.5f;
        mid.attack = 15.0f;     mid.release = 100.0f;
        // Banda alta: de-esser
        auto& hi = p.bands[2];
        hi.threshold = -24.0f; hi.ratio = 5.0f;
        hi.attack = 3.0f;       hi.release = 50.0f;
        hi.scHpf = 5000.0f;
        hi.compType = 0;         // Digital
        presets.push_back (p);
    }

    return presets;
}

} // namespace pedacomp

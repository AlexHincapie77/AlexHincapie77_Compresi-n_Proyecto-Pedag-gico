#pragma once

#include <juce_data_structures/juce_data_structures.h>

// ===========================================================================
//  Presets — 7 presets pedagógicos
// ===========================================================================

namespace Presets
{

inline juce::ValueTree getPresets()
{
    juce::ValueTree presets("Presets");

    // 0. Default (bypass-effectivo: 1 banda, settings neutros)
    {
        juce::ValueTree p("Preset");
        p.setProperty("name", "Default", nullptr);
        p.setProperty("numBands", 1, nullptr);
        p.setProperty("type0", 0, nullptr);
        p.setProperty("thr0",   0.0f, nullptr);
        p.setProperty("rat0",   4.0f, nullptr);
        p.setProperty("att0",   10.0f, nullptr);
        p.setProperty("rel0",  100.0f, nullptr);
        p.setProperty("mk0",    0.0f, nullptr);
        presets.addChild(p, -1, nullptr);
    }

    // 1. Master Bus Glue (VCA, 2 bandas)
    {
        juce::ValueTree p("Preset");
        p.setProperty("name", "Master Bus Glue", nullptr);
        p.setProperty("numBands", 2, nullptr);
        p.setProperty("cross0", 150.0f, nullptr);
        p.setProperty("type0", 1, nullptr);
        p.setProperty("thr0", -10.0f, nullptr);
        p.setProperty("rat0", 2.5f, nullptr);
        p.setProperty("att0", 30.0f, nullptr);
        p.setProperty("rel0", 250.0f, nullptr);
        p.setProperty("mk0", 3.0f, nullptr);
        p.setProperty("type1", 1, nullptr);
        p.setProperty("thr1", -8.0f, nullptr);
        p.setProperty("rat1", 3.0f, nullptr);
        p.setProperty("att1", 20.0f, nullptr);
        p.setProperty("rel1", 200.0f, nullptr);
        p.setProperty("mk1", 2.5f, nullptr);
        presets.addChild(p, -1, nullptr);
    }

    // 2. Vocal De-Esser (Optical, 3 bandas)
    {
        juce::ValueTree p("Preset");
        p.setProperty("name", "Vocal De-Esser", nullptr);
        p.setProperty("numBands", 3, nullptr);
        p.setProperty("cross0", 200.0f, nullptr);
        p.setProperty("cross1", 5000.0f, nullptr);
        p.setProperty("type0", 3, nullptr);
        p.setProperty("thr0", -20.0f, nullptr);
        p.setProperty("rat0", 2.0f, nullptr);
        p.setProperty("att0", 5.0f, nullptr);
        p.setProperty("rel0", 80.0f, nullptr);
        p.setProperty("mk0", 0.0f, nullptr);
        p.setProperty("type1", 3, nullptr);
        p.setProperty("thr1", -18.0f, nullptr);
        p.setProperty("rat1", 2.5f, nullptr);
        p.setProperty("att1", 8.0f, nullptr);
        p.setProperty("rel1", 100.0f, nullptr);
        p.setProperty("mk1", 1.0f, nullptr);
        p.setProperty("type2", 2, nullptr);  // FET en la banda de sibilancia
        p.setProperty("thr2", -12.0f, nullptr);
        p.setProperty("rat2", 6.0f, nullptr);
        p.setProperty("att2", 1.0f, nullptr);
        p.setProperty("rel2", 50.0f, nullptr);
        p.setProperty("mk2", 0.0f, nullptr);
        presets.addChild(p, -1, nullptr);
    }

    // 3. Drum Bus Punch (FET 1176, 1 banda)
    {
        juce::ValueTree p("Preset");
        p.setProperty("name", "Drum Bus Punch (1176)", nullptr);
        p.setProperty("numBands", 1, nullptr);
        p.setProperty("type0", 2, nullptr);
        p.setProperty("thr0", -10.0f, nullptr);
        p.setProperty("rat0", 4.0f, nullptr);
        p.setProperty("att0", 1.0f, nullptr);
        p.setProperty("rel0", 50.0f, nullptr);
        p.setProperty("mk0", 2.0f, nullptr);
        presets.addChild(p, -1, nullptr);
    }

    // 4. Bass Tight (Vari-Mu, 2 bandas)
    {
        juce::ValueTree p("Preset");
        p.setProperty("name", "Bass Tight (Fairchild)", nullptr);
        p.setProperty("numBands", 2, nullptr);
        p.setProperty("cross0", 120.0f, nullptr);
        p.setProperty("type0", 4, nullptr);
        p.setProperty("thr0", -15.0f, nullptr);
        p.setProperty("rat0", 4.0f, nullptr);
        p.setProperty("att0", 15.0f, nullptr);
        p.setProperty("rel0", 150.0f, nullptr);
        p.setProperty("mk0", 2.0f, nullptr);
        p.setProperty("type1", 4, nullptr);
        p.setProperty("thr1", -10.0f, nullptr);
        p.setProperty("rat1", 3.0f, nullptr);
        p.setProperty("att1", 10.0f, nullptr);
        p.setProperty("rel1", 80.0f, nullptr);
        p.setProperty("mk1", 1.5f, nullptr);
        presets.addChild(p, -1, nullptr);
    }

    // 5. Stereo Width Keeper (4 bandas, stereo link 0)
    {
        juce::ValueTree p("Preset");
        p.setProperty("name", "4-Band Stereo Master", nullptr);
        p.setProperty("numBands", 4, nullptr);
        p.setProperty("cross0", 120.0f, nullptr);
        p.setProperty("cross1", 1500.0f, nullptr);
        p.setProperty("cross2", 8000.0f, nullptr);
        p.setProperty("type0", 1, nullptr);
        p.setProperty("thr0", -15.0f, nullptr);
        p.setProperty("rat0", 2.0f, nullptr);
        p.setProperty("att0", 20.0f, nullptr);
        p.setProperty("rel0", 200.0f, nullptr);
        p.setProperty("mk0", 1.0f, nullptr);
        p.setProperty("type1", 1, nullptr);
        p.setProperty("thr1", -12.0f, nullptr);
        p.setProperty("rat1", 2.5f, nullptr);
        p.setProperty("att1", 15.0f, nullptr);
        p.setProperty("rel1", 180.0f, nullptr);
        p.setProperty("mk1", 1.5f, nullptr);
        p.setProperty("type2", 1, nullptr);
        p.setProperty("thr2", -10.0f, nullptr);
        p.setProperty("rat2", 3.0f, nullptr);
        p.setProperty("att2", 10.0f, nullptr);
        p.setProperty("rel2", 150.0f, nullptr);
        p.setProperty("mk2", 2.0f, nullptr);
        p.setProperty("type3", 1, nullptr);
        p.setProperty("thr3", -8.0f, nullptr);
        p.setProperty("rat3", 2.0f, nullptr);
        p.setProperty("att3", 5.0f, nullptr);
        p.setProperty("rel3", 100.0f, nullptr);
        p.setProperty("mk3", 1.5f, nullptr);
        presets.addChild(p, -1, nullptr);
    }

    // 6. Broadcast Limiter (1 banda, ratio alto)
    {
        juce::ValueTree p("Preset");
        p.setProperty("name", "Broadcast Limiter", nullptr);
        p.setProperty("numBands", 1, nullptr);
        p.setProperty("type0", 2, nullptr);
        p.setProperty("thr0", -6.0f, nullptr);
        p.setProperty("rat0", 10.0f, nullptr);
        p.setProperty("att0", 2.0f, nullptr);
        p.setProperty("rel0", 100.0f, nullptr);
        p.setProperty("mk0", 4.0f, nullptr);
        presets.addChild(p, -1, nullptr);
    }

    return presets;
}

inline int getNumPresets() { return 7; }

inline juce::String getPresetName(int idx)
{
    auto presets = getPresets();
    if (idx >= 0 && idx < presets.getNumChildren())
        return presets.getChild(idx).getProperty("name");
    return "Default";
}

} // namespace Presets

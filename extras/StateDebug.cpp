/*
 * StateDebug.cpp — diagnóstico del roundtrip de estado (headless, sin GUI)
 * Prueba 1: uniforme (todos a 0.8 → guardar → 0.2 → restaurar)
 * Prueba 2: aleatoria por parámetro, 20 rondas (como pluginval)
 */

#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <cmath>
#include <typeinfo>
#include <vector>
#include <memory>

static int runUniformTest (PedagogicalCompressorAudioProcessor& proc)
{
    auto& params = proc.getParameters();

    for (auto* p : params)
        p->setValue (0.8f);

    std::vector<float> expected;
    expected.reserve ((size_t) params.size());
    for (auto* p : params)
        expected.push_back (p->getValue());

    juce::MemoryBlock saved;
    proc.getStateInformation (saved);
    std::printf ("estado guardado: %d bytes\n", (int) saved.getSize());

    // ── dump del XML para los primeros bools ──
    {
        auto xml = PedagogicalCompressorAudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        if (xml)
        {
            for (auto* el : xml->getChildIterator())
            {
                const juce::String id = el->getStringAttribute ("id");
                if (id == "GlobalBypass" || id == "OutputLimiterOn" || id == "Band0Enabled")
                    std::printf ("  XML guardado: id=%s value=%s\n", id.toRawUTF8(),
                                 el->getStringAttribute ("value").toRawUTF8());
            }
        }
        else
            std::printf ("  XML guardado: PARSE FAIL\n");
    }

    for (auto* p : params)
        p->setValue (0.2f);

    proc.setStateInformation (saved.getData(), (int) saved.getSize());

    int bad = 0;
    for (int i = 0; i < (int) params.size(); ++i)
    {
        const float got = params[i]->getValue();
        if (std::abs (got - expected[(size_t) i]) > 0.0001f)
        {
            if (bad < 5)
                std::printf ("  NO restauró [%d] %-28s esperado %.4f, obtenido %.4f\n",
                             i, params[i]->getName (64).toRawUTF8(),
                             expected[(size_t) i], got);
            ++bad;
        }
    }

    if (bad == 0)
        std::printf ("✔ ROUNDTRIP UNIFORME EXACTO (%d parámetros)\n", (int) params.size());
    else
        std::printf ("✘ uniforme: %d/%d no restaurados\n", bad, (int) params.size());
    return bad;
}

static int runRandomTest (PedagogicalCompressorAudioProcessor& proc, int numRounds)
{
    auto& params = proc.getParameters();
    juce::Random rnd (12345);
    int totalBad = 0;

    for (int round = 0; round < numRounds; ++round)
    {
        for (auto* p : params)
            p->setValue (rnd.nextFloat());

        std::vector<float> expected;
        expected.reserve ((size_t) params.size());
        for (auto* p : params)
            expected.push_back (p->getValue());

        juce::MemoryBlock saved;
        proc.getStateInformation (saved);

        for (auto* p : params)
            p->setValue (rnd.nextFloat());

        proc.setStateInformation (saved.getData(), (int) saved.getSize());

        for (int i = 0; i < (int) params.size(); ++i)
        {
            const float got = params[i]->getValue();
            if (std::abs (got - expected[(size_t) i]) > 0.0001f)
            {
                if (totalBad < 6)
                {
                    auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (params[i]);
                    const auto ch = proc.getAPVTS().state.getChildWithProperty ("id",
                        ranged != nullptr ? ranged->getParameterID() : juce::String());
                    std::printf ("  ronda %d: [%d] %-24s esperado %.4f, obtenido %.4f, treeValue=%s\n",
                                 round, i, params[i]->getName (64).toRawUTF8(),
                                 expected[(size_t) i], got,
                                 ch.isValid() ? ch.getProperty ("value", "??").toString().toRawUTF8() : "sin-child");
                }
                ++totalBad;
            }
        }
    }

    if (totalBad == 0)
        std::printf ("✔ RANDOM ROUNDTRIP EXACTO (%d rondas x %d parámetros)\n",
                     numRounds, (int) params.size());
    else
        std::printf ("✘ random: %d desajustes en %d rondas\n", totalBad, numRounds);
    return totalBad;
}

int main()
{
    PedagogicalCompressorAudioProcessor proc;
    auto& params = proc.getParameters();
    std::printf ("num params: %d\n", (int) params.size());

    std::printf ("\n--- prueba 1: uniforme ---\n");
    const int bad1 = runUniformTest (proc);

    std::printf ("\n--- prueba 2: aleatoria (20 rondas) ---\n");
    const int bad2 = runRandomTest (proc, 20);

    return (bad1 + bad2) == 0 ? 0 : 1;
}

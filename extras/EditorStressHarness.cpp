/*
#include <vector>
 * EditorStressHarness.cpp — Arnés de estrés del editor (solo desarrollo)
 * ─────────────────────────────────────────────────────────────────────────
 * Replica el ciclo de vida que hace un DAW real (REAPER/Bitwig) con el
 * editor de un plugin, SIN la capa XEmbed de pluginval:
 *
 *   por cada iteración:
 *     1. createEditor() + addToDesktop
 *     2. resize a 6 tamaños distintos (incluye menores que el mínimo y
 *        enormes — los hosts hacen esto)
 *     3. cambio de preset y de número de bandas CON el editor abierto
 *        (el timer de 15 Hz está corriendo)
 *     4. 10 bloques de audio a 0.5 de amplitud mientras el editor vive
 *     5. destrucción del editor
 *     6. repetir 25 veces
 *   + audio tras cerrar todos los editores
 *
 * Se compila con -DPEDACOMP_BUILD_TEST_HARNESS=ON (OFF por defecto).
 */

#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>
#include <cmath>
#include <memory>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto* mm = juce::MessageManager::getInstance();

    std::printf ("[harness] creando processor…\n");
    std::unique_ptr<PedagogicalCompressorAudioProcessor> proc;
    proc.reset (new PedagogicalCompressorAudioProcessor());
    proc->prepareToPlay (48000.0, 512);
    std::printf ("[harness] processor preparado (48 kHz / 512)\n");

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double phase = 0.0;

    auto fillSine = [&] (float amp)
    {
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float s = amp * (float) std::sin (phase);
            phase += 2.0 * juce::MathConstants<double>::pi * 220.0 / 48000.0;
            if (phase > 2.0 * juce::MathConstants<double>::pi)
                phase -= 2.0 * juce::MathConstants<double>::pi;
            buf.setSample (0, i, s);
            buf.setSample (1, i, s);
        }
    };

    int failures = 0;
    constexpr int numIters = 25;

    for (int iter = 0; iter < numIters; ++iter)
    {
        // 1. crear editor
        auto* ed = proc->createEditor();
        if (ed == nullptr)
        {
            std::printf ("[harness] FALLO: createEditor() devolvio null (iter %d)\n", iter);
            ++failures;
            break;
        }

        ed->addToDesktop (juce::ComponentPeer::windowIsResizable);
        ed->setVisible (true);
        mm->runDispatchLoopUntil (30);

        // 2. tamaños variados (los hosts redimensionan sin piedad)
        const int sizes[][2] = { { 1200, 760 }, { 600, 380 }, { 50, 50 },
                                 { 1400, 900 }, { 1000, 640 }, { 1800, 1200 } };
        for (const auto& s : sizes)
        {
            ed->setSize (s[0], s[1]);
            mm->runDispatchLoopUntil (25);
        }

        // 3. presets y bandas con el editor abierto (timer corriendo)
        proc->applyFactoryPreset (iter % 7);
        proc->setBandCount (1 + (iter % 4));
        mm->runDispatchLoopUntil (100);

        // 4. audio mientras el editor vive
        for (int b = 0; b < 10; ++b)
        {
            fillSine (0.5f);
            proc->processBlock (buf, midi);
        }
        mm->runDispatchLoopUntil (50);

        // 5. destruir editor (en el message thread, como los DAWs)
        ed->removeFromDesktop();
        delete ed;
        mm->runDispatchLoopUntil (30);

        if ((iter + 1) % 5 == 0)
            std::printf ("[harness] %d/%d iteraciones OK\n", iter + 1, numIters);
    }

    // audio tras cerrar todo
    for (int b = 0; b < 20; ++b)
    {
        fillSine (0.8f);
        proc->processBlock (buf, midi);
    }

    // medir salida: el limiter debe mantener los picos bajo control
    float peak = 0.0f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            peak = std::max (peak, std::abs (buf.getSample (ch, i)));

    std::printf ("[harness] pico de salida final: %.4f (limiter ceiling -0.3 dB = 0.966)\n", peak);
    if (peak > 0.97f)
    {
        std::printf ("[harness] FALLO: salida excede el ceiling del limiter\n");
        ++failures;
    }

    // ── Prueba de estado: la MISMA que hace pluginval, pero directa sobre
    //    el processor (sin la cola asíncrona del wrapper VST3) ──
    {
        juce::Random rnd (juce::Random::getSystemRandom().nextInt64());
        int stateFailures = 0;

        for (int round = 0; round < 100; ++round)
        {
            // randomizar TODOS los parámetros con basura
            const auto& params = proc->getParameters();
            for (auto* p : params)
                p->setValue (rnd.nextFloat());

            juce::MemoryBlock saved;
            proc->getStateInformation (saved);

            // guardar los valores esperados (denormalizados)
            std::vector<float> expected;
            expected.reserve ((size_t) params.size());
            for (auto* p : params)
                expected.push_back (p->getValue());

            // cambiar TODO a otros valores aleatorios
            for (auto* p : params)
                p->setValue (rnd.nextFloat());

            // restaurar y verificar EXACTITUD total
            proc->setStateInformation (saved.getData(), (int) saved.getSize());

            for (int i = 0; i < params.size(); ++i)
            {
                const float got = params[i]->getValue();
                if (std::abs (got - expected[(size_t) i]) > 0.0001f)
                {
                    if (stateFailures < 5)
                        std::printf ("[harness] estado: param %d (%s) esperado %.4f, obtenido %.4f (ronda %d)\n",
                                     i, params[i]->getName (64).toRawUTF8(),
                                     expected[(size_t) i], got, round);
                    ++stateFailures;
                }
            }
        }

        if (stateFailures == 0)
            std::printf ("[harness] ✔ estado: 100 rondas save/restore EXACTAS (todos los parámetros)\n");
        else
        {
            std::printf ("[harness] ✘ estado: %d desajustes en 100 rondas\n", stateFailures);
            ++failures;
        }
    }

    proc->releaseResources();
    proc.reset();
    mm->runDispatchLoopUntil (50);

    if (failures == 0)
    {
        std::printf ("[harness] ✔ HARNESS COMPLETO SIN CRASHES\n");
        return 0;
    }

    std::printf ("[harness] ✘ FALLOS: %d\n", failures);
    return 1;
}

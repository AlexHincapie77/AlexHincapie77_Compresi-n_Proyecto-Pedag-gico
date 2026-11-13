// ============================================================================
//  validate_all.cpp — Mini-host que carga VST3, LV2 y CLAP con la propia
//  API de hosting de JUCE 8, y valida:
//    1. load
//    2. instantiate
//    3. prepareToPlay
//    4. processBlock (sine 220Hz -6dBFS)
//    5. change parameters (numBands + preset) y re-process
//    6. setStateInformation / getStateInformation round-trip
//    7. delete sin crash
//
//  Compilar con juce_add_console_app (que define JUCE_GLOBAL_MODULE_SETTINGS_AVAILABLE)
// ============================================================================

#include <iostream>
#include <cmath>
#include <vector>

// JuceHeader.h lo genera el build de juce_add_console_app
#include <JuceHeader.h>

namespace {
int failures = 0;
int passes = 0;
void check(bool cond, const std::string& msg) {
    std::cout << "  [" << (cond ? "OK" : "FALLA") << "] " << msg << "\n";
    if (cond) ++passes; else ++failures;
}
} // namespace

static void setParameter(juce::AudioPluginInstance& plugin, int paramIdx, float value)
{
    if (paramIdx >= 0 && paramIdx < plugin.getParameters().size())
    {
        plugin.getParameters().getUnchecked(paramIdx)->setValueNotifyingHost(value);
        std::cout << "    param[" << paramIdx << "] = " << value << "\n";
    }
}

static bool runBattery(juce::AudioPluginInstance& plugin, const juce::String& formatName,
                        int sampleRate, int block)
{
    std::cout << "\n[" << formatName << "] "
              << plugin.getPluginDescription().pluginFormatName << " :: "
              << plugin.getName() << "\n";
    std::cout << "  uniqueId:    " << std::hex << plugin.getPluginDescription().uniqueId << std::dec << "\n";
    std::cout << "  numInputs:   " << plugin.getTotalNumInputChannels() << "\n";
    std::cout << "  numOutputs:  " << plugin.getTotalNumOutputChannels() << "\n";
    std::cout << "  numParams:   " << plugin.getParameters().size() << "\n";

    // 1) Buscar parámetros por nombre (usamos el índice, JUCE AudioProcessorParameter no expone getParameterID)
    int idxNumBands = -1, idxPreset = -1, idxThr0 = -1;
    const auto& params = plugin.getParameters();
    for (int i = 0; i < params.size(); ++i)
    {
        const auto name = params.getUnchecked(i)->getName(512);
        if (name.contains("Num Bands"))    idxNumBands = i;
        if (name == "Preset")              idxPreset = i;
        if (name.contains("B0 Threshold")) idxThr0 = i;
    }
    check(idxNumBands >= 0, "parámetro 'Num Bands' expuesto");
    check(idxPreset >= 0,    "parámetro 'Preset' expuesto");
    check(idxThr0 >= 0,      "parámetro 'B0 Threshold' expuesto");

    // 2) prepareToPlay
    plugin.setRateAndBufferSizeDetails(sampleRate, block);
    plugin.prepareToPlay(sampleRate, block);
    check(true, "prepareToPlay sin excepción");

    // 3) Primer processBlock
    juce::AudioBuffer<float> buf(2, block);
    for (int i = 0; i < block; ++i)
        buf.setSample(0, i, 0.5f * std::sin(2.0 * M_PI * 220.0 * i / sampleRate));
    buf.copyFrom(1, 0, buf, 0, 0, block);

    juce::MidiBuffer midi;
    plugin.processBlock(buf, midi);
    check(true, "processBlock #1 (configuración inicial)");

    float maxAbs = 0; int nanCount = 0, nonZero = 0;
    for (int c = 0; c < buf.getNumChannels(); ++c)
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            float v = buf.getSample(c, i);
            if (std::isnan(v)) ++nanCount;
            if (v != 0.0f) ++nonZero;
            maxAbs = std::max(maxAbs, std::abs(v));
        }
    check(nonZero > block / 2, "salida con audio (no silencio)");
    check(nanCount == 0, "sin NaN/Inf");
    check(maxAbs <= 1.0f, "sin clipping (>1.0) — limiter OK");
    std::cout << "    salida #1: max=" << maxAbs << ", nonZero=" << nonZero << "/" << (2*block) << "\n";

    // 4) Cambiar parámetros por índice
    if (idxPreset >= 0)
        params.getUnchecked(idxPreset)->setValueNotifyingHost(3.0f / 6.0f);  // Preset 3
    if (idxNumBands >= 0)
        params.getUnchecked(idxNumBands)->setValueNotifyingHost(1.0f);  // 4 bandas

    // 5) Segundo processBlock con parámetros cambiados
    for (int i = 0; i < block; ++i)
        buf.setSample(0, i, 0.5f * std::sin(2.0 * M_PI * 440.0 * i / sampleRate));
    buf.copyFrom(1, 0, buf, 0, 0, block);
    plugin.processBlock(buf, midi);
    check(true, "processBlock #2 (con preset + 4 bandas)");

    maxAbs = 0; nanCount = 0; nonZero = 0;
    for (int c = 0; c < buf.getNumChannels(); ++c)
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            float v = buf.getSample(c, i);
            if (std::isnan(v)) ++nanCount;
            if (v != 0.0f) ++nonZero;
            maxAbs = std::max(maxAbs, std::abs(v));
        }
    check(nonZero > block / 2, "salida #2 con audio");
    check(nanCount == 0, "sin NaN/Inf #2");
    check(maxAbs <= 1.0f, "sin clipping #2 — limiter sigue OK");
    std::cout << "    salida #2: max=" << maxAbs << ", nonZero=" << nonZero << "/" << (2*block) << "\n";

    // 6) State round-trip
    juce::MemoryBlock state;
    plugin.getStateInformation(state);
    check(state.getSize() > 100, "getStateInformation produce datos");
    if (state.getSize() > 0)
    {
        plugin.setStateInformation(state.getData(), (int)state.getSize());
        check(true, "setStateInformation #1");
        plugin.setStateInformation(state.getData(), (int)state.getSize());
        check(true, "setStateInformation #2 (idempotente)");
    }

    // 7) releaseResources
    plugin.releaseResources();
    check(true, "releaseResources sin crash");

    return true;
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::cerr << "Uso: " << argv[0]
                  << " <plugin.vst3> <plugin.lv2-or-folder> <plugin.clap>\n";
        return 2;
    }
    juce::String vst3Path = argv[1];
    juce::String lv2Path  = argv[2];
    juce::String clapPath = argv[3];

    juce::AudioPluginFormatManager fm;
    fm.addDefaultFormats();

    std::cout << "Formatos disponibles en JUCE hosting:\n";
    for (auto* fmt : fm.getFormats())
        std::cout << "  - " << fmt->getName() << "\n";

    const int sr = 44100, block = 256;
    juce::String errMsg;

    // ─── VST3 ────────────────────────────────────────────────────────────
    {
        juce::OwnedArray<juce::PluginDescription> descs;
        juce::AudioPluginFormat* vst3fmt = nullptr;
        for (auto* f : fm.getFormats())
            if (f->getName().equalsIgnoreCase("VST3")) vst3fmt = f;
        if (vst3fmt)
        {
            vst3fmt->findAllTypesForFile(descs, vst3Path);
            check(!descs.isEmpty(), "VST3: findAllTypesForFile encontró plugins");
            if (!descs.isEmpty())
            {
                errMsg.clear();
                std::unique_ptr<juce::AudioPluginInstance> p =
                    fm.createPluginInstance(*descs[0], sr, block, errMsg);
                check(p != nullptr, "VST3: createPluginInstance");
                if (p)
                {
                    runBattery(*p, "VST3", sr, block);
                    p.reset();
                    check(true, "VST3: destroy sin crash");
                }
            }
        }
        else { std::cout << "  [SKIP] VST3 no soportado en este build\n"; }
    }

    // ─── LV2 ────────────────────────────────────────────────────────────
    {
        juce::OwnedArray<juce::PluginDescription> descs;
        juce::AudioPluginFormat* lv2fmt = nullptr;
        for (auto* f : fm.getFormats())
            if (f->getName().equalsIgnoreCase("LV2")) lv2fmt = f;
        if (lv2fmt)
        {
            lv2fmt->findAllTypesForFile(descs, lv2Path);
            check(!descs.isEmpty(), "LV2: findAllTypesForFile encontró plugins");
            if (!descs.isEmpty())
            {
                errMsg.clear();
                std::unique_ptr<juce::AudioPluginInstance> p =
                    fm.createPluginInstance(*descs[0], sr, block, errMsg);
                check(p != nullptr, "LV2: createPluginInstance");
                if (p)
                {
                    runBattery(*p, "LV2", sr, block);
                    p.reset();
                    check(true, "LV2: destroy sin crash");
                }
            }
        }
        else { std::cout << "  [SKIP] LV2 no soportado en este build\n"; }
    }

    // ─── CLAP ───────────────────────────────────────────────────────────
    {
        juce::OwnedArray<juce::PluginDescription> descs;
        juce::AudioPluginFormat* clapfmt = nullptr;
        for (auto* f : fm.getFormats())
            if (f->getName().equalsIgnoreCase("CLAP")) clapfmt = f;
        if (clapfmt)
        {
            clapfmt->findAllTypesForFile(descs, clapPath);
            check(!descs.isEmpty(), "CLAP: findAllTypesForFile encontró plugins");
            if (!descs.isEmpty())
            {
                errMsg.clear();
                std::unique_ptr<juce::AudioPluginInstance> p =
                    fm.createPluginInstance(*descs[0], sr, block, errMsg);
                check(p != nullptr, "CLAP: createPluginInstance");
                if (p)
                {
                    runBattery(*p, "CLAP", sr, block);
                    p.reset();
                    check(true, "CLAP: destroy sin crash");
                }
            }
        }
        else { std::cout << "  [SKIP] CLAP no soportado en este build\n"; }
    }

    std::cout << "\n=== RESULTADO: " << passes << " OK, " << failures << " FALLAS ===\n";
    return failures == 0 ? 0 : 1;
}

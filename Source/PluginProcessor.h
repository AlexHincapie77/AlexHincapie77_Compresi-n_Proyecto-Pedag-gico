#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MultibandProcessor.h"
#include "Presets.h"
#include "clap-juce-extensions/clap-juce-extensions.h"

// ===========================================================================
//  IDs de parámetros (compartidos entre processor y editor)
// ===========================================================================
namespace PID
{
    constexpr const char* InputGain      = "inputGain";
    constexpr const char* OutputGain     = "outputGain";
    constexpr const char* GlobalMix      = "globalMix";
    constexpr const char* LimiterCeiling = "limiterCeiling";
    constexpr const char* NumBands       = "numBands";
    constexpr const char* Bypass         = "bypass";
    constexpr const char* Preset         = "preset";
    // v1.2: Auto-Gain global (compensación de volumen)
    constexpr const char* AutoGain       = "autoGain";

    constexpr const char* Threshold = "thr";
    constexpr const char* Ratio     = "rat";
    constexpr const char* Attack    = "att";
    constexpr const char* Release   = "rel";
    constexpr const char* Knee      = "knee";
    constexpr const char* Makeup    = "mk";
    constexpr const char* Mix       = "mix";
    constexpr const char* Type      = "type";
    constexpr const char* Detector  = "det";
    constexpr const char* ScHpf     = "schpf";
    constexpr const char* Lookahead = "la";
    constexpr const char* StereoLink = "sl";
    constexpr const char* BandBypass = "bbyp";
    constexpr const char* ExtSc      = "extsc";
    constexpr const char* Crossover  = "cross";
    // v1.2: por banda
    constexpr const char* Saturation = "sat";
    constexpr const char* AutoMakeup = "amk";

    inline juce::String bandParam(int band, const char* suffix)
    {
        return "b" + juce::String(band) + "_" + suffix;
    }
}

// ===========================================================================
//  PedagogicalCompressorAudioProcessor
//  Hereda de clap_properties: requerido por clap-juce-extensions para que
//  el wrapper CLAP funcione correctamente (is_clap, extensions, etc.)
// ===========================================================================
class PedagogicalCompressorAudioProcessor : public juce::AudioProcessor,
                                              public juce::AudioProcessorValueTreeState::Listener,
                                              public clap_juce_extensions::clap_properties
{
public:
    PedagogicalCompressorAudioProcessor();
    ~PedagogicalCompressorAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return Presets::getNumPresets(); }
    int getCurrentProgram() override { return currentPreset.load(); }
    void setCurrentProgram(int idx) override;
    const juce::String getProgramName(int idx) override { return Presets::getPresetName(idx); }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    MultibandProcessor& getMultiband() { return multiband; }

    float getInputLevelDb() const noexcept { return inputLevelDb.load(); }
    float getOutputLevelDb() const noexcept { return outputLevelDb.load(); }
    float getGainReductionDb() const noexcept { return totalGrDb.load(); }
    float getBandGainReduction(int band) const { return multiband.getBand(band).getGainReductionDb(); }
    float getBandInputLevel(int band) const { return multiband.getBand(band).getInputLevelDb(); }
    float getBandEnvelopeDb(int band) const { return multiband.getBand(band).getEnvelopeDb(); }
    float getBandOutputLevel(int band) const { return multiband.getBand(band).getOutputLevelDb(); }
    float getBandAutoMakeupDb(int band) const { return multiband.getBand(band).getAutoMakeupGainDb(); }
    float getAutoGainDb() const noexcept { return autoGainGainDb.load(); }
    bool  isAutoGainEnabled() const noexcept { return autoGainEnabled.load(); }

    void setLimiterCeiling(float ceilingDb) noexcept { limiterCeilingDb.store(ceilingDb); }
    float getLimiterCeiling() const noexcept { return limiterCeilingDb.load(); }

    void setNumBands(int n);
    int  getNumBands() const noexcept { return multiband.getNumBands(); }

    void setPreset(int idx);

private:
    void parameterChanged(const juce::String& paramID, float newValue) override;
    void syncAllParametersToDSP();
    void applyPresetInternal(const juce::ValueTree& preset);

    juce::AudioProcessorValueTreeState apvts;

    MultibandProcessor multiband;

    std::atomic<float> inputGainDb { 0.0f };
    std::atomic<float> outputGainDb { 0.0f };
    std::atomic<float> globalMix { 1.0f };
    std::atomic<float> limiterCeilingDb { -1.0f };
    std::atomic<bool>  globalBypass { false };
    std::atomic<bool>  autoGainEnabled { false };  // v1.2

    // Auto-Gain global: tracking lento del nivel
    float runningInLevelDb { -60.0f };
    float runningOutLevelDb { -60.0f };
    juce::SmoothedValue<float> autoGainSmoothed { 1.0f };
    std::atomic<float> autoGainGainDb { 0.0f };

    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };
    std::atomic<float> totalGrDb { 0.0f };

    float limiterCeilingLinear { 0.891f };

    std::atomic<int> currentPreset { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedagogicalCompressorAudioProcessor)
};

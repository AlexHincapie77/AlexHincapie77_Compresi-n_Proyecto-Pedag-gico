/*
 * PluginProcessor.h — rev. 3
 * ─────────────────────────────────────────────────────────────────────────
 * Cambios de la rev. 3 (estabilidad):
 *   • currentBandCount / pendingBandCount / isPrepared son ATÓMICOS.
 *   • setBandCount() es thread-safe: solo publica el valor + flag;
 *     el AUDIO THREAD aplica los cambios (multiband.setBandCount,
 *     setParameters, crossovers). Cero carreras GUI↔audio.
 *   • El sidechain externo (canales 3/4) se cablea de verdad al DSP.
 *   • Bypass global = passthrough puro + protección anti-clip.
 */

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "MultibandProcessor.h"

class PedagogicalCompressorAudioProcessor : public juce::AudioProcessor,
                                                private juce::AudioProcessorValueTreeState::Listener
{
public:
    PedagogicalCompressorAudioProcessor();
    ~PedagogicalCompressorAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    pedacomp::MultibandProcessor& getMultiband() noexcept { return multiband; }

    // Thread-safe (llamable desde la GUI)
    int  getBandCount() const noexcept { return currentBandCount.load(); }
    void setBandCount (int count);
    int  getCurrentProgramIndex() const noexcept { return currentProgram; }
    float getCrossoverFrequency (int index) const noexcept { return multiband.getCrossover (index); }
    void  setCrossoverFrequency (int index, float freqHz);   // thread-safe

    float getBandGainReductionDb (int bandIndex) const;
    float getBandPeakInputDb (int bandIndex) const;
    float getBandPeakOutputDb (int bandIndex) const;
    float getOverallInputDb() const noexcept  { return overallInputDb.load(); }
    float getOverallOutputDb() const noexcept { return overallOutputDb.load(); }
    float getOverallGrDb() const noexcept     { return overallGrDb.load(); }

    void applyFactoryPreset (int presetIndex);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void parameterChanged (const juce::String& paramID, float newValue) override;

    // Los tres update* leen el APVTS y escriben el DSP:
    //   · se llaman desde el AUDIO THREAD (flags) durante reproducción
    //   · o desde prepareToPlay / constructor (antes de que el audio corra)
    void updateBandParameters();
    void updateBandCountFromParams();
    void updateCrossoversFromParams();

    void applyOutputSafety (juce::AudioBuffer<float>& buffer);

    juce::AudioProcessorValueTreeState apvts;
    pedacomp::MultibandProcessor multiband;

    juce::dsp::Gain<float> preGain;
    juce::dsp::Gain<float> postGain;
    juce::AudioBuffer<float> dryBuffer;

    std::atomic<int>  currentBandCount { 1 };
    std::atomic<int>  pendingBandCount  { 1 };
    int currentProgram = 0;
    std::atomic<bool> isPrepared { false };

    // Flags atómicos: el message thread los marca, el audio thread los aplica
    std::atomic<bool> needsParameterUpdate { false };
    std::atomic<bool> needsBandCountUpdate  { false };
    std::atomic<bool> needsCrossoverUpdate  { false };

    std::atomic<float> overallInputDb  { -100.0f };
    std::atomic<float> overallOutputDb { -100.0f };
    std::atomic<float> overallGrDb     { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedagogicalCompressorAudioProcessor)
};

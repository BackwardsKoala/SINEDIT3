#pragma once
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "plugids.h"
#include "partial.h"
#include "fft.h"
#include <vector>
#include <mutex>
#include <atomic>

class SinDecomposerProcessor : public Steinberg::Vst::AudioEffect {
public:
    SinDecomposerProcessor();
    ~SinDecomposerProcessor() override = default;

    static Steinberg::FUnknown* createInstance(void*) {
        return (Steinberg::Vst::IAudioProcessor*)new SinDecomposerProcessor();
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API setBusArrangements(
        Steinberg::Vst::SpeakerArrangement* inputs,  Steinberg::int32 numIns,
        Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) override;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) override;

    // Called by controller when user loads a file
    void loadSampleData(const std::vector<float>& mono, float sr);

    // Called by controller to get spectrum data for GUI
    bool getAnalysisResults(std::vector<Partial>& out, std::vector<float>& spectrum);

private:
    void runAnalysis();
    void synthesiseBlock(float** outputs, int numSamples, float gain);

    FFTAnalyser              mAnalyser;
    std::vector<float>       mSampleBuffer;
    float                    mSampleRate   = 44100.0f;
    float                    mGain         = 1.0f;
    float                    mAttack       = 0.01f;   // seconds
    float                    mRelease      = 0.3f;    // seconds
    float                    mEnvelope     = 0.0f;
    bool                     mNoteOn       = false;

    // Shared between audio thread and UI thread
    mutable std::mutex       mPartialMutex;
    std::vector<Partial>     mPartials;          // active set used for synthesis
    std::vector<Partial>     mPendingPartials;   // set by analysis, swapped in safely
    std::vector<float>       mSpectrumData;
    std::atomic<bool>        mNewPartialsPending { false };
    std::atomic<bool>        mAnalysisPending    { false };
    std::atomic<bool>        mAnalysisDone       { false };
};

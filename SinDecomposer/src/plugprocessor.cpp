#include "plugprocessor.h"
#include "plugids.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "base/source/fstreamer.h"
#include <cmath>
#include <algorithm>
#include <thread>

using namespace Steinberg;
using namespace Steinberg::Vst;

static constexpr float kPI  = 3.14159265358979323846f;
static constexpr float k2PI = 2.0f * kPI;

SinDecomposerProcessor::SinDecomposerProcessor() {
    setControllerClass(SinDecomposerControllerUID);
}

tresult PLUGIN_API SinDecomposerProcessor::initialize(FUnknown* context) {
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk) return result;

    addAudioInput(STR16("Stereo In"),  SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
    addEventInput(STR16("Event In"), 1);
    return kResultOk;
}

tresult PLUGIN_API SinDecomposerProcessor::canProcessSampleSize(int32 symbolicSampleSize) {
    return (symbolicSampleSize == kSample32) ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API SinDecomposerProcessor::setBusArrangements(
    SpeakerArrangement* inputs,  int32 numIns,
    SpeakerArrangement* outputs, int32 numOuts)
{
    if (numIns == 1 && numOuts == 1 &&
        inputs[0] == SpeakerArr::kStereo && outputs[0] == SpeakerArr::kStereo)
        return kResultOk;
    return kResultFalse;
}

tresult PLUGIN_API SinDecomposerProcessor::setupProcessing(ProcessSetup& setup) {
    mSampleRate = (float)setup.sampleRate;
    return AudioEffect::setupProcessing(setup);
}

void SinDecomposerProcessor::loadSampleData(const std::vector<float>& mono, float sr) {
    std::lock_guard<std::mutex> lock(mPartialMutex);
    mSampleBuffer = mono;
    mSampleRate   = sr;
    mAnalysisPending = true;
}

void SinDecomposerProcessor::runAnalysis() {
    std::vector<float> localBuf;
    {
        std::lock_guard<std::mutex> lock(mPartialMutex);
        localBuf = mSampleBuffer;
    }

    auto partials = mAnalyser.analyse(localBuf.data(), (int)localBuf.size(), mSampleRate);
    auto spectrum = mAnalyser.getMagnitudeSpectrum();

    {
        std::lock_guard<std::mutex> lock(mPartialMutex);
        mPendingPartials = partials;
        mSpectrumData    = spectrum;
    }
    mNewPartialsPending = true;
    mAnalysisDone       = true;
    mAnalysisPending    = false;
}

bool SinDecomposerProcessor::getAnalysisResults(std::vector<Partial>& out, std::vector<float>& spectrum) {
    if (!mAnalysisDone) return false;
    std::lock_guard<std::mutex> lock(mPartialMutex);
    out      = mPendingPartials;
    spectrum = mSpectrumData;
    return true;
}

void SinDecomposerProcessor::synthesiseBlock(float** outputs, int numSamples, float gain) {
    // Swap in pending partials safely (audio thread side)
    if (mNewPartialsPending.exchange(false)) {
        std::lock_guard<std::mutex> lock(mPartialMutex);
        mPartials = mPendingPartials;
        // Reset synthesis phases
        for (auto& p : mPartials)
            p.synthPhase = p.initPhase;
    }

    if (mPartials.empty()) return;

    float attackCoef  = (mAttack  > 0.0f) ? std::exp(-1.0f / (mSampleRate * mAttack))  : 0.0f;
    float releaseCoef = (mRelease > 0.0f) ? std::exp(-1.0f / (mSampleRate * mRelease)) : 0.0f;

    for (int i = 0; i < numSamples; i++) {
        // Simple AR envelope
        float target = mNoteOn ? 1.0f : 0.0f;
        if (mEnvelope < target)
            mEnvelope = target + (mEnvelope - target) * attackCoef;
        else
            mEnvelope = target + (mEnvelope - target) * releaseCoef;

        float sample = 0.0f;
        for (auto& p : mPartials) {
            if (!p.enabled) continue;
            sample += p.amplitude * std::sin(p.synthPhase);
            p.synthPhase += k2PI * p.frequency / mSampleRate;
            if (p.synthPhase > k2PI) p.synthPhase -= k2PI;
        }

        // Normalise by number of active partials to avoid clipping
        int activeCount = 0;
        for (auto& p : mPartials) if (p.enabled) activeCount++;
        if (activeCount > 0) sample /= std::sqrt((float)activeCount);

        float out = sample * mEnvelope * gain;
        outputs[0][i] = out;
        outputs[1][i] = out;
    }
}

tresult PLUGIN_API SinDecomposerProcessor::process(ProcessData& data) {
    // --- Handle parameter changes ---
    if (data.inputParameterChanges) {
        int32 numParamsChanged = data.inputParameterChanges->getParameterCount();
        for (int32 i = 0; i < numParamsChanged; i++) {
            IParamValueQueue* queue = data.inputParameterChanges->getParameterData(i);
            if (!queue) continue;
            ParamID pid = queue->getParameterId();
            int32 numPoints = queue->getPointCount();
            if (numPoints == 0) continue;
            int32 offset; ParamValue value;
            queue->getPoint(numPoints - 1, offset, value);

            if (pid == kParamGain) {
                mGain = (float)value;
            } else if (pid == kParamAttack) {
                mAttack = (float)(value * 2.0); // 0..2s
            } else if (pid == kParamRelease) {
                mRelease = (float)(value * 4.0); // 0..4s
            } else if (pid == kParamAnalyse && value > 0.5) {
                if (!mSampleBuffer.empty() && !mAnalysisPending) {
                    mAnalysisPending = true;
                    std::thread([this]() { runAnalysis(); }).detach();
                }
            } else if (pid >= kParamPartialBase && pid < kParamPartialBase + MAX_PARTIALS) {
                int idx = pid - kParamPartialBase;
                std::lock_guard<std::mutex> lock(mPartialMutex);
                if (idx < (int)mPartials.size())
                    mPartials[idx].enabled = (value > 0.5);
                if (idx < (int)mPendingPartials.size())
                    mPendingPartials[idx].enabled = (value > 0.5);
            }
        }
    }

    // --- Handle MIDI note on/off ---
    if (data.inputEvents) {
        int32 numEvents = data.inputEvents->getEventCount();
        for (int32 i = 0; i < numEvents; i++) {
            Event e;
            data.inputEvents->getEvent(i, e);
            if (e.type == Event::kNoteOnEvent)
                mNoteOn = (e.noteOn.velocity > 0.0f);
            else if (e.type == Event::kNoteOffEvent)
                mNoteOn = false;
        }
    }

    // --- Synthesise output ---
    if (data.numOutputs > 0 && data.outputs[0].numChannels >= 2) {
        float* outputs[2] = {
            data.outputs[0].channelBuffers32[0],
            data.outputs[0].channelBuffers32[1]
        };
        synthesiseBlock(outputs, data.numSamples, mGain);
        data.outputs[0].silenceFlags = 0;
    }

    return kResultOk;
}

tresult PLUGIN_API SinDecomposerProcessor::setState(IBStream* state) {
    IBStreamer streamer(state, kLittleEndian);
    float gain = 1.0f;
    streamer.readFloat(gain);
    mGain = gain;
    return kResultOk;
}

tresult PLUGIN_API SinDecomposerProcessor::getState(IBStream* state) {
    IBStreamer streamer(state, kLittleEndian);
    streamer.writeFloat(mGain);
    return kResultOk;
}

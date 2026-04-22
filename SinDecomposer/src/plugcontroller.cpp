#include "plugcontroller.h"
#include "editor.h"
#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

tresult PLUGIN_API SinDecomposerController::initialize(FUnknown* context) {
    tresult result = EditController::initialize(context);
    if (result != kResultOk) return result;

    // Gain
    parameters.addParameter(STR16("Gain"), STR16(""), 0, 1.0, ParameterInfo::kCanAutomate, kParamGain);
    // Attack
    parameters.addParameter(STR16("Attack"), STR16("s"), 0, 0.005, ParameterInfo::kCanAutomate, kParamAttack);
    // Release
    parameters.addParameter(STR16("Release"), STR16("s"), 0, 0.075, ParameterInfo::kCanAutomate, kParamRelease);
    // Analyse trigger
    parameters.addParameter(STR16("Analyse"), STR16(""), 1, 0, ParameterInfo::kIsBypass, kParamAnalyse);

    // One boolean parameter per partial
    for (int i = 0; i < MAX_PARTIALS; i++) {
        String128 title;
        Steinberg::UString(title, 128).printInt(i + 1);
        String128 prefix; Steinberg::UString(prefix, 128).assign(STR16("Partial "));
        String128 name;
        Steinberg::UString(name, 128).assign(prefix);
        Steinberg::UString(name, 128).append(title);
        parameters.addParameter(name, STR16(""), 1, 1.0, ParameterInfo::kCanAutomate,
                                 kParamPartialBase + i);
    }

    return kResultOk;
}

IPlugView* PLUGIN_API SinDecomposerController::createView(FIDString name) {
    if (strcmp(name, ViewType::kEditor) == 0)
        return new SinDecomposerEditor(this);
    return nullptr;
}

tresult PLUGIN_API SinDecomposerController::setComponentState(IBStream* state) {
    if (!state) return kResultFalse;
    IBStreamer streamer(state, kLittleEndian);
    float gain = 1.0f;
    streamer.readFloat(gain);
    setParamNormalized(kParamGain, gain);
    return kResultOk;
}

#pragma once
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "plugids.h"

class SinDecomposerController : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return (Steinberg::Vst::IEditController*)new SinDecomposerController();
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) override;
};

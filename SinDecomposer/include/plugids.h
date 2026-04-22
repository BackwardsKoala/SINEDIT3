#pragma once
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

// Generate unique IDs with https://www.guidgenerator.com/
static const Steinberg::FUID SinDecomposerProcessorUID(0xA1B2C3D4, 0xE5F60011, 0x22334455, 0x66778899);
static const Steinberg::FUID SinDecomposerControllerUID(0xB2C3D4E5, 0xF6071122, 0x33445566, 0x778899AA);

enum SinDecomposerParams : Steinberg::Vst::ParamID {
    kParamGain      = 0,
    kParamAttack    = 1,
    kParamRelease   = 2,
    kParamAnalyse   = 3,   // trigger analysis (momentary button)
    // Partials 0..255 enabled = params 100..355
    kParamPartialBase = 100,
    kNumParams = 356
};

static const int MAX_PARTIALS = 256;
static const int FFT_ORDER    = 14;        // 2^14 = 16384 point FFT
static const int FFT_SIZE     = 1 << FFT_ORDER;

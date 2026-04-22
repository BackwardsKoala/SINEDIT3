#include "public.sdk/source/main/pluginfactory.h"
#include "plugprocessor.h"
#include "plugcontroller.h"
#include "plugids.h"
#include "version.h"

#define stringPluginName "SinDecomposer"

BEGIN_FACTORY_DEF("YourName",
                  "https://yourwebsite.com",
                  "mailto:you@yourwebsite.com")

DEF_CLASS2(INLINE_UID_FROM_FUID(SinDecomposerProcessorUID),
           PClassInfo::kManyInstances,
           kVstAudioEffectClass,
           stringPluginName,
           Vst::kDistributable,
           Vst::PlugType::kFxInstrument,
           FULL_VERSION_STR,
           kVstVersionString,
           SinDecomposerProcessor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(SinDecomposerControllerUID),
           PClassInfo::kManyInstances,
           kVstComponentControllerClass,
           stringPluginName "Controller",
           0,
           "",
           FULL_VERSION_STR,
           kVstVersionString,
           SinDecomposerController::createInstance)

END_FACTORY

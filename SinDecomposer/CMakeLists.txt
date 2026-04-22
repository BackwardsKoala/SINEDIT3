cmake_minimum_required(VERSION 3.22)
project(SinDecomposer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch VST3 SDK
include(FetchContent)
FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        v3.7.9_build_16
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES "pluginterfaces;base;public.sdk;vstgui4"
)
FetchContent_MakeAvailable(vst3sdk)

set(SMTG_ADD_VSTGUI ON)
set(SMTG_ADD_VST3_PLUGINS_SAMPLES OFF)
set(SMTG_ADD_VST3_HOSTING_SAMPLES OFF)
set(SMTG_BUILD_UNIVERSAL_BINARY ON)  # arm64 + x86_64 fat binary for Mac

smtg_enable_vst3_sdk()

smtg_add_vst3plugin(SinDecomposer
    src/plugprocessor.cpp
    src/plugcontroller.cpp
    src/plugfactory.cpp
    src/editor.cpp
    src/fft.cpp
    include/plugids.h
    include/plugprocessor.h
    include/plugcontroller.h
    include/editor.h
    include/fft.h
    include/partial.h
)

target_include_directories(SinDecomposer PRIVATE include)

smtg_target_configure_version_file(SinDecomposer)
smtg_target_add_plugin_resources(SinDecomposer
    RESOURCES resources/
)

if(SMTG_MAC)
    smtg_target_set_bundle(SinDecomposer
        BUNDLE_IDENTIFIER "com.yourname.sindecomposer"
        COMPANY_NAME "YourName"
    )
endif()

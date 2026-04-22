cmake_minimum_required(VERSION 3.22)
project(SinDecomposer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES "pluginterfaces;base;public.sdk;vstgui4"
)
FetchContent_MakeAvailable(vst3sdk)

set(SMTG_ADD_VSTGUI ON CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_PLUGINS_SAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VST3_HOSTING_SAMPLES OFF CACHE BOOL "" FORCE)

list(APPEND CMAKE_MODULE_PATH "${vst3sdk_SOURCE_DIR}/cmake/modules")
include(SMTG_VST3_SDK)

smtg_enable_vst3_sdk()

smtg_add_vst3plugin(SinDecomposer
    src/plugprocessor.cpp
    src/plugcontroller.cpp
    src/plugfactory.cpp
    src/editor.cpp
    src/fft.cpp
)

target_include_directories(SinDecomposer PRIVATE
    include
    ${vst3sdk_SOURCE_DIR}
)

if(SMTG_MAC)
    smtg_target_set_bundle(SinDecomposer
        BUNDLE_IDENTIFIER "com.yourname.sindecomposer"
        COMPANY_NAME "YourName"
    )
endif()

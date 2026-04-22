#pragma once
#include <atomic>

struct Partial {
    float frequency  = 0.0f;   // Hz
    float amplitude  = 0.0f;   // normalised 0..1
    float phase      = 0.0f;   // radians, current synthesis phase accumulator
    float initPhase  = 0.0f;   // phase captured at analysis time
    bool  enabled    = true;

    // Running phase for synthesis (updated per sample)
    float synthPhase = 0.0f;
};

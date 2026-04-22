#pragma once
#include <vector>
#include <complex>
#include "partial.h"
#include "plugids.h"

class FFTAnalyser {
public:
    FFTAnalyser();

    // Analyse mono audio buffer, returns up to MAX_PARTIALS peaks sorted by amplitude
    std::vector<Partial> analyse(const float* samples, int numSamples, float sampleRate);

    // Most recent magnitude spectrum (FFT_SIZE/2 bins), for GUI display
    const std::vector<float>& getMagnitudeSpectrum() const { return mMagnitude; }

private:
    void computeFFT(std::vector<std::complex<float>>& buf);
    void buildHannWindow();

    std::vector<float>                mWindow;
    std::vector<float>                mMagnitude;
    std::vector<std::complex<float>>  mFFTBuf;
};

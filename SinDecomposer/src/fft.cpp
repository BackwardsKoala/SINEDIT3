#include "fft.h"
#include <cmath>
#include <algorithm>
#include <numeric>

static constexpr float kPI = 3.14159265358979323846f;
static constexpr float k2PI = 2.0f * kPI;

FFTAnalyser::FFTAnalyser() {
    buildHannWindow();
    mFFTBuf.resize(FFT_SIZE);
    mMagnitude.resize(FFT_SIZE / 2, 0.0f);
}

void FFTAnalyser::buildHannWindow() {
    mWindow.resize(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; i++)
        mWindow[i] = 0.5f * (1.0f - std::cos(k2PI * i / (FFT_SIZE - 1)));
}

// In-place iterative Cooley-Tukey FFT
void FFTAnalyser::computeFFT(std::vector<std::complex<float>>& buf) {
    int N = (int)buf.size();

    // Bit-reversal permutation
    for (int i = 1, j = 0; i < N; i++) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(buf[i], buf[j]);
    }

    // FFT butterfly
    for (int len = 2; len <= N; len <<= 1) {
        float ang = -k2PI / len;
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < N; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; j++) {
                std::complex<float> u = buf[i + j];
                std::complex<float> v = buf[i + j + len / 2] * w;
                buf[i + j]            = u + v;
                buf[i + j + len / 2]  = u - v;
                w *= wlen;
            }
        }
    }
}

std::vector<Partial> FFTAnalyser::analyse(const float* samples, int numSamples, float sampleRate) {
    // Fill FFT buffer with windowed audio (zero-pad if needed)
    int copyLen = std::min(numSamples, FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; i++) {
        float s = (i < copyLen) ? samples[i] : 0.0f;
        mFFTBuf[i] = std::complex<float>(s * mWindow[i], 0.0f);
    }

    computeFFT(mFFTBuf);

    // Compute magnitude spectrum
    float binWidth = sampleRate / FFT_SIZE;
    int halfSize = FFT_SIZE / 2;
    for (int i = 0; i < halfSize; i++)
        mMagnitude[i] = std::abs(mFFTBuf[i]) / (FFT_SIZE / 2);

    // Peak pick — local maxima above noise floor
    struct Peak { float freq, amp, phase; };
    std::vector<Peak> peaks;
    peaks.reserve(MAX_PARTIALS * 2);

    float noiseFloor = *std::max_element(mMagnitude.begin(), mMagnitude.end()) * 0.005f;

    for (int bin = 2; bin < halfSize - 2; bin++) {
        float m = mMagnitude[bin];
        if (m < noiseFloor) continue;
        if (m <= mMagnitude[bin - 1] || m <= mMagnitude[bin + 1]) continue;
        if (m <= mMagnitude[bin - 2] || m <= mMagnitude[bin + 2]) continue;

        // Parabolic interpolation for sub-bin frequency accuracy
        float prev = mMagnitude[bin - 1], next = mMagnitude[bin + 1];
        float denom = prev - 2.0f * m + next;
        float offset = (denom != 0.0f) ? 0.5f * (prev - next) / denom : 0.0f;
        offset = std::max(-0.5f, std::min(0.5f, offset));

        float freq = (bin + offset) * binWidth;
        if (freq < 20.0f || freq > 20000.0f) continue;

        float ph = std::arg(mFFTBuf[bin]);
        peaks.push_back({ freq, m, ph });
    }

    // Sort by amplitude descending, keep top MAX_PARTIALS
    std::sort(peaks.begin(), peaks.end(), [](const Peak& a, const Peak& b) {
        return a.amp > b.amp;
    });
    if ((int)peaks.size() > MAX_PARTIALS)
        peaks.resize(MAX_PARTIALS);

    // Re-sort by frequency ascending for display
    std::sort(peaks.begin(), peaks.end(), [](const Peak& a, const Peak& b) {
        return a.freq < b.freq;
    });

    // Normalise amplitudes
    float maxAmp = peaks.empty() ? 1.0f : peaks[0].amp;
    // Find actual max after re-sort
    for (auto& p : peaks) maxAmp = std::max(maxAmp, p.amp);

    std::vector<Partial> result;
    result.reserve(peaks.size());
    for (auto& p : peaks) {
        Partial part;
        part.frequency  = p.freq;
        part.amplitude  = p.amp / maxAmp;
        part.initPhase  = p.phase;
        part.synthPhase = p.phase;
        part.enabled    = true;
        result.push_back(part);
    }
    return result;
}

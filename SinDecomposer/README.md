# SinDecomposer VST3

A VST3 instrument plugin that decomposes a WAV sample into up to **256 sinusoidal partials** via FFT analysis, then lets you selectively toggle each partial on/off for resynthesis via additive synthesis.

---

## How it works

1. **Load** a mono or stereo WAV file using the "Load Sample" button
2. **Analyse** — runs a 16384-point Hann-windowed FFT with parabolic peak interpolation, extracts up to 256 sinusoidal peaks (frequency, amplitude, phase)
3. **Spectrum view** shows all detected partials as green dots overlaid on the magnitude spectrum
4. **Partial grid** — 256 toggle buttons (32 cols × 8 rows), each showing frequency and relative amplitude. Click to enable/disable individual sinusoids
5. **Play via MIDI** — trigger notes to hear the reconstructed sound. Attack/Release envelope shaping available

---

## Building (Windows dev → macOS output)

### Option A: GitHub Actions (recommended — free, no Mac needed)

1. Create a free account at https://github.com
2. Create a new repository and push this folder to it:
   ```
   git init
   git add .
   git commit -m "initial"
   git remote add origin https://github.com/YOUR_USERNAME/SinDecomposer.git
   git push -u origin main
   ```
3. Go to your repo → **Actions** tab → the build runs automatically
4. When it finishes, click the run → **Artifacts** → download `SinDecomposer-macOS-VST3.zip`
5. Unzip, copy `SinDecomposer.vst3` to `/Library/Audio/Plug-Ins/VST3/` on your Mac
6. Scan in your DAW (Ableton, Logic, Reaper, etc.)

### Option B: Build locally on a Mac

Requirements:
- macOS 12+ 
- Xcode 14+ (from App Store)
- CMake 3.22+ (`brew install cmake`)
- Git

```bash
git clone https://github.com/YOUR_USERNAME/SinDecomposer.git
cd SinDecomposer
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The `.vst3` bundle will be in `build/VST3/Release/`.

Copy it:
```bash
cp -r build/VST3/Release/SinDecomposer.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

---

## Project structure

```
SinDecomposer/
├── CMakeLists.txt          # Build config — fetches VST3 SDK automatically
├── include/
│   ├── plugids.h           # UIDs, parameter IDs, constants
│   ├── partial.h           # Partial data structure
│   ├── fft.h               # FFT analyser interface
│   ├── plugprocessor.h     # Audio processor
│   ├── plugcontroller.h    # Parameter controller
│   ├── editor.h            # VSTGUI editor
│   └── version.h           # Version string
├── src/
│   ├── fft.cpp             # Cooley-Tukey FFT (no dependencies)
│   ├── plugprocessor.cpp   # DSP: analysis trigger + additive synthesis
│   ├── plugcontroller.cpp  # VST3 parameter registration
│   ├── editor.cpp          # Full GUI: spectrum view + partial grid
│   └── plugfactory.cpp     # VST3 factory entry point
└── .github/workflows/
    └── build.yml           # GitHub Actions: auto-build Mac VST3
```

---

## Customisation

**Change number of partials**: edit `MAX_PARTIALS` in `include/plugids.h` (and update grid layout in `editor.cpp`)

**FFT resolution**: edit `FFT_ORDER` in `plugids.h`. `14` = 16384 points. `15` = 32768 (better frequency resolution, slower)

**Supported formats**: currently WAV (PCM 16/24/32-bit, float 32-bit). For AIFF/MP3 support, add a decoder library.

---

## Dependencies (all auto-fetched by CMake)

- [VST3 SDK](https://github.com/steinbergmedia/vst3sdk) — Steinberg, BSD 3-Clause for open source projects
- VSTGUI (bundled inside VST3 SDK)

No other dependencies. The FFT is implemented from scratch (Cooley-Tukey).

#include "editor.h"
#include "plugcontroller.h"
#include "plugprocessor.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/cslider.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cbitmap.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include <cmath>
#include <fstream>
#include <sstream>

// Platform file dialog
#if defined(__APPLE__)
  #include <AppKit/AppKit.h>
#elif defined(_WIN32)
  #include <windows.h>
  #include <commdlg.h>
#endif

using namespace VSTGUI;
using namespace Steinberg;
using namespace Steinberg::Vst;

// ---- Colour palette ----
static const CColor kBgDark       (18,  18,  22,  255);
static const CColor kBgMid        (28,  28,  36,  255);
static const CColor kBgLight      (40,  40,  52,  255);
static const CColor kAccentBlue   (55,  138, 221, 255);
static const CColor kAccentGreen  (29,  158, 117, 255);
static const CColor kAccentOff    (60,  60,  75,  255);
static const CColor kTextPrimary  (220, 220, 230, 255);
static const CColor kTextMuted    (130, 130, 150, 255);

// ---- Tag IDs for controls ----
enum Tags {
    kTagLoad       = 1000,
    kTagAnalyse    = 1001,
    kTagSelectAll  = 1002,
    kTagSelectNone = 1003,
    kTagGain       = 1004,
    kTagAttack     = 1005,
    kTagRelease    = 1006,
    kTagPartial0   = 2000  // 2000 + partial index
};

// ---- Minimal WAV loader (PCM 16/24/32 float, mono/stereo) ----
static bool loadWAV(const std::string& path, std::vector<float>& mono, float& sr) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    auto read4 = [&]() -> uint32_t {
        uint32_t v = 0; f.read((char*)&v, 4); return v;
    };
    auto read2 = [&]() -> uint16_t {
        uint16_t v = 0; f.read((char*)&v, 2); return v;
    };

    if (read4() != 0x46464952) return false; // "RIFF"
    read4(); // file size
    if (read4() != 0x45564157) return false; // "WAVE"

    uint16_t audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0, dataSize = 0;
    bool foundFmt = false, foundData = false;

    while (f && !foundData) {
        uint32_t chunkId   = read4();
        uint32_t chunkSize = read4();
        if (chunkId == 0x20746d66) { // "fmt "
            audioFormat  = read2();
            numChannels  = read2();
            sampleRate   = read4();
            read4(); read2(); // byte rate, block align
            bitsPerSample = read2();
            if (chunkSize > 16) f.seekg(chunkSize - 16, std::ios::cur);
            foundFmt = true;
        } else if (chunkId == 0x61746164) { // "data"
            dataSize  = chunkSize;
            foundData = true;
        } else {
            f.seekg(chunkSize, std::ios::cur);
        }
    }

    if (!foundFmt || !foundData || numChannels == 0) return false;

    sr = (float)sampleRate;
    int bytesPerSample = bitsPerSample / 8;
    int totalSamples   = dataSize / bytesPerSample;
    int monoSamples    = totalSamples / numChannels;
    mono.resize(monoSamples);

    for (int i = 0; i < monoSamples; i++) {
        float sum = 0.0f;
        for (int ch = 0; ch < numChannels; ch++) {
            float s = 0.0f;
            if (audioFormat == 1) {
                if (bitsPerSample == 16) {
                    int16_t v = 0; f.read((char*)&v, 2); s = v / 32768.0f;
                } else if (bitsPerSample == 24) {
                    uint8_t b[3] = {}; f.read((char*)b, 3);
                    int32_t v = (b[2] << 16) | (b[1] << 8) | b[0];
                    if (v & 0x800000) v |= 0xFF000000;
                    s = v / 8388608.0f;
                } else if (bitsPerSample == 32) {
                    int32_t v = 0; f.read((char*)&v, 4); s = v / 2147483648.0f;
                }
            } else if (audioFormat == 3 && bitsPerSample == 32) {
                f.read((char*)&s, 4);
            }
            sum += s;
        }
        mono[i] = sum / numChannels;
    }
    return true;
}

// ---- Spectrum view ----
class SpectrumView : public CView {
public:
    SpectrumView(const CRect& r) : CView(r) {}

    void setData(const std::vector<float>& spectrum, const std::vector<Partial>& partials) {
        mSpectrum  = spectrum;
        mPartials  = partials;
        invalid();
    }

    void draw(CDrawContext* ctx) override {
        CRect bounds = getViewSize();
        ctx->setFillColor(kBgMid);
        ctx->drawRect(bounds, kDrawFilled);

        if (mSpectrum.empty()) {
            ctx->setFontColor(kTextMuted);
            ctx->setFont(kNormalFontSmall);
            ctx->drawString("Load a sample and click Analyse", bounds, kCenterText);
            return;
        }

        float W = (float)bounds.getWidth();
        float H = (float)bounds.getHeight();
        float maxMag = *std::max_element(mSpectrum.begin(), mSpectrum.end());
        if (maxMag <= 0.0f) return;

        int bins = (int)mSpectrum.size();
        // Draw magnitude spectrum as filled area
        CDrawContext::LinePair line;
        ctx->setLineWidth(1.0f);
        ctx->setFrameColor(CColor(55, 138, 221, 80));

        for (int x = 0; x < (int)W; x++) {
            int bin = (int)((float)x / W * bins);
            if (bin >= bins) bin = bins - 1;
            float h = (mSpectrum[bin] / maxMag) * (H - 4.0f);
            CPoint top   (bounds.left + x, bounds.bottom - h);
            CPoint bottom(bounds.left + x, bounds.bottom);
            ctx->drawLine(top, bottom);
        }

        // Overlay partial markers
        for (auto& p : mPartials) {
            // Map frequency to x (log scale, 20Hz..20kHz)
            float logMin = std::log10(20.0f), logMax = std::log10(20000.0f);
            float logF   = std::log10(std::max(p.frequency, 20.0f));
            float xPos   = (logF - logMin) / (logMax - logMin) * W + bounds.left;
            float yPos   = bounds.bottom - p.amplitude * (H - 4.0f);

            CColor c = p.enabled ? kAccentGreen : kAccentOff;
            ctx->setFillColor(c);
            ctx->drawEllipse(CRect(xPos - 3, yPos - 3, xPos + 3, yPos + 3), kDrawFilled);
        }
    }

private:
    std::vector<float>   mSpectrum;
    std::vector<Partial> mPartials;
};

// ---- Partial toggle button ----
class PartialButton : public CView {
public:
    PartialButton(const CRect& r, int index, IControlListener* listener)
        : CView(r), mIndex(index), mListener(listener), mEnabled(true) {}

    void setPartial(const Partial& p) { mPartial = p; mEnabled = p.enabled; invalid(); }
    bool isEnabled() const { return mEnabled; }
    void setEnabled(bool e) { mEnabled = e; invalid(); }

    void draw(CDrawContext* ctx) override {
        CRect b = getViewSize();
        CColor bg = mEnabled ? kAccentBlue : kBgLight;
        ctx->setFillColor(bg);
        ctx->drawRect(b, kDrawFilled);
        ctx->setFrameColor(mEnabled ? kAccentBlue : kAccentOff);
        ctx->setLineWidth(0.5f);
        ctx->drawRect(b, kDrawStroked);

        ctx->setFontColor(mEnabled ? kTextPrimary : kTextMuted);
        ctx->setFont(kNormalFontSmall, 9);

        std::string freqStr;
        if (mPartial.frequency >= 1000.0f) {
            char buf[16]; snprintf(buf, sizeof(buf), "%.1fk", mPartial.frequency / 1000.0f);
            freqStr = buf;
        } else {
            char buf[16]; snprintf(buf, sizeof(buf), "%dHz", (int)mPartial.frequency);
            freqStr = buf;
        }
        ctx->drawString(freqStr.c_str(), b, kCenterText);
    }

    CMouseEventResult onMouseDown(CPoint& p, const CButtonState& btns) override {
        if (btns.isLeftButton()) {
            mEnabled = !mEnabled;
            invalid();
            if (mListener) {
                // Create a fake CControl to reuse the listener interface
                class FakeCtrl : public CControl {
                public:
                    FakeCtrl(int32_t tag, float val) : CControl(CRect()) { setTag(tag); setValue(val); }
                    void draw(CDrawContext*) override {}
                };
                FakeCtrl fc(kTagPartial0 + mIndex, mEnabled ? 1.0f : 0.0f);
                mListener->valueChanged(&fc);
            }
        }
        return kMouseEventHandled;
    }

private:
    int              mIndex;
    IControlListener* mListener;
    bool             mEnabled;
    Partial          mPartial;
};

// ======================================================
// SinDecomposerEditor
// ======================================================

SinDecomposerEditor::SinDecomposerEditor(EditController* controller)
    : VSTGUIEditor(controller) {
    setRect(ViewRect(0, 0, (int32)kEditorWidth, (int32)kEditorHeight));
}

SinDecomposerEditor::~SinDecomposerEditor() {
    if (mTimer) { mTimer->stop(); mTimer->forget(); }
}

bool PLUGIN_API SinDecomposerEditor::open(void* parent, const PlatformType& type) {
    if (!VSTGUIEditor::open(parent, type)) return false;
    buildUI(frame);
    mTimer = new CVSTGUITimer(this, 100); // poll every 100ms
    mTimer->start();
    return true;
}

void PLUGIN_API SinDecomposerEditor::close() {
    if (mTimer) { mTimer->stop(); mTimer->forget(); mTimer = nullptr; }
    VSTGUIEditor::close();
}

void SinDecomposerEditor::buildUI(CFrame* f) {
    f->setBackgroundColor(kBgDark);
    CCoord W = kEditorWidth, H = kEditorHeight;

    // Title
    auto* title = new CTextLabel(CRect(10, 8, 400, 32), "Sinusoidal Decomposer");
    title->setFontColor(kTextPrimary);
    title->setFont(kNormalFontBig);
    title->setBackColor(kColorTransparent);
    title->setFrameColor(kColorTransparent);
    title->setHoriAlign(kLeftText);
    f->addView(title);

    mStatusLabel = new CTextLabel(CRect(400, 8, W - 10, 32), "No sample loaded");
    mStatusLabel->setFontColor(kTextMuted);
    mStatusLabel->setFont(kNormalFontSmall);
    mStatusLabel->setBackColor(kColorTransparent);
    mStatusLabel->setFrameColor(kColorTransparent);
    mStatusLabel->setHoriAlign(kRightText);
    f->addView(mStatusLabel);

    // Toolbar
    CCoord btnY = 40, btnH = 26;
    mLoadBtn = new CTextButton(CRect(10, btnY, 110, btnY + btnH), this, kTagLoad, "Load Sample");
    mLoadBtn->setStyle(CTextButton::kKickStyle);
    f->addView(mLoadBtn);

    mAnalyseBtn = new CTextButton(CRect(120, btnY, 220, btnY + btnH), this, kTagAnalyse, "Analyse");
    mAnalyseBtn->setStyle(CTextButton::kKickStyle);
    f->addView(mAnalyseBtn);

    mSelectAllBtn = new CTextButton(CRect(230, btnY, 320, btnY + btnH), this, kTagSelectAll, "All On");
    mSelectAllBtn->setStyle(CTextButton::kKickStyle);
    f->addView(mSelectAllBtn);

    mSelectNoneBtn = new CTextButton(CRect(330, btnY, 430, btnY + btnH), this, kTagSelectNone, "All Off");
    mSelectNoneBtn->setStyle(CTextButton::kKickStyle);
    f->addView(mSelectNoneBtn);

    // Knob row labels
    auto addSlider = [&](const char* label, CCoord x, int32_t tag, float defVal) {
        auto* lbl = new CTextLabel(CRect(x, btnY, x + 60, btnY + 12), label);
        lbl->setFontColor(kTextMuted); lbl->setFont(kNormalFontSmall, 9);
        lbl->setBackColor(kColorTransparent); lbl->setFrameColor(kColorTransparent);
        f->addView(lbl);
        auto* sl = new CSlider(CRect(x, btnY + 13, x + 60, btnY + btnH), this, tag, 0, 60, nullptr, nullptr);
        sl->setValue(defVal);
        f->addView(sl);
    };
    addSlider("Gain",    550, kTagGain,    1.0f);
    addSlider("Attack",  620, kTagAttack,  0.005f);
    addSlider("Release", 690, kTagRelease, 0.075f);

    // Spectrum view
    CCoord specY = 76, specH = 140;
    mSpectrumView = new SpectrumView(CRect(10, specY, W - 10, specY + specH));
    f->addView(mSpectrumView);

    // Partial grid (256 buttons)
    // Grid: starts at y=226, fills remaining height
    // 32 columns × 8 rows = 256 partials
    CCoord gridY = specY + specH + 8;
    CCoord gridW = W - 20;
    CCoord gridH = H - gridY - 10;
    int cols = 32, rows = MAX_PARTIALS / cols;
    CCoord cellW = gridW / cols;
    CCoord cellH = gridH / rows;

    mPartialButtons.resize(MAX_PARTIALS, nullptr);
    for (int i = 0; i < MAX_PARTIALS; i++) {
        int col = i % cols, row = i / cols;
        CCoord x = 10 + col * cellW, y = gridY + row * cellH;
        auto* btn = new PartialButton(CRect(x + 1, y + 1, x + cellW - 1, y + cellH - 1), i, this);
        f->addView(btn);
        mPartialButtons[i] = btn;
    }
}

void SinDecomposerEditor::valueChanged(CControl* control) {
    int32_t tag = control->getTag();
    float   val = control->getValue();

    if (tag == kTagLoad)        { onLoadSample(); return; }
    if (tag == kTagAnalyse)     { onAnalyse();    return; }
    if (tag == kTagSelectAll)   { onSelectAll(true);  return; }
    if (tag == kTagSelectNone)  { onSelectAll(false); return; }

    if (tag == kTagGain || tag == kTagAttack || tag == kTagRelease) {
        ParamID pid = (tag == kTagGain) ? kParamGain : (tag == kTagAttack) ? kParamAttack : kParamRelease;
        getController()->setParamNormalized(pid, val);
        if (auto* host = getController()->getComponentHandler())
            host->performEdit(pid, val);
        return;
    }

    if (tag >= kTagPartial0 && tag < kTagPartial0 + MAX_PARTIALS) {
        int idx  = tag - kTagPartial0;
        ParamID pid = kParamPartialBase + idx;
        getController()->setParamNormalized(pid, val);
        if (auto* host = getController()->getComponentHandler())
            host->performEdit(pid, val);
        // Update internal state
        if (idx < (int)mPartials.size()) mPartials[idx].enabled = (val > 0.5f);
    }
}

CMessageResult SinDecomposerEditor::notify(CBaseObject* sender, const char* msg) {
    if (std::string(msg) == CVSTGUITimer::kMsgTimer) {
        // Ask processor for analysis results via component handler
        // In a real plugin this would use IMessage/IConnectionPoint
        // For simplicity we poll via the controller's component state here
        // (A production plugin would implement IConnectionPoint messaging)
        return kMessageNotified;
    }
    return VSTGUIEditor::notify(sender, msg);
}

void SinDecomposerEditor::refreshPartialGrid() {
    for (int i = 0; i < MAX_PARTIALS; i++) {
        if (!mPartialButtons[i]) continue;
        auto* btn = static_cast<PartialButton*>(mPartialButtons[i]);
        if (i < (int)mPartials.size()) {
            btn->setPartial(mPartials[i]);
        } else {
            Partial empty; empty.frequency = 0; empty.amplitude = 0; empty.enabled = false;
            btn->setPartial(empty);
        }
    }
    if (mSpectrumView) {
        auto* sv = static_cast<SpectrumView*>(mSpectrumView);
        sv->setData(mSpectrum, mPartials);
    }
}

void SinDecomposerEditor::onLoadSample() {
    std::string path;

#if defined(__APPLE__)
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setAllowedFileTypes:@[@"wav", @"WAV"]];
    [panel setCanChooseFiles:YES];
    [panel setCanChooseDirectories:NO];
    if ([panel runModal] == NSModalResponseOK) {
        path = [[[panel URLs] firstObject] path].UTF8String;
    }
#elif defined(_WIN32)
    char buf[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.lpstrFilter  = "WAV Files\0*.wav\0All Files\0*.*\0";
    ofn.lpstrFile    = buf;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) path = buf;
#endif

    if (path.empty()) return;

    std::vector<float> mono;
    float sr = 44100.0f;
    if (!loadWAV(path, mono, sr)) {
        mStatusLabel->setText("Error: could not load WAV");
        return;
    }

    mLoadedFilePath = path;
    std::string name = path.substr(path.find_last_of("/\\") + 1);
    mStatusLabel->setText(("Loaded: " + name).c_str());

    // Send to processor via IMessage
    // In production: use IConnectionPoint to pass data cross-thread
    // For this build: store locally and trigger analysis
    // (The processor's loadSampleData is called via a custom message below)

    // Send a custom message to the processor
    if (auto* msg = getController()->allocateMessage()) {
        msg->setMessageID("LoadSample");
        // Encode audio data
        msg->getAttributes()->setBinary("data", mono.data(), (uint32_t)(mono.size() * sizeof(float)));
        msg->getAttributes()->setFloat("sr", sr);
        getController()->sendMessage(msg);
        msg->release();
    }
}

void SinDecomposerEditor::onAnalyse() {
    // Trigger analysis in processor
    getController()->setParamNormalized(kParamAnalyse, 1.0);
    if (auto* host = getController()->getComponentHandler())
        host->performEdit(kParamAnalyse, 1.0);
    getController()->setParamNormalized(kParamAnalyse, 0.0);

    mStatusLabel->setText("Analysing...");
    mAnalysisDirty = true;
}

void SinDecomposerEditor::onSelectAll(bool state) {
    for (int i = 0; i < (int)mPartials.size(); i++) {
        mPartials[i].enabled = state;
        if (mPartialButtons[i])
            static_cast<PartialButton*>(mPartialButtons[i])->setEnabled(state);
        float val = state ? 1.0f : 0.0f;
        ParamID pid = kParamPartialBase + i;
        getController()->setParamNormalized(pid, val);
        if (auto* host = getController()->getComponentHandler())
            host->performEdit(pid, val);
    }
}

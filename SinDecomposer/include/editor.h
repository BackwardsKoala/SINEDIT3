#pragma once
#include "public.sdk/source/vst/vstguieditor.h"
#include "plugids.h"
#include "partial.h"
#include <vector>
#include <string>

// Forward declare VSTGUI types
namespace VSTGUI {
    class CFrame;
    class CView;
    class CTextLabel;
    class CTextButton;
    class CSlider;
    class CVSTGUITimer;
}

class SinDecomposerEditor : public Steinberg::Vst::VSTGUIEditor,
                             public VSTGUI::IControlListener,
                             public VSTGUI::CBaseObject
{
public:
    explicit SinDecomposerEditor(Steinberg::Vst::EditController* controller);
    ~SinDecomposerEditor() override;

    bool PLUGIN_API open(void* parent, const VSTGUI::PlatformType& type) override;
    void PLUGIN_API close() override;

    // IControlListener
    void valueChanged(VSTGUI::CControl* control) override;

    // Timer callback for polling analysis results
    VSTGUI::CMessageResult notify(VSTGUI::CBaseObject* sender, const char* msg) override;

    static const VSTGUI::CCoord kEditorWidth  = 900;
    static const VSTGUI::CCoord kEditorHeight = 620;

private:
    void buildUI(VSTGUI::CFrame* frame);
    void refreshPartialGrid();
    void drawSpectrum();
    void onLoadSample();
    void onAnalyse();
    void onSelectAll(bool state);

    std::vector<Partial>         mPartials;
    std::vector<float>           mSpectrum;
    std::vector<VSTGUI::CView*>  mPartialButtons; // one per partial

    VSTGUI::CView*       mSpectrumView  = nullptr;
    VSTGUI::CTextLabel*  mStatusLabel   = nullptr;
    VSTGUI::CTextButton* mLoadBtn       = nullptr;
    VSTGUI::CTextButton* mAnalyseBtn    = nullptr;
    VSTGUI::CTextButton* mSelectAllBtn  = nullptr;
    VSTGUI::CTextButton* mSelectNoneBtn = nullptr;
    VSTGUI::CVSTGUITimer* mTimer        = nullptr;

    std::string mLoadedFilePath;
    bool        mAnalysisDirty = false;
};

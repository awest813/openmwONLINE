#ifndef OPENMW_MWGUI_PERFORMANCEOVERLAY_H
#define OPENMW_MWGUI_PERFORMANCEOVERLAY_H

#include "windowbase.hpp"

namespace MWGui
{
    class PerformanceOverlay : public WindowBase
    {
    public:
        PerformanceOverlay();

        void onFrame(float dt) override;

    private:
        MyGUI::TextBox* mFPS;
        MyGUI::TextBox* mFrameTime;
        MyGUI::TextBox* mDrawCalls;
        MyGUI::TextBox* mObjects;
        MyGUI::TextBox* mOccluded;
        MyGUI::TextBox* mTriangles;
        MyGUI::TextBox* mVRAM;
        MyGUI::TextBox* mCPUCull;
        MyGUI::TextBox* mCPUDraw;
        MyGUI::TextBox* mGPU;
        MyGUI::TextBox* mTerrain;
        MyGUI::TextBox* mStatus;
    };
}

#endif

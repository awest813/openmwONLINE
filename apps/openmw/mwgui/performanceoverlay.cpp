#include "performanceoverlay.hpp"
#include "../../../performance_toolkit/toolkit.hpp"
#include <iomanip>
#include <sstream>

namespace MWGui
{
    PerformanceOverlay::PerformanceOverlay()
        : WindowBase("openmw_performance_toolkit.layout")
    {
        getWidget(mFPS, "FPS");
        getWidget(mFrameTime, "FrameTime");
        getWidget(mDrawCalls, "DrawCalls");
        getWidget(mObjects, "Objects");
        getWidget(mOccluded, "Occluded");
        getWidget(mTriangles, "Triangles");
        getWidget(mVRAM, "VRAM");
        getWidget(mCPUCull, "CPU_Cull");
        getWidget(mGPU, "GPU");
        getWidget(mTerrain, "Terrain");
        getWidget(mCombat, "Combat");
        getWidget(mStatus, "Status");
        
        mMainWidget->setVisible(false);
    }

    void PerformanceOverlay::onFrame(float dt)
    {
        auto& toolkit = PerformanceToolkit::Toolkit::getInstance();
        if (!toolkit.isOverlayEnabled())
        {
            if (mMainWidget->getVisible())
                mMainWidget->setVisible(false);
            return;
        }

        if (!mMainWidget->getVisible())
            mMainWidget->setVisible(true);

        const auto& stats = toolkit.getLiveStats();

        std::stringstream ss;
        ss << std::fixed << std::setprecision(1);

        ss.str(""); ss << "FPS: " << stats.fps;
        mFPS->setCaption(ss.str());
        if (stats.fps < 30.0f) mFPS->setTextColour(MyGUI::Colour::Red);
        else if (stats.fps < 60.0f) mFPS->setTextColour(MyGUI::Colour(1.0f, 0.8f, 0.0f)); 
        else mFPS->setTextColour(MyGUI::Colour::Green);

        ss.str(""); ss << "Frame: " << stats.frameTime << " ms";
        mFrameTime->setCaption(ss.str());
        mFrameTime->setTextColour(stats.frameTime > 33.3f ? MyGUI::Colour::Red : MyGUI::Colour::White);

        ss.str(""); ss << "Draw Calls: " << stats.drawCalls;
        mDrawCalls->setCaption(ss.str());

        ss.str(""); ss << "Objects: " << stats.visibleObjects;
        mObjects->setCaption(ss.str());

        ss.str(""); ss << "Occluded: " << stats.occludedObjects;
        mOccluded->setCaption(ss.str());

        ss.str(""); ss << "Triangles: " << (stats.triangleCount / 1000) << "k";
        mTriangles->setCaption(ss.str());
        mTriangles->setTextColour(stats.triangleCount > 2000000 ? MyGUI::Colour::Red : MyGUI::Colour::White);

        ss.str(""); ss << "VRAM: " << std::fixed << std::setprecision(1) << stats.textureMemoryMB << " MB";
        mVRAM->setCaption(ss.str());
        mVRAM->setTextColour(stats.textureMemoryMB > 512.0f ? MyGUI::Colour::Red : MyGUI::Colour::White);

        ss.str(""); ss << "Cull: " << stats.cpuCullTime << " ms";
        mCPUCull->setCaption(ss.str());

        ss.str(""); ss << "Draw: " << stats.cpuDrawTime << " ms";
        mCPUDraw->setCaption(ss.str());

        ss.str(""); ss << "GPU: " << stats.gpuTime << " ms";
        mGPU->setCaption(ss.str());

        ss.str(""); ss << "Terrain: " << stats.terrainChunks << " Chk | " << stats.terrainNodes << " N";
        mTerrain->setCaption(ss.str());

        ss.str(""); ss << "Combat: " << stats.combatTicks << " Tick | " << stats.pathfindUpdates << " PF";
        mCombat->setCaption(ss.str());

        // Status Update
        if (toolkit.isBenchmarking())
        {
            mStatus->setCaption("STATUS: BENCHMARKING...");
            mStatus->setTextColour(MyGUI::Colour(1.0f, 0.5f, 0.0f)); // Orange
        }
        else
        {
            mStatus->setCaption("STATUS: MONITORING");
            mStatus->setTextColour(MyGUI::Colour(0.4f, 0.4f, 0.4f)); // Gray
        }
    }
}

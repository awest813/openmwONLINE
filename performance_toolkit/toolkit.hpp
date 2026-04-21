#ifndef PERFORMANCE_TOOLKIT_HPP
#define PERFORMANCE_TOOLKIT_HPP

#include <memory>
#include <string>
#include <vector>
#include <filesystem>

namespace osg { class Stats; }

namespace PerformanceToolkit
{
    struct LiveStats
    {
        float fps;
        float frameTime;
        unsigned int drawCalls;
        unsigned int visibleObjects;
        unsigned int occludedObjects;
        unsigned int triangleCount;
        float textureMemoryMB;
        unsigned int terrainNodes;
        unsigned int terrainChunks;
        unsigned int terrainCompositeCount;
        unsigned int combatTicks;
        unsigned int pathfindUpdates;
        double cpuCullTime;
        double cpuDrawTime;
        double gpuTime;
    };

    struct FrameStats
    {
        float dt;
        unsigned int drawCalls;
        unsigned int visibleObjects;
        unsigned int terrainNodes;
        unsigned int terrainChunks;
        unsigned int terrainCompositeCount;
        double cpuMainTime;
        double cpuCullTime;
        double cpuDrawTime;
        double gpuTime;
    };

    struct BenchmarkResult
    {
        std::string sceneName;
        float avgFps;
        float minFps;
        float avgFrameTime;
        float maxFrameTime;
        float p99FrameTime;
        unsigned int totalFrames;
        std::vector<FrameStats> frames;
    };

    class Toolkit
    {
    public:
        static Toolkit& getInstance();

        void update(float dt);
        void reportStats(unsigned int frameNumber, osg::Stats& stats);

        // Benchmarking
        void startBenchmark(const std::string& sceneName, float duration);
        bool isBenchmarking() const { return mIsBenchmarking; }
        
        // Debug HUD
        void toggleOverlay(bool enable) { mOverlayEnabled = enable; }
        bool isOverlayEnabled() const { return mOverlayEnabled; }
        const LiveStats& getLiveStats() const { return mLiveStats; }
        LiveStats& getLiveStatsMutable() { return mLiveStats; }

    private:
        Toolkit();
        ~Toolkit();

        bool mIsBenchmarking = false;
        std::string mCurrentBenchmarkScene;
        float mBenchmarkTimer = 0.f;
        float mBenchmarkDuration = 0.f;
        
        bool mOverlayEnabled = false;
        LiveStats mLiveStats;

        std::vector<FrameStats> mBenchmarkFrames;
        unsigned int mFrameCount = 0;

        // Phase 3: Automation
        float mAutoBenchmarkThreshold = 30.0f; // Trigger if < 30 FPS
        float mLowFrameTimer = 0.0f;
        bool mAutoBenchmarkEnabled = true;

        void finalizeBenchmark();
    };
}

#endif

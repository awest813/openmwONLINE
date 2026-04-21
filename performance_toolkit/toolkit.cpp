#include "toolkit.hpp"
#include "occlusion/occlusion_system.hpp"
#include <osg/Stats>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace PerformanceToolkit
{
    Toolkit& Toolkit::getInstance()
    {
        static Toolkit instance;
        return instance;
    }

    Toolkit::Toolkit() {}
    Toolkit::~Toolkit() {}

    void Toolkit::update(float dt)
    {
        OcclusionSystem::getInstance().update();

        if (mIsBenchmarking)
        {
            mBenchmarkTimer += dt;
            if (mBenchmarkTimer >= mBenchmarkDuration)
                finalizeBenchmark();
        }
        else if (mAutoBenchmarkEnabled && mLiveStats.fps > 0.0f && mLiveStats.fps < mAutoBenchmarkThreshold)
        {
            mLowFrameTimer += dt;
            if (mLowFrameTimer >= 2.0f) // Sustained low performance for 2 seconds
            {
                startBenchmark("AutoTriggered_LowFPS", 10.0f);
                mLowFrameTimer = 0.0f;
            }
        }
        else
        {
            mLowFrameTimer = 0.0f;
        }
    }

    void Toolkit::reportStats(unsigned int frameNumber, osg::Stats& stats)
    {
        double dt = 0, cullTime = 0, drawTime = 0, gpuTime = 0;
        stats.getAttribute(frameNumber, "Frame duration", dt);
        stats.getAttribute(frameNumber, "Cull traversal duration", cullTime);
        stats.getAttribute(frameNumber, "Draw traversal duration", drawTime);
        stats.getAttribute(frameNumber, "GPU draw traversal duration", gpuTime);

        double drawCalls = 0, glPrimitives = 0;
        stats.getAttribute(frameNumber, "Draw calls", drawCalls);
        stats.getAttribute(frameNumber, "Visible objects", glPrimitives);

        if (mIsBenchmarking)
        {
            FrameStats fs;
            fs.dt = static_cast<float>(dt);
            fs.cpuCullTime = cullTime;
            fs.cpuDrawTime = drawTime;
            fs.gpuTime = gpuTime;
            fs.drawCalls = static_cast<unsigned int>(drawCalls);
            fs.visibleObjects = static_cast<unsigned int>(glPrimitives);

            mBenchmarkFrames.push_back(fs);
        }

        if (mOverlayEnabled)
        {
            mLiveStats.frameTime = static_cast<float>(dt);
            mLiveStats.fps = (dt > 0) ? 1.0f / static_cast<float>(dt) : 0.0f;
            mLiveStats.drawCalls = static_cast<unsigned int>(drawCalls);
            mLiveStats.visibleObjects = static_cast<unsigned int>(glPrimitives);
            mLiveStats.occludedObjects = OcclusionSystem::getInstance().getOccludedCount();
            
            // Phase 2: Mesh & Texture Budgets
            // In a real run, we'd only do this when the cell changes
            mLiveStats.triangleCount = 1200000; // Placeholder for demo
            mLiveStats.textureMemoryMB = 256.4f; // Placeholder for demo

            mLiveStats.cpuCullTime = cullTime;
            mLiveStats.cpuDrawTime = drawTime;
            mLiveStats.gpuTime = gpuTime;
        }
    }

    void Toolkit::startBenchmark(const std::string& sceneName, float duration)
    {
        mCurrentBenchmarkScene = sceneName;
        mBenchmarkDuration = duration;
        mBenchmarkTimer = 0.f;
        mBenchmarkFrames.clear();
        mIsBenchmarking = true;
        std::cout << "Starting benchmark: " << sceneName << " for " << duration << "s" << std::endl;
    }

    void Toolkit::finalizeBenchmark()
    {
        mIsBenchmarking = false;
        
        if (mBenchmarkFrames.empty())
        {
            std::cout << "Benchmark finished with no data." << std::endl;
            return;
        }

        BenchmarkResult res;
        res.sceneName = mCurrentBenchmarkScene;
        res.totalFrames = mBenchmarkFrames.size();
        
        float totalTime = 0;
        res.minFps = 1e10f;
        res.maxFrameTime = 0;
        
        std::vector<float> dts;
        for (const auto& f : mBenchmarkFrames)
        {
            totalTime += f.dt;
            dts.push_back(f.dt);
            if (f.dt > 0)
                res.minFps = std::min(res.minFps, 1.0f / f.dt);
            res.maxFrameTime = std::max(res.maxFrameTime, f.dt);
        }
        
        res.avgFrameTime = totalTime / res.totalFrames;
        res.avgFps = 1.0f / res.avgFrameTime;
        
        std::sort(dts.begin(), dts.end());
        res.p99FrameTime = dts[static_cast<size_t>(dts.size() * 0.99)];

        // Write to file
        std::string filename = "benchmark_" + mCurrentBenchmarkScene + ".json";
        std::ofstream ofs(filename);
        ofs << "{\n";
        ofs << "  \"scene\": \"" << res.sceneName << "\",\n";
        ofs << "  \"avgFps\": " << res.avgFps << ",\n";
        ofs << "  \"minFps\": " << res.minFps << ",\n";
        ofs << "  \"avgFrameTime\": " << res.avgFrameTime << ",\n";
        ofs << "  \"maxFrameTime\": " << res.maxFrameTime << ",\n";
        ofs << "  \"p99FrameTime\": " << res.p99FrameTime << ",\n";
        ofs << "  \"totalFrames\": " << res.totalFrames << "\n";
        ofs << "}\n";
        
        std::cout << "Benchmark results saved to " << filename << std::endl;
    }
}

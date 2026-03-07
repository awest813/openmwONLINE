#include "resourcesystem.hpp"

#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <components/debug/debuglog.hpp>
#endif

#include "animblendrulesmanager.hpp"
#include "bgsmfilemanager.hpp"
#include "imagemanager.hpp"
#include "keyframemanager.hpp"
#include "niffilemanager.hpp"
#include "scenemanager.hpp"

namespace Resource
{

    ResourceSystem::ResourceSystem(
        const VFS::Manager* vfs, double expiryDelay, const ToUTF8::StatelessUtf8Encoder* encoder)
        : mVFS(vfs)
    {
        mNifFileManager = std::make_unique<NifFileManager>(vfs, encoder);
        mBgsmFileManager = std::make_unique<BgsmFileManager>(vfs, expiryDelay);
        mImageManager = std::make_unique<ImageManager>(vfs, expiryDelay);
        mSceneManager = std::make_unique<SceneManager>(
            vfs, mImageManager.get(), mNifFileManager.get(), mBgsmFileManager.get(), expiryDelay);
        mKeyframeManager = std::make_unique<KeyframeManager>(vfs, mSceneManager.get(), expiryDelay, encoder);
        mAnimBlendRulesManager = std::make_unique<AnimBlendRulesManager>(vfs, expiryDelay);

        addResourceManager(mNifFileManager.get());
        addResourceManager(mBgsmFileManager.get());
        addResourceManager(mKeyframeManager.get());
        // note, scene references images so add images afterwards for correct implementation of updateCache()
        addResourceManager(mSceneManager.get());
        addResourceManager(mImageManager.get());
        addResourceManager(mAnimBlendRulesManager.get());
    }

    ResourceSystem::~ResourceSystem()
    {
        // this has to be defined in the .cpp file as we can't delete incomplete types

        mResourceManagers.clear();
    }

    SceneManager* ResourceSystem::getSceneManager()
    {
        return mSceneManager.get();
    }

    ImageManager* ResourceSystem::getImageManager()
    {
        return mImageManager.get();
    }

    BgsmFileManager* ResourceSystem::getBgsmFileManager()
    {
        return mBgsmFileManager.get();
    }

    NifFileManager* ResourceSystem::getNifFileManager()
    {
        return mNifFileManager.get();
    }

    KeyframeManager* ResourceSystem::getKeyframeManager()
    {
        return mKeyframeManager.get();
    }

    AnimBlendRulesManager* ResourceSystem::getAnimBlendRulesManager()
    {
        return mAnimBlendRulesManager.get();
    }

    void ResourceSystem::setExpiryDelay(double expiryDelay)
    {
        for (std::vector<BaseResourceManager*>::iterator it = mResourceManagers.begin(); it != mResourceManagers.end();
             ++it)
            (*it)->setExpiryDelay(expiryDelay);

        // NIF files aren't needed any more once the converted objects are cached in SceneManager / BulletShapeManager,
        // so no point in using an expiry delay
        mNifFileManager->setExpiryDelay(0.0);
    }

    void ResourceSystem::updateCache(double referenceTime)
    {
#ifdef __EMSCRIPTEN__
        // Under memory pressure in the WASM build, use a shorter expiry delay for this purge
        // cycle so that stale assets are reclaimed sooner.  The WASM heap is limited (typically
        // 2 GiB); when more than 75% of the configured ceiling is in use we halve the configured
        // expiry to free memory more aggressively, logging once per pressure episode.
        static constexpr double sWasmPressureThreshold = 0.75;
        static constexpr double sWasmPressureMultiplier = 0.5;
        // Use 2 GiB as the reference ceiling (Emscripten's ALLOW_MEMORY_GROWTH default cap).
        // EM_ASM_INT is not safe from worker threads, so we use a compile-time constant.
        static constexpr size_t sWasmMaxBytes = 2ULL * 1024 * 1024 * 1024;
        static bool sPressureLogged = false;

        // __builtin_wasm_memory_size(0) returns the current allocated memory in 64 KiB pages.
        const size_t usedBytes = static_cast<size_t>(__builtin_wasm_memory_size(0)) * 65536u;
        const bool underPressure = (double)usedBytes / (double)sWasmMaxBytes > sWasmPressureThreshold;

        if (underPressure)
        {
            if (!sPressureLogged)
            {
                Log(Debug::Verbose) << "ResourceSystem: WASM heap pressure detected ("
                                    << (usedBytes / (1024 * 1024)) << " / " << (sWasmMaxBytes / (1024 * 1024))
                                    << " MiB used); halving cache expiry delay this cycle";
                sPressureLogged = true;
            }
            // Temporarily apply the shorter expiry to all non-NIF managers for this single purge.
            std::vector<double> savedDelays;
            savedDelays.reserve(mResourceManagers.size());
            for (BaseResourceManager* mgr : mResourceManagers)
            {
                savedDelays.push_back(mgr->getExpiryDelay());
                if (mgr->getExpiryDelay() > 0.0)
                    mgr->setExpiryDelay(mgr->getExpiryDelay() * sWasmPressureMultiplier);
            }
            for (BaseResourceManager* mgr : mResourceManagers)
                mgr->updateCache(referenceTime);
            // Restore configured delays.
            for (size_t i = 0; i < mResourceManagers.size(); ++i)
                mResourceManagers[i]->setExpiryDelay(savedDelays[i]);
            return;
        }
        sPressureLogged = false;
#endif
        for (std::vector<BaseResourceManager*>::iterator it = mResourceManagers.begin(); it != mResourceManagers.end();
             ++it)
            (*it)->updateCache(referenceTime);
    }

    void ResourceSystem::clearCache()
    {
        for (std::vector<BaseResourceManager*>::iterator it = mResourceManagers.begin(); it != mResourceManagers.end();
             ++it)
            (*it)->clearCache();
    }

    void ResourceSystem::addResourceManager(BaseResourceManager* resourceMgr)
    {
        mResourceManagers.push_back(resourceMgr);
    }

    void ResourceSystem::removeResourceManager(BaseResourceManager* resourceMgr)
    {
        std::vector<BaseResourceManager*>::iterator found
            = std::find(mResourceManagers.begin(), mResourceManagers.end(), resourceMgr);
        if (found != mResourceManagers.end())
            mResourceManagers.erase(found);
    }

    const VFS::Manager* ResourceSystem::getVFS() const
    {
        return mVFS;
    }

    void ResourceSystem::reportStats(unsigned int frameNumber, osg::Stats* stats) const
    {
        for (std::vector<BaseResourceManager*>::const_iterator it = mResourceManagers.begin();
             it != mResourceManagers.end(); ++it)
            (*it)->reportStats(frameNumber, stats);
    }

    void ResourceSystem::releaseGLObjects(osg::State* state)
    {
        for (std::vector<BaseResourceManager*>::const_iterator it = mResourceManagers.begin();
             it != mResourceManagers.end(); ++it)
            (*it)->releaseGLObjects(state);
    }

}

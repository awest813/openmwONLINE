#ifndef COMPONENTS_TERRAIN_BUFFERCACHE_H
#define COMPONENTS_TERRAIN_BUFFERCACHE_H

#include <osg/Array>
#include <osg/PrimitiveSet>
#include <osg/ref_ptr>

#include <map>
#include <mutex>

namespace Terrain
{

    /// @brief Implements creation and caching of vertex buffers for terrain chunks.
    class BufferCache
    {
    public:
        /// @param flags first 4*4 bits are LOD deltas on each edge, respectively (4 bits each)
        ///              next 4 bits are LOD level of the index buffer (LOD 0 = don't omit any vertices)
        /// @note Thread safe.
        osg::ref_ptr<osg::DrawElements> getIndexBuffer(unsigned int numVerts, unsigned int flags);

        /// @note Thread safe.
        osg::ref_ptr<osg::Vec2Array> getUVBuffer(unsigned int numVerts);

        // Milestone 2: Array Recycling
        osg::ref_ptr<osg::Vec3Array> takeVec3Array(size_t size);
        void returnVec3Array(osg::ref_ptr<osg::Vec3Array> array);
        
        osg::ref_ptr<osg::Vec4ubArray> takeVec4ubArray(size_t size);
        void returnVec4ubArray(osg::ref_ptr<osg::Vec4ubArray> array);

        void clearCache();

        void releaseGLObjects(osg::State* state);

    private:
        // Index buffers are shared across terrain batches where possible. There is one index buffer for each
        // combination of LOD deltas and index buffer LOD we may need.
        std::map<std::pair<int, int>, osg::ref_ptr<osg::DrawElements>> mIndexBufferMap;
        std::mutex mIndexBufferMutex;

        std::map<int, osg::ref_ptr<osg::Vec2Array>> mUvBufferMap;
        std::mutex mUvBufferMutex;

        // Milestone 2 Pools
        std::map<size_t, std::vector<osg::ref_ptr<osg::Vec3Array>>> mVec3Pool;
        std::map<size_t, std::vector<osg::ref_ptr<osg::Vec4ubArray>>> mVec4ubPool;
        std::mutex mPoolMutex;
    };

}

#endif

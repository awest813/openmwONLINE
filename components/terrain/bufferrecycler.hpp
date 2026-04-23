#ifndef OPENMW_COMPONENTS_TERRAIN_BUFFERRECYCLER_H
#define OPENMW_COMPONENTS_TERRAIN_BUFFERRECYCLER_H

#include <osg/Array>
#include <osg/ref_ptr>

namespace Terrain
{
    class BufferRecycler
    {
    public:
        virtual ~BufferRecycler() = default;
        virtual void returnVec3Array(osg::ref_ptr<osg::Vec3Array> array) = 0;
        virtual void returnVec4ubArray(osg::ref_ptr<osg::Vec4ubArray> array) = 0;
    };
}

#endif

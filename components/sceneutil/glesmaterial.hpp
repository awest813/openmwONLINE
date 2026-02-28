#ifndef OPENMW_COMPONENTS_SCENEUTIL_GLESMATERIAL_H
#define OPENMW_COMPONENTS_SCENEUTIL_GLESMATERIAL_H

#ifdef __EMSCRIPTEN__

#include <osg/Material>
#include <osg/NodeCallback>
#include <osg/StateSet>

namespace SceneUtil
{
    void addGLESMaterialUniforms(osg::StateSet* stateSet);
    void updateGLESMaterialUniforms(osg::StateSet* stateSet, const osg::Material* material);
    void addGLESFogUniforms(osg::StateSet* stateSet);
}

#endif

#endif

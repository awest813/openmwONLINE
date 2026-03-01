#ifndef OPENMW_COMPONENTS_SCENEUTIL_GLES3UNIFORMS_H
#define OPENMW_COMPONENTS_SCENEUTIL_GLES3UNIFORMS_H

#ifdef __EMSCRIPTEN__

#include <osg/Material>
#include <osg/StateSet>
#include <osg/TexMat>
#include <osg/Uniform>

namespace SceneUtil
{
    /// Utilities for providing fixed-function state as uniforms in GLES3/WebGL 2.0 builds.
    /// When OSG is built with GLES3 profile (OSG_GL_FIXED_FUNCTION_AVAILABLE=OFF),
    /// gl_FrontMaterial, gl_Fog, and gl_TextureMatrix are unavailable. These functions
    /// mirror the corresponding osg::StateAttribute values as custom uniforms that the
    /// transformed GLSL ES 3.00 shaders expect.
    namespace GLES3Uniforms
    {
        /// Apply material properties from an osg::Material as omw_FrontMaterial.* uniforms.
        /// Should be called whenever a Material state attribute changes.
        void applyMaterial(osg::StateSet* stateset, const osg::Material* material);

        /// Apply default material uniforms (white diffuse, no emission, no specular).
        void applyDefaultMaterial(osg::StateSet* stateset);

        /// Apply fog parameters as omw_Fog.* uniforms.
        void applyFog(osg::StateSet* stateset, const osg::Vec4f& color, float start, float end);

        /// Apply default fog uniforms.
        void applyDefaultFog(osg::StateSet* stateset);

        /// Apply a texture matrix as omw_TextureMatrixN uniform.
        void applyTextureMatrix(osg::StateSet* stateset, unsigned int unit, const osg::TexMat* texMat);

        /// Apply identity texture matrices for all units.
        void applyDefaultTextureMatrices(osg::StateSet* stateset);

        /// Apply all default uniforms (material, fog, texture matrices) to a root state set.
        void applyAllDefaults(osg::StateSet* stateset);
    }
}

#endif // __EMSCRIPTEN__

#endif // OPENMW_COMPONENTS_SCENEUTIL_GLES3UNIFORMS_H

#include "gles3uniforms.hpp"

#ifdef __EMSCRIPTEN__

#include <osg/Matrixf>

namespace SceneUtil
{
    namespace GLES3Uniforms
    {
        void applyMaterial(osg::StateSet* stateset, const osg::Material* material)
        {
            if (!stateset || !material)
                return;

            const osg::Material::Face face = osg::Material::FRONT;
            const osg::Vec4f& emission = material->getEmission(face);
            const osg::Vec4f& ambient = material->getAmbient(face);
            const osg::Vec4f& diffuse = material->getDiffuse(face);
            const osg::Vec4f& specular = material->getSpecular(face);
            float shininess = material->getShininess(face);

            stateset->addUniform(new osg::Uniform("omw_FrontMaterial.emission", emission));
            stateset->addUniform(new osg::Uniform("omw_FrontMaterial.ambient", ambient));
            stateset->addUniform(new osg::Uniform("omw_FrontMaterial.diffuse", diffuse));
            stateset->addUniform(new osg::Uniform("omw_FrontMaterial.specular", specular));
            stateset->addUniform(new osg::Uniform("omw_FrontMaterial.shininess", shininess));
        }

        void applyDefaultMaterial(osg::StateSet* stateset)
        {
            if (!stateset)
                return;

            // OpenGL default material values
            stateset->addUniform(new osg::Uniform("omw_FrontMaterial.emission", osg::Vec4f(0.0f, 0.0f, 0.0f, 1.0f)));
            stateset->addUniform(new osg::Uniform("omw_FrontMaterial.ambient", osg::Vec4f(0.2f, 0.2f, 0.2f, 1.0f)));
            stateset->addUniform(new osg::Uniform("omw_FrontMaterial.diffuse", osg::Vec4f(0.8f, 0.8f, 0.8f, 1.0f)));
            stateset->addUniform(new osg::Uniform("omw_FrontMaterial.specular", osg::Vec4f(0.0f, 0.0f, 0.0f, 1.0f)));
            stateset->addUniform(new osg::Uniform("omw_FrontMaterial.shininess", 0.0f));
        }

        void applyFog(osg::StateSet* stateset, const osg::Vec4f& color, float start, float end)
        {
            if (!stateset)
                return;

            float scale = (end > start) ? 1.0f / (end - start) : 0.0f;

            stateset->addUniform(new osg::Uniform("omw_Fog.color", color));
            stateset->addUniform(new osg::Uniform("omw_Fog.density", 1.0f));
            stateset->addUniform(new osg::Uniform("omw_Fog.start", start));
            stateset->addUniform(new osg::Uniform("omw_Fog.end", end));
            stateset->addUniform(new osg::Uniform("omw_Fog.scale", scale));
        }

        void applyDefaultFog(osg::StateSet* stateset)
        {
            if (!stateset)
                return;

            // Default fog: very far, white
            applyFog(stateset, osg::Vec4f(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 10000.0f);
        }

        void applyTextureMatrix(osg::StateSet* stateset, unsigned int unit, const osg::TexMat* texMat)
        {
            if (!stateset || unit > 7)
                return;

            std::string name = "omw_TextureMatrix" + std::to_string(unit);
            osg::Matrixf mat = texMat ? texMat->getMatrix() : osg::Matrixf::identity();
            stateset->addUniform(new osg::Uniform(name.c_str(), mat));
        }

        void applyDefaultTextureMatrices(osg::StateSet* stateset)
        {
            if (!stateset)
                return;

            osg::Matrixf identity = osg::Matrixf::identity();
            for (unsigned int i = 0; i < 8; ++i)
            {
                std::string name = "omw_TextureMatrix" + std::to_string(i);
                stateset->addUniform(new osg::Uniform(name.c_str(), identity));
            }
        }

        void applyLightModel(osg::StateSet* stateset, const osg::Vec4f& ambient)
        {
            if (!stateset)
                return;

            stateset->addUniform(new osg::Uniform("omw_LightModel.ambient", ambient));
        }

        void applyDefaultLightModel(osg::StateSet* stateset)
        {
            if (!stateset)
                return;

            applyLightModel(stateset, osg::Vec4f(0.2f, 0.2f, 0.2f, 1.0f));
        }

        void applyAllDefaults(osg::StateSet* stateset)
        {
            if (!stateset)
                return;

            applyDefaultMaterial(stateset);
            applyDefaultFog(stateset);
            applyDefaultTextureMatrices(stateset);
            applyDefaultLightModel(stateset);
        }
    }
}

#endif // __EMSCRIPTEN__

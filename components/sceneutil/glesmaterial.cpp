#ifdef __EMSCRIPTEN__

#include "glesmaterial.hpp"

#include <osg/Fog>
#include <osg/Uniform>

namespace SceneUtil
{
    void addGLESMaterialUniforms(osg::StateSet* stateSet)
    {
        stateSet->addUniform(new osg::Uniform("osg_FrontMaterialEmission", osg::Vec4f(0, 0, 0, 1)));
        stateSet->addUniform(new osg::Uniform("osg_FrontMaterialAmbient", osg::Vec4f(0.2f, 0.2f, 0.2f, 1)));
        stateSet->addUniform(new osg::Uniform("osg_FrontMaterialDiffuse", osg::Vec4f(0.8f, 0.8f, 0.8f, 1)));
        stateSet->addUniform(new osg::Uniform("osg_FrontMaterialSpecular", osg::Vec4f(0, 0, 0, 1)));
        stateSet->addUniform(new osg::Uniform("osg_FrontMaterialShininess", 0.0f));
    }

    void updateGLESMaterialUniforms(osg::StateSet* stateSet, const osg::Material* material)
    {
        if (!material || !stateSet)
            return;

        if (auto* u = stateSet->getUniform("osg_FrontMaterialEmission"))
            u->set(material->getEmission(osg::Material::FRONT));
        if (auto* u = stateSet->getUniform("osg_FrontMaterialAmbient"))
            u->set(material->getAmbient(osg::Material::FRONT));
        if (auto* u = stateSet->getUniform("osg_FrontMaterialDiffuse"))
            u->set(material->getDiffuse(osg::Material::FRONT));
        if (auto* u = stateSet->getUniform("osg_FrontMaterialSpecular"))
            u->set(material->getSpecular(osg::Material::FRONT));
        if (auto* u = stateSet->getUniform("osg_FrontMaterialShininess"))
            u->set(material->getShininess(osg::Material::FRONT));
    }

    void addGLESFogUniforms(osg::StateSet* stateSet)
    {
        stateSet->addUniform(new osg::Uniform("osg_FogColor", osg::Vec4f(1, 1, 1, 1)));
        stateSet->addUniform(new osg::Uniform("osg_FogStart", 0.0f));
        stateSet->addUniform(new osg::Uniform("osg_FogEnd", 1.0f));
        stateSet->addUniform(new osg::Uniform("osg_FogScale", 1.0f));
        stateSet->addUniform(new osg::Uniform("osg_LightModelAmbient", osg::Vec4f(0.2f, 0.2f, 0.2f, 1.0f)));
    }
}

#endif

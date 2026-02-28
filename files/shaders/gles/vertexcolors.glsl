centroid in vec4 passColor;

uniform int colorMode;

uniform vec4 osg_FrontMaterialEmission;
uniform vec4 osg_FrontMaterialAmbient;
uniform vec4 osg_FrontMaterialDiffuse;
uniform vec4 osg_FrontMaterialSpecular;

const int ColorMode_None = 0;
const int ColorMode_Emission = 1;
const int ColorMode_AmbientAndDiffuse = 2;
const int ColorMode_Ambient = 3;
const int ColorMode_Diffuse = 4;
const int ColorMode_Specular = 5;

vec4 getEmissionColor()
{
    if (colorMode == ColorMode_Emission)
        return passColor;
    return osg_FrontMaterialEmission;
}

vec4 getAmbientColor()
{
    if (colorMode == ColorMode_AmbientAndDiffuse || colorMode == ColorMode_Ambient)
        return passColor;
    return osg_FrontMaterialAmbient;
}

vec4 getDiffuseColor()
{
    if (colorMode == ColorMode_AmbientAndDiffuse || colorMode == ColorMode_Diffuse)
        return passColor;
    return osg_FrontMaterialDiffuse;
}

vec4 getSpecularColor()
{
    if (colorMode == ColorMode_Specular)
        return passColor;
    return osg_FrontMaterialSpecular;
}

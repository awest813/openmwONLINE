#version 300 es
precision highp float;

uniform mat4 osg_ModelViewProjectionMatrix;
uniform mat4 osg_ModelViewMatrix;
uniform mat4 osg_TextureMatrix0;
uniform vec4 osg_FrontMaterialDiffuse;

in vec4 osg_Vertex;
in vec4 osg_Color;
in vec4 osg_MultiTexCoord0;

out vec2 diffuseMapUV;
out float alphaPassthrough;

uniform int colorMode;
uniform bool useTreeAnim;
uniform bool useDiffuseMapForShadowAlpha;
uniform bool alphaTestShadows;

void main(void)
{
    gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;

    if (useDiffuseMapForShadowAlpha)
        diffuseMapUV = (osg_TextureMatrix0 * osg_MultiTexCoord0).xy;
    else
        diffuseMapUV = vec2(0.0);
    if (colorMode == 2)
        alphaPassthrough = useTreeAnim ? 1.0 : osg_Color.a;
    else
        alphaPassthrough = osg_FrontMaterialDiffuse.a;
}

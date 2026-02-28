#version 300 es
precision highp float;

in vec4 osg_Vertex;
in vec4 osg_Color;
in vec4 osg_MultiTexCoord0;

uniform mat4 osg_TextureMatrix0;
uniform vec4 osg_FrontMaterialDiffuse;

out vec2 diffuseMapUV;
out float alphaPassthrough;

#include "lib/core/vertex.h.glsl"
#include "gles/vertexcolors.glsl"

void main()
{
    gl_Position = modelToClip(osg_Vertex);

    if (colorMode == 2)
        alphaPassthrough = osg_Color.a;
    else
        alphaPassthrough = osg_FrontMaterialDiffuse.a;

    diffuseMapUV = (osg_TextureMatrix0 * osg_MultiTexCoord0).xy;
}

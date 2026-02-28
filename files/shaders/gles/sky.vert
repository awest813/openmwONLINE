#version 300 es
precision highp float;

#include "lib/core/vertex.h.glsl"

#include "lib/sky/passes.glsl"

uniform int pass;
uniform mat4 osg_TextureMatrix0;

in vec4 osg_Vertex;
in vec4 osg_Color;
in vec4 osg_MultiTexCoord0;

out vec4 passColor;
out vec2 diffuseMapUV;

void main()
{
    gl_Position = modelToClip(osg_Vertex);
    passColor = osg_Color;

    if (pass == PASS_CLOUDS)
        diffuseMapUV = (osg_TextureMatrix0 * osg_MultiTexCoord0).xy;
    else
        diffuseMapUV = osg_MultiTexCoord0.xy;
}

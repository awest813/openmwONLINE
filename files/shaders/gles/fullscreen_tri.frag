#version 300 es
precision highp float;

in vec2 uv;

#include "lib/core/fragment.h.glsl"

out vec4 osg_FragColor;

void main()
{
    osg_FragColor = samplerLastShader(uv);
}

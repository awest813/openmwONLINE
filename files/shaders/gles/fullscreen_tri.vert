#version 300 es
precision highp float;

uniform vec2 scaling;

in vec4 osg_Vertex;

out vec2 uv;

#include "lib/core/vertex.h.glsl"

void main()
{
    gl_Position = vec4(osg_Vertex.xy, 0.0, 1.0);
    uv = (gl_Position.xy * 0.5 + 0.5) * scaling;
}

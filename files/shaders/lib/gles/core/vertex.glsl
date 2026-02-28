#version 300 es
precision highp float;

#include "lib/core/vertex.h.glsl"

uniform mat4 projectionMatrix;
uniform mat4 osg_ModelViewMatrix;

vec4 modelToClip(vec4 pos)
{
    return projectionMatrix * modelToView(pos);
}

vec4 modelToView(vec4 pos)
{
    return osg_ModelViewMatrix * pos;
}

vec4 viewToClip(vec4 pos)
{
    return projectionMatrix * pos;
}

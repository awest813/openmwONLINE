#version 300 es
precision highp float;

#include "lib/luminance/constants.glsl"

in vec2 uv;
uniform sampler2D sceneTex;

out vec4 osg_FragColor;

void main()
{
    float lum = dot(texture(sceneTex, uv).rgb, vec3(0.2126, 0.7152, 0.0722));
    lum = max(lum, epsilon);

    osg_FragColor.r = clamp((log2(lum) - minLog) * invLogLumRange, 0.0, 1.0);
}

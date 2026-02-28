#version 300 es
precision highp float;

#include "lib/luminance/constants.glsl"

in vec2 uv;
uniform sampler2D luminanceSceneTex;
uniform sampler2D prevLuminanceSceneTex;

uniform float osg_DeltaFrameTime;

out vec4 osg_FragColor;

void main()
{
    float prevLum = texture(prevLuminanceSceneTex, vec2(0.5, 0.5)).r;
    float currLum = texture(luminanceSceneTex, vec2(0.5, 0.5)).r;

    float avgLum = exp2((currLum * logLumRange) + minLog);
    osg_FragColor.r = prevLum + (avgLum - prevLum) * (1.0 - exp(-osg_DeltaFrameTime * hdrExposureTime));
}

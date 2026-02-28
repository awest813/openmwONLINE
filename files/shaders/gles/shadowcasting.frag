#version 300 es
precision highp float;

uniform sampler2D diffuseMap;
in vec2 diffuseMapUV;

in float alphaPassthrough;

uniform bool useDiffuseMapForShadowAlpha;
uniform bool alphaTestShadows;
uniform float alphaRef;

#include "lib/material/alpha.glsl"

out vec4 osg_FragColor;

void main()
{
    osg_FragColor.rgb = vec3(1.0);
    if (useDiffuseMapForShadowAlpha)
        osg_FragColor.a = texture(diffuseMap, diffuseMapUV).a * alphaPassthrough;
    else
        osg_FragColor.a = alphaPassthrough;

    osg_FragColor.a = alphaTest(osg_FragColor.a, alphaRef);

    if (alphaTestShadows && osg_FragColor.a <= 0.5)
        discard;
}

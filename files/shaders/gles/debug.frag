#version 300 es
precision highp float;

#include "gles/vertexcolors.glsl"

in vec3 vertexNormal;

uniform bool useAdvancedShader;

out vec4 osg_FragColor;

void main()
{
    vec3 lightDir = normalize(vec3(-1.0, -0.5, -2.0));

    float lightAttenuation = dot(-lightDir, vertexNormal) * 0.5 + 0.5;

    if (!useAdvancedShader)
    {
        osg_FragColor = getDiffuseColor();
    }
    else
    {
        osg_FragColor = vec4(passColor.xyz * lightAttenuation, 1.0);
    }
}

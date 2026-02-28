#version 300 es
precision highp float;

uniform sampler2D diffuseMap;

in vec2 diffuseMapUV;
in float alphaPassthrough;

out vec4 osg_FragColor;

void main()
{
    float alpha = texture(diffuseMap, diffuseMapUV).a * alphaPassthrough;

    const float alphaRef = 0.499;

    if (alpha < alphaRef)
        discard;
}

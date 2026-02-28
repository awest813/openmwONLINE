#version 300 es
precision highp float;

uniform sampler2D diffuseMap;

in vec2 diffuseMapUV;
in vec4 passColor;

out vec4 osg_FragColor;

void main()
{
    osg_FragColor = texture(diffuseMap, diffuseMapUV) * passColor;
}

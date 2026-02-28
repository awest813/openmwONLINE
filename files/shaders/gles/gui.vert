#version 300 es
precision highp float;

uniform mat4 gl_ModelViewProjectionMatrix;
uniform mat4 osg_TextureMatrix0;

in vec4 osg_Vertex;
in vec4 osg_MultiTexCoord0;
in vec4 osg_Color;

out vec2 diffuseMapUV;
out vec4 passColor;

void main()
{
    gl_Position = vec4(osg_Vertex.xyz, 1.0);
    diffuseMapUV = (osg_TextureMatrix0 * osg_MultiTexCoord0).xy;
    passColor = osg_Color;
}

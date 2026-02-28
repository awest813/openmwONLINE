#version 300 es
precision highp float;

#include "lib/core/vertex.h.glsl"

uniform vec3 color;
uniform vec3 trans;
uniform vec3 scale;
uniform bool useNormalAsColor;
uniform bool useAdvancedShader;

in vec4 osg_Vertex;
in vec4 osg_Color;
in vec3 osg_Normal;

centroid out vec4 passColor;
out vec3 vertexNormal;

void main()
{
    if (!useAdvancedShader)
    {
        gl_Position = modelToClip(osg_Vertex);
        vertexNormal = vec3(1.0, 1.0, 1.0);
        passColor = osg_Color;
    }
    else
    {
        gl_Position = modelToClip(vec4(osg_Vertex.xyz * scale + trans, 1.0));

        vertexNormal = useNormalAsColor ? vec3(1.0, 1.0, 1.0) : osg_Normal;
        vec3 colorOut = useNormalAsColor ? osg_Normal : color;
        passColor = vec4(colorOut, 1.0);
    }
}

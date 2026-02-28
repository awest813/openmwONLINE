#version 300 es
precision highp float;

#include "lib/core/vertex.h.glsl"

out vec4  position;
out float linearDepth;

#include "gles/shadows_vertex.glsl"
#include "lib/view/depth.glsl"

uniform vec3 nodePosition;
uniform vec3 playerPos;
uniform mat3 osg_NormalMatrix;

in vec4 osg_Vertex;
in vec3 osg_Normal;

out vec3 worldPos;
out vec2 rippleMapUV;

void main(void)
{
    gl_Position = modelToClip(osg_Vertex);

    position = osg_Vertex;

    worldPos = position.xyz + nodePosition.xyz;
    rippleMapUV = (worldPos.xy - playerPos.xy + (@rippleMapSize * @rippleMapWorldScale / 2.0)) / @rippleMapSize / @rippleMapWorldScale;

    vec4 viewPos = modelToView(osg_Vertex);
    linearDepth = getLinearDepth(gl_Position.z, viewPos.z);

    setupShadowCoords(viewPos, normalize((osg_NormalMatrix * osg_Normal).xyz));
}

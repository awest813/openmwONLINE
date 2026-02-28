#version 300 es
precision highp float;

#define PER_PIXEL_LIGHTING 1

#include "lib/core/vertex.h.glsl"

in vec4 osg_Vertex;
in vec4 osg_Color;
in vec3 osg_Normal;
in vec4 osg_MultiTexCoord0;
in vec4 osg_MultiTexCoord7;

uniform mat3 osg_NormalMatrix;

#if @diffuseMap
uniform mat4 osg_TextureMatrix@diffuseMapUV;
out vec2 diffuseMapUV;
#endif

#if @emissiveMap
uniform mat4 osg_TextureMatrix@emissiveMapUV;
out vec2 emissiveMapUV;
#endif

#if @normalMap
uniform mat4 osg_TextureMatrix@normalMapUV;
out vec2 normalMapUV;
out vec4 passTangent;
#endif

out float euclideanDepth;
out float linearDepth;

out vec3 passViewPos;
out vec3 passNormal;

#include "lib/light/lighting.glsl"
#include "lib/view/depth.glsl"

#include "gles/vertexcolors.glsl"
#include "gles/shadows_vertex.glsl"
#include "gles/normals.glsl"

void main(void)
{
    gl_Position = modelToClip(osg_Vertex);

    vec4 viewPos = modelToView(osg_Vertex);
    euclideanDepth = length(viewPos.xyz);
    linearDepth = getLinearDepth(gl_Position.z, viewPos.z);
    passColor = osg_Color;
    passViewPos = viewPos.xyz;
    passNormal = osg_Normal.xyz;
    normalToViewMatrix = osg_NormalMatrix;

#if @normalMap
    normalToViewMatrix *= generateTangentSpace(osg_MultiTexCoord7.xyzw, passNormal);
#endif

#if @diffuseMap
    diffuseMapUV = (osg_TextureMatrix@diffuseMapUV * osg_MultiTexCoord@diffuseMapUV).xy;
#endif

#if @emissiveMap
    emissiveMapUV = (osg_TextureMatrix@emissiveMapUV * osg_MultiTexCoord@emissiveMapUV).xy;
#endif

#if @normalMap
    normalMapUV = (osg_TextureMatrix@normalMapUV * osg_MultiTexCoord@normalMapUV).xy;
#endif

#if @shadows_enabled
    vec3 viewNormal = normalize(osg_NormalMatrix * passNormal);
    setupShadowCoords(viewPos, viewNormal);
#endif
}

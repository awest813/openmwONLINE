#version 300 es
precision highp float;

#include "lib/core/vertex.h.glsl"

in vec4 osg_Vertex;
in vec4 osg_Color;
in vec3 osg_Normal;
in vec4 osg_MultiTexCoord0;

uniform mat3 osg_NormalMatrix;

#if @diffuseMap
uniform mat4 osg_TextureMatrix@diffuseMapUV;
out vec2 diffuseMapUV;
#endif

out vec3 passNormal;
out vec3 passViewPos;
out float euclideanDepth;
out float linearDepth;
out float passFalloff;

uniform bool useFalloff;
uniform vec4 falloffParams;

#include "lib/view/depth.glsl"

#include "gles/vertexcolors.glsl"
#include "gles/shadows_vertex.glsl"

void main(void)
{
    gl_Position = modelToClip(osg_Vertex);

    vec4 viewPos = modelToView(osg_Vertex);
    euclideanDepth = length(viewPos.xyz);
    linearDepth = getLinearDepth(gl_Position.z, viewPos.z);

#if @diffuseMap
    diffuseMapUV = (osg_TextureMatrix@diffuseMapUV * osg_MultiTexCoord@diffuseMapUV).xy;
#endif

    passColor = osg_Color;
    passViewPos = viewPos.xyz;
    passNormal = osg_Normal.xyz;

    if (useFalloff)
    {
        vec3 viewNormal = osg_NormalMatrix * normalize(osg_Normal.xyz);
        vec3 viewDir = normalize(viewPos.xyz);
        float viewAngle = abs(dot(viewNormal, viewDir));
        passFalloff = smoothstep(falloffParams.x, falloffParams.y, viewAngle);

        float startOpacity = min(falloffParams.z, 1.0);
        float stopOpacity = max(falloffParams.w, 0.0);

        passFalloff = mix(startOpacity, stopOpacity, passFalloff);
    }
    else
    {
        passFalloff = 1.0;
    }

#if @shadows_enabled
    vec3 viewNormal = normalize(osg_NormalMatrix * passNormal);
    setupShadowCoords(viewPos, viewNormal);
#endif
}

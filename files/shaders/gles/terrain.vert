#version 300 es
precision highp float;

#include "lib/core/vertex.h.glsl"

in vec4 osg_Vertex;
in vec4 osg_Color;
in vec3 osg_Normal;
in vec4 osg_MultiTexCoord0;
in vec4 osg_MultiTexCoord7;

uniform mat3 osg_NormalMatrix;
uniform float osg_FrontMaterialShininess;

out vec2 uv;
out float euclideanDepth;
out float linearDepth;

#define PER_PIXEL_LIGHTING (@normalMap || @specularMap || @forcePPL)

#if !PER_PIXEL_LIGHTING
centroid out vec3 passLighting;
centroid out vec3 passSpecular;
centroid out vec3 shadowDiffuseLighting;
centroid out vec3 shadowSpecularLighting;
#endif
out vec3 passViewPos;
out vec3 passNormal;

#include "gles/vertexcolors.glsl"
#include "gles/shadows_vertex.glsl"
#include "gles/normals.glsl"

#include "lib/light/lighting.glsl"
#include "lib/view/depth.glsl"

void main(void)
{
    gl_Position = modelToClip(osg_Vertex);

    vec4 viewPos = modelToView(osg_Vertex);
    euclideanDepth = length(viewPos.xyz);
    linearDepth = getLinearDepth(gl_Position.z, viewPos.z);

    passColor = osg_Color;
    passNormal = osg_Normal.xyz;
    passViewPos = viewPos.xyz;
    normalToViewMatrix = osg_NormalMatrix;

#if @normalMap
    mat3 tbnMatrix = generateTangentSpace(vec4(1.0, 0.0, 0.0, -1.0), passNormal);
    tbnMatrix[0] = -normalize(cross(tbnMatrix[2], tbnMatrix[1]));
    normalToViewMatrix *= tbnMatrix;
#endif

#if !PER_PIXEL_LIGHTING || @shadows_enabled
    vec3 viewNormal = normalize(osg_NormalMatrix * passNormal);
#endif

#if !PER_PIXEL_LIGHTING
    vec3 diffuseLight, ambientLight, specularLight;
    doLighting(viewPos.xyz, viewNormal, osg_FrontMaterialShininess, diffuseLight, ambientLight, specularLight, shadowDiffuseLighting, shadowSpecularLighting);
    passLighting = getDiffuseColor().xyz * diffuseLight + getAmbientColor().xyz * ambientLight + getEmissionColor().xyz;
    passSpecular = getSpecularColor().xyz * specularLight;
    clampLightingResult(passLighting);
    shadowDiffuseLighting *= getDiffuseColor().xyz;
    shadowSpecularLighting *= getSpecularColor().xyz;
#endif

    uv = osg_MultiTexCoord0.xy;

#if (@shadows_enabled)
    setupShadowCoords(viewPos, viewNormal);
#endif
}

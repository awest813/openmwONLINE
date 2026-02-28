#version 300 es
precision highp float;

#include "lib/core/vertex.h.glsl"

in vec4 osg_Vertex;
in vec4 osg_Color;
in vec3 osg_Normal;
in vec4 osg_MultiTexCoord0;
in vec4 osg_MultiTexCoord1;
in vec4 osg_MultiTexCoord2;
in vec4 osg_MultiTexCoord3;
in vec4 osg_MultiTexCoord4;
in vec4 osg_MultiTexCoord5;
in vec4 osg_MultiTexCoord6;
in vec4 osg_MultiTexCoord7;

uniform mat3 osg_NormalMatrix;
uniform mat4 osg_ModelViewMatrix;
uniform float osg_FrontMaterialShininess;

#if @diffuseMap
uniform mat4 osg_TextureMatrix@diffuseMapUV;
out vec2 diffuseMapUV;
#endif

#if @darkMap
uniform mat4 osg_TextureMatrix@darkMapUV;
out vec2 darkMapUV;
#endif

#if @detailMap
uniform mat4 osg_TextureMatrix@detailMapUV;
out vec2 detailMapUV;
#endif

#if @decalMap
uniform mat4 osg_TextureMatrix@decalMapUV;
out vec2 decalMapUV;
#endif

#if @emissiveMap
uniform mat4 osg_TextureMatrix@emissiveMapUV;
out vec2 emissiveMapUV;
#endif

#if @normalMap
uniform mat4 osg_TextureMatrix@normalMapUV;
out vec2 normalMapUV;
#endif

#if @envMap
out vec2 envMapUV;
#endif

#if @bumpMap
uniform mat4 osg_TextureMatrix@bumpMapUV;
out vec2 bumpMapUV;
#endif

#if @specularMap
uniform mat4 osg_TextureMatrix@specularMapUV;
out vec2 specularMapUV;
#endif

#if @glossMap
uniform mat4 osg_TextureMatrix@glossMapUV;
out vec2 glossMapUV;
#endif

#define PER_PIXEL_LIGHTING (@normalMap || @specularMap || @forcePPL)

#if !PER_PIXEL_LIGHTING
centroid out vec3 passLighting;
centroid out vec3 passSpecular;
centroid out vec3 shadowDiffuseLighting;
centroid out vec3 shadowSpecularLighting;
uniform float emissiveMult;
uniform float specStrength;
#endif
out vec3 passViewPos;
out vec3 passNormal;
#if @normalMap || @diffuseParallax
out vec4 passTangent;
#endif

#include "gles/vertexcolors.glsl"
#include "gles/shadows_vertex.glsl"
#include "gles/normals.glsl"

#include "lib/light/lighting.glsl"
#include "lib/view/depth.glsl"

#if @particleOcclusion
out vec3 orthoDepthMapCoord;

uniform mat4 depthSpaceMatrix;
uniform mat4 osg_ViewMatrixInverse;
#endif

void main(void)
{
#if @particleOcclusion
    mat4 model = osg_ViewMatrixInverse * osg_ModelViewMatrix;
    orthoDepthMapCoord = ((depthSpaceMatrix * model) * vec4(osg_Vertex.xyz, 1.0)).xyz;
#endif

    gl_Position = modelToClip(osg_Vertex);

    vec4 viewPos = modelToView(osg_Vertex);
    passColor = osg_Color;
    passViewPos = viewPos.xyz;
    passNormal = osg_Normal.xyz;
    normalToViewMatrix = osg_NormalMatrix;

#if @normalMap || @diffuseParallax
    passTangent = osg_MultiTexCoord7.xyzw;
    normalToViewMatrix *= generateTangentSpace(passTangent, passNormal);
#endif

#if @envMap || !PER_PIXEL_LIGHTING || @shadows_enabled
    vec3 viewNormal = normalize(osg_NormalMatrix * passNormal);
#endif

#if @envMap
    vec3 viewVec = normalize(viewPos.xyz);
    vec3 r = reflect( viewVec, viewNormal );
    float m = 2.0 * sqrt( r.x*r.x + r.y*r.y + (r.z+1.0)*(r.z+1.0) );
    envMapUV = vec2(r.x/m + 0.5, r.y/m + 0.5);
#endif

#if @diffuseMap
    diffuseMapUV = (osg_TextureMatrix@diffuseMapUV * osg_MultiTexCoord@diffuseMapUV).xy;
#endif

#if @darkMap
    darkMapUV = (osg_TextureMatrix@darkMapUV * osg_MultiTexCoord@darkMapUV).xy;
#endif

#if @detailMap
    detailMapUV = (osg_TextureMatrix@detailMapUV * osg_MultiTexCoord@detailMapUV).xy;
#endif

#if @decalMap
    decalMapUV = (osg_TextureMatrix@decalMapUV * osg_MultiTexCoord@decalMapUV).xy;
#endif

#if @emissiveMap
    emissiveMapUV = (osg_TextureMatrix@emissiveMapUV * osg_MultiTexCoord@emissiveMapUV).xy;
#endif

#if @normalMap
    normalMapUV = (osg_TextureMatrix@normalMapUV * osg_MultiTexCoord@normalMapUV).xy;
#endif

#if @bumpMap
    bumpMapUV = (osg_TextureMatrix@bumpMapUV * osg_MultiTexCoord@bumpMapUV).xy;
#endif

#if @specularMap
    specularMapUV = (osg_TextureMatrix@specularMapUV * osg_MultiTexCoord@specularMapUV).xy;
#endif

#if @glossMap
    glossMapUV = (osg_TextureMatrix@glossMapUV * osg_MultiTexCoord@glossMapUV).xy;
#endif

#if !PER_PIXEL_LIGHTING
    vec3 diffuseLight, ambientLight, specularLight;
    doLighting(viewPos.xyz, viewNormal, osg_FrontMaterialShininess, diffuseLight, ambientLight, specularLight, shadowDiffuseLighting, shadowSpecularLighting);
    passLighting = getDiffuseColor().xyz * diffuseLight + getAmbientColor().xyz * ambientLight + getEmissionColor().xyz * emissiveMult;
    passSpecular = getSpecularColor().xyz * specularLight * specStrength;
    clampLightingResult(passLighting);
    shadowDiffuseLighting *= getDiffuseColor().xyz;
    shadowSpecularLighting *= getSpecularColor().xyz * specStrength;
#endif

#if (@shadows_enabled)
    setupShadowCoords(viewPos, viewNormal);
#endif
}

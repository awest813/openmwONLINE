#version 300 es
precision highp float;

in vec2 uv;

uniform sampler2D diffuseMap;
uniform mat4 osg_TextureMatrix0;
uniform mat4 osg_TextureMatrix1;
uniform mat3 osg_NormalMatrix;
uniform float osg_FrontMaterialShininess;

#if @normalMap
uniform sampler2D normalMap;
#endif

#if @blendMap
uniform sampler2D blendMap;
#endif

in float euclideanDepth;
in float linearDepth;

#define PER_PIXEL_LIGHTING (@normalMap || @specularMap || @forcePPL)

#if !PER_PIXEL_LIGHTING
centroid in vec3 passLighting;
centroid in vec3 passSpecular;
centroid in vec3 shadowDiffuseLighting;
centroid in vec3 shadowSpecularLighting;
#endif
in vec3 passViewPos;
in vec3 passNormal;

uniform vec2 screenRes;
uniform float far;

#include "gles/vertexcolors.glsl"
#include "gles/shadows_fragment.glsl"
#include "lib/light/lighting.glsl"
#include "lib/material/parallax.glsl"
#include "gles/fog.glsl"
#include "gles/normals.glsl"

out vec4 osg_FragColor;

void main()
{
    vec2 adjustedUV = (osg_TextureMatrix0 * vec4(uv, 0.0, 1.0)).xy;

#if @parallax
    float height = texture(normalMap, adjustedUV).a;
    adjustedUV += getParallaxOffset(transpose(normalToViewMatrix) * normalize(-passViewPos), height);
#endif
    vec4 diffuseTex = texture(diffuseMap, adjustedUV);
    osg_FragColor = vec4(diffuseTex.xyz, 1.0);

    vec4 diffuseColor = getDiffuseColor();
    osg_FragColor.a *= diffuseColor.a;

#if @blendMap
    vec2 blendMapUV = (osg_TextureMatrix1 * vec4(uv, 0.0, 1.0)).xy;
    osg_FragColor.a *= texture(blendMap, blendMapUV).a;
#endif

#if @normalMap
    vec4 normalTex = texture(normalMap, adjustedUV);
    vec3 normal = normalTex.xyz * 2.0 - 1.0;
#if @reconstructNormalZ
    normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));
#endif
    vec3 viewNormal = normalToView(normal);
#else
    vec3 viewNormal = normalize(osg_NormalMatrix * passNormal);
#endif

    float shadowing = unshadowedLightRatio(linearDepth);
    vec3 lighting, specular;
#if !PER_PIXEL_LIGHTING
    lighting = passLighting + shadowDiffuseLighting * shadowing;
    specular = passSpecular + shadowSpecularLighting * shadowing;
#else
#if @specularMap
    float shininess = 128.0;
    vec3 specularColor = vec3(diffuseTex.a);
#else
    float shininess = osg_FrontMaterialShininess;
    vec3 specularColor = getSpecularColor().xyz;
#endif
    vec3 diffuseLight, ambientLight, specularLight;
    doLighting(passViewPos, viewNormal, shininess, shadowing, diffuseLight, ambientLight, specularLight);
    lighting = diffuseColor.xyz * diffuseLight + getAmbientColor().xyz * ambientLight + getEmissionColor().xyz;
    specular = specularColor * specularLight;
#endif

    clampLightingResult(lighting);
    osg_FragColor.xyz = osg_FragColor.xyz * lighting + specular;

    osg_FragColor = applyFogAtDist(osg_FragColor, euclideanDepth, linearDepth, far);

    applyShadowDebugOverlay();
}

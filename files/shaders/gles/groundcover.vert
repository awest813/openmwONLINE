#version 300 es
precision highp float;

#include "lib/core/vertex.h.glsl"

#define GROUNDCOVER

in vec4 osg_Vertex;
in vec3 osg_Normal;
in vec4 osg_MultiTexCoord0;
in vec4 osg_MultiTexCoord7;
in vec4 aOffset;
in vec3 aRotation;

uniform mat3 osg_NormalMatrix;
uniform mat4 osg_ModelViewMatrix;
uniform float osg_FrontMaterialShininess;

#if @diffuseMap
uniform mat4 osg_TextureMatrix@diffuseMapUV;
out vec2 diffuseMapUV;
#endif

#if @normalMap
uniform mat4 osg_TextureMatrix@normalMapUV;
out vec2 normalMapUV;
#endif

#define PER_PIXEL_LIGHTING @normalMap

out float euclideanDepth;
out float linearDepth;

#if PER_PIXEL_LIGHTING
out vec3 passViewPos;
#else
centroid out vec3 passLighting;
centroid out vec3 shadowDiffuseLighting;
#endif

out vec3 passNormal;

#include "gles/shadows_vertex.glsl"
#include "gles/normals.glsl"
#include "lib/light/lighting.glsl"
#include "lib/view/depth.glsl"

uniform float osg_SimulationTime;
uniform mat4 osg_ViewMatrixInverse;
uniform mat4 osg_ViewMatrix;
uniform float windSpeed;
uniform vec3 playerPos;

#if @groundcoverStompMode == 0
#else
    #define STOMP 1
    #if @groundcoverStompMode == 2
        #define STOMP_HEIGHT_SENSITIVE 1
    #endif
    #define STOMP_INTENSITY_LEVEL @groundcoverStompIntensity
#endif

vec2 groundcoverDisplacement(in vec3 worldpos, float h)
{
    vec2 windDirection = vec2(1.0);
    vec3 footPos = playerPos;
    vec3 windVec = vec3(windSpeed * windDirection, 1.0);

    float v = length(windVec);
    vec2 displace = vec2(2.0 * windVec.xy + 0.1);
    vec2 harmonics = vec2(0.0);

    harmonics += vec2((1.0 - 0.10*v) * sin(1.0*osg_SimulationTime + worldpos.xy / 1100.0));
    harmonics += vec2((1.0 - 0.04*v) * cos(2.0*osg_SimulationTime + worldpos.xy / 750.0));
    harmonics += vec2((1.0 + 0.14*v) * sin(3.0*osg_SimulationTime + worldpos.xy / 500.0));
    harmonics += vec2((1.0 + 0.28*v) * sin(5.0*osg_SimulationTime + worldpos.xy / 200.0));

    vec2 stomp = vec2(0.0);
#if STOMP
    float d = length(worldpos.xy - footPos.xy);
#if STOMP_INTENSITY_LEVEL == 0
    const float STOMP_RANGE = 50.0;
    const float STOMP_DISTANCE = 20.0;
#elif STOMP_INTENSITY_LEVEL == 1
    const float STOMP_RANGE = 80.0;
    const float STOMP_DISTANCE = 40.0;
#elif STOMP_INTENSITY_LEVEL == 2
    const float STOMP_RANGE = 150.0;
    const float STOMP_DISTANCE = 60.0;
#endif
    if (d < STOMP_RANGE && d > 0.0)
        stomp = (STOMP_DISTANCE / d - STOMP_DISTANCE / STOMP_RANGE) * (worldpos.xy - footPos.xy);

#ifdef STOMP_HEIGHT_SENSITIVE
    stomp *= clamp((worldpos.z - footPos.z) / h, 0.0, 1.0);
#endif
#endif

    return clamp(0.02 * h, 0.0, 1.0) * (harmonics * displace + stomp);
}

mat4 rotation(in vec3 angle)
{
    float sin_x = sin(angle.x);
    float cos_x = cos(angle.x);
    float sin_y = sin(angle.y);
    float cos_y = cos(angle.y);
    float sin_z = sin(angle.z);
    float cos_z = cos(angle.z);

    return mat4(
        cos_z*cos_y+sin_x*sin_y*sin_z, -sin_z*cos_x, cos_z*sin_y+sin_z*sin_x*cos_y, 0.0,
        sin_z*cos_y+cos_z*sin_x*sin_y, cos_z*cos_x, sin_z*sin_y-cos_z*sin_x*cos_y, 0.0,
        -sin_y*cos_x, sin_x, cos_x*cos_y, 0.0,
        0.0, 0.0, 0.0, 1.0);
}

mat3 rotation3(in mat4 rot4)
{
    return mat3(
        rot4[0].xyz,
        rot4[1].xyz,
        rot4[2].xyz);
}

void main(void)
{
    vec3 position = aOffset.xyz;
    float scale = aOffset.w;

    mat4 rot = rotation(aRotation);
    vec4 displacedVertex = rot * scale * osg_Vertex;

    displacedVertex = vec4(displacedVertex.xyz + position, 1.0);

    vec4 worldPos = osg_ViewMatrixInverse * osg_ModelViewMatrix * displacedVertex;
    worldPos.xy += groundcoverDisplacement(worldPos.xyz, osg_Vertex.z);
    vec4 viewPos = osg_ViewMatrix * worldPos;

    euclideanDepth = length(viewPos.xyz);

    if (length(osg_ModelViewMatrix * vec4(position, 1.0)) > @groundcoverFadeEnd)
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    else
        gl_Position = viewToClip(viewPos);

    linearDepth = getLinearDepth(gl_Position.z, viewPos.z);

    passNormal = rotation3(rot) * osg_Normal.xyz;
    normalToViewMatrix = osg_NormalMatrix;
#if @normalMap
    normalToViewMatrix *= generateTangentSpace(osg_MultiTexCoord7.xyzw * rot, passNormal);
#endif

#if (!PER_PIXEL_LIGHTING || @shadows_enabled)
    vec3 viewNormal = normalize(osg_NormalMatrix * passNormal);
#endif

#if @diffuseMap
    diffuseMapUV = (osg_TextureMatrix@diffuseMapUV * osg_MultiTexCoord@diffuseMapUV).xy;
#endif

#if @normalMap
    normalMapUV = (osg_TextureMatrix@normalMapUV * osg_MultiTexCoord@normalMapUV).xy;
#endif

#if PER_PIXEL_LIGHTING
    passViewPos = viewPos.xyz;
#else
    vec3 diffuseLight, ambientLight, specularLight;
    vec3 unusedShadowSpecular;
    doLighting(viewPos.xyz, viewNormal, osg_FrontMaterialShininess, diffuseLight, ambientLight, specularLight, shadowDiffuseLighting, unusedShadowSpecular);
    passLighting = diffuseLight + ambientLight;
    clampLightingResult(passLighting);
#endif

#if (@shadows_enabled)
    setupShadowCoords(viewPos, viewNormal);
#endif
}

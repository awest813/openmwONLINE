#version 300 es
precision highp float;

#include "lib/sky/passes.glsl"

uniform int pass;
uniform sampler2D diffuseMap;
uniform sampler2D maskMap;
uniform float opacity;
uniform vec4 moonBlend;
uniform vec4 atmosphereFade;

uniform vec4 osg_FrontMaterialEmission;
uniform vec4 osg_FrontMaterialDiffuse;
uniform vec4 osg_FogColor;

in vec2 diffuseMapUV;
in vec4 passColor;

out vec4 osg_FragColor;

void paintAtmosphere(inout vec4 color)
{
    color = osg_FrontMaterialEmission;
    color.a *= passColor.a;
}

void paintAtmosphereNight(inout vec4 color)
{
    color = texture(diffuseMap, diffuseMapUV);
    color.a *= passColor.a * opacity;
}

void paintClouds(inout vec4 color)
{
    color = texture(diffuseMap, diffuseMapUV);
    color.a *= passColor.a * opacity;
    color.xyz = clamp(color.xyz * osg_FrontMaterialEmission.xyz, 0.0, 1.0);

    color = mix(vec4(osg_FogColor.xyz, color.a), color, passColor.a);
}

void paintMoon(inout vec4 color)
{
    vec4 phase = texture(diffuseMap, diffuseMapUV);
    vec4 mask = texture(maskMap, diffuseMapUV);

    vec4 blendedLayer = phase * moonBlend;
    color = vec4(blendedLayer.xyz + atmosphereFade.xyz, atmosphereFade.a * mask.a);
}

void paintSun(inout vec4 color)
{
    color = texture(diffuseMap, diffuseMapUV);
    color.a *= osg_FrontMaterialDiffuse.a;
}

void paintSunglare(inout vec4 color)
{
    color = osg_FrontMaterialEmission;
    color.a = osg_FrontMaterialDiffuse.a;
}

void processSunflashQuery()
{
    const float threshold = 0.8;

    if (texture(diffuseMap, diffuseMapUV).a <= threshold)
        discard;
}

void main()
{
    vec4 color = vec4(0.0);

    if (pass == PASS_ATMOSPHERE)
        paintAtmosphere(color);
    else if (pass == PASS_ATMOSPHERE_NIGHT)
        paintAtmosphereNight(color);
    else if (pass == PASS_CLOUDS)
        paintClouds(color);
    else if (pass == PASS_MOON)
        paintMoon(color);
    else if (pass == PASS_SUN)
        paintSun(color);
    else if (pass == PASS_SUNGLARE)
        paintSunglare(color);
    else if (pass == PASS_SUNFLASH_QUERY)
    {
        processSunflashQuery();
        return;
    }

    osg_FragColor = color;
}

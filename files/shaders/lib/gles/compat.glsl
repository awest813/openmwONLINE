// GLSL ES 3.00 compatibility definitions for the OpenMW compatibility shader set.
// Provides #define mappings from GLSL 1.20 built-ins to GLSL ES 3.00 equivalents.
#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp sampler2DShadow;
precision highp samplerCube;

#define texture1D texture
#define texture2D texture
#define texture3D texture
#define textureCube texture
#define texture2DProj textureProj
#define shadow2DProj textureProj

#define gl_FragColor omw_FragColor
out vec4 omw_FragColor;

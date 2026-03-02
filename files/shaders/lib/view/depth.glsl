#ifndef LIB_VIEW_DEPTH
#define LIB_VIEW_DEPTH

float linearizeDepth(float depth, float near, float far)
{
#if @reverseZ
    depth = 1.0 - depth;
#endif
    float z_n = 2.0 * depth - 1.0;
    depth = 2.0 * near * far / (far + near - z_n * (far - near));
    return depth;
}

float getLinearDepth(in float z, in float viewZ)
{
#if @reverseZ
    // With reverse-Z the clip-space z is non-linearly remapped, so derive
    // linear depth directly from view-space z (negated because the camera
    // looks along -Z in view space).
    return -viewZ;
#else
    return z;
#endif
}

#endif

#ifndef PERFORMANCE_TOOLKIT_VISIBILITY_HPP
#define PERFORMANCE_TOOLKIT_VISIBILITY_HPP

namespace PerformanceToolkit
{
    enum VisibilityState
    {
        Visible            = 0,
        FrustumCulled      = 1 << 0,
        DistanceCulled     = 1 << 1,
        SizeCulled         = 1 << 2,
        TerrainHidden      = 1 << 3,
        WaterHidden        = 1 << 4,
        CandidateOccluded  = 1 << 5,
        ForcedVisible      = 1 << 6,
        DebugExempt        = 1 << 7
    };
}

#endif

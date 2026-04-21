#ifndef PERFORMANCE_TOOLKIT_SCANNER_HPP
#define PERFORMANCE_TOOLKIT_SCANNER_HPP

#include <string>
#include <vector>
#include <osg/Vec3>
#include <osg/BoundingBox>

namespace MWWorld {
    class Ptr;
}

namespace PerformanceToolkit
{
    struct AssetMetrics
    {
        std::string name;
        unsigned int triangleCount = 0;
        unsigned int drawCalls = 0;
        unsigned int textureMemoryBytes = 0;
        bool isHighRisk = false;
    };

    class Scanner
    {
    public:
        static Scanner& getInstance();

        void scanCell(const MWWorld::CellStore* cell);
        
        AssetMetrics auditNode(osg::Node* node);
        
        const AssetMetrics& getCellMetrics() const { return mCurrentCellMetrics; }

        const std::vector<AssetMetrics>& getCandidates() const { return mCandidates; }

    private:
        Scanner();
        std::vector<AssetMetrics> mCandidates;
        AssetMetrics mCurrentCellMetrics;
    };
}

#endif

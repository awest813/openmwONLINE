#ifndef PERFORMANCE_TOOLKIT_OCCLUSION_SYSTEM_HPP
#define PERFORMANCE_TOOLKIT_OCCLUSION_SYSTEM_HPP

#include <osg/ref_ptr>
#include <osg/Group>
#include <map>
#include <vector>

namespace PerformanceToolkit
{
    /**
     * @brief Manages occlusion queries and occluder registration.
     */
    class OcclusionSystem
    {
    public:
        static OcclusionSystem& getInstance();

        void update();
        
        // Register a node as a potential occluder (large building, wall, etc.)
        void registerOccluder(osg::Node* node);
        void unregisterOccluder(osg::Node* node);
        
        // Wrap a group of nodes that should be occluded by occluders
        void wrapOccludee(osg::Group* parent, osg::Node* occludee, unsigned int drawCallImpact = 1);
        void removeOccludee(osg::Node* node);

        unsigned int getOccludedCount() const { return mOccludedCount; }

    private:
        OcclusionSystem();
        
        std::vector<osg::ref_ptr<osg::Node>> mOccluders;
        std::vector<osg::ref_ptr<osg::OcclusionQueryNode>> mOccludees;
        unsigned int mOccludedCount = 0;
        
        // Phase 3: Throttling
        unsigned int mMaxActiveQueries = 256; 
        unsigned int mActiveQueryCount = 0;
    };
}

#endif

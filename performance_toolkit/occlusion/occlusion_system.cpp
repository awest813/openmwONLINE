#include "occlusion_system.hpp"
#include <osgOcclusionQuery/OcclusionQueryNode>

namespace PerformanceToolkit
{
    OcclusionSystem& OcclusionSystem::getInstance()
    {
        static OcclusionSystem instance;
        return instance;
    }

    OcclusionSystem::OcclusionSystem() {}

    void OcclusionSystem::update()
    {
        mOccludedCount = 0;
        
        // Maintenance: Remove nodes that are no longer in the scene
        auto prune = [](auto& container) {
            container.erase(std::remove_if(container.begin(), container.end(), 
                [](const auto& node) { return !node.valid() || node->getNumParents() == 0; }), 
                container.end());
        };
        
        prune(mOccluders);
        prune(mOccludees);

        for (auto& oqn : mOccludees)
        {
            // Only count if the query has actually finished to avoid flickering stats
            if (oqn->getQueryResultReady())
            {
                if (!oqn->getPassed()) 
                    mOccludedCount++;
            }
        }
    }

    void OcclusionSystem::registerOccluder(osg::Node* node)
    {
        if (!node) return;
        
        // Prevent double registration
        if (std::find(mOccluders.begin(), mOccluders.end(), node) != mOccluders.end())
            return;

        mOccluders.push_back(node);
    }

    void OcclusionSystem::unregisterOccluder(osg::Node* node)
    {
        mOccluders.erase(std::remove(mOccluders.begin(), mOccluders.end(), node), mOccluders.end());
    }

    void OcclusionSystem::wrapOccludee(osg::Group* parent, osg::Node* occludee, unsigned int drawCallImpact)
    {
        if (!parent || !occludee) return;

        // Phase 3: Throttling - Only query if it's "worth it" or we have spare slots
        if (mOccludees.size() >= mMaxActiveQueries && drawCallImpact < 5)
            return;

        // Wrap the occludee in an OcclusionQueryNode
        osg::ref_ptr<osg::OcclusionQueryNode> oqn = new osg::OcclusionQueryNode;
        oqn->addChild(occludee);
        oqn->setQueriesEnabled(true);
        
        mOccludees.push_back(oqn);
        // Replace original occludee with the OQN
        parent->replaceChild(occludee, oqn.get());
    }

    void OcclusionSystem::removeOccludee(osg::Node* node)
    {
        // Find the OQN that wraps this node
        mOccludees.erase(std::remove_if(mOccludees.begin(), mOccludees.end(), 
            [node](const auto& oqn) { 
                return oqn->getNumChildren() > 0 && oqn->getChild(0) == node; 
            }), 
            mOccludees.end());
    }
}

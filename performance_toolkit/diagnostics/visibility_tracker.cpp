#include "visibility_tracker.hpp"

namespace PerformanceToolkit
{
    void VisibilityTracker::operator()(osg::Node* node, osg::NodeVisitor* nv)
    {
        if (nv->getVisitorType() == osg::NodeVisitor::CULL_VISITOR)
        {
            osgUtil::CullVisitor* cv = static_cast<osgUtil::CullVisitor*>(nv);
            
            // Note: If we are in the CullCallback, the parent was already checked.
            // cv->isCulled returns true if the node's bounding box is outside the frustum.
            if (cv->isCulled(node->getBound()))
            {
                mState = FrustumCulled;
            }
            else
            {
                mState = Visible;
            }
        }
        traverse(node, nv);
    }

    void VisibilityTracker::attach(osg::Node* node)
    {
        if (!node) return;
        
        // Don't add multiple times
        if (node->getCullCallback())
        {
            if (dynamic_cast<VisibilityTracker*>(node->getCullCallback()))
                return;
        }
        
        node->setCullCallback(new VisibilityTracker());
    }
}

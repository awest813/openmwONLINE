#ifndef PERFORMANCE_TOOLKIT_VISIBILITY_TRACKER_HPP
#define PERFORMANCE_TOOLKIT_VISIBILITY_TRACKER_HPP

#include <osg/NodeCallback>
#include <osgUtil/CullVisitor>
#include "../visibility.hpp"

namespace PerformanceToolkit
{
    class VisibilityTracker : public osg::NodeCallback
    {
    public:
        void operator()(osg::Node* node, osg::NodeVisitor* nv) override;
        
        VisibilityState getState() const { return mState; }
        
        static void attach(osg::Node* node);

    private:
        VisibilityState mState = Visible;
    };
}

#endif

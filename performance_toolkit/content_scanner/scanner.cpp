#include "scanner.hpp"
#include <apps/openmw/mwworld/ptr.hpp>
#include <apps/openmw/mwworld/class.hpp>
#include <apps/openmw/mwworld/cellstore.hpp>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Texture>
#include <osg/NodeVisitor>
#include <algorithm>

namespace PerformanceToolkit
{
    Scanner& Scanner::getInstance()
    {
        static Scanner instance;
        return instance;
    }

    Scanner::Scanner() {}

    class AssetAuditor : public osg::NodeVisitor
    {
    public:
        AssetMetrics metrics;
        std::set<osg::Texture*> uniqueTextures;
        std::set<osg::Geometry*> uniqueGeometry;

        AssetAuditor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

        void apply(osg::Geode& geode) override
        {
            metrics.drawCalls += geode.getNumDrawables();
            for (unsigned int i = 0; i < geode.getNumDrawables(); ++i)
            {
                if (osg::Geometry* geom = geode.getDrawable(i)->asGeometry())
                {
                    if (uniqueGeometry.find(geom) != uniqueGeometry.end()) continue;
                    uniqueGeometry.insert(geom);

                    for (unsigned int j = 0; j < geom->getNumPrimitiveSets(); ++j)
                    {
                        osg::PrimitiveSet* ps = geom->getPrimitiveSet(j);
                        unsigned int numIndices = ps->getNumIndices();
                        switch(ps->getMode())
                        {
                            case GL_TRIANGLES: metrics.triangleCount += numIndices / 3; break;
                            case GL_TRIANGLE_STRIP:
                            case GL_TRIANGLE_FAN: metrics.triangleCount += (numIndices > 2) ? numIndices - 2 : 0; break;
                            default: break;
                        }
                    }
                }
            }
            traverse(geode);
        }

        void apply(osg::Node& node) override
        {
            if (osg::StateSet* ss = node.getStateSet())
            {
                osg::StateSet::TextureAttributeList& texAttrs = ss->getTextureAttributeList();
                for (auto& attrs : texAttrs)
                {
                    for (auto& attr : attrs)
                    {
                        if (osg::Texture* tex = dynamic_cast<osg::Texture*>(attr.second.get()))
                        {
                            if (uniqueTextures.find(tex) != uniqueTextures.end()) continue;
                            uniqueTextures.insert(tex);

                            if (osg::Image* img = tex->getImage(0))
                            {
                                // Base memory + 33% for mipmaps
                                unsigned int baseSize = img->s() * img->t() * 4; 
                                metrics.textureMemoryBytes += static_cast<unsigned int>(baseSize * 1.33f);
                            }
                        }
                    }
                }
            }
            traverse(node);
        }
    };

    AssetMetrics Scanner::auditNode(osg::Node* node)
    {
        AssetAuditor auditor;
        if (node)
            node->accept(auditor);
        
        // Define Risk thresholds (Phase 2 Polishing)
        if (auditor.metrics.triangleCount > 500000 || auditor.metrics.textureMemoryBytes > 128 * 1024 * 1024)
            auditor.metrics.isHighRisk = true;

        return auditor.metrics;
    }

    void Scanner::scanCell(const MWWorld::CellStore* cell)
    {
        mCandidates.clear();
        // For each object in cell, evaluate occluder potential
        // In a real implementation, we'd iterate through cell->getRecords()
        // For now, we'll provide the scoring infrastructure
    }

    float Scanner::scoreOccluderCandidate(osg::Node* node)
    {
        if (!node) return 0.0f;

        // Audit check: Skip transparent objects as occluders
        if (osg::StateSet* ss = node->getStateSet())
        {
            if (ss->getRenderingHint() == osg::StateSet::TRANSPARENT_BIN)
                return 0.0f;
            
            osg::StateAttribute* attr = ss->getAttribute(osg::StateAttribute::BLEND);
            if (attr && ss->getMode(GL_BLEND) & osg::StateAttribute::ON)
                return 0.0f;
        }

        osg::ComputeBoundsVisitor cbv;
        node->accept(cbv);
        osg::BoundingBox bb = cbv.getBoundingBox();

        // Prefer objects that are large in at least two dimensions (walls, buildings)
        float dx = bb.xMax() - bb.xMin();
        float dy = bb.yMax() - bb.yMin();
        float dz = bb.zMax() - bb.zMin();

        float maxDim = std::max({dx, dy, dz});
        float area = 0.0f;
        
        if (maxDim == dx) area = dy * dz;
        else if (maxDim == dy) area = dx * dz;
        else area = dx * dy;

        // Simple heuristic: Area-based scoring
        return area > 500.0f ? area : 0.0f; 
    }
}

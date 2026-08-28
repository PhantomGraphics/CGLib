#pragma once

#include "../../CGLib/VkAppBase/IVkSubRenderer.h"
#include "SpaceHashPanel.h"
#include "CompactSpaceHashPanel.h"
#include "KDTreePanel.h"
#include "OctreePanel.h"
#include "SignedDistancePanel.h"

namespace VKSpace {

class World;
class Renderer;

class SpaceMenuPanel : public ::VKG::IVkUIPanel {
public:
    void init(World* world, Renderer* renderer);

    void onImGuiMenuBar();
    void onImGui() override;

    void setActiveByName(const std::string& name);
    void runActive(World& world);
    bool setActiveParam(const std::string& name, const std::string& value);

private:
    enum class AlgoType {
        SpaceHash,
        CompactSpaceHash,
        KDTree,
        Octree,
        SignedDistance,
    };

    World* world_ = nullptr;
    Renderer* renderer_ = nullptr;

    SpaceHashPanel spaceHashView_;
    CompactSpaceHashPanel compactHashView_;
    KDTreePanel kdTreeView_;
    OctreePanel octreeView_;
    SignedDistancePanel signedDistanceView_;

    IAlgorithmView* activeView_ = nullptr;
    AlgoType activeType_ = AlgoType::SpaceHash;

    static const char* algoName(AlgoType t);
    IAlgorithmView* viewOf(AlgoType t);
    void setActive(AlgoType t);
};

} // namespace VKSpace

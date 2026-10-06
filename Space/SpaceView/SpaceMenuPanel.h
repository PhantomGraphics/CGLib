#pragma once

#include "../../CGLib/VkAppBase/IVkSubRenderer.h"
#include "SpaceHashPanel.h"
#include "CompactSpaceHashPanel.h"
#include "KDTreePanel.h"
#include "OctreePanel.h"
#include "SignedDistancePanel.h"

#include <functional>
#include <string>

class ViewShell;

namespace VKSpace {

class World;
class Renderer;

class SpaceMenuPanel : public ::VKG::IVkUIPanel {
public:
    void init(World* world, Renderer* renderer);

    // The Control window is a shell panel (hidden until opened). GUI actions are
    // sent as commands through `submit` so they take the same path as typed ones.
    void setShell(ViewShell* s) { shell_ = s; }
    void setSubmit(std::function<void(const std::string&)> f) { submit_ = std::move(f); }
    void setLocked(bool v) { locked_ = v; }
    const char* activeName() const { return algoName(activeType_); }

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

    ViewShell* shell_ = nullptr;
    std::function<void(const std::string&)> submit_;
    bool locked_ = false;
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

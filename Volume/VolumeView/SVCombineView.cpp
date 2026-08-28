#include "SVCombineView.h"

#include "World.h"
#include "VolumeScene.h"

#include "../Volume/SparseVolumeTree/SparseVolume.h"
#include "../Volume/SparseVolumeTree/Interpolator.h"
#include "../Volume/SparseVolumeTree/Coord.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace VkVolumeView {

void SVCombineView::onImGui(World& world, int /*activeSceneId*/,
                             const std::function<void()>& onRebuild)
{
    const auto& scenes = world.getScenes();

    if (static_cast<int>(scenes.size()) < 2) {
        ImGui::TextDisabled("Need at least 2 scenes");
        return;
    }

    std::vector<const char*> names;
    names.reserve(scenes.size());
    for (const auto& s : scenes) names.push_back(s->getName().c_str());

    const int n = static_cast<int>(names.size());
    idxA_ = std::clamp(idxA_, 0, n - 1);
    idxB_ = std::clamp(idxB_, 0, n - 1);

    ImGui::Combo("Scene A", &idxA_, names.data(), n);
    ImGui::Combo("Scene B", &idxB_, names.data(), n);

    int op = static_cast<int>(op_);
    ImGui::RadioButton("Union",        &op, 0); ImGui::SameLine();
    ImGui::RadioButton("Intersection", &op, 1); ImGui::SameLine();
    ImGui::RadioButton("Diff (A-B)",   &op, 2);
    op_ = static_cast<CsgOp>(op);

    const bool canApply = (idxA_ != idxB_) &&
                          scenes[idxA_]->getShape() &&
                          scenes[idxB_]->getShape();
    ImGui::BeginDisabled(!canApply);
    if (ImGui::Button("Apply")) {
        const auto* shapeA = scenes[idxA_]->getShape();
        const auto* shapeB = scenes[idxB_]->getShape();

        const float cellSize = shapeA->getVoxelSize();
        auto result = std::make_unique<Phantom::Volume::SparseVolumef>(1e6f);
        result->setVoxelSize(cellSize);

        // World-space union bounding box of both volumes
        const auto bboxA = shapeA->getBoundingBox();
        const auto bboxB = shapeB->getBoundingBox();
        const Phantom::Math::Vector3df wMin(
            std::min(bboxA.getMin().x, bboxB.getMin().x),
            std::min(bboxA.getMin().y, bboxB.getMin().y),
            std::min(bboxA.getMin().z, bboxB.getMin().z));
        const Phantom::Math::Vector3df wMax(
            std::max(bboxA.getMax().x, bboxB.getMax().x),
            std::max(bboxA.getMax().y, bboxB.getMax().y),
            std::max(bboxA.getMax().z, bboxB.getMax().z));

        // Index bounds in result's coordinate system (add 1 voxel padding)
        const Phantom::Volume::Coord iMin = result->worldToIndex(wMin);
        const Phantom::Volume::Coord iMax = result->worldToIndex(wMax);

        Phantom::Volume::TrilinearInterpolator<float> interpA(*shapeA);
        Phantom::Volume::TrilinearInterpolator<float> interpB(*shapeB);
        constexpr float kBg = 1e6f;

        for (int i = iMin.x - 1; i <= iMax.x + 1; ++i) {
            for (int j = iMin.y - 1; j <= iMax.y + 1; ++j) {
                for (int k = iMin.z - 1; k <= iMax.z + 1; ++k) {
                    const Phantom::Volume::Coord idx(i, j, k);
                    const auto wp = result->indexToWorld(idx);
                    const float a = interpA.getValue(wp);
                    const float b = interpB.getValue(wp);

                    float val = kBg;
                    switch (op_) {
                    case CsgOp::Union:        val = std::min(a, b);  break;
                    case CsgOp::Intersection: val = std::max(a, b);  break;
                    case CsgOp::Difference:   val = std::max(a, -b); break;
                    }

                    if (val < kBg * 0.99f) {
                        result->setValue(idx, val);
                    }
                }
            }
        }

        static const char* kOpNames[] = { "union", "inter", "diff" };
        const std::string name = scenes[idxA_]->getName() + "_" +
                                 kOpNames[static_cast<int>(op_)] + "_" +
                                 scenes[idxB_]->getName() + "_" +
                                 std::to_string(count_++);
        auto* scene = world.addScene(name);
        scene->setShape(std::move(result));
        statusMsg_ = "Created: " + name +
                     " (" + std::to_string(scene->getShape()->getActiveVoxelCount()) + " vx)";
        if (onRebuild) onRebuild();
    }
    ImGui::EndDisabled();

    if (!statusMsg_.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", statusMsg_.c_str());
    }
}

} // namespace VkVolumeView

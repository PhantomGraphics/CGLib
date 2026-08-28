#include "SVMeshView.h"

#include "World.h"
#include "VolumeScene.h"

#include "../Volume/LevelSet.h"
#include "../Volume/SparseVolumeTree/SparseVolume.h"

#include "CGLib/File/File/STLFileReader.h"

#include "../../../CGLib/UIWidgets/FileOpenDialog.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace VkVolumeView {

void SVMeshView::onImGui(World& world, int /*activeSceneId*/,
                          const std::function<void()>& onRebuild)
{
    ImGui::InputText("STL file", pathBuf_, sizeof(pathBuf_));
    ImGui::SameLine();

    if (ImGui::SmallButton("Browse...")) {
        Phantom::UI::FileOpenDialog dlg("Load STL");
        dlg.addFilter("*.stl");
        dlg.show();
        const auto p = dlg.getFilePath();
        if (!p.empty()) {
            const auto s = p.string();
            // strncpy_s/_TRUNCATE is an MSVC CRT extension, unavailable on
            // Linux/glibc -- std::snprintf is the portable equivalent (always
            // null-terminates when size > 0, truncating silently like _TRUNCATE).
            std::snprintf(pathBuf_, sizeof(pathBuf_), "%s", s.c_str());
            statusMsg_.clear();
        }
    }

    ImGui::SliderFloat("Cell Size", &cellSize_, 0.05f, 10.0f);
    ImGui::TextDisabled("NOTE: computation time scales with triangle count.");

    const bool canApply = (pathBuf_[0] != '\0');
    ImGui::BeginDisabled(!canApply);
    if (ImGui::Button("Apply")) {
        const std::filesystem::path fpath(pathBuf_);
        Phantom::File::STLFileReader reader;
        const bool ok = Phantom::File::STLFileReader::isBinary(fpath)
                        ? reader.readBinary(fpath)
                        : reader.readAscii(fpath);

        if (!ok) {
            statusMsg_ = "Failed to read: " + std::string(pathBuf_);
        } else {
            const auto stl = reader.getSTL();
            std::vector<Phantom::Math::Triangle3df> triangles;
            triangles.reserve(stl.faces.size());
            for (const auto& f : stl.faces)
                triangles.push_back(f.triangle);

            if (triangles.empty()) {
                statusMsg_ = "No triangles in file.";
            } else {
                const float cell = std::max(cellSize_, 0.05f);
                auto volume = std::make_unique<Phantom::Volume::SparseVolumef>(1e6f);
                volume->setVoxelSize(cell);

                Phantom::Volume::LevelSet levelSet;
                levelSet.setSignedDistance(triangles, *volume);

                const std::string stem = fpath.stem().string();
                const std::string name = stem + "_sdf_" + std::to_string(count_++);
                auto* scene = world.addScene(name);
                scene->setShape(std::move(volume));
                statusMsg_ = "Created: " + name +
                             " (" + std::to_string(scene->getShape()->getActiveVoxelCount()) + " vx)";
                if (onRebuild) onRebuild();
            }
        }
    }
    ImGui::EndDisabled();

    if (!statusMsg_.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", statusMsg_.c_str());
    }
}

} // namespace VkVolumeView

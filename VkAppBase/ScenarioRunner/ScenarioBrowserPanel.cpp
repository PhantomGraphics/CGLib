#include "ScenarioBrowserPanel.h"
#include "imgui.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// ---- setup -----------------------------------------------------------

void ScenarioBrowserPanel::setDefaultFolder(const std::string& folder) {
    const size_t n = std::min(folder.size(), sizeof(folderBuf_) - 1);
    std::memcpy(folderBuf_, folder.data(), n);
    folderBuf_[n] = '\0';
}

// ---- folder scan -------------------------------------------------------

void ScenarioBrowserPanel::refresh() {
    entries_.clear();

    std::error_code ec;
    const fs::path dir(folderBuf_);
    if (!fs::is_directory(dir, ec)) return;

    std::vector<fs::path> files;
    for (const auto& de : fs::directory_iterator(dir, ec)) {
        if (de.is_regular_file() && de.path().extension() == ".json")
            files.push_back(de.path());
    }
    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
        return a.filename().string() < b.filename().string();
    });

    for (const auto& p : files) {
        Entry e;
        e.path = p.string();
        e.name = p.filename().string();
        entries_.push_back(std::move(e));
    }
}

// ---- queue state machine (call once per frame, before drawing) --------

void ScenarioBrowserPanel::tickQueue() {
    if (!host_) return;

    if (currentIndex_ >= 0 && !host_->isScenarioActive()) {
        Entry& e = entries_[(size_t)currentIndex_];
        e.status      = host_->scenarioHasFailed() ? Entry::Status::Failed : Entry::Status::Passed;
        e.steps       = host_->scenarioStepCount();
        e.failMessage = host_->scenarioFailMessage();
        currentIndex_ = -1;
    }

    if (currentIndex_ < 0 && !queue_.empty()) {
        const size_t idx = queue_.front();
        queue_.erase(queue_.begin());

        Entry& e = entries_[idx];
        e.status = Entry::Status::Running;
        e.failMessage.clear();

        // GUI-driven runs must never close the window on completion.
        host_->setExitOnScenarioComplete(false);
        if (host_->loadScenario(e.path)) {
            currentIndex_ = (int)idx;
        } else {
            e.status      = Entry::Status::Failed;
            e.failMessage = "failed to load";
        }
    }
}

// ---- rendering ----------------------------------------------------------

void ScenarioBrowserPanel::onImGui() {
    tickQueue();

    if (!visible_) return;

    if (!initialized_) {
        refresh();
        initialized_ = true;
    }

    ImGui::SetNextWindowSize(ImVec2(440, 380), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scenario Browser", &visible_)) {
        ImGui::End();
        return;
    }

    drawUI();

    ImGui::End();
}

void ScenarioBrowserPanel::drawUI() {
    const bool running = (currentIndex_ >= 0) || !queue_.empty();

    ImGui::SetNextItemWidth(-90.f);
    const bool folderSubmitted = ImGui::InputText(
        "##folder", folderBuf_, sizeof(folderBuf_), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::BeginDisabled(running);
    const bool refreshClicked = ImGui::Button("Refresh");
    ImGui::EndDisabled();
    if (!running && (folderSubmitted || refreshClicked)) refresh();

    ImGui::Separator();

    ImGui::BeginDisabled(running || entries_.empty());
    if (ImGui::SmallButton("Select All")) {
        for (auto& e : entries_) e.checked = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Select None")) {
        for (auto& e : entries_) e.checked = false;
    }
    ImGui::EndDisabled();

    if (ImGui::BeginTable("##scenarios", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                          ImVec2(0.f, 200.f))) {
        ImGui::TableSetupColumn("##chk", ImGuiTableColumnFlags_WidthFixed, 24.f);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.f);
        ImGui::TableSetupColumn("Steps", ImGuiTableColumnFlags_WidthFixed, 50.f);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < entries_.size(); ++i) {
            Entry& e = entries_[i];
            ImGui::PushID((int)i);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::BeginDisabled(running);
            ImGui::Checkbox("##sel", &e.checked);
            ImGui::EndDisabled();

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(e.name.c_str());
            ImGui::SameLine();
            ImGui::BeginDisabled(running);
            if (ImGui::SmallButton("Run")) {
                queue_.clear();
                queue_.push_back(i);
            }
            ImGui::EndDisabled();

            ImGui::TableSetColumnIndex(2);
            switch (e.status) {
            case Entry::Status::Pending:
                ImGui::TextDisabled("-");
                break;
            case Entry::Status::Running:
                ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.30f, 1.f), "RUN");
                break;
            case Entry::Status::Passed:
                ImGui::TextColored(ImVec4(0.40f, 1.00f, 0.40f, 1.f), "PASS");
                break;
            case Entry::Status::Failed:
                ImGui::TextColored(ImVec4(1.00f, 0.40f, 0.40f, 1.f), "FAIL");
                if (ImGui::IsItemHovered() && !e.failMessage.empty())
                    ImGui::SetTooltip("%s", e.failMessage.c_str());
                break;
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%zu", e.steps);

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Separator();

    ImGui::BeginDisabled(running || entries_.empty());
    if (ImGui::Button("Run Selected")) {
        queue_.clear();
        for (size_t i = 0; i < entries_.size(); ++i)
            if (entries_[i].checked) queue_.push_back(i);
    }
    ImGui::SameLine();
    if (ImGui::Button("Run All")) {
        queue_.clear();
        for (size_t i = 0; i < entries_.size(); ++i) queue_.push_back(i);
    }
    ImGui::EndDisabled();

    if (running) {
        ImGui::SameLine();
        if (ImGui::Button("Stop After Current")) queue_.clear();
    }

    if (currentIndex_ >= 0) {
        ImGui::Text("Running: %s", entries_[(size_t)currentIndex_].name.c_str());
    } else if (!queue_.empty()) {
        ImGui::Text("Queued: %d remaining", (int)queue_.size());
    } else {
        int passed = 0, failed = 0;
        for (const auto& e : entries_) {
            if (e.status == Entry::Status::Passed) ++passed;
            else if (e.status == Entry::Status::Failed) ++failed;
        }
        if (passed + failed > 0)
            ImGui::Text("Result: %d PASSED, %d FAILED", passed, failed);
    }
}

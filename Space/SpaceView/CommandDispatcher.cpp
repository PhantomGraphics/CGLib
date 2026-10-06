#include "CommandDispatcher.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <limits>
#include <string>

namespace VKSpace {

namespace {
bool parseDouble(const std::string& s, double& out) {
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{};
}
} // namespace

void CommandDispatcher::dispatch(const std::string& command) {
    queue_.submit(command);
}

std::vector<std::string> CommandDispatcher::collectResponses() {
    return queue_.collectResponses();
}

std::vector<CommandInfo> CommandDispatcher::commandCatalog() const {
    return {
        {"SetAlgorithm", "SpaceHash|CompactSpaceHash|KDTree|Octree|SignedDistance", "Select the active algorithm"},
        {"Run", "", "Run the active algorithm"},
        {"SetParam", "name:value", "Set a parameter of the active algorithm"},
        {"GetLineCount", "", ""},
        {"GetPointCount", "", ""},
        {"GetPointPositionMin", "", ""},
        {"GetPointPositionMax", "", ""},
        {"GetDirty", "", ""},
        {"ResetCamera", "", ""},
        {"GetCameraDistance", "", ""},
        {"Scroll", "dy", "Zoom as a mouse-wheel scroll"},
    };
}

std::string CommandDispatcher::cmdCheckCommandCatalog() {
    // Run/SetParam write into the world, so probe against a scratch one.
    World scratch;
    World* const saved = world_;
    world_ = &scratch;
    std::string missing;
    for (const auto& c : commandCatalog()) {
        const std::string probe = c.args.empty() ? c.name : c.name + ":x";
        if (route(probe).rfind("Error:unknown command", 0) == 0)
            missing += (missing.empty() ? "" : ",") + c.name;
    }
    world_ = saved;
    return missing.empty() ? "OK" : "Error:unrouted catalog entries: " + missing;
}

void CommandDispatcher::processQueue() {
    std::queue<std::string> local = queue_.takeAll();

    while (!local.empty()) {
        std::string cmd = std::move(local.front());
        local.pop();
        const bool fromUi = takeUiMark(cmd);
        std::string resp = route(cmd);
        if (fromUi) continue;
        queue_.respond(std::move(resp));
    }
}

std::string CommandDispatcher::route(const std::string& cmd) {
    if (cmd == "CheckCommandCatalog") return cmdCheckCommandCatalog();

    if (cmd.rfind("SetAlgorithm:", 0) == 0) {
        if (!menuPanel_) return "Error:no panel";
        menuPanel_->setActiveByName(cmd.substr(13));
        return "OK";
    }

    if (cmd == "Run") {
        if (!menuPanel_ || !world_) return "Error:not initialized";
        menuPanel_->runActive(*world_);
        return "OK";
    }

    if (cmd.rfind("SetParam:", 0) == 0) {
        const std::string rest  = cmd.substr(9);
        const auto        colon = rest.find(':');
        if (colon == std::string::npos) return "Error:bad SetParam format";
        const std::string name = rest.substr(0, colon);
        const std::string val  = rest.substr(colon + 1);
        if (menuPanel_ && menuPanel_->setActiveParam(name, val)) return "OK";
        return "Error:unknown param " + name;
    }

    if (cmd == "GetLineCount") {
        if (!world_) return "Error:no world";
        return "LineCount:" + std::to_string(world_->getResult().lineIndices.size());
    }

    if (cmd == "GetPointCount") {
        if (!world_) return "Error:no world";
        return "PointCount:" + std::to_string(world_->getResult().pointSizes.size());
    }

    if (cmd == "GetPointPositionMin") {
        if (!world_) return "Error:no world";
        const auto& pts = world_->getResult().pointPositions;
        if (pts.empty()) return "0";
        float mn = pts[0];
        for (float v : pts) mn = std::min(mn, v);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%f", mn);
        return buf;
    }

    if (cmd == "GetPointPositionMax") {
        if (!world_) return "Error:no world";
        const auto& pts = world_->getResult().pointPositions;
        if (pts.empty()) return "0";
        float mx = pts[0];
        for (float v : pts) mx = std::max(mx, v);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%f", mx);
        return buf;
    }

    if (cmd == "GetDirty") {
        if (!world_) return "Error:no world";
        return world_->getResult().dirty ? "Dirty:1" : "Dirty:0";
    }

    if (cmd == "ResetCamera") {
        if (renderer_) renderer_->resetCamera();
        return "OK";
    }

    if (cmd == "GetCameraDistance") {
        if (!renderer_) return "Error:no renderer";
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%f", renderer_->getCameraDistance());
        return buf;
    }

    if (cmd.rfind("Scroll:", 0) == 0) {
        double dy = 0.0;
        if (!parseDouble(cmd.substr(7), dy)) return "Error:bad Scroll value";
        if (renderer_) renderer_->handleScroll(dy);
        return "OK";
    }

    return "Error:unknown command '" + cmd + "'";
}

} // namespace VKSpace

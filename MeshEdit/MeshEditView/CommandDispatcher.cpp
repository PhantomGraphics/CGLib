#include "CommandDispatcher.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

using namespace Phantom::MeshEdit;

namespace Phantom::MeshView {

namespace {

bool parseDouble(const std::string& s, double& out) {
    double v = 0.0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    // from_chars accepts "nan"/"inf", which would poison the camera and every geometry operation.
    if (s.empty() || ec != std::errc{} || ptr != s.data() + s.size() || !std::isfinite(v)) return false;
    out = v;
    return true;
}

bool parseFloat(const std::string& s, float& out) {
    double v = 0.0;
    if (!parseDouble(s, v)) return false;
    out = static_cast<float>(v);
    return true;
}

bool parseBool(const std::string& s, bool& out) {
    if (s == "1" || s == "true") { out = true; return true; }
    if (s == "0" || s == "false") { out = false; return true; }
    return false;
}

std::string lowered(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::vector<std::string> splitOn(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream stream(s);
    std::string part;
    while (std::getline(stream, part, sep)) out.push_back(part);
    return out;
}

// "Verb:first key=value key=value": the verb, the positional argument(s) and the options.
struct Command {
    std::string verb;
    std::vector<std::string> positional;
    std::map<std::string, std::string> options;
};

Command parse(const std::string& line) {
    Command c;
    const auto colon = line.find(':');
    c.verb = colon == std::string::npos ? line : line.substr(0, colon);
    if (colon == std::string::npos) return c;
    std::stringstream stream(line.substr(colon + 1));
    std::string token;
    while (stream >> token) {
        const auto eq = token.find('=');
        if (eq == std::string::npos) c.positional.push_back(token);
        else c.options[lowered(token.substr(0, eq))] = token.substr(eq + 1);
    }
    return c;
}

std::string fail(const std::string& message) { return "Error:" + message; }
std::string result(const std::string& error) { return error.empty() ? "OK" : fail(error); }

const char* kindName(ElementKind k) { return elementKindName(k); }

// Option value as a float, or `fallback` when absent; false on a malformed number.
bool optFloat(const Command& c, const char* key, float fallback, float& out) {
    const auto it = c.options.find(key);
    if (it == c.options.end()) { out = fallback; return true; }
    return parseFloat(it->second, out);
}

bool optMode(const Command& c, SelectMode& out) {
    const auto it = c.options.find("mode");
    if (it == c.options.end()) { out = SelectMode::Replace; return true; }
    return selectModeFromName(it->second, out);
}

}  // namespace

void CommandDispatcher::dispatch(const std::string& command) { queue_.submit(command); }

std::vector<std::string> CommandDispatcher::collectResponses() { return queue_.collectResponses(); }

std::vector<CommandInfo> CommandDispatcher::commandCatalog() const {
    return {
        {"NewMesh", "box|plane [divs=N]", "Replace the mesh with a box or a divided plane"},
        {"GetCounts", "", "Counts:vertices,edges,faces of the editing mesh"},
        {"GetEvaluatedCounts", "", "Counts after the modifier stack"},
        {"SetMode", "edit|object", "Enter or leave edit mode"},
        {"GetMode", "", ""},
        {"SelectElement", "vertex|edge|face", "Choose the element type to select"},
        {"Select", "ids|all|none [mode=replace|add|toggle]", "Select elements by id (0,1,4-6)"},
        {"GetSelection", "", "Selection:kind:count:ids"},
        {"Pick", "x,y [mode=]", "Select the element under a pixel"},
        {"BoxSelect", "x0,y0,x1,y1 [mode=]", "Select the elements inside a pixel rectangle"},
        {"ProjectVertex", "id", "Pixel:x,y of a vertex in the current view"},
        {"SetXray", "0|1", "Pick / box select reach hidden elements"},
        {"Extrude", "distance=d", "Extrude the selected faces"},
        {"Inset", "thickness=t [depth=d]", "Inset the selected faces"},
        {"Bevel", "width=w", "Bevel the selected edges"},
        {"LoopCut", "factor=f", "Cut the quad ring through the selected edge"},
        {"Shade", "smooth|flat [angle=a]", "Shade the selected faces"},
        {"Merge", "center|first|last", "Merge the selected vertices"},
        {"Delete", "", "Delete the selected elements"},
        {"Move", "dx,dy,dz", "Move the selected elements"},
        {"AddModifier", "mirror [axis=xy] | subdivision [levels=n]", "Append a modifier"},
        {"ClearModifiers", "", ""},
        {"ApplyModifiers", "", "Bake the modifier stack into the mesh"},
        {"GetModifierCount", "", ""},
        {"Undo", "", ""},
        {"Redo", "", ""},
        {"GetHistory", "", "History:undo=n,redo=m"},
        {"SetDisplay", "faces= edges= vertices= normals= orientation=", "Display options (0|1)"},
        {"GetDisplay", "", ""},
        {"GetOrientation", "", "Orientation:front=n,back=m triangles seen from the camera"},
        {"GetVolume", "", "Volume:signed,closed=0|1 -- positive when counter-clockwise from outside"},
        {"ResetCamera", "", ""},
        {"SetCamera", "theta,phi,distance", ""},
        {"GetCameraDistance", "", ""},
        {"Scroll", "dy", "Zoom as a mouse-wheel scroll"},
    };
}

std::string CommandDispatcher::cmdCheckCommandCatalog() {
    // Operations write into the world, so probe against a scratch one.
    MeshWorld scratch;
    MeshWorld* const saved = world_;
    world_ = &scratch;
    std::string missing;
    for (const auto& c : commandCatalog()) {
        const std::string probe = c.args.empty() ? c.name : c.name + ":x";
        if (route(probe).rfind("Error:unknown command", 0) == 0) missing += (missing.empty() ? "" : ",") + c.name;
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

std::string CommandDispatcher::route(const std::string& line) {
    if (line == "CheckCommandCatalog") return cmdCheckCommandCatalog();
    if (!world_) return fail("no world");
    MeshWorld& w = *world_;
    const Command c = parse(line);
    const std::string first = c.positional.empty() ? std::string() : c.positional[0];
    char buf[96];

    if (c.verb == "NewMesh") {
        Shape shape;
        if (first == "box") shape = Shape::Box;
        else if (first == "plane") shape = Shape::Plane;
        else return fail("expected NewMesh:box|plane");
        float divs = 1.f;
        if (!optFloat(c, "divs", 1.f, divs) || divs < 1.f || divs > 64.f) return fail("divs must be 1..64");
        w.newMesh(shape, static_cast<int>(divs));
        return "OK";
    }
    if (c.verb == "GetCounts" || c.verb == "GetEvaluatedCounts") {
        const Counts n = c.verb == "GetCounts" ? w.baseCounts() : w.evaluatedCounts();
        std::snprintf(buf, sizeof(buf), "Counts:%zu,%zu,%zu", n.vertices, n.edges, n.faces);
        return buf;
    }
    if (c.verb == "SetMode") {
        if (first != "edit" && first != "object") return fail("expected SetMode:edit|object");
        return result(w.setEditMode(first == "edit"));
    }
    if (c.verb == "GetMode") return w.editMode() ? "Mode:edit" : "Mode:object";
    if (c.verb == "SelectElement") {
        ElementKind kind;
        if (!elementKindFromName(first, kind)) return fail("expected SelectElement:vertex|edge|face");
        return result(w.setElementKind(kind));
    }
    if (c.verb == "Select") {
        SelectMode mode;
        if (c.positional.empty() || !optMode(c, mode)) return fail("expected Select:<ids|all|none> [mode=replace|add|toggle]");
        return result(w.select(first, mode));
    }
    if (c.verb == "GetSelection") {
        std::string ids;
        for (uint32_t id : w.edit().ids) ids += (ids.empty() ? "" : ",") + std::to_string(id);
        return std::string("Selection:") + kindName(w.edit().kind) + ":" + std::to_string(w.edit().ids.size()) + ":" + ids;
    }
    if (c.verb == "Pick" || c.verb == "BoxSelect") {
        const bool box = c.verb == "BoxSelect";
        SelectMode mode;
        std::vector<double> n;
        if (!camera_.pickView) return fail("no camera");
        if (c.positional.empty() || !optMode(c, mode)) return fail("expected " + c.verb + ":" + (box ? "x0,y0,x1,y1" : "x,y") + " [mode=]");
        MeshArgError argError;
        if (!parseNumberList(first, box ? 4u : 2u, n, argError)) return fail(argError.message);
        const MeshPickView view = camera_.pickView();
        return result(box ? w.boxSelect(view, n[0], n[1], n[2], n[3], mode) : w.pick(view, n[0], n[1], mode));
    }
    if (c.verb == "ProjectVertex") {
        double id = 0;
        if (!camera_.pickView || !parseDouble(first, id) || id < 0) return fail("expected ProjectVertex:<vertex id>");
        const EditableVertex* v = w.base().findVertex(static_cast<uint32_t>(id));
        if (!v) return fail("vertex " + first + " does not exist");
        glm::vec2 px;
        if (!projectToPixels(camera_.pickView(), v->pos, px)) return fail("the vertex is behind the camera");
        std::snprintf(buf, sizeof(buf), "Pixel:%.1f,%.1f", px.x, px.y);
        return buf;
    }
    if (c.verb == "SetXray") {
        bool on = false;
        if (!parseBool(first, on)) return fail("expected SetXray:0|1");
        w.setXray(on);
        return "OK";
    }
    if (c.verb == "Extrude") {
        float d = 0.f;
        if (!c.options.count("distance") || !optFloat(c, "distance", 0.f, d)) return fail("expected Extrude:distance=<number>");
        return result(w.extrude(d));
    }
    if (c.verb == "Inset") {
        float t = 0.f, depth = 0.f;
        if (!c.options.count("thickness") || !optFloat(c, "thickness", 0.f, t) || !optFloat(c, "depth", 0.f, depth))
            return fail("expected Inset:thickness=<number> [depth=<number>]");
        return result(w.inset(t, depth));
    }
    if (c.verb == "Bevel") {
        float width = 0.f;
        if (!c.options.count("width") || !optFloat(c, "width", 0.f, width)) return fail("expected Bevel:width=<number>");
        return result(w.bevel(width));
    }
    if (c.verb == "LoopCut") {
        float f = 0.5f;
        if (!optFloat(c, "factor", 0.5f, f)) return fail("factor must be a number");
        return result(w.loopCut(f));
    }
    if (c.verb == "Shade") {
        float angle = 180.f;
        if ((first != "smooth" && first != "flat") || !optFloat(c, "angle", 180.f, angle)) return fail("expected Shade:smooth|flat [angle=<degrees>]");
        return result(w.shade(first == "smooth", angle));
    }
    if (c.verb == "Merge") {
        MergeTarget target = MergeTarget::Center;
        if (!first.empty() && !mergeTargetFromName(first, target)) return fail("expected Merge:center|first|last");
        return result(w.merge(target));
    }
    if (c.verb == "Delete") return result(w.deleteSelected());
    if (c.verb == "Move") {
        std::vector<double> n;
        MeshArgError argError;
        if (!parseNumberList(first, 3, n, argError)) return fail("expected Move:dx,dy,dz");
        return result(w.moveSelected({ static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2]) }));
    }
    if (c.verb == "AddModifier") {
        MeshModifier m;
        if (first == "mirror") {
            MeshArgError argError;
            const auto axis = c.options.find("axis");
            if (!parseMirrorAxes(axis == c.options.end() ? std::string("x") : axis->second, m.axis, argError)) return fail(argError.message);
            if (!optFloat(c, "merge", m.mergeDistance, m.mergeDistance) || m.mergeDistance < 0.f) return fail("merge must be a non-negative number");
        } else if (first == "subdivision") {
            m.type = MeshModifier::Type::Subdivision;
            float levels = 1.f;
            if (!optFloat(c, "levels", 1.f, levels) || levels < 1.f || levels > 4.f || levels != std::floor(levels)) return fail("levels must be an integer from 1 to 4");
            m.levels = static_cast<int>(levels);
            const auto smooth = c.options.find("smooth");
            if (smooth != c.options.end() && !parseBool(smooth->second, m.smoothShading)) return fail("smooth must be 0 or 1");
        } else {
            return fail("expected AddModifier:mirror|subdivision");
        }
        return result(w.addModifier(m));
    }
    if (c.verb == "ClearModifiers") return result(w.clearModifiers());
    if (c.verb == "ApplyModifiers") return result(w.applyModifiers());
    if (c.verb == "GetModifierCount") return "Modifiers:" + std::to_string(w.modifiers().size());
    if (c.verb == "Undo") return w.undo() ? "OK" : fail("nothing to undo");
    if (c.verb == "Redo") return w.redo() ? "OK" : fail("nothing to redo");
    if (c.verb == "GetHistory") {
        std::snprintf(buf, sizeof(buf), "History:undo=%zu,redo=%zu", w.undoDepth(), w.redoDepth());
        return buf;
    }
    if (c.verb == "SetDisplay") {
        Display d = w.display();
        const std::pair<const char*, bool*> keys[] = { {"faces", &d.faces}, {"edges", &d.edges}, {"vertices", &d.vertices},
                                                        {"normals", &d.normals}, {"orientation", &d.orientation} };
        for (const auto& [key, value] : c.options) {
            bool known = false;
            for (const auto& k : keys)
                if (value.size() && key == k.first) { known = true; if (!parseBool(value, *k.second)) return fail(key + " must be 0 or 1"); }
            if (!known) return fail("unknown display option " + key);
        }
        w.setDisplay(d);
        return "OK";
    }
    if (c.verb == "GetDisplay") {
        const Display& d = w.display();
        std::snprintf(buf, sizeof(buf), "Display:faces=%d,edges=%d,vertices=%d,normals=%d,orientation=%d", d.faces, d.edges, d.vertices, d.normals, d.orientation);
        return buf;
    }
    if (c.verb == "GetOrientation") {
        if (!camera_.eye) return fail("no camera");
        const Facing f = w.facing(camera_.eye());
        std::snprintf(buf, sizeof(buf), "Orientation:front=%zu,back=%zu", f.front, f.back);
        return buf;
    }
    if (c.verb == "GetVolume") {
        bool closed = false;
        const double v = w.signedVolume(&closed);
        std::snprintf(buf, sizeof(buf), "Volume:%.6f,closed=%d", v, closed ? 1 : 0);
        return buf;
    }
    if (c.verb == "ResetCamera") {
        if (camera_.reset) camera_.reset();
        return "OK";
    }
    if (c.verb == "SetCamera") {
        std::vector<double> n;
        MeshArgError argError;
        if (!camera_.set || !parseNumberList(first, 3, n, argError)) return fail("expected SetCamera:theta,phi,distance");
        camera_.set(static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2]));
        return "OK";
    }
    if (c.verb == "GetCameraDistance") {
        if (!camera_.distance) return fail("no camera");
        std::snprintf(buf, sizeof(buf), "%.4f", camera_.distance());
        return buf;
    }
    if (c.verb == "Scroll") {
        double dy = 0;
        if (!camera_.scroll || !parseDouble(first, dy)) return fail("expected Scroll:<dy>");
        camera_.scroll(dy);
        return "OK";
    }
    return fail("unknown command " + c.verb);
}

}  // namespace MeshView

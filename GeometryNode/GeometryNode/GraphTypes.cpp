#include "GraphTypes.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Phantom::GeometryNode {

const char* toString(SocketType t) {
    switch (t) {
        case SocketType::Geometry: return "Geometry";
        case SocketType::Float: return "Float";
        case SocketType::Int: return "Int";
        case SocketType::Bool: return "Bool";
        case SocketType::Vector3: return "Vector3";
        case SocketType::FieldFloat: return "FieldFloat";
        case SocketType::FieldVector3: return "FieldVector3";
        case SocketType::FieldBool: return "FieldBool";
    }
    return "Unknown";
}

bool socketTypeFromString(const std::string& s, SocketType& out) {
    for (SocketType t : {SocketType::Geometry, SocketType::Float, SocketType::Int, SocketType::Bool, SocketType::Vector3,
                         SocketType::FieldFloat, SocketType::FieldVector3, SocketType::FieldBool}) {
        if (s == toString(t)) {
            out = t;
            return true;
        }
    }
    return false;
}

bool valueType(const Value& v, SocketType& out) {
    switch (v.index()) {
        case 1: out = SocketType::Geometry; return true;
        case 2: out = SocketType::Float; return true;
        case 3: out = SocketType::Int; return true;
        case 4: out = SocketType::Bool; return true;
        case 5: out = SocketType::Vector3; return true;
        case 6: {
            const FieldPtr& f = std::get<FieldPtr>(v);
            if (!f) return false;
            out = f->type == FieldType::Float ? SocketType::FieldFloat
                : f->type == FieldType::Vector3 ? SocketType::FieldVector3 : SocketType::FieldBool;
            return true;
        }
        default: return false;
    }
}

bool paramFromJson(SocketType type, const nlohmann::json& j, Value& out) {
    switch (type) {
        case SocketType::Float:
            if (!j.is_number()) return false;
            {
                const double d = j.get<double>();
                if (!std::isfinite(d) || std::fabs(d) > std::numeric_limits<float>::max()) return false;
                out = static_cast<float>(d);
            }
            return true;
        case SocketType::Int:
            if (!j.is_number_integer()) return false;
            {
                const int64_t i = j.get<int64_t>();
                if (i < std::numeric_limits<int32_t>::min() || i > std::numeric_limits<int32_t>::max()) return false;
                out = static_cast<int32_t>(i);
            }
            return true;
        case SocketType::Bool:
            if (!j.is_boolean()) return false;
            out = j.get<bool>();
            return true;
        case SocketType::Vector3: {
            if (!j.is_array() || j.size() != 3) return false;
            float c[3];
            for (size_t i = 0; i < 3; ++i) {
                if (!j[i].is_number()) return false;
                const double d = j[i].get<double>();
                if (!std::isfinite(d) || std::fabs(d) > std::numeric_limits<float>::max()) return false;
                c[i] = static_cast<float>(d);
            }
            out = Vec3{c[0], c[1], c[2]};
            return true;
        }
        case SocketType::Geometry:
        case SocketType::FieldFloat:
        case SocketType::FieldVector3:
        case SocketType::FieldBool:
            return false;  // no literal form
    }
    return false;
}

bool paramToJson(const Value& v, nlohmann::json& out) {
    if (const float* f = std::get_if<float>(&v)) {
        out = *f;
        return true;
    }
    if (const int32_t* i = std::get_if<int32_t>(&v)) {
        out = *i;
        return true;
    }
    if (const bool* b = std::get_if<bool>(&v)) {
        out = *b;
        return true;
    }
    if (const Vec3* p = std::get_if<Vec3>(&v)) {
        out = nlohmann::json::array({p->x, p->y, p->z});
        return true;
    }
    return false;
}

const Node* Graph::findNode(NodeId id) const {
    for (const Node& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}

Node* Graph::findNode(NodeId id) {
    for (Node& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}

Node& addNode(Graph& g, const std::string& type, uint32_t version) {
    NodeId next = 1;
    for (const Node& n : g.nodes) next = std::max(next, n.id + 1);
    Node n;
    n.id = next;
    n.type = type;
    n.version = version;
    g.nodes.push_back(std::move(n));
    return g.nodes.back();
}

void addLink(Graph& g, NodeId from, const std::string& fromSocket, NodeId to, const std::string& toSocket) {
    g.links.push_back(Link{Endpoint{from, fromSocket}, Endpoint{to, toSocket}});
}

void removeNode(Graph& g, NodeId id) {
    g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [id](const Node& n) { return n.id == id; }),
                  g.nodes.end());
    g.links.erase(std::remove_if(g.links.begin(), g.links.end(),
                                 [id](const Link& l) { return l.from.node == id || l.to.node == id; }),
                  g.links.end());
    g.layout.erase(id);
}

bool setParam(Node& n, const std::string& socket, const Value& v) {
    nlohmann::json j;
    if (!paramToJson(v, j)) return false;
    n.params[socket] = std::move(j);
    return true;
}

const char* toString(DiagCode c) {
    switch (c) {
        case DiagCode::DuplicateNodeId: return "DuplicateNodeId";
        case DiagCode::InvalidNodeId: return "InvalidNodeId";
        case DiagCode::UnknownNodeType: return "UnknownNodeType";
        case DiagCode::UnsupportedNodeVersion: return "UnsupportedNodeVersion";
        case DiagCode::UnknownSocket: return "UnknownSocket";
        case DiagCode::UnknownParameter: return "UnknownParameter";
        case DiagCode::InvalidParameter: return "InvalidParameter";
        case DiagCode::TypeMismatch: return "TypeMismatch";
        case DiagCode::MultipleLinks: return "MultipleLinks";
        case DiagCode::MissingInput: return "MissingInput";
        case DiagCode::Cycle: return "Cycle";
        case DiagCode::NoOutputNode: return "NoOutputNode";
        case DiagCode::MultipleOutputNodes: return "MultipleOutputNodes";
        case DiagCode::LimitExceeded: return "LimitExceeded";
        case DiagCode::InvalidGeometry: return "InvalidGeometry";
        case DiagCode::SingularTransform: return "SingularTransform";
        case DiagCode::FieldMismatch: return "FieldMismatch";
        case DiagCode::Cancelled: return "Cancelled";
        case DiagCode::Upstream: return "Upstream";
        case DiagCode::Internal: return "Internal";
    }
    return "Unknown";
}

bool hasErrors(const std::vector<Diagnostic>& d) {
    for (const Diagnostic& x : d)
        if (x.severity == Severity::Error) return true;
    return false;
}

}  // namespace Phantom::GeometryNode

#include "GeometryOps.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Phantom::GeometryNode {

namespace {

constexpr double kPi = 3.14159265358979323846;

bool finite3(const Vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

uint64_t satMul(uint64_t a, uint64_t b) {
    if (a != 0 && b > std::numeric_limits<uint64_t>::max() / a) return std::numeric_limits<uint64_t>::max();
    return a * b;
}

uint64_t satAdd(uint64_t a, uint64_t b) {
    if (b > std::numeric_limits<uint64_t>::max() - a) return std::numeric_limits<uint64_t>::max();
    return a + b;
}

void setMessage(std::string* message, const char* text) {
    if (message) *message = text;
}

}  // namespace

const char* toString(OpStatus s) {
    switch (s) {
        case OpStatus::Ok: return "Ok";
        case OpStatus::InvalidArgument: return "InvalidArgument";
        case OpStatus::LimitExceeded: return "LimitExceeded";
        case OpStatus::SingularTransform: return "SingularTransform";
        case OpStatus::InvalidMesh: return "InvalidMesh";
    }
    return "Unknown";
}

uint64_t estimateMeshBytes(uint64_t vertices, uint64_t indices) {
    const uint64_t perVertex = sizeof(Vec3) * 2 + sizeof(Vec2);
    return satAdd(satMul(vertices, perVertex), satMul(indices, sizeof(uint32_t)));
}

bool fitsLimits(uint64_t vertices, uint64_t indices, const Limits& limits) {
    if (vertices > limits.maxVertices || indices > limits.maxIndices) return false;
    // Indices are uint32_t.
    if (vertices > std::numeric_limits<uint32_t>::max()) return false;
    return estimateMeshBytes(vertices, indices) <= limits.maxMemoryBytes;
}

OpStatus validateMesh(const Mesh& mesh, std::string* message) {
    const size_t n = mesh.positions.size();
    if (!mesh.normals.empty() && mesh.normals.size() != n) {
        setMessage(message, "normals size differs from positions size");
        return OpStatus::InvalidMesh;
    }
    if (!mesh.uvs.empty() && mesh.uvs.size() != n) {
        setMessage(message, "uvs size differs from positions size");
        return OpStatus::InvalidMesh;
    }
    if (mesh.indices.size() % 3 != 0) {
        setMessage(message, "index count is not a multiple of 3");
        return OpStatus::InvalidMesh;
    }
    for (const Vec3& p : mesh.positions) {
        if (!finite3(p)) {
            setMessage(message, "non-finite position");
            return OpStatus::InvalidMesh;
        }
    }
    for (const Vec3& v : mesh.normals) {
        if (!finite3(v)) {
            setMessage(message, "non-finite normal");
            return OpStatus::InvalidMesh;
        }
    }
    for (const Vec2& v : mesh.uvs) {
        if (!std::isfinite(v.x) || !std::isfinite(v.y)) {
            setMessage(message, "non-finite uv");
            return OpStatus::InvalidMesh;
        }
    }
    for (uint32_t i : mesh.indices) {
        if (i >= n) {
            setMessage(message, "index out of range");
            return OpStatus::InvalidMesh;
        }
    }
    return OpStatus::Ok;
}

Bounds computeBounds(const Mesh& mesh) {
    Bounds b;
    if (mesh.positions.empty()) return b;
    b.valid = true;
    b.min = b.max = mesh.positions[0];
    for (const Vec3& p : mesh.positions) {
        b.min.x = std::min(b.min.x, p.x);
        b.min.y = std::min(b.min.y, p.y);
        b.min.z = std::min(b.min.z, p.z);
        b.max.x = std::max(b.max.x, p.x);
        b.max.y = std::max(b.max.y, p.y);
        b.max.z = std::max(b.max.z, p.z);
    }
    return b;
}

OpStatus makeBox(const Vec3& size, const Limits& limits, Mesh& out) {
    if (!finite3(size) || size.x < 0.0f || size.y < 0.0f || size.z < 0.0f) return OpStatus::InvalidArgument;
    if (!fitsLimits(24, 36, limits)) return OpStatus::LimitExceeded;

    const float h[3] = {size.x * 0.5f, size.y * 0.5f, size.z * 0.5f};
    Mesh m;
    m.positions.reserve(24);
    m.normals.reserve(24);
    m.uvs.reserve(24);
    m.indices.reserve(36);

    const Vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    const float ts[4] = {-1, 1, 1, -1};
    const float bs[4] = {-1, -1, 1, 1};
    for (int axis = 0; axis < 3; ++axis) {
        for (int sign = 1; sign >= -1; sign -= 2) {
            // Cyclic axes (n, t, b) satisfy t x b = n; the negative face swaps t and b.
            int ta = (axis + 1) % 3;
            int ba = (axis + 2) % 3;
            if (sign < 0) std::swap(ta, ba);
            const uint32_t base = static_cast<uint32_t>(m.positions.size());
            for (int c = 0; c < 4; ++c) {
                float p[3] = {0, 0, 0};
                float nrm[3] = {0, 0, 0};
                p[axis] = h[axis] * static_cast<float>(sign);
                p[ta] = h[ta] * ts[c];
                p[ba] = h[ba] * bs[c];
                nrm[axis] = static_cast<float>(sign);
                m.positions.push_back({p[0], p[1], p[2]});
                m.normals.push_back({nrm[0], nrm[1], nrm[2]});
                m.uvs.push_back(uv[c]);
            }
            const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
            for (uint32_t i : idx) m.indices.push_back(base + i);
        }
    }
    out = std::move(m);
    return OpStatus::Ok;
}

OpStatus makeGrid(float sizeX, float sizeZ, int32_t verticesX, int32_t verticesZ,
                  const Limits& limits, Mesh& out) {
    if (!std::isfinite(sizeX) || !std::isfinite(sizeZ) || sizeX < 0.0f || sizeZ < 0.0f) return OpStatus::InvalidArgument;
    if (verticesX < 2 || verticesZ < 2) return OpStatus::InvalidArgument;
    const uint64_t vx = static_cast<uint64_t>(verticesX);
    const uint64_t vz = static_cast<uint64_t>(verticesZ);
    const uint64_t verts = satMul(vx, vz);
    const uint64_t idx = satMul(satMul(vx - 1, vz - 1), 6);
    if (!fitsLimits(verts, idx, limits)) return OpStatus::LimitExceeded;

    Mesh m;
    m.positions.reserve(static_cast<size_t>(verts));
    m.normals.reserve(static_cast<size_t>(verts));
    m.uvs.reserve(static_cast<size_t>(verts));
    m.indices.reserve(static_cast<size_t>(idx));
    for (int32_t z = 0; z < verticesZ; ++z) {
        const float fz = static_cast<float>(z) / static_cast<float>(verticesZ - 1);
        for (int32_t x = 0; x < verticesX; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(verticesX - 1);
            m.positions.push_back({(fx - 0.5f) * sizeX, 0.0f, (fz - 0.5f) * sizeZ});
            m.normals.push_back({0.0f, 1.0f, 0.0f});
            m.uvs.push_back({fx, fz});
        }
    }
    for (int32_t z = 0; z + 1 < verticesZ; ++z) {
        for (int32_t x = 0; x + 1 < verticesX; ++x) {
            const uint32_t v00 = static_cast<uint32_t>(z * verticesX + x);
            const uint32_t v10 = v00 + 1;
            const uint32_t v01 = v00 + static_cast<uint32_t>(verticesX);
            const uint32_t v11 = v01 + 1;
            const uint32_t quad[6] = {v00, v01, v10, v10, v01, v11};
            for (uint32_t i : quad) m.indices.push_back(i);
        }
    }
    out = std::move(m);
    return OpStatus::Ok;
}

OpStatus transformMesh(const Mesh& in, const Vec3& translation, const Vec3& rotationDegrees,
                       const Vec3& scale, Mesh& out) {
    if (!finite3(translation) || !finite3(rotationDegrees) || !finite3(scale)) return OpStatus::InvalidArgument;
    const double eps = 1.0e-12;
    if (std::fabs(scale.x) < eps || std::fabs(scale.y) < eps || std::fabs(scale.z) < eps) {
        return OpStatus::SingularTransform;
    }
    if (OpStatus s = validateMesh(in); s != OpStatus::Ok) return s;

    const double rx = rotationDegrees.x * kPi / 180.0;
    const double ry = rotationDegrees.y * kPi / 180.0;
    const double rz = rotationDegrees.z * kPi / 180.0;
    const double cx = std::cos(rx), sx = std::sin(rx);
    const double cy = std::cos(ry), sy = std::sin(ry);
    const double cz = std::cos(rz), sz = std::sin(rz);
    // R = Rz * Ry * Rx, row-major.
    const double R[3][3] = {
        {cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx},
        {sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx},
        {-sy, cy * sx, cy * cx},
    };
    const double S[3] = {scale.x, scale.y, scale.z};
    double M[3][3];  // M = R * diag(S)
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) M[r][c] = R[r][c] * S[c];

    const double det = S[0] * S[1] * S[2];
    if (!std::isfinite(det) || std::fabs(det) < eps) return OpStatus::SingularTransform;

    // Normal matrix = cofactor(M) (= det * M^-T); the sign of det is removed so a
    // mirrored transform keeps normals pointing outwards.
    double N[3][3];
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            const int r1 = (r + 1) % 3, r2 = (r + 2) % 3;
            const int c1 = (c + 1) % 3, c2 = (c + 2) % 3;
            N[r][c] = M[r1][c1] * M[r2][c2] - M[r1][c2] * M[r2][c1];
            if (det < 0.0) N[r][c] = -N[r][c];
        }
    }

    Mesh m;
    m.positions.resize(in.positions.size());
    for (size_t i = 0; i < in.positions.size(); ++i) {
        const Vec3& p = in.positions[i];
        m.positions[i] = {
            static_cast<float>(M[0][0] * p.x + M[0][1] * p.y + M[0][2] * p.z + translation.x),
            static_cast<float>(M[1][0] * p.x + M[1][1] * p.y + M[1][2] * p.z + translation.y),
            static_cast<float>(M[2][0] * p.x + M[2][1] * p.y + M[2][2] * p.z + translation.z),
        };
    }
    if (!in.normals.empty()) {
        m.normals.resize(in.normals.size());
        for (size_t i = 0; i < in.normals.size(); ++i) {
            const Vec3& n = in.normals[i];
            double x = N[0][0] * n.x + N[0][1] * n.y + N[0][2] * n.z;
            double y = N[1][0] * n.x + N[1][1] * n.y + N[1][2] * n.z;
            double z = N[2][0] * n.x + N[2][1] * n.y + N[2][2] * n.z;
            const double len = std::sqrt(x * x + y * y + z * z);
            if (len > 0.0) {
                x /= len;
                y /= len;
                z /= len;
            }
            m.normals[i] = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
        }
    }
    m.uvs = in.uvs;
    m.indices = in.indices;
    if (det < 0.0) {
        for (size_t i = 0; i + 2 < m.indices.size(); i += 3) std::swap(m.indices[i + 1], m.indices[i + 2]);
    }
    if (OpStatus s = validateMesh(m); s != OpStatus::Ok) return s;  // overflow to Inf
    out = std::move(m);
    return OpStatus::Ok;
}

std::vector<Vec3> computeSmoothNormals(const Mesh& mesh) {
    std::vector<double> acc(mesh.positions.size() * 3, 0.0);
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const uint32_t tri[3] = {mesh.indices[t], mesh.indices[t + 1], mesh.indices[t + 2]};
        if (tri[0] >= mesh.positions.size() || tri[1] >= mesh.positions.size() || tri[2] >= mesh.positions.size()) continue;
        const Vec3& a = mesh.positions[tri[0]];
        const Vec3& b = mesh.positions[tri[1]];
        const Vec3& c = mesh.positions[tri[2]];
        const double ux = double(b.x) - a.x, uy = double(b.y) - a.y, uz = double(b.z) - a.z;
        const double vx = double(c.x) - a.x, vy = double(c.y) - a.y, vz = double(c.z) - a.z;
        const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;  // area-weighted
        for (uint32_t i : tri) {
            acc[i * 3 + 0] += nx;
            acc[i * 3 + 1] += ny;
            acc[i * 3 + 2] += nz;
        }
    }
    std::vector<Vec3> out(mesh.positions.size());
    for (size_t i = 0; i < out.size(); ++i) {
        const double x = acc[i * 3], y = acc[i * 3 + 1], z = acc[i * 3 + 2];
        const double len = std::sqrt(x * x + y * y + z * z);
        out[i] = len > 0.0 ? Vec3{float(x / len), float(y / len), float(z / len)} : Vec3{0, 1, 0};
    }
    return out;
}

OpStatus joinMeshes(const std::vector<GeometryPtr>& inputs, const Limits& limits, Mesh& out) {
    uint64_t verts = 0, idx = 0;
    bool anyNormals = false, anyUvs = false;
    for (const GeometryPtr& g : inputs) {
        if (!g) return OpStatus::InvalidArgument;
        verts = satAdd(verts, g->positions.size());
        idx = satAdd(idx, g->indices.size());
        anyNormals = anyNormals || !g->normals.empty();
        anyUvs = anyUvs || !g->uvs.empty();
    }
    if (!fitsLimits(verts, idx, limits)) return OpStatus::LimitExceeded;
    for (const GeometryPtr& g : inputs) {
        if (OpStatus s = validateMesh(*g); s != OpStatus::Ok) return s;
    }

    Mesh m;
    m.positions.reserve(static_cast<size_t>(verts));
    if (anyNormals) m.normals.reserve(static_cast<size_t>(verts));
    if (anyUvs) m.uvs.reserve(static_cast<size_t>(verts));
    m.indices.reserve(static_cast<size_t>(idx));
    for (const GeometryPtr& g : inputs) {
        const uint32_t offset = static_cast<uint32_t>(m.positions.size());  // < 2^32, checked by fitsLimits
        m.positions.insert(m.positions.end(), g->positions.begin(), g->positions.end());
        if (anyNormals) {
            if (!g->normals.empty()) {
                m.normals.insert(m.normals.end(), g->normals.begin(), g->normals.end());
            } else {
                const std::vector<Vec3> n = computeSmoothNormals(*g);
                m.normals.insert(m.normals.end(), n.begin(), n.end());
            }
        }
        if (anyUvs) {
            if (!g->uvs.empty()) m.uvs.insert(m.uvs.end(), g->uvs.begin(), g->uvs.end());
            else m.uvs.insert(m.uvs.end(), g->positions.size(), Vec2{0, 0});
        }
        for (uint32_t i : g->indices) m.indices.push_back(i + offset);
    }
    out = std::move(m);
    return OpStatus::Ok;
}

}  // namespace Phantom::GeometryNode

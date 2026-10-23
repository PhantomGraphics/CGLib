#pragma once
#include "MarchingCubesTable.h"
#include "../../Graphics/Core/MeshData.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace Phantom::Volume {
struct Metaball {
    int id = 1;
    std::array<float, 3> center{};
    float radius = 1.f, strength = 1.f;
};
struct MetaballModel {
    std::vector<Metaball> balls{{}};
    int nextId = 2;
    int resolution = 64; // cells along the longest side
    float isoLevel = .5f;
};
inline bool validateMetaballs(const MetaballModel& m, std::string& error) {
    auto bad = [&](const char* text) { error = text; return false; };
    if (m.balls.size() > 256 || m.resolution < 16 || m.resolution > 128 ||
        !std::isfinite(m.isoLevel) || m.isoLevel < .01f || m.isoLevel > 100.f)
        return bad("metaballs require at most 256 balls, resolution 16..128 and iso 0.01..100");
    std::map<int, bool> ids;
    for (const auto& b : m.balls) {
        if (b.id <= 0 || b.id >= m.nextId || !ids.emplace(b.id, true).second ||
            !std::isfinite(b.radius) || b.radius < .001f || b.radius > 10000.f ||
            !std::isfinite(b.strength) || std::abs(b.strength) > 100.f)
            return bad("invalid metaball id, radius (0.001..10000), or strength (-100..100)");
        for (float c : b.center) if (!std::isfinite(c) || std::abs(c) > 1000000.f) return bad("invalid metaball center");
    }
    if (m.nextId <= 0 || m.nextId == std::numeric_limits<int>::max()) return bad("metaball id budget exhausted");
    error.clear(); return true;
}
// Compact C2 kernel. Positive values are inside; the outward normal is minus the gradient.
inline float metaballField(const MetaballModel& m, const std::array<float, 3>& p, std::array<float, 3>* gradient = nullptr) {
    float field = 0.f;
    if (gradient) *gradient = {};
    for (const auto& b : m.balls) {
        float d2 = 0.f;
        for (int a = 0; a < 3; ++a) d2 += (p[a] - b.center[a]) * (p[a] - b.center[a]);
        const float rr = b.radius * b.radius;
        if (d2 >= rr) continue;
        const float t = 1.f - d2 / rr;
        field += b.strength * t * t * t;
        if (gradient) for (int a = 0; a < 3; ++a) (*gradient)[a] -= 6.f * b.strength * t * t * (p[a] - b.center[a]) / rr;
    }
    return field;
}

// Indexed marching cubes. Lattice-edge keys share vertices across adjacent cells. A fresh result is
// returned only on success; cancellation and budgets never expose a partially generated surface.
inline bool buildMetaballs(const MetaballModel& m, Graphics::MeshData& output, std::string& error,
                          int resolution = 0, const std::function<bool()>& cancelled = {}) {
    if (!validateMetaballs(m, error)) return false;
    std::array<float, 3> lo{INFINITY, INFINITY, INFINITY}, hi{-INFINITY, -INFINITY, -INFINITY};
    bool positive = false;
    for (const auto& b : m.balls) if (b.strength > 0.f) {
        positive = true;
        for (int a = 0; a < 3; ++a) { lo[a] = std::min(lo[a], b.center[a] - b.radius); hi[a] = std::max(hi[a], b.center[a] + b.radius); }
    }
    if (!positive) { output = {}; return true; }
    const int res = resolution ? resolution : m.resolution;
    if (res < 16 || res > 128) { error = "invalid generation resolution"; return false; }
    const float extent = std::max({hi[0]-lo[0], hi[1]-lo[1], hi[2]-lo[2]});
    const float step = extent / static_cast<float>(res);
    std::array<int, 3> n;
    for (int a = 0; a < 3; ++a) { n[a] = static_cast<int>(std::ceil((hi[a]-lo[a])/step)) + 3; lo[a] -= step; }
    const size_t samples = size_t(n[0])*n[1]*n[2];
    if (samples > 2300000 || samples * m.balls.size() > 300000000) { error = "metaball generation budget exceeded; reduce quality or ball count"; return false; }
    auto index = [&](int x, int y, int z) { return uint32_t((z*n[1]+y)*n[0]+x); };
    auto position = [&](int x, int y, int z) { return std::array<float,3>{lo[0]+x*step,lo[1]+y*step,lo[2]+z*step}; };
    std::vector<float> field(samples);
    for (int z = 0; z < n[2]; ++z) {
        if (cancelled && cancelled()) { error = "cancelled"; return false; }
        for (int y = 0; y < n[1]; ++y) for (int x = 0; x < n[0]; ++x) field[index(x,y,z)] = metaballField(m, position(x,y,z));
    }
    constexpr int corners[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    constexpr int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    Graphics::MeshData mesh;
    std::unordered_map<uint64_t,uint32_t> shared;
    using namespace Space::MarchingCubesTable;
    for (int z=0;z<n[2]-1;++z) {
        if (cancelled && cancelled()) { error = "cancelled"; return false; }
        for (int y=0;y<n[1]-1;++y) for(int x=0;x<n[0]-1;++x) {
            uint32_t lattice[8]; std::array<float,3> p[8]; int code=0;
            for(int c=0;c<8;++c) { const auto& o=corners[c]; lattice[c]=index(x+o[0],y+o[1],z+o[2]); p[c]=position(x+o[0],y+o[1],z+o[2]); if(field[lattice[c]]<m.isoLevel) code|=1<<c; }
            if(!edgeTable[code]) continue;
            uint32_t v[12]{};
            for(int e=0;e<12;++e) if(edgeTable[code]&(1<<e)) {
                const int a=edges[e][0],b=edges[e][1];
                const float t=std::clamp((m.isoLevel-field[lattice[a]])/(field[lattice[b]]-field[lattice[a]]),0.f,1.f);
                // Exact endpoint intersections must also share across distinct incident lattice edges.
                uint32_t ka=lattice[a],kb=lattice[b];
                if(t==0.f) kb=ka; else if(t==1.f) ka=kb;
                const uint64_t key=(uint64_t(std::min(ka,kb))<<32)|std::max(ka,kb);
                auto it=shared.find(key);
                if(it!=shared.end()) { v[e]=it->second; continue; }
                Graphics::MeshVertex vertex{}; std::array<float,3> q,g;
                for(int axis=0;axis<3;++axis) vertex.pos[axis]=q[axis]=p[a][axis]+t*(p[b][axis]-p[a][axis]);
                metaballField(m,q,&g);
                const float len=std::sqrt(g[0]*g[0]+g[1]*g[1]+g[2]*g[2]);
                for(int axis=0;axis<3;++axis) vertex.nrm[axis]=len>1.e-12f ? -g[axis]/len : (axis==1?1.f:0.f);
                vertex.uv[0]=q[0]/extent; vertex.uv[1]=q[2]/extent;
                v[e]=uint32_t(mesh.vertices.size()); shared.emplace(key,v[e]); mesh.vertices.push_back(vertex);
            }
            for(int t=0;Space::triTable[code][t]>=0;t+=3) {
                uint32_t a=v[Space::triTable[code][t]],b=v[Space::triTable[code][t+1]],c=v[Space::triTable[code][t+2]];
                if(a==b||b==c||a==c) continue;
                const auto& va=mesh.vertices[a]; const auto& vb=mesh.vertices[b]; const auto& vc=mesh.vertices[c];
                float u[3],w[3],normal[3];
                for(int k=0;k<3;++k) { u[k]=vb.pos[k]-va.pos[k]; w[k]=vc.pos[k]-va.pos[k]; }
                for(int k=0;k<3;++k) normal[k]=u[(k+1)%3]*w[(k+2)%3]-u[(k+2)%3]*w[(k+1)%3];
                float area=0.f,dot=0.f; for(int k=0;k<3;++k) { area+=normal[k]*normal[k]; dot+=normal[k]*(va.nrm[k]+vb.nrm[k]+vc.nrm[k]); }
                if(area==0.f) continue;
                if(dot<0.f) std::swap(b,c);
                mesh.indices.insert(mesh.indices.end(),{a,b,c});
                if(mesh.indices.size()>3000000) { error="metaball triangle budget exceeded"; return false; }
            }
        }
    }
    output=std::move(mesh); error.clear(); return true;
}
}

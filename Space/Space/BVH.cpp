#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stack>
#include <vector>

#include "BVH.h"


using namespace Phantom::Math;
using namespace Phantom::Space;


BVH::BVH(const std::vector<BVHObject*>& objects, int leafMax)
    : m_leafMax(std::max(1, leafMax)) {
    m_objects = objects;
    int n = (int)m_objects.size();
    m_indices.resize(n);
    for (int i = 0; i < n; ++i) m_indices[i] = i;

    m_nodes.reserve(std::max(1, 2 * n));
    buildRecursive(0, n);
}

std::vector<BVHObject*> BVH::queryOverlaps(const Box3df& query) const {
	std::vector<BVHObject*> results;
    if (m_nodes.empty()) return {};
    std::stack<int> st;
    st.push(0);
    while (!st.empty()) {
        int ni = st.top(); st.pop();
        const BVHNode& node = m_nodes[ni];
        if (!node.box.intersects(query)) continue;

        if (node.isLeaf()) {
            for (int i = 0; i < node.count; ++i) {
                BVHObject* obj = m_objects[m_indices[node.first + i]];
                if (obj->box.intersects(query)) {
                    results.push_back(obj);
                }
            }
        }
        else {
            st.push(node.left);
            st.push(node.right);
        }
    }
    return results;
}

std::vector<std::pair<int, int>> BVH::findAllPairs() const {
    std::vector<std::pair<int, int>> pairs;
    if (m_nodes.empty()) return pairs;

    // ・ｽm・ｽ[・ｽh・ｽy・ｽA・ｽﾌス・ｽ^・ｽb・ｽN・ｽﾅ木間の鯉ｿｽ・ｽ・ｽ・ｽ・ｽT・ｽ・ｽ
    std::vector<std::pair<int, int>> stack;
    stack.emplace_back(0, 0);

    while (!stack.empty()) {
        auto tmp = stack.back();
        auto a = tmp.first;
        auto b = tmp.second;
        stack.pop_back();

        const BVHNode& na = m_nodes[a];
        const BVHNode& nb = m_nodes[b];

        if (!na.box.intersects(nb.box)) continue;

        if (na.isLeaf() && nb.isLeaf()) {
            // ・ｽt・ｽ~・ｽt: ・ｽﾂ別オ・ｽu・ｽW・ｽF・ｽN・ｽg・ｽ・ｽ・ｽm・ｽ・ｽ・ｽ`・ｽF・ｽb・ｽN
            for (int i = 0; i < na.count; ++i) {
                auto A = m_objects[m_indices[na.first + i]];
                for (int j = 0; j < nb.count; ++j) {
                    auto B = m_objects[m_indices[nb.first + j]];
                    if (A->id < B->id && A->box.intersects(B->box)) {
                        pairs.emplace_back(A->id, B->id);
                    }
                }
            }
        }
        else {
            // ・ｽ・ｽ・ｽﾈゑｿｽ・ｽﾆゑｿｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽm・ｽ[・ｽh・ｽﾈら分・ｽ・ｽ・ｽ・ｽ・ｽﾄ積ゑｿｽ
            if (!na.isLeaf() && !nb.isLeaf()) {
                stack.emplace_back(na.left, nb.left);
                stack.emplace_back(na.left, nb.right);
                stack.emplace_back(na.right, nb.left);
                stack.emplace_back(na.right, nb.right);
            }
            else if (!na.isLeaf()) {
                stack.emplace_back(na.left, b);
                stack.emplace_back(na.right, b);
            }
            else { // !nb.isLeaf()
                stack.emplace_back(a, nb.left);
                stack.emplace_back(a, nb.right);
            }
        }
    }
    return pairs;
}

int BVH::buildRecursive(int begin, int end) {
    BVHNode node;
    // ・ｽﾍ囲ゑｿｽAABB・ｽ・ｽ・ｽv・ｽZ (Box3df ・ｽﾌ退会ｿｽ・ｽ{・ｽb・ｽN・ｽX・ｽ・ｽp・ｽ・ｽ・ｽ・ｽ)
    Box3df bounds = Box3df::createDegeneratedBox();
    for (int i = begin; i < end; ++i) {
        bounds.add(m_objects[m_indices[i]]->box);
    }
    node.box = bounds;

    int count = end - begin;
    int nodeIndex = (int)m_nodes.size();
    m_nodes.push_back(node);

    if (count <= m_leafMax) {
        // ・ｽt・ｽm・ｽ[・ｽh
        m_nodes[nodeIndex].first = begin;
        m_nodes[nodeIndex].count = count;
        return nodeIndex;
    }

    // ・ｽX・ｽv・ｽ・ｽ・ｽb・ｽg・ｽ・ｽ・ｽI・ｽ・ｽ: ・ｽﾅ托ｿｽG・ｽN・ｽX・ｽe・ｽ・ｽ・ｽg・ｽ・ｽ
    auto ext = bounds.getLength();
    int axis = 0;
    if (ext.y > ext.x && ext.y >= ext.z) axis = 1;
    else if (ext.z > ext.x && ext.z >= ext.y) axis = 2;

    // ・ｽ・ｽ・ｽf・ｽB・ｽA・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽi・ｽ・ｽ・ｽS・ｽ・ｽ・ｽW・ｽﾅ）
    int mid = (begin + end) / 2;
    std::nth_element(m_indices.begin() + begin,
        m_indices.begin() + mid,
        m_indices.begin() + end,
        [&](int a, int b) {
            const auto ca = m_objects[a]->box.getCenter();
            const auto cb = m_objects[b]->box.getCenter();
            float va = axis == 0 ? ca.x : (axis == 1 ? ca.y : ca.z);
            float vb = axis == 0 ? cb.x : (axis == 1 ? cb.y : cb.z);
            return va < vb;
        });

    // ・ｽﾄ帰・ｽ\・ｽz
    int leftChild = buildRecursive(begin, mid);
    int rightChild = buildRecursive(mid, end);

    m_nodes[nodeIndex].left = leftChild;
    m_nodes[nodeIndex].right = rightChild;
    return nodeIndex;
}

std::vector<BVHObject*> BVH::queryRay(
    const Math::Vector3df& origin,
    const Math::Vector3df& dir,
    float tMin, float tMax) const
{
    std::vector<BVHObject*> results;
    if (m_nodes.empty()) return results;

    auto intersectsRay = [&](const Box3df& box) -> bool {
        float lo = tMin, hi = tMax;
        const auto bmin = box.getMin();
        const auto bmax = box.getMax();
        for (int i = 0; i < 3; ++i) {
            const float invD = 1.0f / dir[i];
            float t0 = (bmin[i] - origin[i]) * invD;
            float t1 = (bmax[i] - origin[i]) * invD;
            if (invD < 0.0f) std::swap(t0, t1);
            lo = std::max(lo, t0);
            hi = std::min(hi, t1);
            if (hi < lo) return false;
        }
        return true;
    };

    std::stack<int> st;
    st.push(0);
    while (!st.empty()) {
        const int ni = st.top(); st.pop();
        const BVHNode& node = m_nodes[ni];
        if (!intersectsRay(node.box)) continue;

        if (node.isLeaf()) {
            for (int i = 0; i < node.count; ++i) {
                BVHObject* obj = m_objects[m_indices[node.first + i]];
                if (intersectsRay(obj->box)) {
                    results.push_back(obj);
                }
            }
        } else {
            st.push(node.left);
            st.push(node.right);
        }
    }
    return results;
}

Box3df BVH::refitRecursive(int nodeIndex) {
    BVHNode& node = m_nodes[nodeIndex];
    if (node.isLeaf()) {
        Box3df b = Box3df::createDegeneratedBox();
        for (int i = 0; i < node.count; ++i) {
            b.add(m_objects[m_indices[node.first + i]]->box);
        }
        node.box = b;
        return b;
    }
    else {
        Box3df leftB = refitRecursive(node.left);
        Box3df rightB = refitRecursive(node.right);
        Box3df b = Box3df::createDegeneratedBox();
        b.add(leftB);
        b.add(rightB);
        node.box = b;
        return b;
    }
}


// ---------------------------------------------
// ・ｽg・ｽp・ｽ・ｽ
// ---------------------------------------------
//int main() {
//    // ・ｽK・ｽ・ｽ・ｽ・ｽAABB・ｽQ・ｽ・ｽ・ｽ・ｬ・ｽiID・ｽﾍ茨ｿｽﾓ）
//    std::vector<Object> objs;
//    objs.push_back({ 0, AABB(Vec3(0,0,0), Vec3(1,1,1)) });
//    objs.push_back({ 1, AABB(Vec3(0.5f,0.5f,0.5f), Vec3(2,2,2)) });
//    objs.push_back({ 2, AABB(Vec3(3,3,3), Vec3(4,4,4)) });
//    objs.push_back({ 3, AABB(Vec3(-1,-1,-1), Vec3(0.2f,0.2f,0.2f)) });
//
//    BVH bvh(objs, /*leafMax=*/2);
//
//    // ・ｽN・ｽG・ｽ・ｽAABB・ｽﾆ重・ｽﾈゑｿｽI・ｽu・ｽW・ｽF・ｽN・ｽg・ｽﾌ暦ｿｽ
//    AABB query(Vec3(0.9f, 0.9f, 0.9f), Vec3(1.1f, 1.1f, 1.1f));
//    std::vector<int> hits;
//    bvh.queryOverlaps(query, hits);
//
//    std::cout << "Query overlaps with IDs: ";
//    for (int id : hits) std::cout << id << " ";
//    std::cout << "\n";
//
//    // ・ｽS・ｽy・ｽA・ｽ刀i・ｽL・ｽ`・ｽ・ｽ・ｽ・ｽj
//    auto pairs = bvh.findAllPairs();
//    std::cout << "Overlap pairs:\n";
//    for (auto& p : pairs) {
//        std::cout << p.first << " - " << p.second << "\n";
//    }
//
//    // ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽI・ｽX・ｽV・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾈゑｿｽAABB・ｽ・ｽﾏ更・ｽ・ｽ・ｽ・ｽrefit
//    objs[2].box = AABB(Vec3(0.8f, 0.8f, 0.8f), Vec3(1.2f, 1.2f, 1.2f));
//    // refit・ｽ・ｽBVH・ｽ・ｽ・ｽ・ｽ・ｽ・ｽobjects・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽ驍ｽ・ｽﾟ、・ｽK・ｽX・ｽZ・ｽb・ｽg・ｽﾖ撰ｿｽ・ｽ・ｽp・ｽﾓゑｿｽ・ｽﾄ難ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾝ計・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
//    // ・ｽ・ｽ・ｽﾌ暦ｿｽﾅは簡易に再構・ｽz・ｽ・ｽ・ｽ・ｽi・ｽ{・ｽﾔでゑｿｽBVH・ｽﾉセ・ｽb・ｽg・ｽX・ｽV・ｽﾖ撰ｿｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾌゑｿｽ・ｽﾇゑｿｽ・ｽj
//    BVH bvh2(objs, 2);
//    auto pairs2 = bvh2.findAllPairs();
//    std::cout << "After move, overlap pairs:\n";
//    for (auto& p : pairs2) {
//        std::cout << p.first << " - " << p.second << "\n";
//    }
//
//    return 0;
//}
#include <gtest/gtest.h>

#include <cmath>
#include <set>

#include "../MeshEdit/EditableMesh.h"

// Logical topology of the EditableMesh (the MeshData conversions and scene round trips are tested in PhantomStudio).
using namespace Phantom::MeshEdit;

TEST(EditableMeshTest, BoxHasLogicalTopology) {
    const EditableMesh box = makeEditableBox(0.5f);
    std::string error;
    EXPECT_TRUE(box.validate(&error)) << error;
    EXPECT_EQ(8u, box.vertices.size());
    EXPECT_EQ(12u, box.edges.size());
    EXPECT_EQ(6u, box.faces.size());
    for (const auto& f : box.faces) EXPECT_EQ(4u, f.loop.size());
}

TEST(EditableMeshTest, ValidationRejectsBadData) {
    std::string error;
    {
        EditableMesh m = makeEditableBox();
        m.vertices[3].pos[1] = std::nanf("");
        EXPECT_FALSE(m.validate(&error));
    }
    {
        EditableMesh m = makeEditableBox();
        m.faces[0].loop[1].vertex = 99;  // missing reference
        EXPECT_FALSE(m.validate(&error));
    }
    {
        EditableMesh m = makeEditableBox();
        m.faces[0].loop[1].vertex = m.faces[0].loop[0].vertex;  // repeated vertex
        EXPECT_FALSE(m.validate(&error));
    }
    {
        EditableMesh m = makeEditableBox();
        for (int v : { 2, 5, 6 })  // collapses the whole +X face (1,2,6,5) to a point
            for (int k = 0; k < 3; ++k) m.vertices[v].pos[k] = m.vertices[1].pos[k];
        EXPECT_FALSE(m.validate(&error));
    }
    {
        EditableMesh m = makeEditableBox();
        std::swap(m.faces[0].loop[1], m.faces[0].loop[3]);  // flipped winding next to its neighbours
        EXPECT_FALSE(m.validate(&error));
    }
    {
        EditableMesh m = makeEditableBox();
        m.edges.pop_back();  // stale edge table
        EXPECT_FALSE(m.validate(&error));
    }
}

TEST(EditableMeshTest, RebuildEdgesKeepsIdsAndNeverReusesRemovedOnes) {
    EditableMesh m = makeEditablePlane(1.f, 1);
    const uint32_t keep = m.findEdge(0, 1)->id;
    const uint32_t before = m.nextEdgeId;
    // Replace the quad by a triangle sharing edge 0-1: the shared edge keeps its id, the new edge gets a fresh one.
    m.faces.clear();
    std::vector<EditableCorner> tri(3);
    tri[0].vertex = 0; tri[1].vertex = 2; tri[2].vertex = 1;
    // (0,2,1): is it counter-clockwise from +Y? vertices: 0=(-1,0,-1) 1=(1,0,-1) 2=(-1,0,1)
    m.addFace(tri);
    m.rebuildEdges();
    ASSERT_NE(nullptr, m.findEdge(1, 0));
    EXPECT_EQ(keep, m.findEdge(0, 1)->id);
    EXPECT_EQ(3u, m.edges.size());
    EXPECT_EQ(before + 1, m.nextEdgeId);  // exactly one new edge (2-1) was allocated
    std::set<uint32_t> ids;
    for (const auto& e : m.edges) ids.insert(e.id);
    EXPECT_EQ(3u, ids.size());
}

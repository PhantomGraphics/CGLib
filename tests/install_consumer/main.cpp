// Uses only the installed package: headers via "CGLib/...", libraries via CGLib::<Component>.
#include "CGLib/Math/Core/Ellipse3d.h"
#include "CGLib/Math/Core/SafeNormalize.h"
#include "CGLib/Math/Core/Statistics.h"
#include "CGLib/Math/Core/Vector3d.h"
#include "CGLib/Numerics/Core/SVD3d.h"
#include "CGLib/Space/Core/BVH.h"
#include "CGLib/Volume/Core/Volume.h"
#include "CGLib/File/Core/OBJFileReader.h"
#include "CGLib/Animation/Core/Animator.h"
#include "CGLib/Animation/Core/AnimationTrack.h"
#include "CGLib/Graphics/Core/PrimitiveMesh.h"
#include "CGLib/MeshEdit/Core/EditableMeshConvert.h"
#include "CGLib/GltfRenderer/Gltf/CharacterPoseBake.h"
#include "CGLib/GeometryNode/Core/GeometryOps.h"
#include "CGLib/SceneRuntime/Core/SceneGraph.h"

#include <cstdio>
#include <sstream>

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
        if (!ok) ++failures;
    };

    const Phantom::Math::Vector3df v(3.0f, 4.0f, 0.0f);
    check(Phantom::Math::getLength(v) > 4.9f, "Math");

    // Math contracts: try* APIs fail without touching `out`, and succeed on valid input.
    Phantom::Math::Vector3df normal(9.0f, 9.0f, 9.0f);
    const Phantom::Math::Ellipse3df flat({0, 0, 0}, {2, 0, 0}, {0, 3, 0});
    const Phantom::Math::Ellipse3df degenerate({0, 0, 0}, {1, 0, 0}, {2, 0, 0});
    check(!degenerate.tryGetNormal(normal) && normal.x == 9.0f, "Math tryGetNormal failure keeps out");
    check(flat.tryGetNormal(normal) && normal.z > 0.999f, "Math tryGetNormal");
    float mean = 0.0f;
    check(!Phantom::Math::Statistics<float>().tryGetAverage(mean), "Math Statistics empty fails");
    check(Phantom::Math::Statistics<float>({1.0f, 3.0f}).tryGetAverage(mean) && mean == 2.0f, "Math Statistics average");

    Phantom::Numerics::SVD3d svd;
    Phantom::Math::Matrix3dd identity(1.0);
    check(svd.calculate(identity).isOk, "Numerics (Eigen bundled)");

    std::istringstream obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    Phantom::File::OBJFileReader reader;
    check(reader.read(obj) && reader.getOBJ().positions.size() == 3, "File");

    Phantom::GeometryNode::Mesh terrain;
    check(Phantom::GeometryNode::makeTerrain(Phantom::GeometryNode::TerrainParams{}, Phantom::GeometryNode::Limits{}, terrain) == Phantom::GeometryNode::OpStatus::Ok, "GeometryNode terrain");

    // Animation tracks, the shared CPU mesh and its edit conversion, and the CPU character pose (GltfCore).
    Phantom::Animation::AnimTracks tracks;
    Phantom::Animation::TrackKey k0, k1;
    k0.frame = 0;  k0.value = {0.0f, 0.0f, 0.0f};
    k1.frame = 10; k1.value = {10.0f, 0.0f, 0.0f};
    tracks.track(Phantom::Animation::AnimChannel::Translation) = {k0, k1};
    Phantom::Animation::TrackTransform pose;
    Phantom::Animation::evaluateTracks(tracks, 5.0, pose);
    check(pose.translation[0] > 4.99f && pose.translation[0] < 5.01f, "Animation tracks");

    const Phantom::Graphics::MeshData box = Phantom::Graphics::buildBox(0.5f);
    check(Phantom::Graphics::validate(box).inconsistentTriangles == 0, "Graphics primitive mesh");
    Phantom::MeshEdit::EditableMesh editable;
    check(Phantom::MeshEdit::makeEditableFromMeshData(box, editable) && editable.faces.size() == 12, "MeshEdit convert");

    Phantom::Gltf::CharacterPoseSource source;
    check(Phantom::Gltf::bakeSkinnedPose(source, 0.0, 30.0).empty(), "Gltf pose bake without a document");

    Phantom::SceneRuntime::SceneGraph graph;
    check(graph.size() == 0, "SceneRuntime (nlohmann bundled)");

    return failures == 0 ? 0 : 1;
}

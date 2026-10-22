#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <string>

#include "../MeshEditView/CommandDispatcher.h"
#include "../MeshEditView/MeshWorld.h"

// MeshEditView without a window: the world (mesh, selection, history, draw data) and the command router the viewer's menu,
// Command window and scenarios share. The Vulkan side is covered by the viewer's scenarios (MeshEditView/scenarios).
using namespace Phantom::MeshView;
using namespace Phantom::MeshEdit;

namespace {

// 1280x720 viewport looking at the origin from a corner, as the viewer's default camera does.
CameraAccess stubCamera(glm::vec3& eye) {
    CameraAccess c;
    c.eye = [&eye] { return eye; };
    c.pickView = [&eye] {
        MeshPickView v;
        v.view = glm::lookAt(eye, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
        v.proj = glm::perspective(glm::radians(45.f), 1280.f / 720.f, 0.01f, 100.f);
        v.proj[1][1] *= -1.f;
        v.width = 1280.f;
        v.height = 720.f;
        return v;
    };
    c.distance = [&eye] { return glm::length(eye); };
    c.reset = [&eye] { eye = glm::vec3(2.f, 1.5f, 2.f); };
    c.set = [&eye](float, float, float d) { eye = glm::normalize(glm::vec3(1.f, 0.7f, 1.f)) * d; };
    c.scroll = [](double) {};
    return c;
}

struct Fixture {
    MeshWorld world;
    CommandDispatcher dispatcher;
    glm::vec3 eye{ 2.f, 1.5f, 2.f };
    Fixture() {
        dispatcher.setWorld(&world);
        dispatcher.setCamera(stubCamera(eye));
    }
    std::string run(const std::string& command) { return dispatcher.route(command); }
};

size_t count(const std::string& counts, int index) {   // "Counts:8,12,6"
    size_t pos = counts.find(':') + 1;
    for (int i = 0; i < index; ++i) pos = counts.find(',', pos) + 1;
    return static_cast<size_t>(std::stoul(counts.substr(pos)));
}

}  // namespace

TEST(MeshWorld, StartsAsABoxWithItsLogicalTopology) {
    MeshWorld w;
    const Counts c = w.baseCounts();
    EXPECT_EQ(8u, c.vertices); EXPECT_EQ(12u, c.edges); EXPECT_EQ(6u, c.faces);
    EXPECT_FALSE(w.editMode());
    EXPECT_TRUE(w.modifiers().empty());
}

TEST(MeshWorld, OperationsNeedEditModeAndTheRightElementType) {
    MeshWorld w;
    EXPECT_FALSE(w.extrude(0.2f).empty());                 // object mode
    ASSERT_TRUE(w.setEditMode(true).empty());
    EXPECT_FALSE(w.extrude(0.2f).empty());                 // nothing selected
    ASSERT_TRUE(w.setElementKind(ElementKind::Edge).empty());
    ASSERT_TRUE(w.select("0", SelectMode::Replace).empty());
    EXPECT_FALSE(w.extrude(0.2f).empty());                 // extrude wants faces
    EXPECT_FALSE(w.inset(0.1f, 0.f).empty());
    EXPECT_FALSE(w.shade(true, 180.f).empty());
    EXPECT_TRUE(w.undo() == false);                        // refused calls leave no history
}

TEST(MeshWorld, ExtrudeSelectionSurvivesAndUndoRedoWalkTheHistory) {
    MeshWorld w;
    w.setEditMode(true);
    w.setElementKind(ElementKind::Face);
    ASSERT_TRUE(w.select("2", SelectMode::Replace).empty());
    ASSERT_TRUE(w.extrude(0.4f).empty());
    EXPECT_EQ(12u, w.baseCounts().vertices);
    EXPECT_EQ((std::vector<uint32_t>{ 2 }), w.edit().ids);   // the moved face keeps its id
    EXPECT_EQ(1u, w.undoDepth());
    ASSERT_TRUE(w.undo());
    EXPECT_EQ(8u, w.baseCounts().vertices);
    EXPECT_EQ(1u, w.redoDepth());
    ASSERT_TRUE(w.redo());
    EXPECT_EQ(12u, w.baseCounts().vertices);
    // A new edit drops the redo branch.
    ASSERT_TRUE(w.undo());
    ASSERT_TRUE(w.select("0", SelectMode::Replace).empty());
    ASSERT_TRUE(w.extrude(0.1f).empty());
    EXPECT_EQ(0u, w.redoDepth());
}

TEST(MeshWorld, UndoDropsSelectionThatNoLongerExists) {
    MeshWorld w;
    w.setEditMode(true);
    w.setElementKind(ElementKind::Vertex);
    w.select("all", SelectMode::Replace);
    ASSERT_EQ(8u, w.edit().ids.size());
    w.setElementKind(ElementKind::Face);
    w.select("2", SelectMode::Replace);
    ASSERT_TRUE(w.extrude(0.3f).empty());
    w.setElementKind(ElementKind::Vertex);
    w.select("all", SelectMode::Replace);
    EXPECT_EQ(12u, w.edit().ids.size());
    ASSERT_TRUE(w.undo());                                  // back to 8 vertices: ids 8..11 are gone
    EXPECT_EQ(8u, w.edit().ids.size());
}

TEST(MeshWorld, ModifiersAreNonDestructiveUntilApplied) {
    MeshWorld w;
    MeshModifier sub;
    sub.type = MeshModifier::Type::Subdivision;
    ASSERT_TRUE(w.addModifier(sub).empty());
    EXPECT_EQ(8u, w.baseCounts().vertices);
    EXPECT_EQ(26u, w.evaluatedCounts().vertices);
    EXPECT_EQ(24u, w.evaluatedCounts().faces);
    ASSERT_TRUE(w.applyModifiers().empty());
    EXPECT_EQ(26u, w.baseCounts().vertices);
    EXPECT_TRUE(w.modifiers().empty());
    ASSERT_TRUE(w.undo());                                  // the bake and the stack come back together
    EXPECT_EQ(8u, w.baseCounts().vertices);
    EXPECT_EQ(1u, w.modifiers().size());
    EXPECT_TRUE(w.applyModifiers().empty());                // the restored stack can be baked again
    EXPECT_FALSE(w.applyModifiers().empty());               // and then there is nothing left to bake
}

TEST(MeshWorld, ABadModifierIsRefusedAndLeavesTheStackAlone) {
    MeshWorld w;
    MeshModifier m;
    m.type = MeshModifier::Type::Subdivision;
    m.levels = 9;
    EXPECT_FALSE(w.addModifier(m).empty());
    EXPECT_TRUE(w.modifiers().empty());
    EXPECT_EQ(0u, w.undoDepth());
}

TEST(MeshWorld, ClosedMeshesEncloseAPositiveVolume) {
    MeshWorld w;
    bool closed = false;
    EXPECT_NEAR(1.0, w.signedVolume(&closed), 1e-9);
    EXPECT_TRUE(closed);
    w.newMesh(Shape::Plane, 2);
    EXPECT_NEAR(0.0, w.signedVolume(&closed), 1e-9);
    EXPECT_FALSE(closed);
}

TEST(MeshWorld, FacingFollowsTheWindingAndTheCamera) {
    MeshWorld w;
    const Facing corner = w.facing(glm::vec3(3.f, 3.f, 3.f));
    EXPECT_EQ(6u, corner.front);                            // three quads, two triangles each
    EXPECT_EQ(6u, corner.back);
    w.newMesh(Shape::Plane, 1);
    EXPECT_EQ(2u, w.facing(glm::vec3(0.f, 3.f, 0.f)).front);   // the plane faces +Y
    EXPECT_EQ(2u, w.facing(glm::vec3(0.f, -3.f, 0.f)).back);
}

TEST(MeshWorld, DrawDataHasOneVertexPerCornerAndOneLinePerEdge) {
    MeshWorld w;
    MeshDraw& d = w.draw();
    EXPECT_EQ(24u * 3, d.triPositions.size());
    EXPECT_EQ(24u * 4, d.triColors.size());
    EXPECT_EQ(36u, d.triIndices.size());
    EXPECT_EQ(12u * 2, d.lineIndices.size());
    EXPECT_TRUE(d.pointSizes.empty());                      // vertices are an edit-mode display
    w.setEditMode(true);
    EXPECT_EQ(8u, w.draw().pointSizes.size());
}

TEST(MeshWorld, SelectionIsHighlightedInTheDrawData) {
    MeshWorld w;
    w.setEditMode(true);
    w.setElementKind(ElementKind::Vertex);
    const std::vector<float> plain = w.draw().pointColors;
    w.select("3", SelectMode::Replace);
    const MeshDraw& d = w.draw();
    ASSERT_EQ(plain.size(), d.pointColors.size());
    int changed = 0;
    for (size_t v = 0; v < d.pointSizes.size(); ++v)
        for (int ch = 0; ch < 3; ++ch) if (d.pointColors[v * 4 + ch] != plain[v * 4 + ch]) { ++changed; break; }
    EXPECT_EQ(1, changed);
    float maxSize = 0.f;
    for (float s : d.pointSizes) maxSize = std::max(maxSize, s);
    EXPECT_GT(maxSize, 7.f);                                // the selected vertex is drawn larger
}

TEST(MeshWorld, OrientationViewColoursFrontBlueAndBackRed) {
    MeshWorld w;
    Display d;
    d.orientation = true;
    w.setDisplay(d);
    w.setEye(glm::vec3(3.f, 3.f, 3.f));
    const MeshDraw& draw = w.draw();
    int blue = 0, red = 0;
    for (size_t v = 0; v < draw.triPositions.size() / 3; ++v) {
        const float r = draw.triColors[v * 4], b = draw.triColors[v * 4 + 2];
        if (b > r + 0.1f) ++blue; else if (r > b + 0.1f) ++red;
    }
    EXPECT_EQ(12, blue);                                    // three front quads, four corners each
    EXPECT_EQ(12, red);
    // Moving the camera rebuilds the colours; a plane seen from below is all back faces.
    w.newMesh(Shape::Plane, 1);
    w.setEye(glm::vec3(0.f, -3.f, 0.f));
    const MeshDraw& below = w.draw();
    for (size_t v = 0; v < below.triPositions.size() / 3; ++v)
        EXPECT_GT(below.triColors[v * 4], below.triColors[v * 4 + 2]);
}

TEST(MeshWorld, TheEyeOnlyInvalidatesWhenTheOrientationViewNeedsIt) {
    MeshWorld w;
    w.draw().dirty = false;
    w.setEye(glm::vec3(5.f, 5.f, 5.f));
    EXPECT_FALSE(w.draw().dirty);
}

TEST(MeshWorld, ModifiedMeshShowsTheCageAboveTheResultInEditMode) {
    MeshWorld w;
    MeshModifier sub;
    sub.type = MeshModifier::Type::Subdivision;
    w.addModifier(sub);
    const size_t resultLines = w.draw().lineIndices.size();
    w.setEditMode(true);
    EXPECT_GT(w.draw().lineIndices.size(), resultLines);    // + the 12 edges of the editing cage
    EXPECT_EQ(resultLines + 12u * 2, w.draw().lineIndices.size());
}

// ---- command router ----

TEST(MeshEditViewCommands, EditingThroughCommandsMatchesTheWorld) {
    Fixture f;
    EXPECT_EQ("OK", f.run("NewMesh:box"));
    EXPECT_EQ("Counts:8,12,6", f.run("GetCounts"));
    EXPECT_EQ("Error:not in edit mode (SetMode:edit)", f.run("Extrude:distance=0.4"));
    EXPECT_EQ("OK", f.run("SetMode:edit"));
    EXPECT_EQ("OK", f.run("SelectElement:face"));
    EXPECT_EQ("OK", f.run("Select:2"));
    EXPECT_EQ("Selection:face:1:2", f.run("GetSelection"));
    EXPECT_EQ("OK", f.run("Extrude:distance=0.4"));
    EXPECT_EQ("Counts:12,20,10", f.run("GetCounts"));
    EXPECT_EQ("Volume:1.400000,closed=1", f.run("GetVolume"));
}

TEST(MeshEditViewCommands, SelectAcceptsListsRangesAndModes) {
    Fixture f;
    f.run("SetMode:edit");
    f.run("SelectElement:vertex");
    EXPECT_EQ("OK", f.run("Select:0,1,4-6"));
    EXPECT_EQ("Selection:vertex:5:0,1,4,5,6", f.run("GetSelection"));
    EXPECT_EQ("OK", f.run("Select:6,7 mode=toggle"));
    EXPECT_EQ("Selection:vertex:5:0,1,4,5,7", f.run("GetSelection"));
    EXPECT_EQ("OK", f.run("Select:2 mode=add"));
    EXPECT_EQ("Selection:vertex:6:0,1,2,4,5,7", f.run("GetSelection"));
    EXPECT_EQ("OK", f.run("Select:none"));
    EXPECT_EQ("Selection:vertex:0:", f.run("GetSelection"));
    EXPECT_EQ(0u, f.run("Select:99").rfind("Error:", 0));
    EXPECT_EQ(0u, f.run("Select:1 mode=subtract").rfind("Error:", 0));
}

TEST(MeshEditViewCommands, PickingUsesTheCameraOfTheViewer) {
    Fixture f;
    f.run("SetMode:edit");
    f.run("SelectElement:vertex");
    const std::string pixel = f.run("ProjectVertex:6");     // the corner nearest the camera
    ASSERT_EQ(0u, pixel.rfind("Pixel:", 0)) << pixel;
    EXPECT_EQ("OK", f.run("Pick:" + pixel.substr(6)));
    EXPECT_EQ("Selection:vertex:1:6", f.run("GetSelection"));
    EXPECT_EQ("OK", f.run("Pick:5,5"));                     // empty space clears a replace selection
    EXPECT_EQ("Selection:vertex:0:", f.run("GetSelection"));
    f.run("SetXray:1");
    EXPECT_EQ("OK", f.run("BoxSelect:0,0,1280,720"));
    EXPECT_EQ("Selection:vertex:8:0,1,2,3,4,5,6,7", f.run("GetSelection"));
}

TEST(MeshEditViewCommands, MalformedCommandsAreRefusedWithoutChangingAnything) {
    Fixture f;
    for (const char* bad : { "NewMesh:cube", "NewMesh:plane divs=0", "SetMode:sideways", "SelectElement:solid", "Extrude:distance=abc",
                             "Extrude:distance=nan", "Move:1,2", "Move:a,b,c", "AddModifier:bevel", "AddModifier:mirror axis=q",
                             "Scroll:inf", "SetCamera:1,2", "Pick:1", "Shade:medium", "Merge:middle", "Frobnicate" })
        EXPECT_EQ(0u, f.run(bad).rfind("Error:", 0)) << bad;
    EXPECT_EQ("Counts:8,12,6", f.run("GetCounts"));
    EXPECT_EQ("History:undo=0,redo=0", f.run("GetHistory"));
}

TEST(MeshEditViewCommands, DisplayOptionsRoundTrip) {
    Fixture f;
    EXPECT_EQ("Display:faces=1,edges=1,vertices=1,normals=0,orientation=0", f.run("GetDisplay"));
    EXPECT_EQ("OK", f.run("SetDisplay:faces=0 normals=1 orientation=1"));
    EXPECT_EQ("Display:faces=0,edges=1,vertices=1,normals=1,orientation=1", f.run("GetDisplay"));
    EXPECT_EQ(0u, f.run("SetDisplay:glow=1").rfind("Error:", 0));
    EXPECT_EQ(0u, f.run("SetDisplay:edges=maybe").rfind("Error:", 0));
    EXPECT_EQ("Display:faces=0,edges=1,vertices=1,normals=1,orientation=1", f.run("GetDisplay"));  // refused: unchanged
}

TEST(MeshEditViewCommands, OrientationCountsFollowTheCamera) {
    Fixture f;
    f.run("SetCamera:0,0,4");
    EXPECT_EQ("Orientation:front=6,back=6", f.run("GetOrientation"));
    f.run("NewMesh:plane");
    f.eye = glm::vec3(0.f, 4.f, 0.1f);
    EXPECT_EQ("Orientation:front=2,back=0", f.run("GetOrientation"));
    f.eye = glm::vec3(0.f, -4.f, 0.1f);
    EXPECT_EQ("Orientation:front=0,back=2", f.run("GetOrientation"));
}

TEST(MeshEditViewCommands, EveryCatalogEntryIsRouted) {
    Fixture f;
    EXPECT_EQ("OK", f.run("CheckCommandCatalog"));
    const auto catalog = f.dispatcher.commandCatalog();
    EXPECT_GT(catalog.size(), 30u);
    for (const auto& c : catalog) EXPECT_FALSE(c.name.empty());
}

TEST(MeshEditViewCommands, QueueRunsCommandsInOrderAndRepliesOncePerCommand) {
    Fixture f;
    f.dispatcher.dispatch("NewMesh:plane divs=2");
    f.dispatcher.dispatch("GetCounts");
    f.dispatcher.submitUi("GetMode");                      // a GUI command: no reply
    f.dispatcher.dispatch("Frobnicate");
    f.dispatcher.processQueue();
    const auto replies = f.dispatcher.collectResponses();
    ASSERT_EQ(3u, replies.size());
    EXPECT_EQ("OK", replies[0]);
    EXPECT_EQ("Counts:9,12,4", replies[1]);
    EXPECT_EQ(0u, replies[2].rfind("Error:unknown command", 0));
    EXPECT_EQ(9u, count(replies[1], 0));
}

TEST(MeshWorldBoolean, CutterPreviewDrawsWithoutHistoryAndCommitsOnce) {
    MeshWorld w;
    const size_t lines = w.draw().lineIndices.size();
    ASSERT_TRUE(w.setCutter(false).empty());
    EXPECT_GT(w.draw().lineIndices.size(), lines);
    EXPECT_EQ(1u, w.undoDepth());
    auto original = w.cutter().transform;
    auto t = original;
    t.position = {.5f, 0, 0};
    for (int i = 0; i < 5; ++i) {
        t.position[0] = .5f + .01f * i;
        ASSERT_TRUE(w.setCutterTransform(t, true).empty());
        EXPECT_EQ(original, w.cutter().transform);
        EXPECT_EQ(1u, w.undoDepth());
        EXPECT_FALSE(w.draw().linePositions.empty());
    }
    ASSERT_TRUE(w.setCutterTransform(t).empty());
    EXPECT_EQ(2u, w.undoDepth());
    ASSERT_TRUE(w.undo());
    EXPECT_EQ(original, w.cutter().transform);
    ASSERT_TRUE(w.redo());
    EXPECT_EQ(t, w.cutter().transform);
}

TEST(MeshWorldBoolean, BooleanBakesModifiersAsOneUndoStep) {
    MeshWorld w;
    ASSERT_TRUE(w.setEditMode(true).empty());
    ASSERT_TRUE(w.select("all", SelectMode::Replace).empty());
    ASSERT_TRUE(w.moveSelected({.75f, 0, 0}).empty());
    ASSERT_TRUE(w.setEditMode(false).empty());
    MeshModifier mirror;
    mirror.type = MeshModifier::Type::Mirror;
    ASSERT_TRUE(w.addModifier(mirror).empty());
    const auto evaluated = w.evaluatedCounts();
    ASSERT_TRUE(w.setCutter(false).empty());
    auto t = w.cutter().transform;
    t.position = {4, 0, 0};
    ASSERT_TRUE(w.setCutterTransform(t).empty());
    const size_t before = w.undoDepth();
    const auto originalRevision = w.base().revision;
    const auto booleanError = w.applyBoolean(BooleanOperation::Union);
    ASSERT_TRUE(booleanError.empty()) << booleanError;
    EXPECT_EQ(before + 1, w.undoDepth());
    EXPECT_TRUE(w.modifiers().empty());
    EXPECT_GT(w.baseCounts().faces, evaluated.faces);
    const auto resultRevision = w.base().revision;
    ASSERT_TRUE(w.undo());
    EXPECT_EQ(originalRevision, w.base().revision);
    EXPECT_EQ(1u, w.modifiers().size());
    ASSERT_TRUE(w.redo());
    EXPECT_EQ(resultRevision, w.base().revision);
    EXPECT_TRUE(w.modifiers().empty());
}

TEST(MeshWorldBoolean, TransformCopyAndQueriesRemainIndependentOfCutterDisplay) {
    MeshWorld w;
    ASSERT_TRUE(w.setCutter(true).empty());
    const auto copy = w.cutter().mesh;
    EXPECT_TRUE(w.cutter().copied);
    auto t = w.cutter().transform;
    t.position = {0, 0, 0};
    t.rotation = {0, 90, 0};
    t.scale = {.5f, 1, 1};
    ASSERT_TRUE(w.setCutterTransform(t).empty());
    EXPECT_DOUBLE_EQ(1, w.signedVolume());
    EXPECT_EQ(6u, w.baseCounts().faces);
    const auto booleanError = w.applyBoolean(BooleanOperation::Difference);
    ASSERT_TRUE(booleanError.empty()) << booleanError;
    EXPECT_NEAR(.5, w.signedVolume(), 1e-5);
    EXPECT_EQ(copy, w.cutter().mesh);
    EXPECT_EQ(6u, copy->faces.size());
    w.setCutterVisible(false);
    EXPECT_NEAR(.5, w.signedVolume(), 1e-5);
}

TEST(MeshWorldBoolean, EmptyResultsDrawAndUndo) {
    MeshWorld w;
    ASSERT_TRUE(w.setCutter(true).empty());
    CutterTransform t;
    t.position = {0, 0, 0};
    ASSERT_TRUE(w.setCutterTransform(t).empty());
    const auto booleanError = w.applyBoolean(BooleanOperation::Difference);
    ASSERT_TRUE(booleanError.empty()) << booleanError;
    EXPECT_EQ(0u, w.baseCounts().faces);
    EXPECT_TRUE(w.draw().triIndices.empty());
    EXPECT_FALSE(w.draw().lineIndices.empty());
    w.setCutterVisible(false);
    EXPECT_TRUE(w.draw().lineIndices.empty());
    ASSERT_TRUE(w.undo()); // visibility
    ASSERT_TRUE(w.undo()); // Boolean
    EXPECT_EQ(6u, w.baseCounts().faces);
    EXPECT_EQ(1, w.signedVolume());
}

TEST(MeshEditViewBooleanCommands, RoundTripAndUndo) {
    Fixture f;
    EXPECT_EQ("OK", f.run("SetCutter:box"));
    EXPECT_EQ("OK", f.run("SetCutterTransform:position=0.5,0,0 rotation=0,0,0 scale=1,1,1"));
    EXPECT_EQ("Cutter:box,visible=1 position=0.5,0,0 rotation=0,0,0 scale=1,1,1", f.run("GetCutter"));
    EXPECT_EQ("OK", f.run("Boolean:intersection"));
    EXPECT_EQ("Volume:0.500000,closed=1", f.run("GetVolume"));
    EXPECT_EQ("History:undo=3,redo=0", f.run("GetHistory"));
    EXPECT_EQ("OK", f.run("Undo"));
    EXPECT_EQ("Counts:8,12,6", f.run("GetCounts"));
    EXPECT_EQ("OK", f.run("Redo"));
    EXPECT_EQ("Volume:0.500000,closed=1", f.run("GetVolume"));
}

TEST(MeshEditViewBooleanCommands, FailuresLeaveGeometryCutterAndHistoryAlone) {
    Fixture f;
    const auto original = f.run("GetCutter");
    for (const char* cmd :
         {"Boolean:x", "Boolean:union unknown=1", "Boolean:union extra", "SetCutter:plane",
          "SetCutterTransform:position=1,2", "SetCutterTransform:scale=1,0,1",
          "SetCutterTransform:scale=-1,1,1", "SetCutterTransform:position=nan,0,0",
          "SetCutterTransform:position=1e100,0,0", "SetCutterTransform:rotation=1e39,0,0",
          "SetCutterTransform:foo=1,2,3", "SetCutterTransform:preview=yes", "SetCutterVisible:yes"})
        EXPECT_EQ(0u, f.run(cmd).rfind("Error:", 0)) << cmd;
    EXPECT_EQ(original, f.run("GetCutter"));
    EXPECT_EQ("History:undo=0,redo=0", f.run("GetHistory"));
    EXPECT_EQ("OK", f.run("SetMode:edit"));
    EXPECT_EQ("OK", f.run("Select:all"));
    const auto selection = f.run("GetSelection");
    EXPECT_EQ(0u, f.run("Boolean:union").rfind("Error:", 0));
    EXPECT_EQ(0u, f.run("SetCutter:copy").rfind("Error:", 0));
    EXPECT_EQ(selection, f.run("GetSelection"));
    EXPECT_EQ("OK", f.run("NewMesh:plane"));
    const auto revision = f.world.base().revision;
    EXPECT_EQ(0u, f.run("Boolean:union").rfind("Error:", 0));
    EXPECT_EQ(revision, f.world.base().revision);
    EXPECT_EQ("History:undo=0,redo=0", f.run("GetHistory"));
}

TEST(MeshEditViewBooleanCommands, PreviewAndUiErrorsUseCommandQueue) {
    Fixture f;
    const auto original = f.run("GetCutter");
    EXPECT_EQ("OK", f.run("SetCutterTransform:position=0.5,0,0 preview=1"));
    EXPECT_EQ(original, f.run("GetCutter"));
    EXPECT_EQ("History:undo=0,redo=0", f.run("GetHistory"));
    f.dispatcher.submitUi("Boolean:invalid");
    f.dispatcher.processQueue();
    EXPECT_FALSE(f.dispatcher.lastUiError().empty());
    EXPECT_TRUE(f.dispatcher.collectResponses().empty());
    f.dispatcher.submitUi("SetCutterTransform:position=0.5,0,0");
    f.dispatcher.processQueue();
    EXPECT_TRUE(f.dispatcher.lastUiError().empty());
    EXPECT_EQ("History:undo=1,redo=0", f.run("GetHistory"));
}

TEST(MeshWorldBoolean, UnsuitableEvaluatedMeshIsRejectedWithoutBaking) {
    MeshWorld w;
    MeshModifier sub;
    sub.type = MeshModifier::Type::Subdivision;
    ASSERT_TRUE(w.addModifier(sub).empty());
    const auto revision = w.base().revision;
    const auto depth = w.undoDepth();
    EXPECT_FALSE(w.applyBoolean(BooleanOperation::Union).empty());
    EXPECT_EQ(revision, w.base().revision);
    EXPECT_EQ(depth, w.undoDepth());
    EXPECT_EQ(1u, w.modifiers().size());
}

TEST(MeshWorldBoolean, CutterNormalsFollowInverseTranspose) {
    MeshWorld w;
    ASSERT_TRUE(w.setEditMode(true).empty());
    ASSERT_TRUE(w.setElementKind(ElementKind::Face).empty());
    ASSERT_TRUE(w.select("all", SelectMode::Replace).empty());
    ASSERT_TRUE(w.shade(true, 180).empty());
    ASSERT_TRUE(w.setEditMode(false).empty());
    ASSERT_TRUE(w.setCutter(true).empty());
    auto t = w.cutter().transform;
    t.position = {4, 0, 0};
    t.rotation = {0, 90, 0};
    t.scale = {.5f, 1, 2};
    ASSERT_TRUE(w.setCutterTransform(t).empty());
    auto error = w.applyBoolean(BooleanOperation::Union);
    ASSERT_TRUE(error.empty()) << error;
    bool checked = false;
    for (const auto& f : w.base().faces)
        for (const auto& c : f.loop) {
            const auto* v = w.base().findVertex(c.vertex);
            if (v->pos[0] > 2) {
                ASSERT_TRUE(c.hasNormal);
                const auto expected = glm::normalize(glm::vec3(
                    v->pos[0] > 4 ? .5f : -.5f, v->pos[1] > 0 ? 1.f : -1.f, v->pos[2] > 0 ? 2.f : -2.f));
                for (int k = 0; k < 3; ++k)
                    EXPECT_NEAR(expected[k], c.normal[k], 1e-6);
                checked = true;
            }
        }
    EXPECT_TRUE(checked);
}

TEST(MeshEditViewReduction, CommandsHistoryAndAtomicFailure) {
    Fixture f;
    EXPECT_EQ("OK", f.run("NewMesh:plane divs=8"));
    const auto before = f.run("GetCounts");
    EXPECT_EQ("Reduction:input=128,output=64,target=64,changed=1,reached=1", f.run("Reduce:ratio=0.5"));
    EXPECT_EQ(64u, f.world.baseCounts().faces);
    EXPECT_TRUE(f.world.base().validate());
    const auto after = f.run("GetCounts");
    EXPECT_EQ("OK", f.run("Undo"));
    EXPECT_EQ(before, f.run("GetCounts"));
    for (const auto* cmd : {"Reduce:ratio=nan", "Reduce:ratio=0", "Reduce:target=-1",
                            "Reduce:target=1.5", "Reduce:ratio=0.5 target=10", "Reduce:unknown=1"})
        EXPECT_EQ(0u, f.run(cmd).find("Error:")) << cmd;
    EXPECT_EQ("History:undo=0,redo=1", f.run("GetHistory"));
    EXPECT_EQ("OK", f.run("Redo"));
    EXPECT_EQ(after, f.run("GetCounts"));
    EXPECT_EQ("OK", f.run("SetMode:edit"));
    EXPECT_EQ(0u, f.run("Reduce:ratio=0.5").find("Error:"));
    EXPECT_EQ(after, f.run("GetCounts"));
}

TEST(MeshEditViewReduction, ProtectedNoOpAndModifierBake) {
    Fixture f;
    EXPECT_EQ("Reduction:input=12,output=12,target=6,changed=0,reached=0", f.run("Reduce:ratio=0.5"));
    EXPECT_EQ("History:undo=0,redo=0", f.run("GetHistory"));
    EXPECT_EQ("OK", f.run("NewMesh:plane divs=8"));
    EXPECT_EQ("OK", f.run("AddModifier:subdivision levels=1"));
    const auto evaluated = f.run("GetEvaluatedCounts");
    EXPECT_EQ("Reduction:input=512,output=256,target=256,changed=1,reached=1", f.run("Reduce:target=256"));
    EXPECT_EQ("Modifiers:0", f.run("GetModifierCount"));
    EXPECT_EQ("OK", f.run("Undo"));
    EXPECT_EQ("Modifiers:1", f.run("GetModifierCount"));
    EXPECT_EQ(evaluated, f.run("GetEvaluatedCounts"));
    EXPECT_EQ("OK", f.run("Redo"));
    EXPECT_EQ("Modifiers:0", f.run("GetModifierCount"));
}

TEST(MeshEditViewReduction, CoreFailureAndNoOpKeepRedoAndReport) {
    Fixture f;
    ASSERT_EQ("OK", f.run("NewMesh:plane divs=8"));
    ASSERT_EQ(0u, f.run("Reduce:ratio=0.5").find("Reduction:"));
    ASSERT_EQ("OK", f.run("Undo"));
    const auto revision = f.world.base().revision;
    MeshReductionOptions options;
    options.maxWork = 1;
    MeshReductionReport report;
    report.work = 123;
    EXPECT_FALSE(f.world.reduce(options, report).empty());
    EXPECT_EQ(123u, report.work);
    EXPECT_EQ(revision, f.world.base().revision);
    EXPECT_EQ("History:undo=0,redo=1", f.run("GetHistory"));
    EXPECT_EQ(64u, f.world.lastReduction()->outputTriangles);
    EXPECT_EQ("Reduction:input=128,output=128,target=128,changed=0,reached=1", f.run("Reduce:ratio=1"));
    EXPECT_EQ(revision, f.world.base().revision);
    EXPECT_EQ("History:undo=0,redo=1", f.run("GetHistory"));
    EXPECT_EQ("OK", f.run("Redo"));
}

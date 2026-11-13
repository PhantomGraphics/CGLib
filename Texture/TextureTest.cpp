#include "Texture.h"
#include <gtest/gtest.h>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <chrono>
#include <thread>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../ThirdParty/stb/stb_image.h"
using namespace Phantom::Texture;
TEST(TextureRecipe, StableRoundTrip) {
    auto r=example(); std::string a,b,d; ASSERT_TRUE(serialize(r,a,d))<<d;
    Recipe loaded; ASSERT_TRUE(deserialize(a,loaded,d))<<d;
    std::reverse(loaded.nodes.begin(),loaded.nodes.end());
    ASSERT_TRUE(serialize(loaded,b,d)); EXPECT_EQ(a,b);
    Image first,second; ASSERT_TRUE(evaluate(r,first,d)); ASSERT_TRUE(evaluate(loaded,second,d)); EXPECT_EQ(first.pixels,second.pixels);
}
TEST(TextureRecipe, RejectsInvalidGraphsAndPreservesState) {
    std::string d; auto r=example(); auto original=r.output;
    EXPECT_FALSE(deserialize("{}",r,d)); EXPECT_EQ(r.output,original);
    r.formatVersion=2; EXPECT_FALSE(validate(r,d)); r=example();
    r.connections.push_back({"base","pattern",0}); EXPECT_FALSE(validate(r,d));
    r=example(); r.nodes[0].type=NodeType::BaseColor; r.connections.push_back({"base","pattern",0}); EXPECT_FALSE(validate(r,d));
    r=example(); r.nodes.push_back(r.nodes[0]); EXPECT_FALSE(validate(r,d));
    r=example(); r.connections.clear(); EXPECT_FALSE(validate(r,d));
    r=example(); r.nodes[0].type=NodeType::Blend;
    r.connections={{"pattern","pattern",0},{"pattern","pattern",1},{"pattern","pattern",2},{"pattern","base",0}};
    EXPECT_FALSE(validate(r,d)); EXPECT_NE(d.find("cycle"),std::string::npos);
}
TEST(TextureEvaluator, SeedAndCancellation) {
    auto r=example(); r.nodes[0].type=NodeType::Noise; r.nodes[0].seed=42;
    Image a,b; std::string d; ASSERT_TRUE(evaluate(r,a,d)); ASSERT_TRUE(evaluate(r,b,d)); EXPECT_EQ(a.pixels,b.pixels);
    r.nodes[0].seed=43; ASSERT_TRUE(evaluate(r,b,d)); EXPECT_NE(a.pixels,b.pixels);
    std::atomic_bool cancel=true; auto old=b.pixels; EXPECT_FALSE(evaluate(r,b,d,&cancel)); EXPECT_EQ(old,b.pixels);
}
TEST(TextureEvaluator, LinearAlphaComposite) {
    Recipe r=example(); Node a,b,mask,blend,out;
    a.id="a"; a.first={0,0,0,1}; b.id="b"; b.first={1,1,1,0.5f};
    mask.id="mask"; mask.first={1,1,1,1}; blend.id="blend"; blend.type=NodeType::Blend;
    out.id="out"; out.type=NodeType::BaseColor;
    r.nodes={a,b,mask,blend,out}; r.output="out";
    r.connections={{"a","blend",0},{"b","blend",1},{"mask","blend",2},{"blend","out",0}};
    Image image; std::string d; ASSERT_TRUE(evaluate(r,image,d))<<d;
    EXPECT_FLOAT_EQ(image.pixels[0][0],0.5f); EXPECT_FLOAT_EQ(image.pixels[0][3],1);
}
TEST(TextureRecipe, DiskRoundTripWithoutCache) {
    auto dir=std::filesystem::temp_directory_path()/"cglib-texture-test";
    std::filesystem::create_directories(dir); auto path=dir/"test.texture.json";
    std::string d; auto r=example(); ASSERT_TRUE(save(path,r,d)); ASSERT_TRUE(save(path,r,d));
    Image a,b; ASSERT_TRUE(evaluate(r,a,d)); ASSERT_TRUE(writePng(dir/"test.png",a,d));
    std::filesystem::remove(dir/"test.png"); Recipe loaded; ASSERT_TRUE(load(path,loaded,d)); ASSERT_TRUE(evaluate(loaded,b,d)); EXPECT_EQ(a.pixels,b.pixels);
    std::filesystem::remove(path); std::filesystem::remove(dir);
}

TEST(TextureEvaluator, GradientCheckerAndSrgbPng) {
    auto r=example(); std::string d; Image image;
    r.nodes[0].type=NodeType::Gradient; r.nodes[0].first={0,0,0,1}; r.nodes[0].second={1,1,1,1};
    ASSERT_TRUE(evaluate(r,image,d)); EXPECT_NEAR(image.pixels[0][0],0.5f/512,1e-6f);
    EXPECT_NEAR(image.pixels[511][0],511.5f/512,1e-6f);
    r.nodes[0].type=NodeType::Checker; ASSERT_TRUE(evaluate(r,image,d));
    EXPECT_EQ(image.pixels[0],r.nodes[0].first); EXPECT_EQ(image.pixels[64],r.nodes[0].second);
    Image gray; gray.width=gray.height=1; gray.pixels={{0.5f,0.5f,0.5f,0.5f}};
    auto path=std::filesystem::temp_directory_path()/"cglib-texture-srgb.png";
    ASSERT_TRUE(writePng(path,gray,d));
    std::ifstream file(path,std::ios::binary); std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{}); file.close();
    int w=0,h=0,channels=0; auto* pixels=stbi_load_from_memory(bytes.data(),int(bytes.size()),&w,&h,&channels,4);
    ASSERT_NE(pixels,nullptr); EXPECT_EQ(w,1); EXPECT_EQ(h,1); EXPECT_EQ(pixels[0],188); EXPECT_EQ(pixels[3],128);
    stbi_image_free(pixels); std::filesystem::remove(path);
    Recipe loaded; std::string json; ASSERT_TRUE(serialize(r,json,d));
    auto pos=json.find("\"resolution\": 512"); ASSERT_NE(pos,std::string::npos);
    json.replace(pos,17,"\"resolution\": 512.5"); EXPECT_FALSE(deserialize(json,loaded,d));
}

TEST(TexturePaint, InterpolationEraseAndClip) {
    auto r=example(); r.formatVersion=r.evaluatorVersion=2; r.nodes[0].type=NodeType::Paint;
    Stroke s; s.radius=0.04f; s.hardness=1; s.samples={{0.1f,0.5f},{0.9f,0.5f}};
    r.nodes[0].strokes={s}; Image image; std::string d;
    ASSERT_TRUE(evaluate(r,image,d))<<d;
    for (unsigned x=52;x<460;++x) EXPECT_FLOAT_EQ(image.pixels[256*512+x][3],1);
    EXPECT_EQ(image.pixels[0],(Color{0,0,0,0}));
    Stroke eraser=s; eraser.erase=true; eraser.radius=0.08f; eraser.samples={{0.5f,0.5f}};
    r.nodes[0].strokes.push_back(eraser); ASSERT_TRUE(evaluate(r,image,d));
    EXPECT_FLOAT_EQ(image.pixels[256*512+256][3],0);
    EXPECT_FLOAT_EQ(image.pixels[256*512+100][3],1);
    s.samples={{-0.1f,0},{0.5f,0}}; r.nodes[0].strokes={s};
    ASSERT_TRUE(evaluate(r,image,d)); EXPECT_FLOAT_EQ(image.pixels[0][3],1);
    EXPECT_FLOAT_EQ(image.pixels[511*512][3],0);
}
TEST(TexturePaint, RoundTripResolutionAndInvalidStrokes) {
    auto r=example(); r.formatVersion=r.evaluatorVersion=2; r.nodes[0].type=NodeType::Paint;
    Stroke s; s.radius=0.08f; s.hardness=0; s.strength=0.5f; s.samples={{0.5f,0.5f}};
    r.nodes[0].strokes={s}; std::string text,again,d; Recipe loaded; Image first,second;
    ASSERT_TRUE(serialize(r,text,d)); ASSERT_TRUE(deserialize(text,loaded,d));
    ASSERT_TRUE(serialize(loaded,again,d)); EXPECT_EQ(text,again);
    ASSERT_TRUE(evaluate(r,first,d)); ASSERT_TRUE(evaluate(loaded,second,d)); EXPECT_EQ(first.pixels,second.pixels);
    EXPECT_NEAR(first.pixels[256*512+256][3],0.5f,0.015f);
    EXPECT_GT(first.pixels[256*512+260][3],first.pixels[256*512+285][3]);
    loaded.resolution=1024; ASSERT_TRUE(evaluate(loaded,second,d));
    EXPECT_NEAR(first.pixels[256*512+256][3],second.pixels[512*1024+512][3],0.01f);
    auto painted=std::find_if(loaded.nodes.begin(),loaded.nodes.end(),[](const Node& n){return n.type==NodeType::Paint;});
    ASSERT_NE(painted,loaded.nodes.end()); painted->strokes[0].radius=0; EXPECT_FALSE(validate(loaded,d));
    loaded=r; loaded.nodes[0].strokes[0].samples.clear(); EXPECT_FALSE(validate(loaded,d));
    loaded=r; loaded.formatVersion=loaded.evaluatorVersion=1; EXPECT_FALSE(validate(loaded,d));
    loaded=r; loaded.nodes[0].type=NodeType::Solid; EXPECT_FALSE(validate(loaded,d));
    auto old=second.pixels; std::atomic_bool cancel=true; EXPECT_FALSE(evaluate(r,second,d,&cancel)); EXPECT_EQ(old,second.pixels);
}

TEST(TexturePreview, PackedBytesMatchFloatEvaluationPngAndCancellation) {
    auto r=example(); r.formatVersion=r.evaluatorVersion=2; r.resolution=1024;
    Node paint; paint.id="paint"; paint.type=NodeType::Paint;
    Stroke stroke; stroke.color={0.2f,0.6f,0.9f,0.7f}; stroke.radius=0.08f; stroke.strength=0.6f; stroke.samples={{0.1f,0.2f},{0.9f,0.8f}};
    paint.strokes={stroke};
    Node mask; mask.id="mask"; mask.first={1,1,1,1};
    Node blend; blend.id="blend"; blend.type=NodeType::Blend;
    r.nodes.push_back(paint); r.nodes.push_back(mask); r.nodes.push_back(blend);
    r.connections={{"pattern","blend",0},{"paint","blend",1},{"mask","blend",2},{"blend","base",0}};
    Image linear; ByteImage preview,rgba; std::string d;
    ASSERT_TRUE(evaluate(r,linear,d))<<d;
    ASSERT_TRUE(evaluateSrgb8(r,preview,d,true))<<d;
    ASSERT_TRUE(evaluateSrgb8(r,rgba,d,false))<<d;
    auto path=std::filesystem::temp_directory_path()/"cglib-texture-preview.png";
    ASSERT_TRUE(writePng(path,linear,d));
    std::ifstream file(path,std::ios::binary); std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{}); file.close();
    int w=0,h=0,channels=0;
    auto* decoded=stbi_load_from_memory(bytes.data(),int(bytes.size()),&w,&h,&channels,4);
    ASSERT_NE(decoded,nullptr); ASSERT_EQ(w,1024); ASSERT_EQ(h,1024);
    std::vector<uint8_t> expected(decoded,decoded+size_t(w)*h*4); stbi_image_free(decoded);
    EXPECT_EQ(rgba.pixels,expected);
    for(size_t p=0;p<expected.size();p+=4) std::swap(expected[p],expected[p+2]);
    EXPECT_EQ(preview.pixels,expected);
    auto original=preview.pixels; std::atomic_bool cancel=true;
    EXPECT_FALSE(evaluateSrgb8(r,preview,d,true,&cancel)); EXPECT_EQ(original,preview.pixels);
    r.resolution=17; EXPECT_FALSE(evaluateSrgb8(r,preview,d)); EXPECT_EQ(original,preview.pixels);
    std::filesystem::remove(path);
}

TEST(TexturePreview, IncrementalCacheMatchesFullReplayAndInvalidates) {
    auto r=example(); r.formatVersion=r.evaluatorVersion=2; r.resolution=1024;
    r.nodes[0].type=NodeType::Paint;
    PreviewEvaluator cache; ByteImage actual,expected; std::string d;
    auto compare=[&] {
        ASSERT_TRUE(cache.evaluate(r,actual,d))<<d;
        ASSERT_TRUE(evaluateSrgb8(r,expected,d,true))<<d;
        EXPECT_EQ(actual.width,expected.width); EXPECT_EQ(actual.pixels,expected.pixels);
    };
    compare();
    Stroke s; s.color={0.1f,0.4f,0.8f,0.7f}; s.strength=0.35f; s.radius=0.035f; s.samples={{-0.02f,0.1f},{0.5f,0.5f},{1.1f,0.9f}};
    r.nodes[0].strokes.push_back(s); compare();
    s.erase=true; s.samples={{0.5f,0.5f}}; r.nodes[0].strokes.push_back(s); compare();
    s.erase=false; s.samples={{-0.9f,-0.9f}}; r.nodes[0].strokes.push_back(s); compare(); // outside, no changed pixels
    auto previous=actual.pixels; std::atomic_bool cancel=true;
    s.samples={{0.6f,0.2f}}; r.nodes[0].strokes.push_back(s);
    EXPECT_FALSE(cache.evaluate(r,actual,d,&cancel)); EXPECT_EQ(previous,actual.pixels);
    compare(); // cancelled work did not advance cache
    r.nodes[0].strokes[0].color={0.9f,0.2f,0.4f,0.5f}; compare(); // edited stroke rebuilds
    r.nodes[0].strokes.pop_back(); compare(); // undo rebuilds
    r.nodes[0].strokes.push_back(s); compare(); // redo / append
    r.nodes[0].x=200; compare(); // placement does not affect evaluation
    r.resolution=512; compare();
    r.nodes[0].strokes.clear(); r.nodes[0].type=NodeType::Noise; compare(); // topology and parameters rebuild
    r.nodes[0].seed=123; compare();
    auto saved=actual.pixels; r.connections.clear(); EXPECT_FALSE(cache.evaluate(r,actual,d)); EXPECT_EQ(saved,actual.pixels);
}

TEST(TexturePreview, CancellationDuringWorkRollsBackCachedPaint) {
    auto r=example(); r.formatVersion=r.evaluatorVersion=2; r.resolution=2048; r.nodes[0].type=NodeType::Paint;
    PreviewEvaluator cache; ByteImage preview,expected; std::string d;
    ASSERT_TRUE(cache.evaluate(r,preview,d)); const auto original=preview.pixels;
    auto heavy=r; Stroke fill; fill.radius=0.4f; fill.strength=0.2f; fill.color={0,1,0,0.8f};
    fill.samples={{0.1f,0.1f},{0.9f,0.9f},{0.1f,0.9f},{0.9f,0.1f}}; heavy.nodes[0].strokes.push_back(fill);
    std::atomic_bool cancel=false;
    std::thread request([&]{std::this_thread::sleep_for(std::chrono::milliseconds(10));cancel.store(true);});
    const bool completed=cache.evaluate(heavy,preview,d,&cancel); request.join();
    EXPECT_FALSE(completed); EXPECT_EQ(original,preview.pixels);
    // A subsequent translucent stroke reveals corruption of the retained paint
    // layer even if cancellation had restored the packed output alone.
    Stroke small; small.color={1,0,0,0.3f}; small.radius=0.05f; small.samples={{0.5f,0.5f}};
    r.nodes[0].strokes.push_back(small);
    ASSERT_TRUE(cache.evaluate(r,preview,d)); ASSERT_TRUE(evaluateSrgb8(r,expected,d,true)); EXPECT_EQ(preview.pixels,expected.pixels);
}

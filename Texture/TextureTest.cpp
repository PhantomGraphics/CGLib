#include "Texture.h"
#include <gtest/gtest.h>
#include <fstream>
#include <algorithm>
#include <iterator>
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

#include "WaterSurfaceReference.h"
#include <gtest/gtest.h>
#include <limits>
using namespace Phantom::Effects::WaterSurface;
TEST(WaterSurfaceReference, InvalidConfiguration) {
    WaterSurfaceReference s; WaterSurfaceConfig c;
    c.columns=2; EXPECT_FALSE(s.create(c)); c.columns=17;
    c.width=0; EXPECT_FALSE(s.create(c)); c.width=10;
    c.damping=std::numeric_limits<float>::quiet_NaN(); EXPECT_FALSE(s.create(c));
}
TEST(WaterSurfaceReference, RestAndReset) {
    WaterSurfaceReference s; WaterSurfaceConfig c; c.columns=c.rows=17;
    ASSERT_TRUE(s.create(c)); s.advance(1);
    for(auto cell:s.cells()) { EXPECT_EQ(cell.height,0); EXPECT_EQ(cell.velocity,0); }
    ASSERT_TRUE(s.addImpulse({5,5,1,1})); s.singleStep();
    EXPECT_GT(s.cells()[8*17+8].height,0); s.reset();
    for(auto cell:s.cells()) EXPECT_EQ(cell.height,0);
}
TEST(WaterSurfaceReference, SymmetryAndFixedBoundary) {
    WaterSurfaceReference s; WaterSurfaceConfig c; c.columns=c.rows=17;
    ASSERT_TRUE(s.create(c)); ASSERT_TRUE(s.addImpulse({5,5,1,1}));
    for(int i=0;i<100;++i) s.singleStep();
    for(int y=0;y<17;++y) for(int x=0;x<17;++x) {
        EXPECT_NEAR(s.cells()[y*17+x].height,s.cells()[y*17+16-x].height,1e-6f);
        if(x==0 || y==0 || x==16 || y==16) EXPECT_EQ(s.cells()[y*17+x].height,0);
    }
}
TEST(WaterSurfaceReference, FramePartitionAndPause) {
    WaterSurfaceReference a,b; WaterSurfaceConfig c; c.columns=23; c.rows=17;
    ASSERT_TRUE(a.create(c)); ASSERT_TRUE(b.create(c));
    a.addImpulse({5,5,1,1}); b.addImpulse({5,5,1,1});
    a.advance(1.0/60); b.advance(1.0/120); b.advance(1.0/120);
    for(size_t i=0;i<a.cells().size();++i) EXPECT_NEAR(a.cells()[i].height,b.cells()[i].height,1e-6f);
    auto cells=a.cells(); a.advance(100,true);
    for(size_t i=0;i<cells.size();++i) EXPECT_EQ(cells[i].height,a.cells()[i].height);
}
TEST(WaterSurfaceReference, HighSpeedRemainsFinite) {
    WaterSurfaceReference s; WaterSurfaceConfig c; c.columns=51; c.rows=17; c.waveSpeed=100; c.damping=1;
    ASSERT_TRUE(s.create(c)); s.addImpulse({5,5,1,1});
    for(int i=0;i<1000;++i) s.advance(10);
    for(auto cell:s.cells()) { EXPECT_TRUE(std::isfinite(cell.height)); EXPECT_LT(std::abs(cell.height),1); }
}
TEST(WaterSurfaceReference, Superposition) {
    WaterSurfaceReference a,b,sum; WaterSurfaceConfig c; c.columns=c.rows=21;
    ASSERT_TRUE(a.create(c)); ASSERT_TRUE(b.create(c)); ASSERT_TRUE(sum.create(c));
    WaterImpulse p{4,5,1,1},q{6,5,1,-0.7f};
    a.addImpulse(p); b.addImpulse(q); sum.addImpulse(p); sum.addImpulse(q);
    for(int i=0;i<60;++i) { a.singleStep(); b.singleStep(); sum.singleStep(); }
    for(size_t i=0;i<a.cells().size();++i)
        EXPECT_NEAR(sum.cells()[i].height,a.cells()[i].height+b.cells()[i].height,1e-6f);
}
TEST(WaterSurfaceReference, DampingRemovesEnergy) {
    WaterSurfaceReference damped,undamped; WaterSurfaceConfig c; c.columns=c.rows=21; c.damping=2;
    ASSERT_TRUE(damped.create(c)); c.damping=0; ASSERT_TRUE(undamped.create(c));
    damped.addImpulse({5,5,1,1}); undamped.addImpulse({5,5,1,1});
    for(int i=0;i<1200;++i) { damped.singleStep(); undamped.singleStep(); }
    auto energy=[](const WaterSurfaceReference& s) {
        double e=0;
        for(int y=1;y<20;++y) for(int x=1;x<20;++x) {
            auto i=y*21+x;
            const double dx=(s.cells()[i+1].height-s.cells()[i].height)/0.5;
            const double dz=(s.cells()[i+21].height-s.cells()[i].height)/0.5;
            e+=s.cells()[i].velocity*s.cells()[i].velocity+dx*dx+dz*dz;
        }
        return e;
    };
    EXPECT_LT(energy(damped),energy(undamped)*0.01);
}

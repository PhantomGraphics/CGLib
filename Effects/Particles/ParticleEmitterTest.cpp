#include "ParticleEmitter.h"
#include <gtest/gtest.h>
#include <limits>
using namespace Phantom::Effects::Particles;
TEST(ParticleEmitter, FractionalEmissionAndReset) {
    ParticleEmitter emitter;emitter.params().emitRate=2;
    EXPECT_EQ(emitter.update(.125f,10).count,0u);
    EXPECT_EQ(emitter.update(.125f,10).count,0u);
    EXPECT_EQ(emitter.update(.125f,10).count,0u);
    auto batch=emitter.update(.125f,10);EXPECT_EQ(batch.count,1u);EXPECT_EQ(batch.start,0u);
    emitter.reset();emitter.params().emitRate=8;
    auto first=emitter.update(.125f,10);EXPECT_EQ(first.count,1u);EXPECT_EQ(first.start,0u);EXPECT_EQ(first.seed,1u);
}
TEST(ParticleEmitter, RejectsInvalidStepsAndDoesNotAccumulateWhileDisabled) {
    ParticleEmitter emitter;emitter.params().emitRate=8;
    EXPECT_EQ(emitter.update(-1,10).count,0u);
    EXPECT_EQ(emitter.update(std::numeric_limits<float>::quiet_NaN(),10).count,0u);
    emitter.setEnabled(false);EXPECT_EQ(emitter.update(100,10).count,0u);
    emitter.setEnabled(true);EXPECT_EQ(emitter.update(.125f,10).count,1u);
}
TEST(ParticleEmitter, SaturationDoesNotBuildAnUnboundedBacklog) {
    ParticleEmitter emitter;emitter.params().emitRate=std::numeric_limits<float>::max();
    EXPECT_EQ(emitter.update(1,8).count,8u);
    emitter.params().emitRate=0;
    EXPECT_EQ(emitter.update(1,8).count,0u);
    EXPECT_EQ(emitter.update(1,0).count,0u);
}

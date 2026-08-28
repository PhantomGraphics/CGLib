#include "pch.h"
#include "../Math/Box2d.h"
#include "../Math/Vector2d.h"

using namespace Phantom::Math;

TEST(Box2dTest, DefaultConstructor)
{
    Box2df box;
    EXPECT_EQ(box.getMin().x, 0.0f);
    EXPECT_EQ(box.getMin().y, 0.0f);
    EXPECT_EQ(box.getMax().x, 1.0f);
    EXPECT_EQ(box.getMax().y, 1.0f);
}

TEST(Box2dTest, SinglePointConstructor)
{
    Vector2df pt(2.0f, 3.0f);
    Box2df box(pt);
    EXPECT_EQ(box.getMin(), pt);
    EXPECT_EQ(box.getMax(), pt);
}

TEST(Box2dTest, TwoPointConstructor)
{
    Vector2df pt1(1.0f, 2.0f);
    Vector2df pt2(3.0f, 0.0f);
    Box2df box(pt1, pt2);
    EXPECT_EQ(box.getMin(), Vector2df(1.0f, 0.0f));
    EXPECT_EQ(box.getMax(), Vector2df(3.0f, 2.0f));
}

TEST(Box2dTest, AddPoint)
{
    Box2df box(Vector2df(1.0f, 1.0f));
    box.add(Vector2df(2.0f, 3.0f));
    EXPECT_EQ(box.getMin(), Vector2df(1.0f, 1.0f));
    EXPECT_EQ(box.getMax(), Vector2df(2.0f, 3.0f));
    box.add(Vector2df(-1.0f, 0.5f));
    EXPECT_EQ(box.getMin(), Vector2df(-1.0f, 0.5f));
    EXPECT_EQ(box.getMax(), Vector2df(2.0f, 3.0f));
}

TEST(Box2dTest, AddBox)
{
    Box2df box1(Vector2df(0.0f, 0.0f), Vector2df(1.0f, 1.0f));
    Box2df box2(Vector2df(-1.0f, 0.5f), Vector2df(0.5f, 2.0f));
    box1.add(box2);
    EXPECT_EQ(box1.getMin(), Vector2df(-1.0f, 0.0f));
    EXPECT_EQ(box1.getMax(), Vector2df(1.0f, 2.0f));
}

TEST(Box2dTest, CreateDegeneratedBox)
{
    auto box = Box2df::createDegeneratedBox();
    EXPECT_GT(box.getMin().x, box.getMax().x);
    EXPECT_GT(box.getMin().y, box.getMax().y);
}
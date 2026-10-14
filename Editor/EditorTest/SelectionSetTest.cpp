#include <gtest/gtest.h>
#include "../Selection/SelectionSet.h"
#include <string>
#include <type_traits>

template <class Id> class OrderedSelectionTest : public ::testing::Test {};
using IdTypes = ::testing::Types<int, std::string>;
TYPED_TEST_SUITE(OrderedSelectionTest, IdTypes);

TYPED_TEST(OrderedSelectionTest, PreservesOrderToggleMultiplicityAndPrune) {
    const auto make = [](int n) -> TypeParam {
        if constexpr (std::is_same_v<TypeParam, int>) return n;
        else return std::to_string(n);
    };
    const auto a = make(1), b = make(2), c = make(3);
    Phantom::Editor::SelectionSet<TypeParam> set;
    EXPECT_TRUE(set.empty());
    set.replace({b, a, b});
    set.add({a, c});
    EXPECT_EQ(set.ids(), (std::vector<TypeParam>{b, a, c}));
    set.toggle({a, a, c});
    EXPECT_EQ(set.ids(), (std::vector<TypeParam>{b, a}));
    set.prune([&](const auto& id) { return id == a; });
    EXPECT_EQ(set.ids(), (std::vector<TypeParam>{a}));
    EXPECT_TRUE(set.contains(a));
    EXPECT_FALSE(set.contains(b));
    set.replace({c, c});
    EXPECT_EQ(set.ids(), (std::vector<TypeParam>{c}));
    set.clear();
    EXPECT_TRUE(set.empty());
}

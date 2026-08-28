#include "gtest/gtest.h"

#include "../Space/IndexedSortBasedSearcher.h"

#include <algorithm>
#include <random>
#include <set>
#include <vector>

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {

// Every unordered pair closer than effectLength, found by checking all
// O(n^2) combinations. Uses the same distance predicate as the searcher so
// the two cannot disagree on a pair sitting exactly on the radius.
std::set<std::pair<int, int>> bruteForcePairs(const std::vector<Vector3df>& positions, const float effectLength)
{
	std::set<std::pair<int, int>> expected;
	const auto effectLengthSquared = effectLength * effectLength;
	for (int i = 0; i < static_cast<int>(positions.size()); ++i) {
		for (int j = i + 1; j < static_cast<int>(positions.size()); ++j) {
			if (getDistanceSquared(positions[i], positions[j]) < effectLengthSquared) {
				expected.emplace(i, j);
			}
		}
	}
	return expected;
}

std::set<std::pair<int, int>> toUnorderedPairSet(const std::vector<ParticlePair>& pairs)
{
	std::set<std::pair<int, int>> actual;
	for (const auto& pair : pairs) {
		actual.emplace(std::min(pair.first, pair.second), std::max(pair.first, pair.second));
	}
	return actual;
}

// A cloud spanning several cells in every direction *including negative
// coordinates* -- the grid ID packing is signed (see IndexedParticle::
// gridStrideY), so a cloud sitting entirely in the positive octant would not
// exercise it.
std::vector<Vector3df> makeRandomCloud(const int count, const float extent, const unsigned int seed)
{
	std::mt19937 engine(seed);
	std::uniform_real_distribution<float> dist(-extent, extent);
	std::vector<Vector3df> positions;
	positions.reserve(count);
	for (int i = 0; i < count; ++i) {
		positions.emplace_back(dist(engine), dist(engine), dist(engine));
	}
	return positions;
}

}

TEST(IndexedSortBasedSearcherTest, TestSearch)
{
	IndexedSortBasedSearcher searcher(1.0f);
	searcher.add(Vector3df(0, 0, 0));
	searcher.add(Vector3df(0.5, 0, 0));
	searcher.search();
	const auto pairs = searcher.getPairs();
	EXPECT_EQ(1, pairs.size());
}

TEST(IndexedSortBasedSearcherTest, PairsBeyondEffectLengthAreExcluded)
{
	IndexedSortBasedSearcher searcher(1.0f);
	searcher.add(Vector3df(0, 0, 0));
	searcher.add(Vector3df(1.5f, 0, 0));
	searcher.search();
	EXPECT_TRUE(searcher.getPairs().empty());
}

// search() used to append onto the pairs left by the previous call, so a
// reused searcher silently reported every pair twice.
TEST(IndexedSortBasedSearcherTest, RepeatedSearchReplacesRatherThanAccumulates)
{
	IndexedSortBasedSearcher searcher(1.0f);
	searcher.add(Vector3df(0, 0, 0));
	searcher.add(Vector3df(0.5f, 0, 0));

	searcher.search();
	const auto firstCount = searcher.getPairs().size();
	searcher.search();

	EXPECT_EQ(firstCount, searcher.getPairs().size());
}

TEST(IndexedSortBasedSearcherTest, ClearDropsAddedPositions)
{
	IndexedSortBasedSearcher searcher(1.0f);
	searcher.add(Vector3df(0, 0, 0));
	searcher.add(Vector3df(0.5f, 0, 0));
	searcher.search();
	ASSERT_FALSE(searcher.getPairs().empty());

	searcher.clear();
	searcher.search();
	EXPECT_TRUE(searcher.getPairs().empty());
}

TEST(IndexedSortBasedSearcherTest, BulkAddMatchesRepeatedSingleAdd)
{
	const auto positions = makeRandomCloud(200, 3.0f, 20260821u);

	IndexedSortBasedSearcher oneByOne(1.0f);
	for (const auto& position : positions) {
		oneByOne.add(position);
	}
	oneByOne.search();

	IndexedSortBasedSearcher bulk(1.0f);
	bulk.add(positions);
	bulk.search();

	EXPECT_EQ(oneByOne.getPairs(), bulk.getPairs());
}

// The 13 forward-cell windows the search scans must cover the whole
// neighborhood: a missed window shows up here as a missing pair, an
// off-by-one in a window's span as a duplicate.
TEST(IndexedSortBasedSearcherTest, MatchesBruteForceOnRandomCloud)
{
	const float effectLength = 1.0f;
	const auto positions = makeRandomCloud(800, 4.0f, 12345u);

	IndexedSortBasedSearcher searcher(effectLength);
	searcher.add(positions);
	searcher.search();

	const auto& pairs = searcher.getPairs();
	const auto actual = toUnorderedPairSet(pairs);

	EXPECT_EQ(bruteForcePairs(positions, effectLength), actual);
	// Each pair exactly once -- toUnorderedPairSet() would have collapsed
	// duplicates, so compare the raw count too.
	EXPECT_EQ(actual.size(), pairs.size());
}

// Same check with the cloud packed tightly enough that most particles share a
// cell, which is the case that stresses the same-row scan rather than the
// forward-row cursors.
TEST(IndexedSortBasedSearcherTest, MatchesBruteForceOnDenseCloud)
{
	const float effectLength = 1.0f;
	const auto positions = makeRandomCloud(400, 0.8f, 777u);

	IndexedSortBasedSearcher searcher(effectLength);
	searcher.add(positions);
	searcher.search();

	EXPECT_EQ(bruteForcePairs(positions, effectLength), toUnorderedPairSet(searcher.getPairs()));
}

TEST(IndexedSortBasedSearcherTest, EmptyInputFindsNoPairs)
{
	IndexedSortBasedSearcher searcher(1.0f);
	searcher.search();
	EXPECT_TRUE(searcher.getPairs().empty());
}

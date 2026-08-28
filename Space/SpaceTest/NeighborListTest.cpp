#include "gtest/gtest.h"

#include "../Space/NeighborList.h"

#include <algorithm>
#include <random>
#include <vector>

using namespace Phantom::Space;

TEST(NeighborListTest, EmptyPairsGivesEmptyNeighbors)
{
	CSRNeighborList neighbors;
	neighbors.build(3, {});

	for (size_t i = 0; i < 3; ++i) {
		EXPECT_EQ(neighbors[i].begin(), neighbors[i].end());
	}
}

TEST(NeighborListTest, SinglePairIsSymmetric)
{
	CSRNeighborList neighbors;
	neighbors.build(2, { ParticlePair(0, 1) });

	std::vector<int> n0(neighbors[0].begin(), neighbors[0].end());
	std::vector<int> n1(neighbors[1].begin(), neighbors[1].end());
	EXPECT_EQ(n0, std::vector<int>{1});
	EXPECT_EQ(n1, std::vector<int>{0});
}

TEST(NeighborListTest, MatchesNaivePerParticleVectorConstruction)
{
	const size_t particleCount = 6;
	const std::vector<ParticlePair> pairs = {
		ParticlePair(0, 1), ParticlePair(0, 2), ParticlePair(1, 2),
		ParticlePair(3, 4), ParticlePair(4, 5), ParticlePair(0, 5),
	};

	std::vector<std::vector<int>> expected(particleCount);
	for (const auto& pair : pairs) {
		expected[pair.first].push_back(pair.second);
		expected[pair.second].push_back(pair.first);
	}

	CSRNeighborList neighbors;
	neighbors.build(particleCount, pairs);

	for (size_t i = 0; i < particleCount; ++i) {
		std::vector<int> actual(neighbors[i].begin(), neighbors[i].end());
		std::sort(actual.begin(), actual.end());
		std::sort(expected[i].begin(), expected[i].end());
		EXPECT_EQ(actual, expected[i]) << "particle " << i;
	}
}

// --- build(positions, effectLength): search + flatten in one call ---------

namespace {

std::vector<std::vector<int>> bruteForceNeighbors(const std::vector<Phantom::Math::Vector3df>& positions, const float effectLength)
{
	std::vector<std::vector<int>> expected(positions.size());
	const auto effectLengthSquared = effectLength * effectLength;
	for (size_t i = 0; i < positions.size(); ++i) {
		for (size_t j = 0; j < positions.size(); ++j) {
			if (i == j) continue;
			if (Phantom::Math::getDistanceSquared(positions[i], positions[j]) < effectLengthSquared) {
				expected[i].push_back(static_cast<int>(j));
			}
		}
	}
	return expected;
}

std::vector<int> sortedRow(const CSRNeighborList& neighbors, const size_t i)
{
	std::vector<int> row(neighbors[i].begin(), neighbors[i].end());
	std::sort(row.begin(), row.end());
	return row;
}

}

TEST(NeighborListTest, BuildFromPositionsMatchesBruteForce)
{
	const float effectLength = 1.0f;
	std::mt19937 engine(20260821u);
	std::uniform_real_distribution<float> dist(-3.0f, 3.0f);

	std::vector<Phantom::Math::Vector3df> positions;
	positions.reserve(300);
	for (int i = 0; i < 300; ++i) {
		positions.emplace_back(dist(engine), dist(engine), dist(engine));
	}

	CSRNeighborList neighbors;
	neighbors.build(positions, effectLength);

	ASSERT_EQ(positions.size(), neighbors.size());

	const auto expected = bruteForceNeighbors(positions, effectLength);
	for (size_t i = 0; i < positions.size(); ++i) {
		auto row = expected[i];
		std::sort(row.begin(), row.end());
		EXPECT_EQ(row, sortedRow(neighbors, i)) << "particle " << i;
	}
}

// The solvers gather per particle (each thread writing only to particles[i]),
// which is only equivalent to the old pair-wise scatter if every row's
// neighbors also list it back.
TEST(NeighborListTest, BuildFromPositionsIsSymmetricAndExcludesSelf)
{
	const std::vector<Phantom::Math::Vector3df> positions = {
		Phantom::Math::Vector3df(0.0f, 0.0f, 0.0f),
		Phantom::Math::Vector3df(0.4f, 0.0f, 0.0f),
		Phantom::Math::Vector3df(0.0f, 0.4f, 0.0f),
		Phantom::Math::Vector3df(9.0f, 9.0f, 9.0f),
	};

	CSRNeighborList neighbors;
	neighbors.build(positions, 1.0f);

	for (size_t i = 0; i < positions.size(); ++i) {
		for (const int neighbor : neighbors[i]) {
			EXPECT_NE(static_cast<int>(i), neighbor) << "particle " << i << " listed itself";
			const auto back = sortedRow(neighbors, static_cast<size_t>(neighbor));
			EXPECT_NE(std::find(back.begin(), back.end(), static_cast<int>(i)), back.end())
				<< "particle " << neighbor << " does not list " << i << " back";
		}
	}
	EXPECT_EQ(0u, neighbors[3].size());
}

// A solver that never called setEffectLength() must get an inert (empty)
// neighborhood rather than a division by zero while binning cells.
TEST(NeighborListTest, NonPositiveEffectLengthGivesEmptyRows)
{
	const std::vector<Phantom::Math::Vector3df> positions = {
		Phantom::Math::Vector3df(0.0f, 0.0f, 0.0f),
		Phantom::Math::Vector3df(0.1f, 0.0f, 0.0f),
	};

	CSRNeighborList neighbors;
	neighbors.build(positions, 0.0f);

	ASSERT_EQ(positions.size(), neighbors.size());
	EXPECT_TRUE(neighbors[0].empty());
	EXPECT_TRUE(neighbors[1].empty());
}

TEST(NeighborListTest, ClearEmptiesTheList)
{
	CSRNeighborList neighbors;
	neighbors.build(2, { ParticlePair(0, 1) });
	ASSERT_EQ(2u, neighbors.size());

	neighbors.clear();
	EXPECT_EQ(0u, neighbors.size());
}

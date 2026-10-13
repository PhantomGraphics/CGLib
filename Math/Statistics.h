#pragma once

#include <vector>

namespace Phantom {
	namespace Math {

template<typename T>
class Statistics
{
public:
	Statistics() = default;

	explicit Statistics(const std::vector<T>& values) { this->values = values; }

	void add(const T value) { this->values.push_back(value); }

	T getAverage() const;

	T getVariance() const;

	T getStandardDeviation() const;

	/// The try* variants return false and leave `out` unchanged when the set is
	/// empty or contains a non-finite value (or the result is non-finite).
	/// Variance is the population variance (divides by N), same as getVariance().
	bool tryGetAverage(T& out) const;

	bool tryGetVariance(T& out) const;

	bool tryGetStandardDeviation(T& out) const;

private:
	std::vector<T> values;
};
	}
}
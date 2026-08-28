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

private:
	std::vector<T> values;
};
	}
}
#include "Statistics.h"

#include <cmath>

using namespace Phantom::Math;

template<typename T>
T Statistics<T>::getAverage() const
{
	T total = 0;
	for (int i = 0; i < values.size(); ++i) {
		total += values[i];
	}
	total /= static_cast<T>(values.size());
	return total;
}

template<typename T>
T Statistics<T>::getVariance() const
{
	const auto average = getAverage();

	T variance = 0;
	for (const auto v : values) {
		variance += (v - average) * (v-average);
	}
	variance /= static_cast<T>( values.size() );
	return variance;
}

template<typename T>
T Statistics<T>::getStandardDeviation() const
{
	return std::sqrt(getVariance());
}


template class Phantom::Math::Statistics<float>;
template class Phantom::Math::Statistics<double>;
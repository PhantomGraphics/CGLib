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


template<typename T>
bool Statistics<T>::tryGetAverage(T& out) const
{
	if (values.empty()) {
		return false;
	}
	T total = 0;
	for (const T v : values) {
		if (!std::isfinite(v)) {
			return false;
		}
		total += v;
	}
	const T average = total / static_cast<T>(values.size());
	if (!std::isfinite(average)) {
		return false;
	}
	out = average;
	return true;
}

template<typename T>
bool Statistics<T>::tryGetVariance(T& out) const
{
	T average;
	if (!tryGetAverage(average)) {
		return false;
	}
	T variance = 0;
	for (const T v : values) {
		variance += (v - average) * (v - average);
	}
	variance /= static_cast<T>(values.size());
	if (!std::isfinite(variance)) {
		return false;
	}
	out = variance;
	return true;
}

template<typename T>
bool Statistics<T>::tryGetStandardDeviation(T& out) const
{
	T variance;
	if (!tryGetVariance(variance)) {
		return false;
	}
	out = std::sqrt(variance);
	return true;
}

template class Phantom::Math::Statistics<float>;
template class Phantom::Math::Statistics<double>;
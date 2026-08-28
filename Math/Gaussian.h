#pragma once

#include <cmath>

namespace Phantom {
	namespace Math {

class Gaussian
{
public:
	Gaussian(const float a, const float b, const float c);

	float getWeight(const float x) const;

	static Gaussian createNormalDistributionFunc();

	static Gaussian createNormalDistributionFunc(const float mu, const float tau);

private:
	float a;
	float b;
	float c;
};

	}
}
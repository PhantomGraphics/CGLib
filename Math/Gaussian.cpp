#include "Gaussian.h"
#include "pi.h"

using namespace Phantom::Math;

Gaussian::Gaussian(const float a, const float b, const float c) :
	a(a),
	b(b),
	c(c)
{
}

float Gaussian::getWeight(const float x) const
{
	const auto power = (x - b) * (x - b) / (2.0f * c * c);
	return a * std::exp(-power);
}

Gaussian Gaussian::createNormalDistributionFunc()
{
	return createNormalDistributionFunc(0.0f, 1.0f);
}

Gaussian Gaussian::createNormalDistributionFunc(const float mu, const float tau)
{
	const auto denomi = std::sqrt(2.0f * PIf) * tau;
	const auto a = 1.0f / denomi;

	const auto b = mu;
	const auto c = tau;
	return Gaussian(a, b, c);
}


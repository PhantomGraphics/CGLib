#include "ZOrderCurve2d.h"

using namespace Phantom::Space;

namespace {
	const auto mask1 = 0b01010101010101010101010101010101;
	const auto mask2 = 0b00110011001100110011001100110011;
	const auto mask4 = 0b00001111000011110000111100001111;
	const auto mask8 = 0b00000000111111110000000011111111;
	const auto mask16 = 0b00000000000000001111111111111111;
	//const auto mask32 = 0b11111111111111111111111111111111;
}

unsigned int ZOrderCurve2d::encode(const std::array<unsigned int, 2>& index) const
{
	const auto ei1 = encode(index[0]);
	const auto ei2 = encode(index[1]);
	return (ei1 | (ei2 << 1));
}

unsigned int ZOrderCurve2d::encode(unsigned int n) const
{
	n = (n | (n << 16)) & mask16; // 0x0000ffff;
	n = (n | (n << 8)) & mask8; // 0x00ff00ff;
	n = (n | (n << 4)) & mask4; // 0x0f0f0f0f;
	n = (n | (n << 2)) & mask2; // 0x33333333;
	n = (n | (n << 1)) & mask1; // 0x55555555;
	return n;
}

std::array<unsigned int, 2> ZOrderCurve2d::decode(unsigned int x) const
{
	const auto ix = decode_(x);
	const auto iy = decode_(x >> 1);
	return { ix, iy };
}

unsigned int ZOrderCurve2d::decode_(unsigned int x) const
{
	x = x & mask1;
	x = (x ^ (x >> 1)) & mask2;
	x = (x ^ (x >> 2)) & mask4;
	x = (x ^ (x >> 4)) & mask8;
	x = (x ^ (x >> 8)) & mask16;
	return x;
}
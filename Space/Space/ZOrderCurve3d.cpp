#include "ZOrderCurve3d.h"

using namespace Phantom::Space;

namespace {
	const auto mask16 = 0b0011111111000000000000000011111111;//0x30000FF;
	const auto mask8 = 0b0000001111000000001111000000001111;//0x300F00F;
	const auto mask4 = 0b0011000011000011000011000011000011;//0x30C30C3;
	const auto mask2 = 0b1001001001001001001001001001001001;//0x9249249;
}

unsigned int ZOrderCurve3d::encode(const std::array<unsigned int, 3>& index)
{
	const auto ei1 = encode(index[0]);
	const auto ei2 = encode(index[1]);
	const auto ei3 = encode(index[2]);

	return (ei1 | (ei2 << 1) | (ei3 << 2));
}

std::array<unsigned int, 3> ZOrderCurve3d::decode(const unsigned int x)
{
	const auto ix = decode_(x);
	const auto iy = decode_(x >> 1);
	const auto iz = decode_(x >> 2);
	return { ix, iy, iz };
}

unsigned int ZOrderCurve3d::encode(unsigned int x)
{
	x = (x | (x << 16)) & mask16;
	x = (x | (x << 8)) & mask8;
	x = (x | (x << 4)) & mask4;
	x = (x | (x << 2)) & mask2;
	return x;
}

unsigned int ZOrderCurve3d::decode_(unsigned int x)
{
	x = x & mask2;
	x = (x ^ (x >> 2)) & mask4;
	x = (x ^ (x >> 4)) & mask8;
	x = (x ^ (x >> 8)) & mask16;
	//	x = (x ^ (x >> 16)) & mask16;
	return x;
}

// see https://edom18.hateblo.jp/entry/2017/07/28/083153.
unsigned int ZOrderCurve3d::getParent(unsigned int ltd, unsigned int rbd)
{
	auto xor_ = ltd ^ rbd; // 2点の位置のXOR
	int i = 0;
	int shift = 0;
	unsigned int spaceIndex = 0;

	while (xor_ != 0)
	{
		// 下位3bitずつマスクして値をチェックする
		if ((xor_ & 0x7) != 0)
		{
			// 空間シフト数を採用
			spaceIndex = (i + 1);
			shift = spaceIndex * 3;
		}

		// 3bitシフトさせて再チェック
		xor_ >>= 3;
		i++;
	}
	return spaceIndex;
}
#pragma once

namespace Phantom {

struct UnCopyable
{
	UnCopyable() = default;
	UnCopyable(const UnCopyable&) = delete;
	UnCopyable& operator=(const UnCopyable&) = delete;
};

}
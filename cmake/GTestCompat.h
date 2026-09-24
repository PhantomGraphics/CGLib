#pragma once

// The repository's NuGet package is GoogleTest 1.8.x, while some optional tests use the
// GTEST_SKIP() stream introduced by newer GoogleTest releases. Keep those tests buildable
// with the package already used by CGApp; unavailable-tool cases become an ordinary early
// return instead of a compile failure. Newer GoogleTest versions already define the macro.
#ifndef GTEST_SKIP
namespace Phantom::CMakeGTestCompat {
struct SkipStream {
    template <typename T>
    SkipStream& operator<<(const T&) { return *this; }
};
} // namespace Phantom::CMakeGTestCompat

#define GTEST_SKIP() if (true) return; else ::Phantom::CMakeGTestCompat::SkipStream{}
#endif

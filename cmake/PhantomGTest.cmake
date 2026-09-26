# One source-built GoogleTest version for standalone CGLib and its consumers.
# No system/NuGet fallback: installed packages must not change test semantics or CRT.
# For offline builds populate FETCHCONTENT_SOURCE_DIR_GOOGLETEST with v1.15.2 sources.
include_guard(GLOBAL)

function(phantom_find_gtest)
    if(TARGET GTest::gtest AND TARGET GTest::gtest_main)
        set(PHANTOM_GTEST_FOUND TRUE CACHE INTERNAL "GoogleTest targets available" FORCE)
        return()
    endif()

    include(FetchContent)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
        GIT_SHALLOW TRUE
    )
    # Build a static test library against the DLL CRT: /MDd in Debug, /MD otherwise.
    set(BUILD_SHARED_LIBS OFF)
    set(gtest_force_shared_crt ON CACHE BOOL "Match the consuming DLL CRT" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "Do not install the test dependency" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "Only GoogleTest is needed" FORCE)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    FetchContent_MakeAvailable(googletest)

    if(NOT TARGET GTest::gtest OR NOT TARGET GTest::gtest_main)
        message(FATAL_ERROR "Pinned GoogleTest v1.15.2 did not provide the required targets")
    endif()
    message(STATUS "Phantom: source-built GoogleTest v1.15.2 (DLL CRT on MSVC)")
    set(PHANTOM_GTEST_FOUND TRUE CACHE INTERNAL "GoogleTest targets available" FORCE)
endfunction()

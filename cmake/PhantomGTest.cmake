# One source-built GoogleTest version for standalone CGLib and its consumers.
# No system/NuGet fallback: installed packages must not change test semantics or CRT.
# For offline builds populate FETCHCONTENT_SOURCE_DIR_GOOGLETEST with v1.15.2 sources.
include_guard(GLOBAL)

function(phantom_find_gtest)
    # Honor an explicit CGLIB_BUILD_TESTING=OFF: no fetch, no test targets. When the
    # variable is undefined (a module configured standalone) tests stay on.
    if(DEFINED CGLIB_BUILD_TESTING AND NOT CGLIB_BUILD_TESTING)
        set(PHANTOM_GTEST_FOUND FALSE CACHE INTERNAL "GoogleTest targets available" FORCE)
        return()
    endif()

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

# cglib_add_test(<target> SOURCES ... [LINK ...] [INCLUDES ...] [WORKING_DIRECTORY dir])
# One GoogleTest executable registered with CTest. No-op unless phantom_find_gtest() found
# GoogleTest (so CGLIB_BUILD_TESTING=OFF creates nothing). The repo root (REPO_ROOT) is on
# the include path when the caller has set it, matching the "CGLib/..." include convention.
function(cglib_add_test target)
    if(NOT PHANTOM_GTEST_FOUND)
        return()
    endif()
    cmake_parse_arguments(A "" "WORKING_DIRECTORY" "SOURCES;LINK;INCLUDES" ${ARGN})
    include(GoogleTest)
    add_executable(${target} ${A_SOURCES})
    if(REPO_ROOT)
        target_include_directories(${target} PRIVATE ${REPO_ROOT})
    endif()
    if(A_INCLUDES)
        target_include_directories(${target} PRIVATE ${A_INCLUDES})
    endif()
    target_link_libraries(${target} PRIVATE ${A_LINK} GTest::gtest GTest::gtest_main)
    target_compile_options(${target} PRIVATE ${PHANTOM_WARN_FLAGS})
    if(A_WORKING_DIRECTORY)
        gtest_discover_tests(${target} WORKING_DIRECTORY ${A_WORKING_DIRECTORY})
    else()
        gtest_discover_tests(${target})
    endif()
endfunction()

# Idempotent GoogleTest discovery for CGLib's standalone CMake build, shared by
# every *Test target (docs/standalone-repository-plan.md Phase 1/2).
#
# Call phantom_find_gtest() once per CMakeLists.txt that defines a *Test
# target, then check PHANTOM_GTEST_FOUND.
#
# Resolution order:
#   1. find_package(GTest CONFIG) / find_package(GTest MODULE) -- a system or
#      package-manager GoogleTest (e.g. libgtest-dev on Linux, vcpkg on Windows).
#   2. FetchContent of a pinned GoogleTest tag -- only when CGLIB_FETCH_GTEST is
#      ON (the default when tests are enabled) and step 1 found nothing. This
#      needs network access on the first configure; set -DCGLIB_FETCH_GTEST=OFF
#      for a strictly offline / distribution build and provide GTest yourself.
#
# Unlike the private-superproject build this was split from, there is no
# reference to a NuGet packages/ directory one level above the repo.

set(CGLIB_GTEST_GIT_TAG "v1.15.2" CACHE STRING "GoogleTest tag used by the FetchContent fallback")
option(CGLIB_FETCH_GTEST "Fetch a pinned GoogleTest if no system/package GTest is found" ON)

function(phantom_find_gtest)
    # Gate on target existence, not the PHANTOM_GTEST_FOUND cache value: IMPORTED
    # targets are recreated every configure pass, so a re-configure of an
    # already-configured build dir must be able to re-run find_package().
    if(TARGET GTest::gtest OR TARGET GTest::gtest_main)
        return()
    endif()

    find_package(GTest CONFIG QUIET)
    if(NOT GTest_FOUND)
        find_package(GTest MODULE QUIET)
    endif()

    if(GTest_FOUND)
        # find_package creates GTest::gtest[_main] scoped to the calling
        # directory; promote to global so *Test targets in sibling
        # add_subdirectory() trees can link them.
        foreach(_t GTest::gtest GTest::gtest_main)
            if(TARGET ${_t})
                get_target_property(_imported ${_t} IMPORTED)
                if(_imported)
                    set_target_properties(${_t} PROPERTIES IMPORTED_GLOBAL TRUE)
                endif()
            endif()
        endforeach()
        message(STATUS "CGLib: using system/package GoogleTest")
        set(PHANTOM_GTEST_FOUND TRUE CACHE INTERNAL "Whether GTest::gtest/GTest::gtest_main are available")
        return()
    endif()

    if(NOT CGLIB_FETCH_GTEST)
        message(WARNING "CGLib: no GoogleTest found and CGLIB_FETCH_GTEST=OFF -- *Test targets are skipped.")
        set(PHANTOM_GTEST_FOUND FALSE CACHE INTERNAL "Whether GTest::gtest/GTest::gtest_main are available")
        return()
    endif()

    message(STATUS "CGLib: fetching GoogleTest ${CGLIB_GTEST_GIT_TAG} (no system GTest found; -DCGLIB_FETCH_GTEST=OFF to disable)")
    include(FetchContent)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG ${CGLIB_GTEST_GIT_TAG}
        GIT_SHALLOW TRUE
    )
    # Match the consuming code's CRT and keep GoogleTest out of `cmake --install`.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Upstream defines gtest/gtest_main; add the namespaced aliases the rest of
    # the build links against if they aren't already present.
    if(TARGET gtest AND NOT TARGET GTest::gtest)
        add_library(GTest::gtest ALIAS gtest)
    endif()
    if(TARGET gtest_main AND NOT TARGET GTest::gtest_main)
        add_library(GTest::gtest_main ALIAS gtest_main)
    endif()

    if(TARGET GTest::gtest OR TARGET gtest)
        set(PHANTOM_GTEST_FOUND TRUE CACHE INTERNAL "Whether GTest::gtest/GTest::gtest_main are available")
    else()
        message(WARNING "CGLib: GoogleTest FetchContent did not produce the expected targets -- *Test targets are skipped.")
        set(PHANTOM_GTEST_FOUND FALSE CACHE INTERNAL "Whether GTest::gtest/GTest::gtest_main are available")
    endif()
endfunction()

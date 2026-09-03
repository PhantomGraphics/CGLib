# CGLibCommon.cmake -- shared setup for CGLib's standalone CMake build.
#
# Included by the top-level CGLib/CMakeLists.txt and by every module
# CMakeLists.txt (Math has none; Space/File/Scene/Numerics/Animation/Volume/
# Renderer/GltfRenderer/Gizmo/Input/Particles/PostProcess each do, and each is
# usable standalone as `cmake -S CGLib/<Module> -B ...`). This file has no
# dependency on anything outside the CGLib directory -- that self-containment
# is the whole point of docs/standalone-repository-plan.md Phase 1.
#
# Callers must set CGLIB_ROOT (absolute path to the CGLib source root) before
# include(CGLibCommon). This file then publishes:
#
#   PHANTOM_WARN_FLAGS / CGLIB_WARN_FLAGS -- /W3 (MSVC) or -Wall (else)
#   CGLIB_HEADER_ROOT                     -- an include dir under which "CGLib/"
#                                            resolves to the source root, so the
#                                            repo-wide `#include "CGLib/..."`
#                                            convention keeps working without
#                                            adding the *parent* of the repo to
#                                            the include path (which a standalone
#                                            checkout does not have).
#   REPO_ROOT                             -- compat alias for CGLIB_HEADER_ROOT;
#                                            the shared *Core builders and the
#                                            per-module test targets still spell
#                                            their include root `${REPO_ROOT}`.

if(NOT DEFINED CGLIB_ROOT)
    message(FATAL_ERROR "CGLibCommon: CGLIB_ROOT must be set before include(CGLibCommon)")
endif()
get_filename_component(CGLIB_ROOT "${CGLIB_ROOT}" ABSOLUTE)

# --- Warning flags + MSVC /utf-8 (several sources carry Japanese comments that
# are invalid in the legacy MSVC source code page -- required, not cosmetic).
# add_compile_options() is directory-scoped, so this must run in every
# directory that include()s this file, not once behind a guard.
if(MSVC)
    set(PHANTOM_WARN_FLAGS /W3)
    add_compile_options(/utf-8)
else()
    set(PHANTOM_WARN_FLAGS -Wall)
endif()
set(CGLIB_WARN_FLAGS ${PHANTOM_WARN_FLAGS})

# --- Header-prefix shim: expose "<dir>/CGLib/..." so that the repo-wide
# `#include "CGLib/Math/Vector3d.h"` convention resolves with only CGLib's own
# directory in play (a standalone checkout has no parent directory to put on
# the include path).
#
# This is done with *generated forwarding headers*, not a symlink/junction: a
# junction introduces a second path prefix for the same physical file, and
# some vendored headers (GLM in particular) are then #include'd through both
# prefixes and their `#pragma once` fails to dedupe -> redefinition errors.
# A forwarding header instead redirects to the single canonical absolute path,
# so every translation unit sees one identity per file.
#
# The top-level ThirdParty/ tree is skipped: nothing includes it via a
# "CGLib/..." path (its libraries are reached through their own include dirs),
# and its headers use relative includes internally that must stay on the real
# path. File/ThirdParty/cgltf and the like are kept -- they *are* referenced
# as "CGLib/File/ThirdParty/...".
set(CGLIB_HEADER_ROOT "${CMAKE_BINARY_DIR}/_cglib_headers" CACHE INTERNAL "Include root under which CGLib/ resolves to the source root")

# Regenerate once per configure run (a GLOBAL property resets each run, unlike
# a cache entry), not once per include() from every module subdirectory.
get_property(_cglib_shim_done GLOBAL PROPERTY CGLIB_HEADER_SHIM_DONE)
if(NOT _cglib_shim_done)
    file(GLOB_RECURSE _cglib_hdrs RELATIVE "${CGLIB_ROOT}"
        "${CGLIB_ROOT}/*.h" "${CGLIB_ROOT}/*.hpp" "${CGLIB_ROOT}/*.inl" "${CGLIB_ROOT}/*.hxx")
    foreach(_h IN LISTS _cglib_hdrs)
        if(_h MATCHES "^(build|out|\\.git|_cglib_headers|ThirdParty)/")
            continue()
        endif()
        set(_fwd "${CGLIB_HEADER_ROOT}/CGLib/${_h}")
        set(_want "#include \"${CGLIB_ROOT}/${_h}\"\n")
        set(_have "")
        if(EXISTS "${_fwd}")
            file(READ "${_fwd}" _have)
        endif()
        if(NOT _have STREQUAL _want)
            file(WRITE "${_fwd}" "${_want}")
        endif()
    endforeach()
    set_property(GLOBAL PROPERTY CGLIB_HEADER_SHIM_DONE TRUE)
endif()

set(REPO_ROOT "${CGLIB_HEADER_ROOT}")

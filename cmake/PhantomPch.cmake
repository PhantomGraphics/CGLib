# Precompiled headers for the legacy per-directory pch.h files.
#
# The Visual Studio projects precompiled each directory's pch.h (/Yu). When the modules
# moved to CMake that was dropped, so every translation unit re-parsed gtest, glm,
# vulkan.h, ImGui, Eigen and the C++20 STL. phantom_use_pch() brings it back without
# changing what any file sees:
#
#   * a source gets the PCH only if its first preprocessor directive is
#     `#include "pch.h"` and it lives in the same directory as <header> (or in an ALSO_FROM
#     directory that has no pch.h of its own), i.e. it already includes exactly that
#     header first (its own #include then becomes a no-op through
#     #pragma once / the include guard);
#   * every other source of the target is marked SKIP_PRECOMPILE_HEADERS. Some pch.h files
#     #define configuration macros (GLM_FORCE_DEPTH_ZERO_TO_ONE, ...), so force-including
#     one into a file that never included it could silently change that file's meaning.
#     SKIP_PRECOMPILE_HEADERS is a per-directory source property, so a file compiled into
#     two targets of one directory (PhysicsView/*.cpp also built into PhysicsTest) and
#     skipped by one of them is skipped in both. That is safe, just not precompiled.
#
# phantom_use_pch_headers(<target> <header>...) is for targets without a pch.h. It
# precompiles a list of headers that do not depend on configuration macros (the STL,
# gtest) and applies them to every C++ source of the target.
#
# Set PHANTOM_ENABLE_PCH=OFF to build without any precompiled headers (e.g. to check
# that every file still includes what it uses).

# Header lists for phantom_use_pch_headers(). Deliberately no glm, vulkan.h or ImGui:
# dozens of headers here #define GLM_FORCE_* before including glm, so precompiling glm
# ahead of them would silently change their configuration.
# They are plain variables, so they are set above the include guard: every directory
# that includes this file needs its own copy in scope.
set(PHANTOM_PCH_STL
    <algorithm> <array> <cmath> <cstdint> <cstring> <functional> <map> <memory>
    <optional> <string> <string_view> <unordered_map> <utility> <vector>)
set(PHANTOM_PCH_GTEST <gtest/gtest.h> ${PHANTOM_PCH_STL})

include_guard(GLOBAL)

option(PHANTOM_ENABLE_PCH "Use precompiled headers (legacy pch.h files and common headers)" ON)

function(_phantom_pch_first_directive_is_pch source out_var)
    set(${out_var} FALSE PARENT_SCOPE)
    if(NOT EXISTS "${source}")
        return()
    endif()
    file(STRINGS "${source}" _directives REGEX "^[ \t]*#" LIMIT_COUNT 1)
    if(_directives MATCHES "^[ \t]*#[ \t]*include[ \t]*\"pch\\.h\"")
        set(${out_var} TRUE PARENT_SCOPE)
    endif()
endfunction()

# phantom_use_pch(<target> <header> [ALSO_FROM <dir>...])
#   ALSO_FROM lists further source directories with no pch.h of their own, whose
#   `#include "pch.h"` reaches <header> through the target's include path.
function(phantom_use_pch target header)
    if(NOT PHANTOM_ENABLE_PCH OR NOT TARGET ${target})
        return()
    endif()
    cmake_parse_arguments(A "" "" "ALSO_FROM" ${ARGN})
    get_filename_component(_header "${header}" ABSOLUTE)
    get_filename_component(_header_dir "${_header}" DIRECTORY)
    set(_pch_dirs "${_header_dir}")
    foreach(_d IN LISTS A_ALSO_FROM)
        get_filename_component(_d "${_d}" ABSOLUTE)
        if(EXISTS "${_d}/pch.h")
            message(FATAL_ERROR "phantom_use_pch(${target}): ${_d} has its own pch.h")
        endif()
        list(APPEND _pch_dirs "${_d}")
    endforeach()
    get_target_property(_target_src_dir ${target} SOURCE_DIR)
    get_target_property(_sources ${target} SOURCES)

    set(_used 0)
    foreach(_src IN LISTS _sources)
        if(NOT _src MATCHES "\\.(cpp|cc|cxx)$")
            continue()
        endif()
        get_filename_component(_abs "${_src}" ABSOLUTE BASE_DIR "${_target_src_dir}")
        get_filename_component(_dir "${_abs}" DIRECTORY)
        set(_use FALSE)
        if(_dir IN_LIST _pch_dirs)
            _phantom_pch_first_directive_is_pch("${_abs}" _use)
        endif()
        if(_use)
            math(EXPR _used "${_used} + 1")
        else()
            # Source-file properties are directory-scoped: set them from the directory
            # that created the target so they apply to it.
            set_source_files_properties("${_abs}" TARGET_DIRECTORY ${target}
                PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
        endif()
    endforeach()

    if(_used GREATER 0)
        _phantom_pch_cxx_only(_entries "${_header}")
        target_precompile_headers(${target} PRIVATE ${_entries})
    endif()
endfunction()

function(phantom_use_pch_headers target)
    if(NOT PHANTOM_ENABLE_PCH OR NOT TARGET ${target})
        return()
    endif()
    _phantom_pch_cxx_only(_entries ${ARGN})
    target_precompile_headers(${target} PRIVATE ${_entries})
endfunction()

# Wrap each header in $<$<COMPILE_LANGUAGE:CXX>:...> so a C source in the target is never
# handed a C++ PCH. `<vector>` has to be spelled `<vector$<ANGLE-R>` inside the genex.
function(_phantom_pch_cxx_only out_var)
    set(_out)
    foreach(_h IN LISTS ARGN)
        if(_h MATCHES "^<(.*)>$")
            set(_h "<${CMAKE_MATCH_1}$<ANGLE-R>")
        endif()
        list(APPEND _out "$<$<COMPILE_LANGUAGE:CXX>:${_h}>")
    endforeach()
    set(${out_var} "${_out}" PARENT_SCOPE)
endfunction()

# CGLibInstall.cmake -- install rules, exported targets and package config for the
# CPU-only components (docs/standalone-repository-plan.md, PLAN_cglib_refactoring Phase 2).
#
# Included from the top-level CMakeLists.txt after every target is defined. Enabled by
# CGLIB_INSTALL (default ON only when CGLib is the top-level project, so Phantom's
# add_subdirectory(CGLib) is unaffected).
#
# Layout of an install prefix:
#   include/CGLib/<same relative paths as the source tree>   (headers only)
#   lib/<Component>Core.lib|.a
#   lib/cmake/CGLib/CGLibConfig.cmake, CGLibConfigVersion.cmake, CGLibTargets*.cmake
#
# Public headers keep the source tree's relative layout because they reach vendored
# headers by relative path (Math/glm.h -> ../ThirdParty/glm-0.9.9.8/glm/glm.hpp,
# Numerics/Converter.h -> ../ThirdParty/eigen-3.4.0/Eigen/Eigen). The install therefore
# bundles glm, nlohmann/json, Eigen and cgltf headers under include/CGLib/. CGLib's own
# LICENSE is installed under share/licenses/CGLib; the vendored libraries keep their
# license files next to their headers where upstream ships them.
#
# Targets are exported as CGLib::<Component> (Math, Graphics, ...) via EXPORT_NAME, the same
# names as the build-tree aliases. Vulkan-dependent components are NOT exported yet.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# component name = target name
set(_cglib_install_components
    Math=MathCore Graphics=GraphicsCore Numerics=NumericsCore Space=SpaceCore
    Scene=SceneCore Volume=VolumeCore File=FileCore Animation=AnimationCore
    GeometryNode=GeometryNodeCore Asset=AssetCore SceneRuntime=SceneRuntimeCore)

set(_cglib_install_targets "")
set(_cglib_install_component_names "")
foreach(_pair IN LISTS _cglib_install_components)
    string(REPLACE "=" ";" _pair "${_pair}")
    list(GET _pair 0 _name)
    list(GET _pair 1 _target)
    if(NOT TARGET ${_target})
        message(FATAL_ERROR "CGLib install: expected CPU target ${_target} is missing")
    endif()
    set_target_properties(${_target} PROPERTIES EXPORT_NAME ${_name})
    list(APPEND _cglib_install_targets ${_target})
    list(APPEND _cglib_install_component_names ${_name})

    # Split each include directory into BUILD_INTERFACE (source/shim path) and
    # INSTALL_INTERFACE (path relative to the prefix). The shim root (REPO_ROOT) holds
    # generated forwarding headers with absolute source paths; it must never be exported.
    get_target_property(_incs ${_target} INTERFACE_INCLUDE_DIRECTORIES)
    set(_new "")
    foreach(_inc IN LISTS _incs)
        if(_inc MATCHES "^\\$<")
            list(APPEND _new "${_inc}")
        elseif(_inc STREQUAL "${REPO_ROOT}")
            list(APPEND _new "$<BUILD_INTERFACE:${_inc}>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
        elseif(_inc STREQUAL "${CGLIB_ROOT}")
            list(APPEND _new "$<BUILD_INTERFACE:${_inc}>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/CGLib>")
        else()
            file(RELATIVE_PATH _rel "${CGLIB_ROOT}" "${_inc}")
            if(_rel MATCHES "^\\.\\.")
                message(FATAL_ERROR "CGLib install: ${_target} exposes include dir outside the source tree: ${_inc}")
            endif()
            list(APPEND _new "$<BUILD_INTERFACE:${_inc}>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/CGLib/${_rel}>")
        endif()
    endforeach()
    set_property(TARGET ${_target} PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${_new}")
endforeach()

install(TARGETS ${_cglib_install_targets} EXPORT CGLibTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(EXPORT CGLibTargets
    NAMESPACE CGLib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CGLib)

# --- Headers -----------------------------------------------------------------
# Public API = headers of the exported components + the vendored headers they include.
# Tests, viewers, *.cpp and Vulkan-facing code are deliberately not installed.
set(_hdr_dest ${CMAKE_INSTALL_INCLUDEDIR}/CGLib)

foreach(_dir Math Graphics Util)
    install(DIRECTORY ${CGLIB_ROOT}/${_dir}/ DESTINATION ${_hdr_dest}/${_dir}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl" PATTERN "*.hxx"
        PATTERN "pch.h" EXCLUDE)
endforeach()
foreach(_dir Numerics Space Scene File Animation Volume GeometryNode AssetCore SceneRuntime)
    # Scene/Scene presenters pull Renderer (Vulkan) headers and are not in SceneCore.
    install(DIRECTORY ${CGLIB_ROOT}/${_dir}/${_dir}/ DESTINATION ${_hdr_dest}/${_dir}/${_dir}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl" PATTERN "*.hxx"
        PATTERN "pch.h" EXCLUDE
        PATTERN "*Presenter.h" EXCLUDE)
endforeach()
# IPresenter.h is part of SceneBase's interface and has no Vulkan dependency.
install(FILES ${CGLIB_ROOT}/Scene/Scene/IPresenter.h DESTINATION ${_hdr_dest}/Scene/Scene)

# Vendored header-only dependencies reached by relative or <angle> includes.
install(DIRECTORY ${CGLIB_ROOT}/ThirdParty/glm-0.9.9.8/glm DESTINATION ${_hdr_dest}/ThirdParty/glm-0.9.9.8)
install(DIRECTORY ${CGLIB_ROOT}/ThirdParty/nlohmann DESTINATION ${_hdr_dest}/ThirdParty)
install(DIRECTORY ${CGLIB_ROOT}/Numerics/ThirdParty/eigen-3.4.0/Eigen
        DESTINATION ${_hdr_dest}/Numerics/ThirdParty/eigen-3.4.0)
install(DIRECTORY ${CGLIB_ROOT}/File/ThirdParty/cgltf DESTINATION ${_hdr_dest}/File/ThirdParty
        FILES_MATCHING PATTERN "*.h")

install(FILES ${CGLIB_ROOT}/LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/CGLib)
# Public third-party dependencies ship their license texts (docs/third-party.md).
install(FILES ${CGLIB_ROOT}/ThirdParty/glm-0.9.9.8/copying.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/CGLib/glm)
file(GLOB _eigen_licenses ${CGLIB_ROOT}/Numerics/ThirdParty/eigen-3.4.0/COPYING.*)
install(FILES ${_eigen_licenses} DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/CGLib/eigen)
install(FILES ${CGLIB_ROOT}/docs/third-party.md DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/CGLib)

# --- Package config ----------------------------------------------------------
set(CGLIB_PACKAGE_COMPONENTS "${_cglib_install_component_names}")
set(CGLIB_PACKAGE_NEEDS_OPENMP OFF)
get_target_property(_space_libs SpaceCore INTERFACE_LINK_LIBRARIES)
if(_space_libs MATCHES "OpenMP")
    set(CGLIB_PACKAGE_NEEDS_OPENMP ON)
endif()

configure_package_config_file(
    ${CGLIB_ROOT}/cmake/CGLibConfig.cmake.in
    ${CMAKE_BINARY_DIR}/CGLibConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CGLib)
write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/CGLibConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_BINARY_DIR}/CGLibConfig.cmake ${CMAKE_BINARY_DIR}/CGLibConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CGLib)

# External-consumer check for the installed CGLib package (PLAN_cglib_refactoring Phase 2).
# Configured against an install prefix only -- it must not reach the CGLib source tree:
#   cmake -S tests/install_consumer -B <dir> -DCMAKE_PREFIX_PATH=<install prefix>
# tests/run_install_consumer.ps1 drives install -> move -> configure/build/run.
cmake_minimum_required(VERSION 3.20)
project(CGLibInstallConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
if(MSVC)
    add_compile_options(/utf-8 /EHsc)
endif()

find_package(CGLib CONFIG REQUIRED COMPONENTS Math Graphics Numerics Space Scene Volume File Animation GeometryNode Asset SceneRuntime)

add_executable(cglib_install_consumer main.cpp)
target_link_libraries(cglib_install_consumer PRIVATE
    CGLib::Math CGLib::Numerics CGLib::Space CGLib::Volume CGLib::File
    CGLib::Animation CGLib::GeometryNode CGLib::SceneRuntime CGLib::Scene)

# Self-containment of every installed public header: one TU per header that includes only
# that header (syntax-checked, never linked). Headers are discovered from the install
# prefix, so a header that depends on something the package does not ship fails here.
# Vendored ThirdParty trees are skipped.
get_filename_component(CGLib_INCLUDE_ROOT "${CGLib_DIR}/../../../include/CGLib" ABSOLUTE)
file(GLOB_RECURSE _cglib_pkg_headers RELATIVE "${CGLib_INCLUDE_ROOT}" CONFIGURE_DEPENDS
    "${CGLib_INCLUDE_ROOT}/*.h" "${CGLib_INCLUDE_ROOT}/*.hpp")
set(_hdr_tus "")
foreach(_h IN LISTS _cglib_pkg_headers)
    if(_h MATCHES "ThirdParty/")
        continue()
    endif()
    string(MAKE_C_IDENTIFIER "${_h}" _id)
    set(_tu "${CMAKE_CURRENT_BINARY_DIR}/header_check/${_id}.cpp")
    file(GENERATE OUTPUT "${_tu}" CONTENT "#include \"CGLib/${_h}\"\n")
    list(APPEND _hdr_tus "${_tu}")
endforeach()
add_library(cglib_header_check OBJECT ${_hdr_tus})
target_link_libraries(cglib_header_check PRIVATE
    CGLib::Math CGLib::Graphics CGLib::Numerics CGLib::Space CGLib::Volume CGLib::File
    CGLib::Animation CGLib::GeometryNode CGLib::SceneRuntime CGLib::Scene CGLib::Asset)

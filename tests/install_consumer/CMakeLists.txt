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

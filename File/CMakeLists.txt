# Cross-platform CMake build for the File core library (Crystal::File) and its
# GoogleTest suite (FileTest).
#
# This is a standalone CMake build, separate from the MSBuild/.vcxproj-based
# Windows build (File/File/File.vcxproj + File/FileTest/FileTest.vcxproj,
# driven by Phantom2026.sln). Part of
# docs/todo/PLAN_crossplatform_non_cgapp_build.md Phase 1 -- follows the same
# pattern as ../../Physics/CMakeLists.txt / ../../PointCloud/CMakeLists.txt.
# FileCore itself is now defined once in ../../cmake/CrystalCoreLibs.cmake
# (Phase 4 §5.4 item 1 dedup) -- this file just calls it and adds FileTest on
# top, so it also works standalone (`cmake -S CGLib/File -B ...`) exactly
# as before, and identically when add_subdirectory()'d from the root
# CMakeLists.txt.
#
# Usage (Linux, from WSL -- avoid /mnt/c/... for speed on a Linux-native checkout):
#   cmake -S CGLib/File -B CGLib/File/build_linux \
#       -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang \
#       -DCMAKE_BUILD_TYPE=Debug
#   cmake --build CGLib/File/build_linux -j
#   ctest --test-dir CGLib/File/build_linux --output-on-failure
#
# Prerequisite: libgtest-dev (provides libgtest.a/libgtest_main.a + the GTest
# CMake package config under /usr/lib/x86_64-linux-gnu/cmake/GTest).
#
# NOTE on PMXFileReader.cpp's portable UTF-16LE decoder and File's on-disk
# source list (File.cpp NOT part of FileCore): see
# cmake/CrystalCoreLibs.cmake's crystal_add_file_core().

cmake_minimum_required(VERSION 3.20)

project(FileCoreLinux LANGUAGES CXX)

# Matches File.vcxproj/FileTest.vcxproj: both explicitly override
# <LanguageStandard> to stdcpp20 (unlike Numerics/Scene/Volume, which fall
# back to BuildConfig.props' repo-wide C++17 default).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CGLIB_ROOT ${REPO_ROOT}/CGLib)
set(FILE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    add_compile_options(/utf-8)
    set(CRYSTAL_WARN_FLAGS /W3)
else()
    set(CRYSTAL_WARN_FLAGS -Wall)
endif()

list(APPEND CMAKE_MODULE_PATH ${REPO_ROOT}/cmake)
include(PhantomGTest)
include(PhantomCoreLibs)

crystal_add_file_core()

# --- FileTest (GoogleTest) ---

crystal_find_gtest()

if(CRYSTAL_GTEST_FOUND)
    enable_testing()

    add_executable(FileTest
        ${FILE_ROOT}/FileTest/BVHFileReaderTest.cpp
        ${FILE_ROOT}/FileTest/FileErrorHandlingTest.cpp
        ${FILE_ROOT}/FileTest/GLTFFileReaderWriterTest.cpp
        ${FILE_ROOT}/FileTest/MTLFileReaderTest.cpp
        ${FILE_ROOT}/FileTest/MTLFileWriterTest.cpp
        ${FILE_ROOT}/FileTest/OBJFileReaderTest.cpp
        ${FILE_ROOT}/FileTest/OBJFileWriterTest.cpp
        ${FILE_ROOT}/FileTest/PLYFileReaderTest.cpp
        ${FILE_ROOT}/FileTest/PLYFileWriterTest.cpp
        ${FILE_ROOT}/FileTest/PMDFileReaderTest.cpp
        ${FILE_ROOT}/FileTest/PMXFileReaderTest.cpp
        ${FILE_ROOT}/FileTest/STLASCIIFileReaderTest.cpp
        ${FILE_ROOT}/FileTest/STLASCIIFileWriterTest.cpp
        ${FILE_ROOT}/FileTest/STLBinaryFileReaderTest.cpp
        ${FILE_ROOT}/FileTest/STLBinaryFileWriterTest.cpp
        ${FILE_ROOT}/FileTest/VMDFileReaderTest.cpp
    )
    target_include_directories(FileTest PRIVATE
        ${REPO_ROOT}
        ${FILE_ROOT}/FileTest
    )
    target_link_libraries(FileTest PRIVATE
        FileCore
        GTest::gtest
        GTest::gtest_main
    )
    target_compile_options(FileTest PRIVATE ${CRYSTAL_WARN_FLAGS})

    # A number of *ReaderTest/*WriterTest cases read/write fixtures relative to
    # the process' current working directory (File/FileTest/TestFiles/...),
    # same assumption the .vcxproj build makes (its working directory is the
    # project dir). Mirror that for ctest/`cmake --build` runs from the build
    # dir by copying TestFiles next to the built binary.
    add_custom_command(TARGET FileTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${FILE_ROOT}/FileTest/TestFiles
            $<TARGET_FILE_DIR:FileTest>/TestFiles
    )

    include(GoogleTest)
    gtest_discover_tests(FileTest WORKING_DIRECTORY $<TARGET_FILE_DIR:FileTest>)
else()
    message(WARNING "GTest not found -- skipping FileTest target")
endif()

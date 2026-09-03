# File core library (Phantom::File -- OBJ/MTL/PLY/STL/BVH/PMD/PMX/VMD/glTF readers and writers) and its GoogleTest suite.
#
# Part of CGLib's standalone CMake build. Usable on its own
#   cmake -S CGLib/File -B build/File -DCMAKE_BUILD_TYPE=Debug && cmake --build build/File
# or configured together with every other module from the top-level CGLib/CMakeLists.txt.
# The shared *Core builders live in cmake/PhantomCoreLibs.cmake / cmake/PhantomVulkanApp.cmake;
# this file adds the module's test / app targets on top. See docs/standalone-repository-plan.md.

cmake_minimum_required(VERSION 3.20)

project(FileCoreLinux LANGUAGES CXX)

# Matches File.vcxproj/FileTest.vcxproj: both explicitly override
# <LanguageStandard> to stdcpp20 (unlike Numerics/Scene/Volume, which fall
# back to BuildConfig.props' repo-wide C++17 default).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FILE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
if(NOT DEFINED CGLIB_ROOT)
    set(CGLIB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()
list(APPEND CMAKE_MODULE_PATH ${CGLIB_ROOT}/cmake)
include(CGLibCommon)
include(PhantomGTest)
include(PhantomCoreLibs)

phantom_add_file_core()

# --- FileTest (GoogleTest) ---

phantom_find_gtest()

if(PHANTOM_GTEST_FOUND)
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
    target_compile_options(FileTest PRIVATE ${PHANTOM_WARN_FLAGS})

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

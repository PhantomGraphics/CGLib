# Small in-tree consumers, built when -DCGLIB_BUILD_EXAMPLES=ON.
#
# These link the namespaced CGLib::<Component> targets exactly as an external
# project would after find_package(CGLib) -- keeping the build-tree and (a
# future) install-tree consumer paths honest.

add_executable(cglib_example_math hello_math/main.cpp)
target_link_libraries(cglib_example_math PRIVATE CGLib::Math)

add_executable(cglib_example_file hello_file/main.cpp)
target_link_libraries(cglib_example_file PRIVATE CGLib::File)

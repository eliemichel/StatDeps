cmake_minimum_required(VERSION 3.0.0...3.24.0 FATAL_ERROR)
project(statdeps VERSION 0.1.0 LANGUAGES CXX C)

# Define the statdeps library
# This is a header-only library that you may simply copy into your source tree.
add_library(statdeps INTERFACE)
target_include_directories(statdeps INTERFACE include)

# Build example only if the current directory is the toplevel CMake project.
get_directory_property(hasParent PARENT_DIRECTORY)
if (NOT hasParent)
	add_subdirectory(example)
endif ()

cmake_minimum_required(VERSION 3.13)
project(DeepTypeLib)

# LLVM setup (adjust path if needed)
set(LLVM_DIR "/usr/lib/llvm-15/lib/cmake/llvm")
find_package(LLVM REQUIRED CONFIG)

# Zlib (required by some LLVM components like Support)
find_package(ZLIB REQUIRED)

# Include and link paths
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${ZLIB_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Source files for the DeepType library
set(SOURCES
  Analyzer.cc
  CallGraph.cc
  Common.cc
)

# Select minimal required LLVM components
llvm_map_components_to_libnames(LLVM_LIBS
  core
  support
  analysis
)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build DeepType as a static library (.a)
add_library(deeptype STATIC ${SOURCES})

# Link against required LLVM and Zlib libs
target_link_libraries(deeptype ${LLVM_LIBS} ${ZLIB_LIBRARIES})

# Public headers for external users
target_include_directories(deeptype PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Define macro to disable cl::opt if needed
target_compile_definitions(deeptype PRIVATE BUILDING_DEEPTYPE_LIB)


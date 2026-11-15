cmake_minimum_required(VERSION 3.26)

project(sokoban)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# libtorch location: first existing candidate wins unless TORCH_PATH is given
if (NOT TORCH_PATH)
  foreach(cand "$ENV{HOME}/.local/libtorch" "$ENV{HOME}/alphago_zero/libtorch")
    if (EXISTS "${cand}/share/cmake/Torch/TorchConfig.cmake")
      set(TORCH_PATH "${cand}")
      break()
    endif()
  endforeach()
endif()
set(TORCH_PATH "${TORCH_PATH}" CACHE PATH "Path to libtorch installation")

# CUDA builds of libtorch need nvcc discoverable at configure time
if (NOT CMAKE_CUDA_COMPILER)
  set(CUDA_HOME "$ENV{CUDA_HOME}")
  if (NOT CUDA_HOME)
    set(CUDA_HOME "/usr/local/cuda")
  endif()
  if (EXISTS "${CUDA_HOME}/bin/nvcc")
    set(CMAKE_CUDA_COMPILER "${CUDA_HOME}/bin/nvcc")
  endif()
endif()

find_package(Torch REQUIRED PATHS ${TORCH_PATH} NO_DEFAULT_PATH)

# set after find_package so libtorch's CUDA probe (try_run) isn't forced to C++26
# GCC < 14 has no c++26 dialect flag known to CMake; fall back to C++23
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
    CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
  set(SOKOBAN_CXX_STANDARD 23)
else()
  set(SOKOBAN_CXX_STANDARD 26)
endif()
set(CMAKE_CXX_STANDARD ${SOKOBAN_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)

set(FTXUI_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(FTXUI_BUILD_DOCS     OFF CACHE INTERNAL "")
set(FTXUI_BUILD_TESTS    OFF CACHE INTERNAL "")
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG        v7.0.0
)
FetchContent_MakeAvailable(ftxui)

FetchContent_Declare(cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11
  GIT_TAG        v2.4.2
)
FetchContent_MakeAvailable(cli11)

add_subdirectory(src)

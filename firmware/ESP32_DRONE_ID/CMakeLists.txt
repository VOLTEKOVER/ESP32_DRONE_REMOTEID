# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

# Set project name
set(PROJECT_NAME esp32-remote-id-scanner)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(${PROJECT_NAME})

# Enable C++ support
enable_language(CXX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti")

# Additional compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-but-set-variable")


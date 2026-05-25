cmake_minimum_required(VERSION 3.12.0)

# Set Project Name and Version
project(Can-Simulator VERSION 0.0)

# Specify C++ standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure Output Directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include sub directories
add_subdirectory(main)
add_subdirectory(can-setup)
add_subdirectory(can-module)
add_subdirectory(db-reader)


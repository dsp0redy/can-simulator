cmake_minimum_required(VERSION 3.12.0)

# Set Project Name and Version
project(Can-Simulator VERSION 0.0)

# Specify C++ standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure Output Directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include sub directories
<<<<<<< HEAD:app/CMakeLists.txt
add_subdirectory(db-reader)
add_subdirectory(can-module)
add_subdirectory(process-data)
=======
add_subdirectory(can-setup)
add_subdirectory(can-module)
add_subdirectory(db-reader)
>>>>>>> a6238003c7cd0cd38eaa86f282947aeef3878ad0:CMakeLists.txt
add_subdirectory(main)


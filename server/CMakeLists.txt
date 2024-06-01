cmake_minimum_required(VERSION 3.23)

project(server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find Boost libraries
# set(Boost_USE_STATIC_LIBS OFF)
# set(Boost_USE_MULTITHREADED ON)
# set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.80.0 REQUIRED)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    add_definitions(-DBOOST_ALL_NO_LIB)
else()
    message(FATAL_ERROR "Could not find Boost libraries")
endif()

add_executable(server src/main.cpp src/server.cpp)
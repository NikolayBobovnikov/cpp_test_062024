cmake_minimum_required(VERSION 3.23)

project(server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(BOOST_ROOT "C:/nx/3rdparty/boost_1_85_0")

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

include(FetchContent)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
)
FetchContent_GetProperties(yaml-cpp)

if(NOT yaml-cpp_POPULATED)
    message(STATUS "Fetching yaml-cpp...")
    FetchContent_Populate(yaml-cpp)
    add_subdirectory(${yaml-cpp_SOURCE_DIR} ${yaml-cpp_BINARY_DIR})
endif()

FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG master
)

FetchContent_MakeAvailable(rapidjson)

if(rapidjson_SOURCE_DIR)
    message(STATUS "RapidJSON source directory: ${rapidjson_SOURCE_DIR}")
else()
    message(FATAL_ERROR "RapidJSON source directory not found!")
endif()

add_executable(server src/main.cpp src/server.cpp src/session.cpp)
target_include_directories(server PRIVATE ${rapidjson_SOURCE_DIR}/include)
target_link_libraries(server PUBLIC yaml-cpp::yaml-cpp) # The library or executable that require yaml-cpp library

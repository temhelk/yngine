cmake_minimum_required(VERSION 3.30)

project(
    Yngine
    VERSION 0.0.0
    LANGUAGES CXX C
)

add_subdirectory(yngine)

if(CMAKE_PROJECT_NAME STREQUAL Yngine)
    include(CTest)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Dependencies
add_library(Xoshiro-cpp INTERFACE)
target_include_directories(Xoshiro-cpp INTERFACE extern/Xoshiro-cpp)

target_link_libraries(Yngine PUBLIC Xoshiro-cpp)


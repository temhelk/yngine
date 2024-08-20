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

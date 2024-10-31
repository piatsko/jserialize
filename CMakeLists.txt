cmake_minimum_required(VERSION 3.11)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(-Wall -Wextra -pedantic -pedantic-errors -DNTEST)

project(serialize 
    VERSION 0.1
    DESCRIPTION "json serialization of aggregate types"
    LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
  qlibs.reflect
  GIT_REPOSITORY https://github.com/qlibs/reflect
  GIT_TAG v1.2.4
)

FetchContent_MakeAvailable(qlibs.reflect)

add_executable (${PROJECT_NAME} main.cpp serialize.h)
target_include_directories(${PROJECT_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${qlibs.reflect_SOURCE_DIR}
)


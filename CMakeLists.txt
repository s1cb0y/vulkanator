
project(VKnatorEngine LANGUAGES CXX)
cmake_minimum_required(VERSION 3.22)


set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build")
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.cpp")

add_executable(vknator ${SOURCES})


#include_directories(inc ${SDL2_INCLUDE_DIRS})
target_include_directories(vknator PUBLIC inc ${SDL2_INCLUDE_DIRS})

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)
target_link_libraries(vknator ${VULKAN_LIBRARIES} ${SDL2_LIBRARIES} )

cmake_minimum_required(VERSION 3.22)
 # Fix behavior of CMAKE_CXX_STANDARD when targeting macOS.
if (POLICY CMP0025)
 cmake_policy(SET CMP0025 NEW)
endif ()

project(VKnatorEngine LANGUAGES CXX)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.cpp")

add_executable(vknator ${SOURCES})
set_property(TARGET vknator PROPERTY CXX_STANDARD 20)

add_subdirectory(vendor)
target_include_directories(vknator PUBLIC inc)
target_link_libraries(vknator PRIVATE ${VULKAN_LIBRARIES} sdl2 spdlog vma vkbootstrap)
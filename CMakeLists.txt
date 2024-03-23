
cmake_minimum_required(VERSION 3.22)
 # Fix behavior of CMAKE_CXX_STANDARD when targeting macOS.
if (POLICY CMP0025)
 cmake_policy(SET CMP0025 NEW)
endif ()

project(VKnatorEngine LANGUAGES CXX)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build")
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.cpp")

add_executable(vknator ${SOURCES})
set_property(TARGET vknator PROPERTY CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD 20)
add_subdirectory(vendor)
#find_package(Vulkan REQUIRED)
#find_package(SDL2 REQUIRED)

#target_include_directories(vknator PUBLIC inc ${SDL2_INCLUDE_DIRS} vendor/spdlog/include)
#target_link_libraries(vknator PRIVATE ${VULKAN_LIBRARIES} ${SDL2_LIBRARIES} vkbootstrap)
target_include_directories(vknator PUBLIC inc vendor/spdlog/include)
target_link_libraries(vknator vkbootstrap)
target_link_libraries(vknator ${VULKAN_LIBRARIES} sdl2)
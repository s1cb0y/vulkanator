
project(VKnatorEngine LANGUAGES CXX)
cmake_minimum_required(VERSION 3.22)


set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build")
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.cpp")

add_executable(vknator ${SOURCES})
set_property(TARGET vknator PROPERTY CXX_STANDARD 20)

#find_package(Vulkan REQUIRED)
add_subdirectory(vendor)
#find_package(SDL2 REQUIRED)

#target_include_directories(vknator PUBLIC inc ${SDL2_INCLUDE_DIRS} vendor/spdlog/include)
#target_link_libraries(vknator PRIVATE ${VULKAN_LIBRARIES} ${SDL2_LIBRARIES} vkbootstrap)
target_include_directories(vknator PUBLIC inc vendor/spdlog/include)
target_link_libraries(vknator vkbootstrap)
target_link_libraries(vknator ${VULKAN_LIBRARIES} sdl2)
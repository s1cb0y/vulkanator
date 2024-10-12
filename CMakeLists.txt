
cmake_minimum_required(VERSION 3.22)
 # Fix behavior of CMAKE_CXX_STANDARD when targeting macOS.
if (POLICY CMP0025)
 cmake_policy(SET CMP0025 NEW)
endif ()

project(VKnatorEngine LANGUAGES CXX)

#compile shaders
find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/ REQUIRED)
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
    )

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.cpp")

add_executable(vknator ${SOURCES} ${SHADERS})
add_dependencies(vknator Shaders)
set_property(TARGET vknator PROPERTY CXX_STANDARD 20)

add_subdirectory(vendor)
target_include_directories(vknator PUBLIC inc)
target_link_libraries(vknator PRIVATE ${VULKAN_LIBRARIES} SDL2::SDL2 spdlog vma glm vkbootstrap imgui fastgltf::fastgltf stb_image)


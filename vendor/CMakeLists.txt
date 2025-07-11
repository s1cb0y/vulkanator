find_package(Vulkan REQUIRED)

#find_package(SDL2 REQUIRED) # should be used if SDL2 has been installed via paket manager or installer
# -------------------------- VK Bootstrap ----------------------
add_library(vkbootstrap STATIC)
add_library(spdlog INTERFACE)

target_sources(vkbootstrap PRIVATE
vkbootstrap/VkBootstrap.h
vkbootstrap/VkBootstrap.cpp
)

target_include_directories(vkbootstrap PUBLIC vkbootstrap)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)
set_property(TARGET vkbootstrap PROPERTY CXX_STANDARD 20)

# ------------------------------ SPD Log ----------------------
#add_library(spdlog::spdlog ALIAS spdlog)
target_include_directories(spdlog INTERFACE spdlog/include)

# ------------------------------ Memory allocator ----------------------
#both vma and glm and header only libs so we only need the include path
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE vma)
# ------------------------------ GLM ----------------------
add_library(glm INTERFACE)
target_compile_definitions(glm INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(glm INTERFACE glm)
# ------------------------------ FastGltf ----------------------
add_subdirectory(fastgltf)
# ------------------------------ STB Image ----------------------
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE stb_image)
# target_sources(tinyobjloader PRIVATE
#   tinyobjloader/tiny_obj_loader.h
#   tinyobjloader/tiny_obj_loader.cc
#   )

# target_include_directories(tinyobjloader PUBLIC tinyobjloader)


# -------------------------------SDL2 ---------------------------------

# following lines should be used if SDL2 has been installed via paket manager or installer
# add_library(sdl2 INTERFACE)
# target_include_directories(sdl2 INTERFACE ${SDL2_INCLUDE_DIRS})
# target_link_libraries(sdl2 INTERFACE ${SDL2_LIBRARIES})

add_subdirectory(SDL EXCLUDE_FROM_ALL)



# ------------------------------ ImGui ---------------------------------
add_library(imgui STATIC)

target_include_directories(imgui PUBLIC imgui)

target_sources(imgui PRIVATE
    imgui/imgui.h
    imgui/imgui.cpp

    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_impl_vulkan.cpp
    imgui/imgui_impl_sdl2.cpp
  )

target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL2::SDL2)

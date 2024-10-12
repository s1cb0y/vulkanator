#include "stb_image.h"
#include <iostream>
#include <vknator_loader.h>

#include "vknator_engine.h"
#include "vknator_initializers.h"
#include "vknator_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VknatorEngine* engine, std::filesystem::path filePath){
    LOG_DEBUG("Loading GLTF: {}", filePath);
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filePath);
    constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
    fastgltf::Asset gltf;
    fastgltf::Parser parser{};

    auto load = parser.loadBinaryGLTF(&data, filePath.parent_path(), gltfOptions);
    if (load){
        gltf = std::move(load.get());

    } else {
        LOG_ERROR("Failed to load flTF: {}", fastgltf::to_underlying(load.error()));
    }

    std::vector<std::shared_ptr<MeshAsset>> meshes;

    //use the same vectors for all meshes so that the memory doesnt rellocate as often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    for (fastgltf::Mesh& mesh : gltf.meshes){
        MeshAsset newMesh;
        newMesh.name = mesh.name;

        //clear the mesh arrays for each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives){
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t) indices.size();
            newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            std::size_t initial_vtx = vertices.size();

            //load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx){
                        indices.push_back(initial_vtx + idx);
                });
            }
            //load vertex positions
            {
                fastgltf::Accessor& posaccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(indices.size() + posaccessor.count);
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posaccessor,
                    [&](glm::vec3 v, std::size_t index){
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = {1,0,0};
                        newvtx.color = glm::vec4{1.0f};
                        newvtx.uv_x = 0;
                        newvtx.uv_y = 0;
                        vertices[initial_vtx + index] = newvtx;
                });

            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                    [&](glm::vec3 v, std::size_t index) {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                    [&](glm::vec2 v, std::size_t index) {
                        vertices[initial_vtx + index].uv_x = v.x;
                        vertices[initial_vtx + index].uv_y = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, std::size_t index) {
                        vertices[initial_vtx + index].color = v;
                    });
            }
            newMesh.surfaces.push_back(newSurface);
        }

        // display the vertex normals
        constexpr bool OverrideColors = true;
        if (OverrideColors){
            for (Vertex& v : vertices){
                v.color = glm::vec4{v.normal, 1.0f};
            }
        }
        newMesh.meshBuffers = engine->UploadMesh(indices, vertices);
        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));

    }

    return meshes;
}
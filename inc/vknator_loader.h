#pragma once

#include <vknator_types.h>
#include <unordered_map>
#include <filesystem>

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

//forward declaration
class VknatorEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VknatorEngine* engine, std::filesystem::path filePath);

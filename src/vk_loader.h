#pragma once

#include <vk_types.h>
#include <unordered_map>
#include <filesystem>

struct GLTFMaterial 
{
	MaterialInstance data;
};

//a portion of a mesh buffer that represents a surface
struct GeoSurface 
{
    uint32_t startIndex; //starting index in index buffer
    uint32_t count;      
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset 
{
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

//forward declaration
class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine, std::filesystem::path filePath);
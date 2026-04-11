#include <vk_loader.h>

#include "stb_image.h"
#include "mikktspace.h"
#include <iostream>


#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_resources.h"
#include "vk_upload.h"
#include "vk_types.h"
#include "cvars.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <limits>

static AutoCVar_Float cvarModelScale(
    "r.modelScale",
    "Internal render scale",
    1.0,
    FloatCVarOptions{
        .minValue = 0.1,
        .maxValue = 50.0,
        .step = 0.01f,
        .format = "%.2f",
    },
    CVarEditHint::Drag);

static void accumulate_scene_aabb(
    const std::shared_ptr<Node>& node,
    glm::vec3& sceneMin,
    glm::vec3& sceneMax,
    bool& anyGeometry)
{
    if (auto meshNode = std::dynamic_pointer_cast<MeshNode>(node))
    {
        const glm::mat4 M = meshNode->worldTransform;
        for (const GeoSurface& s : meshNode->mesh->surfaces)
        {
            const glm::vec3 o = s.bounds.origin;
            const glm::vec3 e = s.bounds.extents;
            for (int ix = -1; ix <= 1; ix += 2)
            {
                for (int iy = -1; iy <= 1; iy += 2)
                {
                    for (int iz = -1; iz <= 1; iz += 2)
                    {
                        const glm::vec3 corner = o + glm::vec3(static_cast<float>(ix) * e.x, static_cast<float>(iy) * e.y, static_cast<float>(iz) * e.z);
                        const glm::vec3 w = glm::vec3(M * glm::vec4(corner, 1.f));
                        sceneMin = glm::min(sceneMin, w);
                        sceneMax = glm::max(sceneMax, w);
                        anyGeometry = true;
                    }
                }
            }
        }
    }

    for (const auto& child : node->children)
    {
        accumulate_scene_aabb(child, sceneMin, sceneMax, anyGeometry);
    }
}

struct MikkPrimitiveData 
{
    std::vector<Vertex>* vertices;
    const std::vector<uint32_t>* indices;
    std::vector<glm::vec4>* tangents;
    uint32_t startIndex;
    uint32_t indexCount;
};
static int mikk_get_num_faces(const SMikkTSpaceContext* ctx) 
{
    auto* data = static_cast<MikkPrimitiveData*>(ctx->m_pUserData);
    return static_cast<int>(data->indexCount / 3);
}
static int mikk_get_num_verts_of_face(const SMikkTSpaceContext*, const int) 
{
    return 3;
}
static uint32_t mikk_vertex_index(const MikkPrimitiveData& data, int face, int vert) 
{
    return (*data.indices)[data.startIndex + face * 3 + vert];
}
static void mikk_get_position(const SMikkTSpaceContext* ctx, float out[], const int face, const int vert) 
{
    auto* data = static_cast<MikkPrimitiveData*>(ctx->m_pUserData);

    const Vertex& v = (*data->vertices)[mikk_vertex_index(*data, face, vert)];
    out[0] = v.position.x; out[1] = v.position.y; out[2] = v.position.z;
}
static void mikk_get_normal(const SMikkTSpaceContext* ctx, float out[], const int face, const int vert) 
{
    auto* data = static_cast<MikkPrimitiveData*>(ctx->m_pUserData);
    const Vertex& v = (*data->vertices)[mikk_vertex_index(*data, face, vert)];
    out[0] = v.normal.x; out[1] = v.normal.y; out[2] = v.normal.z;
}
static void mikk_get_texcoord(const SMikkTSpaceContext* ctx, float out[], const int face, const int vert) 
{
    auto* data = static_cast<MikkPrimitiveData*>(ctx->m_pUserData);
    const Vertex& v = (*data->vertices)[mikk_vertex_index(*data, face, vert)];
    out[0] = v.uv_x; out[1] = v.uv_y;
}
static void mikk_set_tspace_basic(const SMikkTSpaceContext* ctx, const float tangent[], const float sign, const int face, const int vert) 
{
    auto* data = static_cast<MikkPrimitiveData*>(ctx->m_pUserData);
    (*data->tangents)[face * 3 + vert] = glm::vec4(tangent[0], tangent[1], tangent[2], sign);
}

static bool generate_tangents_for_primitive(
    std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices,
    uint32_t startIndex,
    uint32_t indexCount,
    std::vector<glm::vec4>& tangents)
{
    MikkPrimitiveData data{
        .vertices = &vertices,
        .indices = &indices,
        .tangents = &tangents,
        .startIndex = startIndex,
        .indexCount = indexCount
    };
    SMikkTSpaceInterface iface{};
    iface.m_getNumFaces = mikk_get_num_faces;
    iface.m_getNumVerticesOfFace = mikk_get_num_verts_of_face;
    iface.m_getPosition = mikk_get_position;
    iface.m_getNormal = mikk_get_normal;
    iface.m_getTexCoord = mikk_get_texcoord;
    iface.m_setTSpaceBasic = mikk_set_tspace_basic;
    SMikkTSpaceContext ctx{};
    ctx.m_pInterface = &iface;
    ctx.m_pUserData = &data;
    return genTangSpaceDefault(&ctx) != 0;
}

static void zero_tangents_for_primitive(std::vector<Vertex>& vertices, size_t firstVertex)
{
    for (size_t i = firstVertex; i < vertices.size(); i++)
    {
        vertices[i].tangent = glm::vec4(0.f);
    }
}

static void deindex_primitive_with_tangents(
    std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices,
    size_t initialVertex,
    uint32_t startIndex,
    uint32_t indexCount,
    const std::vector<glm::vec4>& tangents)
{
    std::vector<Vertex> expandedVertices;
    expandedVertices.reserve(indexCount);

    for (uint32_t i = 0; i < indexCount; i++)
    {
        Vertex expanded = vertices[indices[startIndex + i]];
        expanded.tangent = tangents[i];
        expandedVertices.push_back(expanded);
    }

    vertices.resize(initialVertex);
    indices.resize(startIndex);

    const uint32_t newInitialVertex = static_cast<uint32_t>(vertices.size());
    vertices.insert(vertices.end(), expandedVertices.begin(), expandedVertices.end());

    for (uint32_t i = 0; i < indexCount; i++)
    {
        indices.push_back(newInitialVertex + i);
    }
}

VkFilter extract_filter(fastgltf::Filter filter)
{
    switch (filter) 
    {
    // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;

    // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
{
    switch (filter) 
    {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

std::optional<std::size_t> resolve_texture_image_index(const fastgltf::Texture& texture)
{
    if (texture.imageIndex.has_value()) 
    {
        return texture.imageIndex.value();
    }
    if (texture.webpImageIndex.has_value()) 
    {
        return texture.webpImageIndex.value();
    }
    if (texture.ddsImageIndex.has_value()) 
    {
        return texture.ddsImageIndex.value();
    }
    if (texture.basisuImageIndex.has_value()) 
    {
        return texture.basisuImageIndex.value();
    }
    return {};
}

//https://vkguide.dev/docs/new_chapter_5/gltf_textures/
//modified to run at relative to repo root, and to support ByteView 
std::optional<AllocatedImage> load_image_from_gltf(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image, const std::filesystem::path& assetDirectory, VkFormat format)
{
    AllocatedImage newImage {};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor {
            //case 1: textures are stored outside of the gltf/glb file
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath()); // We're only capable of loading
                                                    // local files.

                const std::string localPath(filePath.uri.path().begin(),
                    filePath.uri.path().end()); // Thanks C++.
                const std::filesystem::path resolvedPath = assetDirectory / std::filesystem::path(localPath);
                unsigned char* data = stbi_load(resolvedPath.string().c_str(), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = vkutil::upload_image(*engine, data, imagesize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            // case 2: fastgltf loads the texture into a std::vector type structure
            [&](fastgltf::sources::Vector& vector) {
                unsigned char* data = stbi_load_from_memory(
                    reinterpret_cast<const stbi_uc*>(vector.bytes.data()),
                    static_cast<int>(vector.bytes.size()),
                    &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = vkutil::upload_image(*engine, data, imagesize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            // case 2b: fastgltf v0.9+ stores imported image bytes in sources::Array.
            [&](fastgltf::sources::Array& array) {
                unsigned char* data = stbi_load_from_memory(
                    reinterpret_cast<const stbi_uc*>(array.bytes.data()),
                    static_cast<int>(array.bytes.size()),
                    &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = vkutil::upload_image(*engine, data, imagesize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            // case 3: image bytes are already provided as a view.
            [&](fastgltf::sources::ByteView& byteView) {
                unsigned char* data = stbi_load_from_memory(
                    reinterpret_cast<const stbi_uc*>(byteView.bytes.data()),
                    static_cast<int>(byteView.bytes.size()), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = vkutil::upload_image(*engine, data, imagesize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            // case 4: image file is embedded into a buffer view (common in GLB files).
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor {
                               [](auto& arg) {},
                               [&](fastgltf::sources::Array& array) {
                                   auto* start = reinterpret_cast<const stbi_uc*>(array.bytes.data()) + bufferView.byteOffset;
                                   unsigned char* data = stbi_load_from_memory(start,
                                       static_cast<int>(bufferView.byteLength),
                                       &width, &height, &nrChannels, 4);
                                   if (data) {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;

                                       newImage = vkutil::upload_image(*engine, data, imagesize, format,
                                           VK_IMAGE_USAGE_SAMPLED_BIT,true);

                                       stbi_image_free(data);
                                   }
                               },
                               [&](fastgltf::sources::Vector& vector) {
                                   auto* start = reinterpret_cast<const stbi_uc*>(vector.bytes.data()) + bufferView.byteOffset;
                                   unsigned char* data = stbi_load_from_memory(start,
                                       static_cast<int>(bufferView.byteLength),
                                       &width, &height, &nrChannels, 4);
                                   if (data) {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;

                                       newImage = vkutil::upload_image(*engine, data, imagesize, format,
                                           VK_IMAGE_USAGE_SAMPLED_BIT,true);

                                       stbi_image_free(data);
                                   }
                               },
                               [&](fastgltf::sources::ByteView& byteView) {
                                   auto* start = reinterpret_cast<const stbi_uc*>(byteView.bytes.data()) + bufferView.byteOffset;
                                   unsigned char* data = stbi_load_from_memory(start,
                                       static_cast<int>(bufferView.byteLength),
                                       &width, &height, &nrChannels, 4);
                                   if (data) {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;

                                       newImage = vkutil::upload_image(*engine, data, imagesize, format,
                                           VK_IMAGE_USAGE_SAMPLED_BIT, true);

                                       stbi_image_free(data);
                                   }
                               } },
                    buffer.data);
            },
        },
        image.data);

    // if any of the attempts to load the data failed, we havent written the image
    // so handle is null
    if (newImage.image == VK_NULL_HANDLE) {
        return {};
    } else {
        return newImage;
    }
}


std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine *engine, std::filesystem::path filePath)
{
    fmt::print("Loading GLTF: {}\n", filePath.string());

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    scene->creator = engine;
    LoadedGLTF& file = *scene.get();

    fastgltf::Parser parser {
        fastgltf::Extensions::KHR_texture_transform |
        fastgltf::Extensions::KHR_materials_transmission |
        fastgltf::Extensions::KHR_materials_diffuse_transmission |
        fastgltf::Extensions::KHR_materials_clearcoat |
        fastgltf::Extensions::KHR_materials_iridescence |
        fastgltf::Extensions::KHR_materials_unlit |
        fastgltf::Extensions::KHR_lights_punctual
    };

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers;
    // fastgltf::Options::LoadExternalImages;

    fastgltf::Asset gltf;

    std::filesystem::path path = filePath;
    auto data = fastgltf::GltfDataBuffer::FromPath(filePath);
    if (!data)
    {
        std::cerr << "Failed to read glTF bytes: " << fastgltf::to_underlying(data.error()) << std::endl;
        return {};
    }

    auto load = parser.loadGltf(data.get(), path.parent_path(), gltfOptions);
    if (load)
    {
        gltf = std::move(load.get());
    }
    else
    {
        std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
        return {};
    }

    // estimate what we need
    // we use this descriptor pool for write_material
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = 
    {   
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 11 }, //albedo, metalrough, normal, emissive, ao, diffuseTransmissionColor, diffuseTransmissionFactor, clearcoat, clearcoatRoughness, iridescence, iridescenceThickness
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },          
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } 
    }; //one for each mateiral usually
    file.descriptorPool.init(engine->device(), gltf.materials.size(), sizes);

    // load samplers
    for (fastgltf::Sampler& sampler : gltf.samplers) 
    {
        VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;
        sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        sampl.mipmapMode= extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        sampl.anisotropyEnable = VK_TRUE;
        sampl.maxAnisotropy = 8.0f;

        VkSampler newSampler;
        vkCreateSampler(engine->device(), &sampl, nullptr, &newSampler);

        file.samplers.push_back(newSampler);
    }

    // temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;
    std::unordered_map<std::string, AllocatedImage> imageCache; //name_srgb|unorm : image

    // load all textures

    auto get_cached_image = [&](size_t imageIndex, VkFormat format) -> AllocatedImage
    {
        const std::string key = std::to_string(imageIndex) + "_" + (format == VK_FORMAT_R8G8B8A8_SRGB ? "srgb" : "unorm");

        if (auto it = imageCache.find(key); it != imageCache.end())
        {
            return it->second;
        }

        std::optional<AllocatedImage> img = load_image_from_gltf(engine, gltf, gltf.images[imageIndex], path.parent_path(), format);

        AllocatedImage finalImage = img.value_or(engine->error_checkerboard_image());

        imageCache[key] = finalImage;
        file.images[key] = finalImage; // ownership for cleanup

        return finalImage;
    };

     // create buffer to hold all the material CONSTANTS
     // these are part of the gltf pbr standard
    file.materialDataBuffer = vkutil::create_buffer(engine->allocator(), sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    int data_index = 0;

    //once the buffers created, give the pointer to sceneMaterialConstants
    //get written to at X
    GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants = (GLTFMetallic_Roughness::MaterialConstants*)file.materialDataBuffer.info.pMappedData;

    // LOAD ALL MATERIALS -----------------------------------------------------------
    // The goal is to use write_material() to create a MaterialInstance for every gltf mat (gltf mat has all the info we need).
    // To do that we need to fill out a MaterialResources. We use the descriptor pool we created earlier.
    for (fastgltf::Material& mat : gltf.materials) 
    {
        std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
        materials.push_back(newMat);
        file.materials[mat.name.c_str()] = newMat;

        //MaterialConstants pointer for MaterialResources----------------------------------------
        // (MaterialResources.dataBuffer and MaterialResources.dataBufferOffset)
        // notice we reuse the same buffer once just changing the index.
        GLTFMetallic_Roughness::MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];
        constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
        constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;

        //DIFFUSE TRANSMISSION -------------
        //default values if diffuse transmission extension is not used.
        constants.diffuse_transmission_factors.x = 1.0f;
        constants.diffuse_transmission_factors.y = 1.0f;
        constants.diffuse_transmission_factors.z = 1.0f;
        constants.diffuse_transmission_factors.w = 0.0f; 

        //write diffuse transmission factors here if applciable
        if (mat.diffuseTransmission) 
        {
            const auto& dt = *mat.diffuseTransmission;
            constants.diffuse_transmission_factors.x = dt.diffuseTransmissionColorFactor[0];
            constants.diffuse_transmission_factors.y = dt.diffuseTransmissionColorFactor[1];
            constants.diffuse_transmission_factors.z = dt.diffuseTransmissionColorFactor[2];
            constants.diffuse_transmission_factors.w = dt.diffuseTransmissionFactor;
        }

        //CLEARCOAT -------------
        constants.clearcoat_factors.x = 0.0f;
        constants.clearcoat_factors.y = 0.0f;
        constants.clearcoat_factors.z = 0.0f;
        constants.clearcoat_factors.w = 0.0f;

        if (mat.clearcoat)
        {
            const auto& cc = *mat.clearcoat;
            constants.clearcoat_factors.x = cc.clearcoatFactor;
            constants.clearcoat_factors.y = cc.clearcoatRoughnessFactor;
        }

        // IRIDESCENCE -------------
        constants.iridescence_factors.x = 0.0f;
        constants.iridescence_factors.y = 1.3f;
        constants.iridescence_factors.z = 100.0f;
        constants.iridescence_factors.w = 400.0f;

        if (mat.iridescence)
        {
            const auto& ir = *mat.iridescence;
            constants.iridescence_factors.x = ir.iridescenceFactor;
            constants.iridescence_factors.y = ir.iridescenceIor;
            constants.iridescence_factors.z = ir.iridescenceThicknessMinimum;
            constants.iridescence_factors.w = ir.iridescenceThicknessMaximum;
        }

        // write material parameters to buffer (this is for the pointer in MaterialResources)
        // X
        sceneMaterialConstants[data_index] = constants;
        //--------------------------------------------------------------------------------------
        

        // Pass type, for write_material's pass arg---------------------------------------------
        MaterialPass passType = MaterialPass::MainColor;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend) 
        {
            passType = MaterialPass::Transparent;
        }
        //-------------------------------------------------------------------------------------------


        //set images, samplers----------------------------------------------------------------------
        //-------------------------------------------------------------------------------------------
        GLTFMetallic_Roughness::MaterialResources materialResources;

        // default the material textures
        materialResources.colorImage = engine->white_image();
        materialResources.colorSampler = engine->default_sampler_linear();
        materialResources.metalRoughImage = engine->white_image();
        materialResources.metalRoughSampler = engine->default_sampler_linear();
        materialResources.normalImage = engine->flat_normal_image();
        materialResources.normalSampler = engine->default_sampler_linear();
        materialResources.emissiveImage = engine->black_image();
        materialResources.emissiveSampler = engine->default_sampler_linear();
        materialResources.aoImage = engine->white_image();
        materialResources.aoSampler = engine->default_sampler_linear();
        materialResources.diffuseTransmissionColorImage = engine->white_image();
        materialResources.diffuseTransmissionColorSampler = engine->default_sampler_linear();
        materialResources.diffuseTransmissionFactorImage = engine->white_image();
        materialResources.diffuseTransmissionFactorSampler = engine->default_sampler_linear();
        materialResources.clearcoatImage = engine->white_image(); //factor but no texture is valid, so def shouldnt be black or it will wipe
        materialResources.clearcoatSampler = engine->default_sampler_linear();
        materialResources.clearcoatRoughnessImage = engine->white_image();
        materialResources.clearcoatRoughnessSampler = engine->default_sampler_linear();
        materialResources.iridescenceImage = engine->white_image();
        materialResources.iridescenceSampler = engine->default_sampler_linear();
        materialResources.iridescenceThicknessImage = engine->white_image(); // G=1 => thicknessMax when no texture
        materialResources.iridescenceThicknessSampler = engine->default_sampler_linear();

        // MaterialConstants we made earlier
        materialResources.dataBuffer = file.materialDataBuffer.buffer;
        materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants); //take a portion of all the material constant buffer
        
        // grab textures from gltf file if they exist
        // base color
        if (mat.pbrData.baseColorTexture.has_value()) 
        {
            const auto texIndex = mat.pbrData.baseColorTexture->textureIndex;
            const auto& tex = gltf.textures[texIndex];

            if (auto imageIndex = resolve_texture_image_index(tex); imageIndex.has_value()) 
            {
                materialResources.colorImage = get_cached_image(*imageIndex, VK_FORMAT_R8G8B8A8_SRGB);
            }
            if (tex.samplerIndex.has_value()) 
            {
                materialResources.colorSampler = file.samplers[*tex.samplerIndex];
            }
        }

        // metallic-roughness
        if (mat.pbrData.metallicRoughnessTexture.has_value()) 
        {
            const auto texIndex = mat.pbrData.metallicRoughnessTexture->textureIndex;
            const auto& tex = gltf.textures[texIndex];

            if (auto imageIndex = resolve_texture_image_index(tex); imageIndex.has_value()) 
            {
                materialResources.metalRoughImage = get_cached_image(*imageIndex, VK_FORMAT_R8G8B8A8_UNORM);
            }
            if (tex.samplerIndex.has_value()) 
            {
                materialResources.metalRoughSampler = file.samplers[*tex.samplerIndex];
            }
        }

        if (mat.normalTexture.has_value())
        {
            const auto texIndex = mat.normalTexture->textureIndex;
            const auto& tex = gltf.textures[texIndex];

            if (auto imageIndex = resolve_texture_image_index(tex); imageIndex.has_value())
            {
                materialResources.normalImage = get_cached_image(*imageIndex, VK_FORMAT_R8G8B8A8_UNORM);
            }
            if (tex.samplerIndex.has_value())
            {
                materialResources.normalSampler = file.samplers[*tex.samplerIndex];
            }
        }

        if (mat.emissiveTexture.has_value())
        {
            const auto texIndex = mat.emissiveTexture->textureIndex;
            const auto& tex = gltf.textures[texIndex];

            if (auto imageIndex = resolve_texture_image_index(tex); imageIndex.has_value())
            {
                materialResources.emissiveImage = get_cached_image(*imageIndex, VK_FORMAT_R8G8B8A8_SRGB);
            }
            if (tex.samplerIndex.has_value())
            {
                materialResources.emissiveSampler = file.samplers[*tex.samplerIndex];
            }
        }

        if (mat.occlusionTexture.has_value())
        {
            const auto texIndex = mat.occlusionTexture->textureIndex;
            const auto& tex = gltf.textures[texIndex];

            if (auto imageIndex = resolve_texture_image_index(tex); imageIndex.has_value())
            {
                materialResources.aoImage = get_cached_image(*imageIndex, VK_FORMAT_R8G8B8A8_UNORM);
            }
            if (tex.samplerIndex.has_value())
            {
                materialResources.aoSampler = file.samplers[*tex.samplerIndex];
            }
        }

        //diffuse transmission
        if (mat.diffuseTransmission) 
        {
            const auto& dt = *mat.diffuseTransmission;

            // factor texture r
            if (dt.diffuseTransmissionTexture.has_value()) 
            {
                const auto texIndex = dt.diffuseTransmissionTexture->textureIndex;
                const auto& tex = gltf.textures[texIndex];
                if (auto imageIndex = resolve_texture_image_index(tex); imageIndex.has_value()) 
                {
                    materialResources.diffuseTransmissionFactorImage = get_cached_image(*imageIndex, VK_FORMAT_R8G8B8A8_UNORM);
                }
                if (tex.samplerIndex.has_value())
                {
                    materialResources.diffuseTransmissionFactorSampler = file.samplers[*tex.samplerIndex];
                }
            }

            // color texture rgb
            if (dt.diffuseTransmissionColorTexture.has_value()) 
            {
                const auto texIndex = dt.diffuseTransmissionColorTexture->textureIndex;
                const auto& tex = gltf.textures[texIndex];
                if (auto imageIndex = resolve_texture_image_index(tex); imageIndex.has_value()) 
                {
                    materialResources.diffuseTransmissionColorImage = get_cached_image(*imageIndex, VK_FORMAT_R8G8B8A8_SRGB);
                }
                if (tex.samplerIndex.has_value()) 
                {
                    materialResources.diffuseTransmissionColorSampler = file.samplers[*tex.samplerIndex];
                }
            }
        }

        // KHR_materials_clearcoat
        if (mat.clearcoat)
        {
            const auto& cc = *mat.clearcoat;

            if (cc.clearcoatTexture.has_value())
            {
                const auto texIndex = cc.clearcoatTexture->textureIndex;
                const auto& tex = gltf.textures[texIndex];
                if (auto imageIndex = resolve_texture_image_index(tex); imageIndex.has_value())
                {
                    materialResources.clearcoatImage = get_cached_image(*imageIndex, VK_FORMAT_R8G8B8A8_UNORM);
                }
                if (tex.samplerIndex.has_value())
                {
                    materialResources.clearcoatSampler = file.samplers[*tex.samplerIndex];
                }
            }

            if (cc.clearcoatRoughnessTexture.has_value())
            {
                const auto texIndex = cc.clearcoatRoughnessTexture->textureIndex;
                const auto& tex = gltf.textures[texIndex];
                if (auto imageIndex = resolve_texture_image_index(tex); imageIndex.has_value())
                {
                    materialResources.clearcoatRoughnessImage = get_cached_image(*imageIndex, VK_FORMAT_R8G8B8A8_UNORM);
                }
                if (tex.samplerIndex.has_value())
                {
                    materialResources.clearcoatRoughnessSampler = file.samplers[*tex.samplerIndex];
                }
            }
        }

        // KHR_materials_iridescence
        if (mat.iridescence)
        {
            const auto& ir = *mat.iridescence;

            if (ir.iridescenceTexture.has_value())
            {
                const auto texIndex = ir.iridescenceTexture->textureIndex;
                const auto& tex = gltf.textures[texIndex];
                if (auto imageIndex = resolve_texture_image_index(tex); imageIndex.has_value())
                {
                    materialResources.iridescenceImage = get_cached_image(*imageIndex, VK_FORMAT_R8G8B8A8_UNORM);
                }
                if (tex.samplerIndex.has_value())
                {
                    materialResources.iridescenceSampler = file.samplers[*tex.samplerIndex];
                }
            }

            if (ir.iridescenceThicknessTexture.has_value())
            {
                const auto texIndex = ir.iridescenceThicknessTexture->textureIndex;
                const auto& tex = gltf.textures[texIndex];
                if (auto imageIndex = resolve_texture_image_index(tex); imageIndex.has_value())
                {
                    materialResources.iridescenceThicknessImage = get_cached_image(*imageIndex, VK_FORMAT_R8G8B8A8_UNORM);
                }
                if (tex.samplerIndex.has_value())
                {
                    materialResources.iridescenceThicknessSampler = file.samplers[*tex.samplerIndex];
                }
            }
        }

        //------------------------------------------------------------------------------------------------

        // build material
        newMat->data = engine->material_system().write_material(engine->device(), passType, materialResources, file.descriptorPool);

        //note we jump by size of MaterialConstants
        data_index++;
    }


    //LOAD MESHES---------------------------------------------------------------------------------------------

    // use the same vectors for all meshes so that the memory doesnt reallocate as often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (fastgltf::Mesh& mesh : gltf.meshes) 
    {
        //a mesh asset needs a name, vertex/index buffer address and a vector of its surfaces.
        std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
        meshes.push_back(newmesh);
        file.meshes[mesh.name.c_str()] = newmesh;
        newmesh->name = mesh.name; //name done

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        //each gltf primative maps to one of our GeoSurface structs.
        for (auto&& p : mesh.primitives) 
        {
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t)indices.size(); //index buffer start
            newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) {
                        indices.push_back(idx + initial_vtx);
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index) 
                    {
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = { 1, 0, 0 };
                        newvtx.color = glm::vec4 { 1.f };
                        newvtx.uv_x = 0;
                        newvtx.uv_y = 0;
                        vertices[initial_vtx + index] = newvtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) 
            {

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).accessorIndex],
                    [&](glm::vec3 v, size_t index) 
                    {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end())
            {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).accessorIndex],
                    [&](glm::vec2 v, size_t index)
                    {
                        vertices[initial_vtx + index].uv_x = v.x;
                        vertices[initial_vtx + index].uv_y = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end())
            {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).accessorIndex],
                    [&](glm::vec4 v, size_t index)
                    {
                        vertices[initial_vtx + index].color = v;
                    });
            }

            // load vertex tangents/bitangants
            auto tangents = p.findAttribute("TANGENT");
            const bool canGenerateTangents =
                tangents == p.attributes.end() &&
                normals != p.attributes.end() &&
                uv != p.attributes.end() &&
                (newSurface.count % 3) == 0;

            if (tangents != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[tangents->accessorIndex],
                    [&](glm::vec4 tangent, size_t index)
                    {
                        vertices[initial_vtx + index].tangent = tangent;
                    });
            }
            else if (!canGenerateTangents)
            {
                zero_tangents_for_primitive(vertices, initial_vtx);
            }
            else
            {
                std::vector<glm::vec4> generatedTangents(newSurface.count, glm::vec4(0.f));

                if (generate_tangents_for_primitive(vertices, indices, newSurface.startIndex, newSurface.count, generatedTangents))
                {
                    deindex_primitive_with_tangents(vertices, indices, initial_vtx, newSurface.startIndex, newSurface.count, generatedTangents);
                }
                else
                {
                    zero_tangents_for_primitive(vertices, initial_vtx);
                }
            }
            
            //If it has a material, we created it (as a MaterialInstance) already 
            // so we just find that in our mateiral vector and set it.
            if (p.materialIndex.has_value()) 
            {
                newSurface.material = materials[p.materialIndex.value()];
            } 
            else 
            {
                newSurface.material = materials[0];
            }

            //calcualte bounding box
            glm::vec3 minpos = vertices[initial_vtx].position;
            glm::vec3 maxpos = vertices[initial_vtx].position;
            for (int i = initial_vtx; i < vertices.size(); i++) 
            {
                minpos = glm::min(minpos, vertices[i].position);
                maxpos = glm::max(maxpos, vertices[i].position);
            }
            
            // calculate aa bounding box
            newSurface.bounds.origin = (maxpos + minpos) / 2.f;
            newSurface.bounds.extents = (maxpos - minpos) / 2.f;
            newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);
                
            //vector of surfaces 
            newmesh->surfaces.push_back(newSurface);
        }

        //vector of surfaces done


        //vertex/index buffer done
        newmesh->meshBuffers = vkutil::upload_mesh(*engine, indices, vertices);
    }


    // load all nodes with their meshes / local matrix
    for (fastgltf::Node& node : gltf.nodes) 
    {
        std::shared_ptr<Node> newNode;

        // find if the gltf node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
        if (node.meshIndex.has_value()) 
        {
            newNode = std::make_shared<MeshNode>();
            static_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex]; //we already made our meshes, so just find and set.
        } 
        else 
        {
            newNode = std::make_shared<Node>();
        }

        //calculate LOCAL matrix
        nodes.push_back(newNode);
        file.nodes[node.name.c_str()];
        std::visit(fastgltf::visitor { [&](fastgltf::math::fmat4x4 matrix) {
                                          memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
                                      },
                       [&](fastgltf::TRS transform) {
                           glm::vec3 tl(transform.translation[0], transform.translation[1],
                               transform.translation[2]);
                           glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                               transform.rotation[2]);
                           glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                           glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                           glm::mat4 rm = glm::toMat4(rot);
                           glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                           newNode->localTransform = tm * rm * sm;
                       } },
            node.transform);
    }

    //Set up parenting relationships to build scene graph-----------------------------------

    // run loop again to setup transform hierarchy (fill out Node.children)
    for (int i = 0; i < gltf.nodes.size(); i++) 
    {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = nodes[i];

        for (auto& c : node.children) 
        {
            sceneNode->children.push_back(nodes[c]);
            nodes[c]->parent = sceneNode;
        }
    }

    // find the top nodes, with no parents
    // this sets the world matrices too
    for (auto& node : nodes) 
    {
        //if its a top node
        if (node->parent.lock() == nullptr)
        {
            //add to top node vector
            file.topNodes.push_back(node);

            //set its world matrix (which is identity since its a top node)
            // and set all its children's world matrix.
            node->refreshTransform(glm::mat4 { 1.f });
        }
    }

    {
        glm::vec3 sceneMin(std::numeric_limits<float>::max());
        glm::vec3 sceneMax(std::numeric_limits<float>::lowest());
        bool anyGeometry = false;
        for (const auto& root : file.topNodes)
        {
            accumulate_scene_aabb(root, sceneMin, sceneMax, anyGeometry);
        }
        //before any scaling
        file.sceneCenter = anyGeometry ? 0.5f * (sceneMin + sceneMax) : glm::vec3(0.f);
    }

    return scene;
}

std::optional<AllocatedImage> load_hdr_image(VulkanEngine *engine, const std::filesystem::path &path)
{
    int width, height, channels;

    float* data = stbi_loadf(path.string().c_str(), &width, &height, &channels, 4);
    if (!data) 
    {
        return {};
    }

    VkExtent3D size
    {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        1
    };
    
    size_t byteSize =
        static_cast<size_t>(width) *
        static_cast<size_t>(height) *
        4 *
        sizeof(float);


    AllocatedImage image = vkutil::upload_image(
        *engine,
        data,
        size,
        VulkanEngine::kEnvironmentMapFormat,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        true,
        byteSize);

    stbi_image_free(data);

    return image;
}

glm::vec3 LoadedGLTF::worldSceneCenter() const
{
    const float s = cvarModelScale.Get();
    return glm::vec3(s) * sceneCenter;
}

void LoadedGLTF::Draw(const glm::mat4 &topMatrix, DrawContext &ctx)
{
    // create renderables from the scenenodes
    for (auto& n : topNodes) 
    {
        n->Draw(topMatrix * glm::scale(glm::mat4(1.0f), glm::vec3(cvarModelScale.Get())) , ctx);
    }
}

void LoadedGLTF::clearAll()
{
    VkDevice dv = creator->device();

    //descriptor Pool
    descriptorPool.destroy_pools(dv);

    //uniform buffer
    vkutil::destroy_buffer(creator->allocator(), materialDataBuffer);

    for (auto& [k, v] : meshes) 
    {
		vkutil::destroy_buffer(creator->allocator(), v->meshBuffers.indexBuffer);
		vkutil::destroy_buffer(creator->allocator(), v->meshBuffers.vertexBuffer);
    }

    for (auto& [k, v] : images) 
    {
        
        if (v.image == creator->error_checkerboard_image().image) 
        {
            continue;
        }

        vkutil::destroy_image(creator->allocator(), creator->device(), v);
    }

	for (auto& sampler : samplers) 
    {
		vkDestroySampler(dv, sampler, nullptr);
    }
}

#include <vk_loader.h>

#include "stb_image.h"
#include <iostream>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

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

//https://vkguide.dev/docs/new_chapter_5/gltf_textures/
//modified for linux file path
std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image, const std::filesystem::path& assetDirectory)
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

                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

                    stbi_image_free(data);
                }
            },
            // case 2: fastgltf loads the texture into a std::vector type structure
            [&](fastgltf::sources::Vector& vector) {
                unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                    &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

                    stbi_image_free(data);
                }
            },
            // case 3: image file is embedded into the binary GLB file.
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
                                               // specify LoadExternalBuffers, meaning all buffers
                                               // are already loaded into a vector.
                               [](auto& arg) {},
                               [&](fastgltf::sources::Vector& vector) {
                                   unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
                                       static_cast<int>(bufferView.byteLength),
                                       &width, &height, &nrChannels, 4);
                                   if (data) {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;

                                       newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_USAGE_SAMPLED_BIT,false);

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
    fmt::print("Loading GLTF: {}\n", filePath.c_str());

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    scene->creator = engine;
    LoadedGLTF& file = *scene.get();

    fastgltf::Parser parser {};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
    // fastgltf::Options::LoadExternalImages;

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filePath);

    fastgltf::Asset gltf;

    std::filesystem::path path = filePath;

    auto type = fastgltf::determineGltfFileType(&data);
    if (type == fastgltf::GltfType::glTF) 
    {
        auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
        if (load) 
        {
            gltf = std::move(load.get());
        } 
        else 
        {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    }
    else if (type == fastgltf::GltfType::GLB) 
    {
        auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
        if (load) 
        {
            gltf = std::move(load.get());
        } 
        else 
        {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    } 
    else 
    {
        std::cerr << "Failed to determine glTF container" << std::endl;
        return {};
    }

    // estimate what we need
    // we use this descriptor pool for write_material
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = 
    {   
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 }, //albedo, metalrough, etc
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },          
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } 
    }; //one for each mateiral usually
    file.descriptorPool.init(engine->_device, gltf.materials.size(), sizes);

    // load samplers
    for (fastgltf::Sampler& sampler : gltf.samplers) 
    {
        VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;
        sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        sampl.mipmapMode= extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(engine->_device, &sampl, nullptr, &newSampler);

        file.samplers.push_back(newSampler);
    }

    // temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // load all textures
	for (fastgltf::Image& image : gltf.images) 
    {
		std::optional<AllocatedImage> img = load_image(engine, gltf, image, path.parent_path());

		if (img.has_value()) 
        {
			images.push_back(*img);
			file.images[image.name.c_str()] = *img;
		}
		else
        {
			// we failed to load. Set to checkboard
			images.push_back(engine->_errorCheckerboardImage);
			std::cout << "gltf failed to load texture " << image.name << std::endl;
		}
	}

     // create buffer to hold all the material CONSTANTS
    file.materialDataBuffer = engine->create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
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
        materialResources.colorImage = engine->_whiteImage;
        materialResources.colorSampler = engine->_defaultSamplerLinear;
        materialResources.metalRoughImage = engine->_whiteImage;
        materialResources.metalRoughSampler = engine->_defaultSamplerLinear;

        // MaterialConstants we made earlier
        materialResources.dataBuffer = file.materialDataBuffer.buffer;
        materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants); //take a portion of all the material constant buffer
        
        // grab textures from gltf file if they exist
        // base color
        if (mat.pbrData.baseColorTexture.has_value()) 
        {
            const auto texIndex = mat.pbrData.baseColorTexture->textureIndex;
            const auto& tex = gltf.textures[texIndex];

            if (tex.imageIndex.has_value()) 
            {
                materialResources.colorImage = images[*tex.imageIndex];
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

            if (tex.imageIndex.has_value()) 
            {
                materialResources.metalRoughImage = images[*tex.imageIndex];
            }
            if (tex.samplerIndex.has_value()) 
            {
                materialResources.metalRoughSampler = file.samplers[*tex.samplerIndex];
            }
        }
        //------------------------------------------------------------------------------------------------

        // build material
        newMat->data = engine->metalRoughMaterial.write_material(engine->_device, passType, materialResources, file.descriptorPool);

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
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
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

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                    [&](glm::vec3 v, size_t index) 
                    {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) 
            {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
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

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) 
                    {
                        vertices[initial_vtx + index].color = v;
                    });
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
            
            //vector of surfaces 
            newmesh->surfaces.push_back(newSurface);
        }

        //vector of surfaces done

        //vertex/index buffer done
        newmesh->meshBuffers = engine->uploadMesh(indices, vertices);
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
        std::visit(fastgltf::visitor { [&](fastgltf::Node::TransformMatrix matrix) {
                                          memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
                                      },
                       [&](fastgltf::Node::TRS transform) {
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

    return scene;
}

void LoadedGLTF::Draw(const glm::mat4 &topMatrix, DrawContext &ctx)
{
    // create renderables from the scenenodes
    for (auto& n : topNodes) {
        n->Draw(topMatrix, ctx);
    }
}

void LoadedGLTF::clearAll()
{
    VkDevice dv = creator->_device;

    //descriptor Pool
    descriptorPool.destroy_pools(dv);

    //uniform buffer
    creator->destroy_buffer(materialDataBuffer);

    for (auto& [k, v] : meshes) 
    {
		creator->destroy_buffer(v->meshBuffers.indexBuffer);
		creator->destroy_buffer(v->meshBuffers.vertexBuffer);
    }

    for (auto& [k, v] : images) 
    {
        
        if (v.image == creator->_errorCheckerboardImage.image) 
        {
            continue;
        }

        creator->destroy_image(v);
    }

	for (auto& sampler : samplers) 
    {
		vkDestroySampler(dv, sampler, nullptr);
    }
}

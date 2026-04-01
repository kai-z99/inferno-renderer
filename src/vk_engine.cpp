//> includes
#include "vk_engine.h"
#include "cvars.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_images.h>
#include <vk_resources.h>
#include <vk_upload.h>
#include <vk_pipelines.h>

#include "VkBootstrap.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <glm/gtx/transform.hpp>

#include <chrono>
#include <thread>

static AutoCVar_Float cvarRenderScale(
    "r.renderScale",
    "Internal render scale",
    1.0,
    FloatCVarOptions{
        .minValue = 0.3,
        .maxValue = 1.0,
        .step = 0.01f,
        .format = "%.2f",
    },
    CVarEditHint::TextBox);

static AutoCVar_Float cvarSunPower(
    "r.sunPower",
    "Internal render scale",
    2.1,
    FloatCVarOptions{
        .minValue = 0.3,
        .maxValue = 50.0,
        .step = 0.01f,
        .format = "%.2f",
    },
    CVarEditHint::Slider);

static AutoCVar_Float cvarAmbient(
    "r.ambientLight",
    "Internal render scale",
    0.1,
    FloatCVarOptions{
        .minValue = 0.0,
        .maxValue = 0.5,
        .step = 0.01f,
        .format = "%.2f",
    },
    CVarEditHint::Slider);

static AutoCVar_Vec3 cvarSunDir(
    "r.sun.direction",
    "Sun light direction",
    glm::vec3(0.0f, -1.0f, 0.5f),
    Vec3CVarOptions{
        .minValue = -1.0f,
        .maxValue = 1.0f,
        .step = 0.01f,
        .format = "%.3f",
    },
    CVarEditHint::Drag
);

static AutoCVar_Int cvarTonemapIndex(
    "r.tonemapindex",
    "Internal render scale",
    0,
    IntCVarOptions{
        .minValue = 0,
        .maxValue = 2,
        .step = 1
    },
    CVarEditHint::Step);


VulkanEngine *loadedEngine = nullptr;

VulkanEngine &VulkanEngine::Get() { return *loadedEngine; }

constexpr bool bUseValidationLayers = true;

void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    _window = SDL_CreateWindow(
        "Hello Vulkan!",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

    init_vulkan();

    init_swapchain();

    init_render_targets();

    init_commands();

    init_sync_structures();

    init_default_data();

    init_descriptors();

    init_pipelines();

    init_imgui();

    init_scene();

    // everything went fine
    _isInitialized = true;

    fmt::print("Initialization complete!\n");
}

void VulkanEngine::init_vulkan()
{
    fmt::print("Initializing Vulkan Instance...\n");
    vkb::InstanceBuilder builder;

    auto inst_ret = builder.set_app_name("Hello Vulkan!")
                        .request_validation_layers(bUseValidationLayers)
                        .use_default_debug_messenger()
                        .require_api_version(1, 3, 0)
                        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    // create the window we will be rendering in
    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    // vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features.dynamicRendering = true;
    features.synchronization2 = true;

    // vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    // use vkbootstrap to select a gpu.
    // We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector{vkb_inst};
    vkb::PhysicalDevice physicalDevice = selector
                                             .set_minimum_version(1, 3)
                                             .set_required_features_13(features)
                                             .set_required_features_12(features12)
                                             .set_surface(_surface)
                                             .select()
                                             .value();

    // create the final vulkan device
    vkb::DeviceBuilder deviceBuilder{physicalDevice};

    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    _device = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;

    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // init VMA memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _chosenGPU;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    _mainDeletionQueue.push_function([&]()
                                     { vmaDestroyAllocator(_allocator); });
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
    fmt::print("Creating swapchain...\n");
    vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};
    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
                                      .set_desired_format(VkSurfaceFormatKHR{.format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                                      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                      .set_desired_extent(width, height)
                                      .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                      .build()
                                      .value();

    _swapchainExtent = vkbSwapchain.extent;
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::create_render_targets()
{
    fmt::print("Creating render targets...\n");
    VkExtent3D drawImageExtent = 
    {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // draw image ---

    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawUsages{};
    drawUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    drawUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageCreateInfo drawInfo = vkinit::image_create_info(_drawImage.imageFormat, drawUsages, drawImageExtent);

    vmaCreateImage(_allocator, &drawInfo, &allocInfo, &_drawImage.image, &_drawImage.allocation, nullptr);

    VkImageViewCreateInfo drawViewInfo = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(_device, &drawViewInfo, nullptr, &_drawImage.imageView));

    // msaa image ---

    _msaaImage.imageFormat = _drawImage.imageFormat;
    _msaaImage.imageExtent = drawImageExtent;
    VkImageUsageFlags msaaUsages = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo msaaInfo = vkinit::image_create_info(_msaaImage.imageFormat, msaaUsages, drawImageExtent, VK_SAMPLE_COUNT_4_BIT);

    vmaCreateImage(_allocator, &msaaInfo, &allocInfo, &_msaaImage.image, &_msaaImage.allocation, nullptr);

    VkImageViewCreateInfo msaaViewInfo = vkinit::imageview_create_info(_msaaImage.imageFormat, _msaaImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(_device, &msaaViewInfo, nullptr, &_msaaImage.imageView));


    //depth image ----

    _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage.imageExtent = drawImageExtent;

    VkImageUsageFlags depthUsages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkImageCreateInfo depthInfo = vkinit::image_create_info(_depthImage.imageFormat, depthUsages, drawImageExtent, VK_SAMPLE_COUNT_4_BIT);

    vmaCreateImage(_allocator, &depthInfo, &allocInfo, &_depthImage.image, &_depthImage.allocation, nullptr);

    VkImageViewCreateInfo depthViewInfo = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_device, &depthViewInfo, nullptr, &_depthImage.imageView));

    //tone mapping image ----

    _tonemapImage.imageFormat = _swapchainImageFormat;
    _tonemapImage.imageExtent = drawImageExtent;

    VkImageUsageFlags tonemapUsages{};
    tonemapUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    tonemapUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkImageCreateInfo tonemapInfo = vkinit::image_create_info(_tonemapImage.imageFormat, tonemapUsages, drawImageExtent);

    vmaCreateImage(_allocator, &tonemapInfo, &allocInfo, &_tonemapImage.image, &_tonemapImage.allocation, nullptr);

    VkImageViewCreateInfo tonemapViewInfo = vkinit::imageview_create_info(_tonemapImage.imageFormat, _tonemapImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(_device, &tonemapViewInfo, nullptr, &_tonemapImage.imageView));
}

void VulkanEngine::init_swapchain()
{
    fmt::print("Initializing swapchain...\n");
    create_swapchain(_windowExtent.width, _windowExtent.height);
}

void VulkanEngine::init_render_targets()
{
    fmt::print("Initializing render targets...\n");
    create_render_targets();

    // shadow depth image still lives here for now
    VkExtent3D shadowExtent = { _shadowMapResolution, _shadowMapResolution, 1 };

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    _shadowDepthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    _shadowDepthImage.imageExtent = shadowExtent;

    VkImageUsageFlags shadowImageUsages{};
    shadowImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageCreateInfo simg_info =
        vkinit::image_create_info(_shadowDepthImage.imageFormat, shadowImageUsages, shadowExtent);

    vmaCreateImage(_allocator, &simg_info, &allocInfo, &_shadowDepthImage.image, &_shadowDepthImage.allocation, nullptr);

    VkImageViewCreateInfo sview_info =
        vkinit::imageview_create_info(_shadowDepthImage.imageFormat, _shadowDepthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_device, &sview_info, nullptr, &_shadowDepthImage.imageView));

    _mainDeletionQueue.push_function([=, this]()
    {
        vkDestroyImageView(_device, _shadowDepthImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _shadowDepthImage.image, _shadowDepthImage.allocation);
    });
}

void VulkanEngine::destroy_swapchain()
{
    fmt::print("Destroying swapchain...\n");
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    // destory swapchain resrources
    for (int i = 0; i < _swapchainImageViews.size(); i++)
    {
        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }
}

void VulkanEngine::destroy_render_targets()
{
    fmt::print("Destroying render targets...\n");
    vkDestroyImageView(_device, _drawImage.imageView, nullptr);
    vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

    vkDestroyImageView(_device, _msaaImage.imageView, nullptr);
    vmaDestroyImage(_allocator, _msaaImage.image, _msaaImage.allocation);

    vkDestroyImageView(_device, _depthImage.imageView, nullptr);
    vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);

    vkDestroyImageView(_device, _tonemapImage.imageView, nullptr);
    vmaDestroyImage(_allocator, _tonemapImage.image, _tonemapImage.allocation);
}

void VulkanEngine::resize_swapchain()
{
    fmt::print("Resizing swapchain...\n");
    // wait for gpu commands to be finished
    vkDeviceWaitIdle(_device);

    destroy_swapchain();
    destroy_render_targets();

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _windowExtent.width = w;
    _windowExtent.height = h;

    create_swapchain(_windowExtent.width, _windowExtent.height);
    create_render_targets();
    update_draw_descriptors();

    resize_requested = false;
}

// some descriptor sets point at images that get recreated when render targets change
void VulkanEngine::update_draw_descriptors()
{
    fmt::print("Updating descriptors...\n");
    {
        DescriptorWriter writer;
        writer.write_image(
            0,
            _drawImage.imageView,
            _defaultSamplerLinear,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        writer.update_set(_device, _tonemapDescriptorSet);
    }
}

void VulkanEngine::init_commands()
{
    fmt::print("Initializing command pools/buffers...\n");
    // info for command pool to allocate command buffers
    // flag lets us reset individial command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // create a command pool and buffer for each frame (2)
    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        // pool
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

        // buffer
        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));
    }

    // immediate submission pool/buffer
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

    _mainDeletionQueue.push_function([=, this]()
        { vkDestroyCommandPool(_device, _immCommandPool, nullptr); });
}

void VulkanEngine::init_sync_structures()
{
    fmt::print("Initializing sync...\n");
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to synchronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++)
    {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));
    }

    // immedaite submission fence
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
    _mainDeletionQueue.push_function([=, this]()
        { vkDestroyFence(_device, _immFence, nullptr); });
}

void VulkanEngine::init_default_data()
{
    fmt::print("Initializing default data ...\n");
    // meshes
    //testMeshes = loadGltfMeshes(this, "assets/basicmesh.glb").value();

    //textures
	//3 default textures, white, grey, black. 1 pixel each
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	_whiteImage = vkutil::upload_image(*this, (void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_greyImage = vkutil::upload_image(*this, (void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0.5));
	_blackImage = vkutil::upload_image(*this, (void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 0.5));
	std::array<uint32_t, 16 *16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) 
    {
		for (int y = 0; y < 16; y++) 
        {
			pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	_errorCheckerboardImage = vkutil::upload_image(*this, pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerLinear);

    sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampl.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampl.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    vkCreateSampler(_device, &sampl, nullptr, &_shadowSampler);

    std::optional<AllocatedImage> skybox = load_hdr_image(this, "assets/test_skybox.hdr").value();
    assert(skybox.has_value());
    _skyboxImage = skybox.value();

	_mainDeletionQueue.push_function([&]()
    {
		vkDestroySampler(_device,_defaultSamplerNearest,nullptr);
		vkDestroySampler(_device,_defaultSamplerLinear,nullptr);
		vkDestroySampler(_device,_shadowSampler,nullptr);

		vkutil::destroy_image(_allocator, _device, _whiteImage);
		vkutil::destroy_image(_allocator, _device, _greyImage);
		vkutil::destroy_image(_allocator, _device, _blackImage);
		vkutil::destroy_image(_allocator, _device, _errorCheckerboardImage);
        vkutil::destroy_image(_allocator, _device, _skyboxImage);
	});
}

void VulkanEngine::init_descriptors()
{
    fmt::print("Initializing descriptor sets...\n");
    
    //Create a global descriptor allocator.
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
    {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}, 
    	{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3}
    };
    globalDescriptorAllocator.init(_device, 10, sizes); //10 sets can be allocated  from this one

    // GLTF PIPELINE: descriptor for per frame resources
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); //ubo (PerFrameData_GPU)
        _perFrameDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    // GLTF PIPELINE: descriptor for the shadow map sampler
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        _shadowDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    _shadowDescriptorSet = globalDescriptorAllocator.allocate(_device, _shadowDescriptorLayout);
    {
        DescriptorWriter writer;

        writer.write_image(
            0,
            _shadowDepthImage.imageView,
            _shadowSampler,
            VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        writer.update_set(_device, _shadowDescriptorSet);
    }

    //TONE MAPPING 
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); // color image before tonemapping

        _tonemapDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    _tonemapDescriptorSet = globalDescriptorAllocator.allocate(_device, _tonemapDescriptorLayout);
    {
        DescriptorWriter writer;

        writer.write_image(0, 
            _drawImage.imageView, 
            _defaultSamplerLinear, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, //read only
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); 
        
        writer.update_set(_device, _tonemapDescriptorSet);
    }

    // SKYBOX
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); //equirectanglar texture

        _skyboxDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    _skyboxDescriptorSet = globalDescriptorAllocator.allocate(_device, _skyboxDescriptorLayout);
    {
        DescriptorWriter writer;

        writer.write_image(0, 
            _skyboxImage.imageView, 
            _defaultSamplerLinear, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, //read only
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); 
        
        writer.update_set(_device, _skyboxDescriptorSet);
    }

    //cleanup
    _mainDeletionQueue.push_function([&]()
    {
        globalDescriptorAllocator.destroy_pools(_device);
		vkDestroyDescriptorSetLayout(_device, _perFrameDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _shadowDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _tonemapDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _skyboxDescriptorLayout, nullptr);
    });

    //make our per frame descriptor allocators. We use these each frame to allocate descriptor sets for the per frame resources.
	for (int i = 0; i < FRAME_OVERLAP; i++) 
    {
		// create a descriptor pool
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = 
        { 
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 }, //3 images
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 }, //3 ssbos
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 }, //3 ubos
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 }, //4 combined
		};

		_frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
		_frames[i]._frameDescriptors.init(_device, 1000, frame_sizes);
	
		_mainDeletionQueue.push_function([&, i]() 
        {
			_frames[i]._frameDescriptors.destroy_pools(_device);
		});
	}
}

void VulkanEngine::init_pipelines()
{
    fmt::print("Initializing pipelines...\n");
    //init_background_pipelines();
    _metalRoughMaterial.build_pipelines(this);
    init_shadow_pipeline();
    init_tonemap_pipeline();
    init_skybox_pipeline();
}

void VulkanEngine::init_shadow_pipeline()
{
    VkShaderModule shadowVert;
    if (!vkutil::load_shader_module("shaders/spirv/shadow_pass.vert.spv", _device, &shadowVert)) 
    {
        fmt::println("Error loading shadow vertex shader");
        return;
    }

    VkDescriptorSetLayout setLayouts[] = { _perFrameDescriptorLayout };

    VkPushConstantRange pushRange{};
    pushRange.offset = 0;
    pushRange.size = sizeof(PerObjectData_GPU);
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    VK_CHECK(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_shadowPipelineLayout));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(shadowVert);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.set_depth_format(_shadowDepthImage.imageFormat);

    // depth-only pass, no call set_color_attachment_format()

    pipelineBuilder._pipelineLayout = _shadowPipelineLayout;

    _shadowPipeline = pipelineBuilder.build_pipeline(_device);

    vkDestroyShaderModule(_device, shadowVert, nullptr);

    _mainDeletionQueue.push_function([=, this]()
    {
        vkDestroyPipeline(_device, _shadowPipeline, nullptr);
        vkDestroyPipelineLayout(_device, _shadowPipelineLayout, nullptr);
    });
}

void VulkanEngine::init_tonemap_pipeline()
{
    VkShaderModule fullscreenVert, tonemapFrag;
    if (!vkutil::load_shader_module("shaders/spirv/fullscreen.vert.spv", _device, &fullscreenVert)) 
    {
        fmt::println("Error loading vertex shader");
        return;
    }
    if (!vkutil::load_shader_module("shaders/spirv/tonemapping.frag.spv", _device, &tonemapFrag)) 
    {
        fmt::println("Error loading shadow vertex shader");
        return;
    }

    VkDescriptorSetLayout setLayouts[] = { _tonemapDescriptorLayout };

    VkPushConstantRange pushRange{};
    pushRange.offset = 0;
    pushRange.size = sizeof(int32_t);
    pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    VK_CHECK(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_tonemapPipelineLayout));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(fullscreenVert, tonemapFrag);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_depthtest();
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_color_attachment_format(_tonemapImage.imageFormat);

    // depth-only pass, no call set_color_attachment_format()

    pipelineBuilder._pipelineLayout = _tonemapPipelineLayout;

    _tonemapPipeline= pipelineBuilder.build_pipeline(_device);

    vkDestroyShaderModule(_device, fullscreenVert, nullptr);
    vkDestroyShaderModule(_device, tonemapFrag, nullptr);

    _mainDeletionQueue.push_function([=, this]()
    {
        vkDestroyPipeline(_device, _tonemapPipeline, nullptr);
        vkDestroyPipelineLayout(_device, _tonemapPipelineLayout, nullptr);
    });

}

void VulkanEngine::init_skybox_pipeline()
{
    VkShaderModule skyboxVert, skyboxFrag;
    if (!vkutil::load_shader_module("shaders/spirv/fullscreen.vert.spv", _device, &skyboxVert)) 
    {
        fmt::println("Error loading vertex shader");
        return;
    }
    if (!vkutil::load_shader_module("shaders/spirv/skybox.frag.spv", _device, &skyboxFrag)) 
    {
        fmt::println("Error loading fragment shader");
        return;
    }

    VkDescriptorSetLayout setLayouts[] = { _perFrameDescriptorLayout, _skyboxDescriptorLayout };

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_skyboxPipelineLayout));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(skyboxVert, skyboxFrag);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling(VK_SAMPLE_COUNT_4_BIT); //because we are rendering in the same pass as draw_geometry
    pipelineBuilder.set_depth_format(_depthImage.imageFormat); // we need this to not have a error because we use the same pass as draw geometry
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_color_attachment_format(_msaaImage.imageFormat);

    pipelineBuilder._pipelineLayout = _skyboxPipelineLayout;

    _skyboxPipeline= pipelineBuilder.build_pipeline(_device);

    vkDestroyShaderModule(_device, skyboxVert, nullptr);
    vkDestroyShaderModule(_device, skyboxFrag, nullptr);

    _mainDeletionQueue.push_function([=, this]()
    {
        vkDestroyPipeline(_device, _skyboxPipeline, nullptr);
        vkDestroyPipelineLayout(_device, _skyboxPipelineLayout, nullptr);
    });
}


void VulkanEngine::init_imgui()
{
    fmt::print("Initializing imgui...\n");
    // 1. Imgui vulkan init requires a descriptor pool.
    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

    // 2: initialize imgui library
    // this initializes the core structures of imgui
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(_window);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _chosenGPU;
    init_info.Device = _device;
    init_info.Queue = _graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    // dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat; // drawing imgui directly on the swapchain

    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();

    // add the destroy the imgui created structures
    _mainDeletionQueue.push_function([=, this]()
                                     {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(_device, imguiPool, nullptr); });
}

void VulkanEngine::init_scene()
{
    // init camera
    mainCamera.velocity = glm::vec3(0.f);
	mainCamera.position = glm::vec3(0, 0, 5);
    mainCamera.pitch = 0;
    mainCamera.yaw = 0;

    // init default scene
    //std::string structurePath = { "assets/sponza/Sponza.gltf" };
    //std::string structurePath = { "assets/main_sponza/NewSponza_Main_glTF_003.gltf" };
    std::string structurePath = { "assets/WaterBottle.glb" };
    //std::string structurePath = { "assets/ABeautifulGame.glb" };
    auto structureFile = loadGltf(this, structurePath);

    assert(structureFile.has_value());

    loadedScenes["structure"] = *structureFile;
}

void VulkanEngine::cleanup()
{
    fmt::print("Cleaning up...\n");
    if (_isInitialized)
    {
        // make sure gpu is done
        vkDeviceWaitIdle(_device);

        loadedScenes.clear();

        // Per frame resources
        for (int i = 0; i < FRAME_OVERLAP; i++)
        {
            // destryoing command pool ddestroys all command buffers assocaited with it.
            // VulkanQueues cant be destroyed, as they are a handle to Vkinstance
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

            // destroy sync objects
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);

            _frames[i]._deletionQueue.flush();
        }

        // for (auto &mesh : testMeshes)
        // {
        //     destroy_buffer(mesh->meshBuffers.indexBuffer);
        //     destroy_buffer(mesh->meshBuffers.vertexBuffer);
        // }

        destroy_render_targets();
        _mainDeletionQueue.flush();

        destroy_swapchain();
        

        vkDestroySurfaceKHR(_instance, _surface, nullptr);

        vkDestroyDevice(_device, nullptr);
        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        vkDestroyInstance(_instance, nullptr);

        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

//clip space test 
static bool is_visible_basic(const RenderObject& obj, const glm::mat4& viewproj)
{
    static const std::array<glm::vec3, 8> corners = 
    {
        glm::vec3{ 1,  1,  1},
        glm::vec3{ 1,  1, -1},
        glm::vec3{ 1, -1,  1},
        glm::vec3{ 1, -1, -1},
        glm::vec3{-1,  1,  1},
        glm::vec3{-1,  1, -1},
        glm::vec3{-1, -1,  1},
        glm::vec3{-1, -1, -1},
    };

    glm::mat4 m = viewproj * obj.transform;

    std::array<glm::vec4, 8> clipCorners;
    for (int i = 0; i < 8; ++i)
    {
        glm::vec3 p = obj.bounds.origin + corners[i] * obj.bounds.extents;
        clipCorners[i] = m * glm::vec4(p, 1.0f);
    }

    auto all_outside = [&](auto pred) -> bool
    {
        for (const glm::vec4& v : clipCorners)
        {
            if (!pred(v)) return false;
        }
        return true;
    };

    //x must be in [-w,w]
    if (all_outside([](const glm::vec4& v) { return v.x < -v.w; })) return false;
    if (all_outside([](const glm::vec4& v) { return v.x >  v.w; })) return false;

    //y must be in [-w,w]
    if (all_outside([](const glm::vec4& v) { return v.y < -v.w; })) return false;
    if (all_outside([](const glm::vec4& v) { return v.y >  v.w; })) return false;

    // z must be in [0,w]
    if (all_outside([](const glm::vec4& v) { return v.z < 0.0f; })) return false;
    if (all_outside([](const glm::vec4& v) { return v.z > v.w; })) return false;

    return true;
}

static bool is_visible_planes(RenderObject& obj, const glm::mat4& viewproj)
{
    return 1;
}

static void set_viewport_scissor(VkCommandBuffer cmd, VkExtent2D extent)
{
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanEngine::draw_skybox(VkCommandBuffer cmd, VkDescriptorSet& perFrameDescriptorSet)
{
    // bind pipeline/descriptor set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 0, 1, &perFrameDescriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 1, 1, &_skyboxDescriptorSet, 0, nullptr);

    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
    //reset stat counters
    stats.drawcall_count = 0;
    stats.triangle_count = 0;
    auto start = std::chrono::system_clock::now();

    std::vector<uint32_t> opaque_draws;
    opaque_draws.reserve(mainDrawContext.OpaqueSurfaces.size());

    for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++) 
    {
        if (is_visible_basic(mainDrawContext.OpaqueSurfaces[i], perFrameDataGPU.viewproj))
        opaque_draws.push_back(i);
    }

    //this can be optimized
    std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& iA, const auto& iB) 
    {
        const RenderObject& A = mainDrawContext.OpaqueSurfaces[iA];
        const RenderObject& B = mainDrawContext.OpaqueSurfaces[iB];
        if (A.material == B.material) 
        {
            return A.indexBuffer < B.indexBuffer;
        }
        else 
        {
            return A.material < B.material;
        }
    });

    // begin a render pass connected to our draw image-----

    // VkRenderingAttachmentInfo describes the attachment we are rendering into for dynamic rendering
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_msaaImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    //msaa image resolves to draw image
    colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    colorAttachment.resolveImageView = _drawImage.imageView;
    colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    

    // VkRenderingInfo is the info for vkCmdBeginRendering. It needs to know the region we are drawing and the attachments we are drawing into.
    VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);

    // start dynamic rendering
    vkCmdBeginRendering(cmd, &renderInfo);

    //set vp/scissor
    set_viewport_scissor(cmd, _drawExtent);

    //CREATE PER-FRAME DESCRIPTOR SET (using layout defined in init)-----------------------------------------

    //allocate a new UBO for the scene data
	AllocatedBuffer gpuSceneDataBuffer = vkutil::create_buffer(_allocator, sizeof(PerFrameData_GPU), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    //write the UBO with our cpu data
	PerFrameData_GPU* sceneUniformData = (PerFrameData_GPU*)gpuSceneDataBuffer.allocation->GetMappedData(); //get cpu pointer to buffer mem
	*sceneUniformData = perFrameDataGPU; //set buffer mem to our cpu side scene data. this is updated in update_scene()

	get_current_frame()._deletionQueue.push_function([=, this]() 
    {
		vkutil::destroy_buffer(_allocator, gpuSceneDataBuffer);
	});

	//create a descriptor set (from layout for per frame data we described in setup)
	VkDescriptorSet perFrameDescriptorSet = get_current_frame()._frameDescriptors.allocate(_device, _perFrameDescriptorLayout);

	DescriptorWriter writer;
    //bind our buffer data to binding 0 of that descriptor set.
    //             binding 0                                              set 0
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(PerFrameData_GPU), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); 
	writer.update_set(_device, perFrameDescriptorSet); 
    //-------------------------------------------------------------------------------------------------------

    //no need for material descriptor, as its been written duing loadgltfs.
    
    draw_skybox(cmd, perFrameDescriptorSet);

    // draw all meshes (RenderObjects)
    // note calling MeshNode::Draw in update() fills mainDrawContext with RenderObjects 
    MaterialPipeline* lastPipeline = nullptr;
    MaterialInstance* lastMaterial = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    auto draw = [&](const RenderObject& r) 
    {
        if (r.material != lastMaterial)
        {
            lastMaterial = r.material;

            if (r.material->pipeline != lastPipeline)
            {    
                lastPipeline = r.material->pipeline;
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,r.material->pipeline->layout, 0, 1,
                    &perFrameDescriptorSet, 0, nullptr);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1,
                    &_shadowDescriptorSet, 0, nullptr);
                
                set_viewport_scissor(cmd, _drawExtent);
            }

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 2, 1,
                &r.material->materialSet, 0, nullptr);
        }

        if (r.indexBuffer != lastIndexBuffer) 
        {
            lastIndexBuffer = r.indexBuffer;
            vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
        
        //bind push constants
        PerObjectData_GPU pushConstants;
        pushConstants.vertexBuffer = r.vertexBufferAddress;
        pushConstants.worldMatrix = r.transform;
        vkCmdPushConstants(cmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PerObjectData_GPU), &pushConstants);

        //draw
        vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);

        stats.drawcall_count++;
        stats.triangle_count += r.indexCount / 3;   
    };

    for (auto& r : opaque_draws) 
    {
        //fmt::print("????\n");
        draw(mainDrawContext.OpaqueSurfaces[r]);
    }

    for (auto& r : mainDrawContext.TransparentSurfaces) 
    {
        //fmt::print("!!!!!\n");
        draw(r);
    }
    vkCmdEndRendering(cmd);

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.mesh_draw_time = elapsed.count() / 1000.f;
}

void VulkanEngine::draw_shadow_map(VkCommandBuffer cmd)
{
    VkExtent2D shadowExtent = 
    {
    _shadowDepthImage.imageExtent.width,
    _shadowDepthImage.imageExtent.height
    };

    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_shadowDepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 1.0f);

    // VkRenderingInfo is the info for vkCmdBeginRendering. It needs to know the region we are drawing and the attachments we are drawing into.
    VkRenderingInfo renderInfo = vkinit::rendering_info(shadowExtent, nullptr, &depthAttachment);

    // start dynamic rendering
    vkCmdBeginRendering(cmd, &renderInfo);

    set_viewport_scissor(cmd, shadowExtent);

    // create/write perfrmame ds
	AllocatedBuffer gpuSceneDataBuffer = vkutil::create_buffer(_allocator, sizeof(PerFrameData_GPU), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	PerFrameData_GPU* sceneUniformData = (PerFrameData_GPU*)gpuSceneDataBuffer.allocation->GetMappedData(); //get cpu pointer to buffer mem
	*sceneUniformData = perFrameDataGPU; //set buffer mem to our cpu side scene data. this is updated in update_scene()
	get_current_frame()._deletionQueue.push_function([=, this]() 
    {
		vkutil::destroy_buffer(_allocator, gpuSceneDataBuffer);
	});

	VkDescriptorSet perFrameDescriptorSet = get_current_frame()._frameDescriptors.allocate(_device, _perFrameDescriptorLayout);
	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(PerFrameData_GPU), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); 
	writer.update_set(_device, perFrameDescriptorSet); 


    //bind pipeline and ds
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipelineLayout, 0, 1, &perFrameDescriptorSet, 0, nullptr);


    for (RenderObject& r : mainDrawContext.OpaqueSurfaces)
    {
        //bind push constants
        PerObjectData_GPU pushConstants;
        pushConstants.vertexBuffer = r.vertexBufferAddress;
        pushConstants.worldMatrix = r.transform;
        vkCmdPushConstants(cmd, _shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PerObjectData_GPU), &pushConstants);

        vkCmdBindIndexBuffer(cmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        //draw
        vkCmdDrawIndexed(cmd, r.indexCount, 1, r.firstIndex, 0, 0);
    }

    vkCmdEndRendering(cmd);

}

void VulkanEngine::draw_tonemap(VkCommandBuffer cmd)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_tonemapImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // VkRenderingInfo is the info for vkCmdBeginRendering. It needs to know the region we are drawing and the attachments we are drawing into.
    VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, nullptr);

    // start dynamic rendering
    vkCmdBeginRendering(cmd, &renderInfo);

    //set vp/scissor
    set_viewport_scissor(cmd, _drawExtent);

    // bind pipeline/descriptor set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _tonemapPipeline);
    int32_t index = cvarTonemapIndex.Get();
    vkCmdPushConstants(cmd, _tonemapPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int32_t), &index);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _tonemapPipelineLayout, 0, 1, &_tonemapDescriptorSet, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw()
{
    update_scene();
    // CPU<->GPU SYNC::: wait for gpu to finish rendering last frame (wait max 1 second)
    // note, renderfence starts signalled on frame 0.
    VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));

    // cleanup per frame resources
    get_current_frame()._deletionQueue.flush(); 
    get_current_frame()._frameDescriptors.clear_pools(_device);

    // request image from the swap chain
    uint32_t swapchainImageIndex;
    // aquireNextImageKHR will request for the image index from swapchain, and if it doesnt have any image it can use, it will block for
    //  a max of 1 second.
    //  We signal swapChainSemaphore, so we know we can render into it later.
    //
    //Note that vkAcquireNextImageKHR expects swapchain size to be compatible with window size. If not it will return e.
    VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
    
    if (e == VK_ERROR_OUT_OF_DATE_KHR) {resize_requested = true; return; } //swapchainn no longer usable
    if (e == VK_SUBOPTIMAL_KHR) resize_requested = true; //swapchain can still be used

    //_drawImage.imageExtent = maximum canvas you actually own
    //_swapchainExtent = window size you want to present to
    //_drawExtent = region you choose to render this frame
    //basically the min is there so the draw image extent never goes above the drawImage's actual size.
    //(renderscale <= 1)
    float renderScale = cvarRenderScale.GetFloat();

    _drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * renderScale;
    _drawExtent.width = std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * renderScale;

    // only reset the renderfence if vkAcquireNextImageKHR succeeded. Or else the next iter will wait forever.
    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

    // START RENDERING COMMANDS---------------------------------------------------------
    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer; // get this frame's dedicated command buffer

    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // begin the command buffer recording. Default indo besides hint to tell vulkan we will use the cmd buffer once.
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // skybox
    vkutil::transition_image(cmd, _msaaImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    //vkutil::transition_image(cmd, _skyboxImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT); //done when image is created

    //draw shadow map ---------------
    vkutil::transition_image(cmd, _shadowDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    draw_shadow_map(cmd);

    //read only 
    vkutil::transition_image(cmd, _shadowDepthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    // transution from general -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
    // Note that this is the VkRenderingAttachmentInfo we defined we would be writing into in our VkRenderingInfo (dynamic rendering info)
    //vkutil::transition_image(cmd, _msaaImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

    draw_geometry(cmd); // trinagle

    //run tonemapping pass
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vkutil::transition_image(cmd, _tonemapImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    draw_tonemap(cmd);

    // transition the draw image and the swapchain imagex states so we can blit the image into the swapchain.
    vkutil::transition_image(cmd, _tonemapImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_ASPECT_COLOR_BIT );
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    // blit our draw image onto swapchain
    vkutil::copy_image_to_image(cmd, _tonemapImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

    // set swapchain image layout to Attachment Optimal so we can draw GUI it
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    // draw imgui into the swapchain image
    draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

    // set swapchain image layout to Present so we can show it on the screen
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);

    // finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // END COMMAND RECORDING ----------------------------------------------------------------------

    // time to submit the VKQueue. We want to wait on swapchainSemaphore, as it is signalled when the swapchain is ready
    // we will signal to the renderSemapgore, to sighnal that rendering has finished.
    //  note that vkAcquireNextImageKHR signals the swapchain semaphore.
    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd); // default
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo); // default

    // submit command buffer to the queue and execute it.
    //  _renderFence will block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

    // present to screen
    // we can only present when the render semaphore is singalled. (when our draw commands in the buffer is processed)
    // GPU wait.
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapchainImageIndex;

    //Note that vkQueuePresentKHR expects swapchain size to be compatible with window size. If not it will return error/suboptimal.
    VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ) resize_requested = true;

    _frameNumber++;
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    // no clear value
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // just a single color attachment that points to our swapchain image
    VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr); // default, no depth

    // dynamic rendering
    vkCmdBeginRendering(cmd, &renderInfo); // begin render pass

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void VulkanEngine::update_scene()
{
    auto start = std::chrono::system_clock::now();

	mainDrawContext.OpaqueSurfaces.clear();
    mainDrawContext.TransparentSurfaces.clear();

    loadedScenes["structure"]->Draw(glm::mat4{ 1.f }, mainDrawContext);

    mainCamera.update();

    glm::mat4 view = mainCamera.getViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 10000.f, 0.1f);
    projection[1][1] *= -1;

    //update per frame data
    perFrameDataGPU.view = view;
    perFrameDataGPU.proj = projection;
    perFrameDataGPU.viewproj = projection * view;
	perFrameDataGPU.ambientColor = glm::vec4(cvarAmbient.Get());
	perFrameDataGPU.sunlightColor = glm::vec4(1.f);
	perFrameDataGPU.sunlightDirection = glm::vec4(cvarSunDir.Get(), cvarSunPower.Get());
    perFrameDataGPU.camPos = glm::vec4(mainCamera.position, 1.0f);
    perFrameDataGPU.lightViewProj = get_sun_matrix(); 

    auto end = std::chrono::system_clock::now();

    //convert to microseconds (integer), and then come back to miliseconds
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.scene_update_time = elapsed.count() / 1000.f;
}

glm::mat4 VulkanEngine::get_sun_matrix()
{
    glm::vec3 lightDir = glm::normalize(cvarSunDir.Get()); // same source as sunlightDirection.xyz

    //const float shadowDistance = 80.0f;
    const float orthoHalfSize  = 15.0f;
    const float nearPlane      = 0.1f;
    const float farPlane       = 150.0f;

    glm::vec3 camForward = glm::normalize(glm::vec3(mainCamera.getRotationMatrix() * glm::vec4(0.f, 0.f, -1.f, 0.f)));
    glm::vec3 shadowCenter = {0,0,0}; //just an estimate

    glm::vec3 lightPos = shadowCenter - lightDir * 25.0f;

    glm::vec3 up = (std::abs(lightDir.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    glm::mat4 lightView = glm::lookAt(lightPos, shadowCenter, up);
    glm::mat4 lightProj = glm::ortho(-orthoHalfSize, orthoHalfSize, -orthoHalfSize, orthoHalfSize, nearPlane, farPlane);

    lightProj[1][1] *= -1.0f;
    return lightProj * lightView; 
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit)
    {
        auto start = std::chrono::system_clock::now();

        // Handle events on queue
        while (SDL_PollEvent(&e) != 0)
        {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT)
            {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
                {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED)
                {
                    stop_rendering = false;
                }
            }

            // send SDL event to imgui for handling
            mainCamera.processSDLEvent(e);
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        if (resize_requested)
        {
            resize_swapchain();
        }

        // do not draw if we are minimized
        if (stop_rendering)
        {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Stats");

        ImGui::Text("fps %d", stats.fps);
        ImGui::Text("frametime %f ms", stats.frametime);
        ImGui::Text("draw time %f ms", stats.mesh_draw_time);
        ImGui::Text("update time %f ms", stats.scene_update_time);
        ImGui::Text("triangles %i", stats.triangle_count);
        ImGui::Text("draws %i", stats.drawcall_count);
        ImGui::End();

        if (ImGui::Begin("CVars"))
        {
            CVarSystem::Get()->DrawImguiEditor();
        }
        ImGui::End();

        // make imgui calculate internal draw structures
        ImGui::Render();

        // our draw
        draw();

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        stats.frametime = elapsed.count() / 1000.f;
        if (_frameNumber % 30 == 0) stats.fps = static_cast<int>(1000.0f / stats.frametime);

    }
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function)
{
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr); // no need for semaphores, we are not syncing with swapchain

    // submit command buffer to the queue and execute it.
    //  _immFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}


//legacy
// void VulkanEngine::init_background_pipelines()
// {
//     fmt::print("Initializing BG pipelines...\n"); // “Shaders in this pipeline will look for resources in set N, binding M with these types.”

//     //--------1. describe the data layout with out descriptor table to our pipeline -------------
//     VkPipelineLayoutCreateInfo computeLayout{};
//     computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
//     computeLayout.pNext = nullptr;
//     computeLayout.pSetLayouts = &_drawImageDescriptorLayout; // array of descriptor set layouts to use for this pipeline
//     computeLayout.setLayoutCount = 1;

//     VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_backgroundPipelineLayout));

//     //--------2. describe info to connect the shader to the pipeline --------------------------
//     VkShaderModule skyShader;
//     if (!vkutil::load_shader_module("shaders/spirv/sky.comp.spv", _device, &skyShader))
//     {
//         fmt::print("Error when building the compute shader \n");
//     }

//     VkPipelineShaderStageCreateInfo stageinfo{};
//     stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//     stageinfo.pNext = nullptr;
//     stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
//     stageinfo.module = skyShader;
//     stageinfo.pName = "main";

//     //--------3. create the compute pipeline with info from 1 and 2.------------------------
//     VkComputePipelineCreateInfo computePipelineCreateInfo{};
//     computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
//     computePipelineCreateInfo.pNext = nullptr;
//     computePipelineCreateInfo.layout = _backgroundPipelineLayout; // 1
//     computePipelineCreateInfo.stage = stageinfo;                // 2

//     VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_backgroundPipeline));

//     //--------4. cleanup----------------------------------------
//     vkDestroyShaderModule(_device, skyShader, nullptr);
//     _mainDeletionQueue.push_function([=, this]()
//     {
//         vkDestroyPipelineLayout(_device, _backgroundPipelineLayout, nullptr);
//         vkDestroyPipeline(_device, _backgroundPipeline, nullptr); 
//     });
// }

// void VulkanEngine::draw_background(VkCommandBuffer cmd)
// {
//     if (_backgroundPipeline == VK_NULL_HANDLE)
//     {
//         return;
//     }

//     VkClearColorValue clearValue;
//     float flash = std::abs(std::sin(_frameNumber / 120.f));
//     clearValue = {{0.0f, 0.0f, flash, 1.0f}};

//     // clear image
//     VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT); // clear color buffer
//     vkCmdClearColorImage(cmd, _msaaImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

//     // bind the  compute pipeline
//     vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _backgroundPipeline);

//     // bind the descriptor set containing the draw image for the compute pipeline. Also has push constant layout.
//     vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _backgroundPipelineLayout, 0, 1, &_drawImageDescriptorSet, 0, nullptr);

//     // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
//     vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
// }
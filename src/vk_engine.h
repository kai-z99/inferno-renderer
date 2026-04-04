// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_descriptors.h>
#include <vk_materials.h>
#include <vk_scene.h>
#include <vk_loader.h> 
#include <camera.h>


struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& func)
	{
		deletors.push_back(func);
	}

	void flush()
	{
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
		{
			(*it)();
		}

		deletors.clear();
	}
};

struct FrameData 
{
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;

	DeletionQueue _deletionQueue;
	DescriptorAllocatorGrowable _frameDescriptors;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine 
{
public:
	static VulkanEngine& Get();

	//im going to assume that equi -> hdr stays the same format.
	static constexpr VkFormat kEnvironmentMapFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

	// lifecycle
	void init();
	void cleanup();
	void draw();
	void run();

	// loader-facing accessors
	VkDevice device() const { return _device; }
	VmaAllocator allocator() const { return _allocator; }
	GLTFMetallic_Roughness& material_system() { return _metalRoughMaterial; }
	const AllocatedImage& white_image() const { return _whiteImage; }
	const AllocatedImage& black_image() const { return _blackImage; }
	const AllocatedImage& error_checkerboard_image() const { return _errorCheckerboardImage; }
	VkSampler default_sampler_linear() const { return _defaultSamplerLinear; }
	
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

private:
	friend struct GLTFMetallic_Roughness;

	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; }

	// initialization
	void init_window();
	void init_vulkan();
	void init_swapchain();
	void init_render_targets();
	void init_commands();
	void init_sync_structures();
	void init_descriptor_layouts();
	void init_static_descriptor_sets();
	void init_allocators();
	
	void init_imgui();
	void init_default_data();
	void init_scene();

	//pipelines
	void init_shadow_pipeline();
	void init_tonemap_pipeline();
	void init_skybox_pipeline();
	void init_equi_to_cube_pipeline();
	void init_irradiance_pipeline();
	void init_pipelines();

	AllocatedImage create_cubemap_from_equi(const AllocatedImage& hdrEqui, uint32_t cubeSize);
	AllocatedImage create_irradiance_map(const AllocatedImage& cubemap, uint32_t cubeSize);

	void create_swapchain(uint32_t width, uint32_t height);
	void create_render_targets(bool onWindowResize = false);
	void destroy_swapchain();
	void destroy_render_targets();
	void resize_swapchain_and_render_targets();
	void update_draw_descriptors();

	void draw_scene(VkCommandBuffer cmd);
	void draw_skybox(VkCommandBuffer cmd, VkDescriptorSet& perFrameDescriptorSet);
	void draw_geometry(VkCommandBuffer cmd, VkDescriptorSet& perFrameDescriptorSet);
	void draw_shadow_map(VkCommandBuffer cmd);
	void draw_tonemap(VkCommandBuffer cmd);
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

	// scene
	void update_scene();
	glm::mat4 get_sun_matrix();

	// application state
	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	bool stop_rendering{ false };
	bool resize_requested{ false };
	VkExtent2D _windowExtent{ 1920, 1080 };
	struct SDL_Window* _window{ nullptr };

	// core Vulkan handles
	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;
	VkSurfaceKHR _surface;
	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;
	VmaAllocator _allocator;

	// frame resources
	FrameData _frames[FRAME_OVERLAP];
	DeletionQueue _mainDeletionQueue;

	// immediate submission
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	// swapchain and render targets
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;
	AllocatedImage _msaaImage;
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	AllocatedImage _shadowDepthImage;
	AllocatedImage _tonemapImage;
	AllocatedImage _skyboxImage;
	AllocatedImage _environmentCubemap;
	AllocatedImage _irradianceCubemap;
	VkExtent2D _drawExtent;
	float renderScale = 1.f;
	uint32_t _shadowMapResolution = 1024;
	uint32_t _environmentMapResolution = 1024;
	uint32_t _irradianceMapResolution = 64;

	// descriptors
	DescriptorAllocatorGrowable globalDescriptorAllocator;
	VkDescriptorSet _shadowDescriptorSet;
	VkDescriptorSet _skyboxDescriptorSet;
	VkDescriptorSet _tonemapDescriptorSet;
	VkDescriptorSet _equiToCubeDescriptorSet;
	VkDescriptorSet _irradianceDescriptorSet;
	VkDescriptorSetLayout _perFrameDescriptorLayout;
	VkDescriptorSetLayout _shadowDescriptorLayout;
	VkDescriptorSetLayout _skyboxDescriptorLayout;
	VkDescriptorSetLayout _tonemapDescriptorLayout;
	VkDescriptorSetLayout _equiToCubeDescriptorLayout;
	VkDescriptorSetLayout _irradianceDescriptorLayout;

	// pipelines and materials
	VkPipeline _skyboxPipeline;	
	VkPipeline _shadowPipeline;
	VkPipeline _tonemapPipeline;
	VkPipeline _equiToCubePipeline;
	VkPipeline _irradiancePipeline;
	VkPipelineLayout _skyboxPipelineLayout;
	VkPipelineLayout _shadowPipelineLayout;
	VkPipelineLayout _tonemapPipelineLayout;
	VkPipelineLayout _equiToCubePipelineLayout;
	VkPipelineLayout _irradiancePipelineLayout;

	GLTFMetallic_Roughness _metalRoughMaterial;

	// scene state
	PerFrameData_GPU perFrameDataGPU;
	Camera mainCamera;
	DrawContext mainDrawContext;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;
	std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;
	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	// default resources
	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;
	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;
	VkSampler _shadowSampler;

	// statistics
	EngineStats stats;
};

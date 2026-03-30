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
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_pipelines();
	void init_imgui();
	void init_default_data();
	void init_scene();
	//pipelines
	void init_background_pipelines();
	void init_shadow_pipeline();

	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();
	void resize_swapchain();

	void draw_background(VkCommandBuffer cmd);
	void draw_geometry(VkCommandBuffer cmd);
	void draw_shadow_map(VkCommandBuffer cmd);
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
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	AllocatedImage _shadowDepthImage;
	VkExtent2D _drawExtent;
	float renderScale = 1.f;
	uint32_t _shadowMapResolution = 1024;

	// descriptors
	DescriptorAllocatorGrowable globalDescriptorAllocator;
	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;
	VkDescriptorSetLayout _perFrameDescriptorLayout;

	// pipelines and materials
	VkPipelineLayout _backgroundPipelineLayout;
	VkPipeline _backgroundPipeline{ VK_NULL_HANDLE };
	VkPipeline _shadowPipeline;
	VkPipelineLayout _shadowPipelineLayout;
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

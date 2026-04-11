#pragma once

#include <vk_types.h>
#include <vk_descriptors.h>

class VulkanEngine;

struct GLTFMetallic_Roughness
{
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	// to be written into UBO
	struct MaterialConstants
	{
		glm::vec4 colorFactors;
		glm::vec4 metal_rough_factors;
		glm::vec4 diffuse_transmission_factors; //color: xyz, factor: w
		glm::vec4 clearcoat_factors;            // x: clearcoatFactor, y: clearcoatRoughnessFactor
		glm::vec4 iridescence_factors;          // x: factor, y: IOR, z: thicknessMin(nm), w: thicknessMax(nm)
		// padding, we need it anyway for uniform buffers
		glm::vec4 extra[11];
	};

	// for descriptor set
	struct MaterialResources
	{
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		AllocatedImage normalImage;
		VkSampler normalSampler;
		AllocatedImage emissiveImage;
		VkSampler emissiveSampler;
		AllocatedImage aoImage;
		VkSampler aoSampler;
		AllocatedImage diffuseTransmissionFactorImage;
		VkSampler diffuseTransmissionFactorSampler;
		AllocatedImage diffuseTransmissionColorImage;
		VkSampler diffuseTransmissionColorSampler;
		AllocatedImage clearcoatImage;
		VkSampler clearcoatSampler;
		AllocatedImage clearcoatRoughnessImage;
		VkSampler clearcoatRoughnessSampler;
		AllocatedImage iridescenceImage;
		VkSampler iridescenceSampler;
		AllocatedImage iridescenceThicknessImage;
		VkSampler iridescenceThicknessSampler;

		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	DescriptorWriter writer;

	void build_pipelines(VulkanEngine* engine);
	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

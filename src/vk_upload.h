#pragma once

#include <vk_types.h>

class VulkanEngine;

namespace vkutil
{
	GPUMeshBuffers upload_mesh(VulkanEngine& engine, std::span<uint32_t> indices, std::span<Vertex> vertices);
	AllocatedImage upload_image(VulkanEngine& engine, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false, size_t byteSize = 0);
}

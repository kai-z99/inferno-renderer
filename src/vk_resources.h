#pragma once

#include <vk_types.h>

namespace vkutil
{
	AllocatedBuffer create_buffer(VmaAllocator allocator, size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(VmaAllocator allocator, const AllocatedBuffer& buffer);

	AllocatedImage create_image(VmaAllocator allocator, VkDevice device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroy_image(VmaAllocator allocator, VkDevice device, const AllocatedImage& img);
}

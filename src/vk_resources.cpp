#include "vk_resources.h"

#include "vk_initializers.h"

namespace vkutil
{
	AllocatedBuffer create_buffer(VmaAllocator allocator, size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
	{
		VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.pNext = nullptr;
		bufferInfo.size = allocSize;
		bufferInfo.usage = usage;

		VmaAllocationCreateInfo vmaallocInfo = {};
		vmaallocInfo.usage = memoryUsage;
		vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

		AllocatedBuffer newBuffer;
		VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

		return newBuffer;
	}

	void destroy_buffer(VmaAllocator allocator, const AllocatedBuffer& buffer)
	{
		vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
	}

	AllocatedImage create_image(VmaAllocator allocator, VkDevice device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
	{
		AllocatedImage newImage;
		newImage.imageFormat = format;
		newImage.imageExtent = size;

		VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
		if (mipmapped)
		{
			img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
		}

		VmaAllocationCreateInfo allocinfo = {};
		allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK(vmaCreateImage(allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

		VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
		if (format == VK_FORMAT_D32_SFLOAT)
		{
			aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
		}

		VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
		view_info.subresourceRange.levelCount = img_info.mipLevels;

		VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &newImage.imageView));

		return newImage;
	}

	void destroy_image(VmaAllocator allocator, VkDevice device, const AllocatedImage& img)
	{
		vkDestroyImageView(device, img.imageView, nullptr);
		vmaDestroyImage(allocator, img.image, img.allocation);
	}
}

#include "vk_upload.h"

#include "vk_engine.h"
#include "vk_images.h"
#include "vk_resources.h"

#include <cstring>

namespace vkutil
{
	GPUMeshBuffers upload_mesh(VulkanEngine& engine, std::span<uint32_t> indices, std::span<Vertex> vertices)
	{
		const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
		const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

		GPUMeshBuffers newSurface;

		newSurface.vertexBuffer = create_buffer(
			engine.allocator(),
			vertexBufferSize,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
				VK_BUFFER_USAGE_TRANSFER_DST_BIT |
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY);

		VkBufferDeviceAddressInfo deviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newSurface.vertexBuffer.buffer };
		newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(engine.device(), &deviceAddressInfo);

		newSurface.indexBuffer = create_buffer(
			engine.allocator(),
			indexBufferSize,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
				VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY);

		AllocatedBuffer staging = create_buffer(
			engine.allocator(),
			vertexBufferSize + indexBufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY);

		void* data = staging.info.pMappedData;
		memcpy(data, vertices.data(), vertexBufferSize);
		memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

		engine.immediate_submit([&](VkCommandBuffer cmd)
		{
			VkBufferCopy vertexCopy{ 0 };
			vertexCopy.dstOffset = 0;
			vertexCopy.srcOffset = 0;
			vertexCopy.size = vertexBufferSize;
			vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

			VkBufferCopy indexCopy{ 0 };
			indexCopy.dstOffset = 0;
			indexCopy.srcOffset = vertexBufferSize;
			indexCopy.size = indexBufferSize;
			vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
		});

		destroy_buffer(engine.allocator(), staging);

		return newSurface;
	}

	AllocatedImage upload_image(VulkanEngine& engine, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, size_t byteSize)
	{
		if (byteSize == 0) //default to rgba8
		{
			byteSize =
				static_cast<size_t>(size.width) *
				static_cast<size_t>(size.height) *
				static_cast<size_t>(size.depth) *
				4;
		}
		
		AllocatedBuffer uploadbuffer = create_buffer(engine.allocator(), byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		memcpy(uploadbuffer.info.pMappedData, data, byteSize);

		AllocatedImage new_image = create_image(
			engine.allocator(),
			engine.device(),
			size,
			format,
			usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			mipmapped);

		engine.immediate_submit([&](VkCommandBuffer cmd)
		{
			vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

			VkBufferImageCopy copyRegion = {};
			copyRegion.bufferOffset = 0;
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;
			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.mipLevel = 0;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageExtent = size;

			vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

			if (mipmapped)
			{
				vkutil::generate_mipmaps(cmd, new_image.image, VkExtent2D{ new_image.imageExtent.width, new_image.imageExtent.height });
			}
			else
			{
				vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			}
		});

		destroy_buffer(engine.allocator(), uploadbuffer);

		return new_image;
	}
}

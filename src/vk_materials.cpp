#include "vk_materials.h"

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

void GLTFMetallic_Roughness::build_pipelines(VulkanEngine* engine)
{
	VkShaderModule meshFragShader;
	if (!vkutil::load_shader_module("shaders/spirv/mesh.frag.spv", engine->_device, &meshFragShader))
	{
		fmt::println("Error when building the triangle fragment shader module\n");
	}

	VkShaderModule meshVertexShader;
	if (!vkutil::load_shader_module("shaders/spirv/mesh.vert.spv", engine->_device, &meshVertexShader))
	{
		fmt::println("Error when building the triangle vertex shader module\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(PerObjectData_GPU);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	//layout that contains material info
	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);	     //constants
	layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); //albedo
	layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); //metalrough
	layoutBuilder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); //normal
	materialLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayout layouts[] = { engine->_perFrameDescriptorLayout, engine->_shadowDescriptorLayout, materialLayout };

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 3;
	mesh_layout_info.pSetLayouts = layouts;
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;
	VkPipelineLayout meshPipelineLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->_device, &mesh_layout_info, nullptr, &meshPipelineLayout));

	opaquePipeline.layout = meshPipelineLayout;
	transparentPipeline.layout = meshPipelineLayout;

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.set_multisampling(VK_SAMPLE_COUNT_4_BIT);
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	pipelineBuilder.set_color_attachment_format(engine->_drawImage.imageFormat);
	pipelineBuilder.set_depth_format(engine->_depthImage.imageFormat);
	pipelineBuilder._pipelineLayout = meshPipelineLayout;

	opaquePipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	pipelineBuilder.enable_blending_additive();
	pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	transparentPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

	vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
	vkDestroyShaderModule(engine->_device, meshVertexShader, nullptr);

	engine->_mainDeletionQueue.push_function([=, this]()
	{
		vkDestroyPipelineLayout(engine->_device, meshPipelineLayout, nullptr);
		vkDestroyPipeline(engine->_device, transparentPipeline.pipeline, nullptr);
		vkDestroyPipeline(engine->_device, opaquePipeline.pipeline, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, materialLayout, nullptr);
	});
}

void GLTFMetallic_Roughness::clear_resources(VkDevice device)
{
}

MaterialInstance GLTFMetallic_Roughness::write_material(
	VkDevice device,
	MaterialPass pass,
	const MaterialResources& resources,
	DescriptorAllocatorGrowable& descriptorAllocator)
{
	MaterialInstance matData;

	matData.passType = pass;

	if (pass == MaterialPass::Transparent)
	{
		matData.pipeline = &transparentPipeline;
	}
	else
	{
		matData.pipeline = &opaquePipeline;
	}

	matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

	writer.clear();
	writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(3, resources.normalImage.imageView, resources.normalSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	writer.update_set(device, matData.materialSet);

	return matData;
}

#include "vertex.h"
#include <stdint.h>
#include <sys/types.h>
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <helpers/helpers.h>
#include <vulkan/vulkan_core.h>

#include "vksetup.h"

static VkPresentModeKHR
get_presentmode(const struct swapchain_details *swapchain_details);
static VkSurfaceFormatKHR get_format(const struct swapchain_details *swapchain_details);

static VkExtent2D get_swapextent(const struct swapchain_details *swapchain_detail);

static uint32_t get_images_n(const struct swapchain_details *swapchain_details);

static int first_missing_extension(uint32_t extensions_n,
				   const char *const *extensions,
				   uint32_t properties_n,
				   const VkExtensionProperties *properties);
static int is_device_suitable(struct Graphics *graphics);
static void get_suitable_device(struct Graphics *graphics, uint32_t devices_n,
				const VkPhysicalDevice *devices);

static int
rate_physical_device(const VkPhysicalDeviceProperties *deviceProperties,
		     const VkPhysicalDeviceFeatures *deviceFeatures);

static int check_extensions_support(uint32_t properties_n,
				    const VkExtensionProperties *properties,
				    uint32_t extensions_n,
				    const char *const *extensions);

static int check_layers_support(uint32_t layers_n, const char *const *layers);
static int check_device_extensions_support(VkPhysicalDevice device);
static int check_instance_extensions_support(uint32_t extensions_n,
					     const char *const *extensions);

static const char *const *get_layers(uint32_t *layers_n);
static const char *const *get_device_exttensions(uint32_t *extensions_n);

static void destroy_swapchain(struct Graphics *graphics);

static VkResult init_swapchain(struct Graphics *graphics);
static VkResult init_imageviews(struct Graphics *graphics);
static VkResult init_framebufers(struct Graphics *graphics);

static int find_memory_type(struct Graphics *graphics, uint32_t filter,
				 VkMemoryPropertyFlags properties)
{

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(graphics->physicalDevice,
					    &memProperties);

	for(int i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((filter & (1 << i)) &&
		    ((memProperties.memoryTypes[i].propertyFlags &
		      properties) == properties))
			return i;
	}

	return -1;
}

void destroy_vertexbuffer(struct Graphics *graphics)
{
	vkDestroyBuffer(graphics->device, graphics->vertexbuffer, 0);
	vkFreeMemory(graphics->device, graphics->vertex_buffer_memory, 0);
}

int create_vertexbuffer(struct Graphics *graphics)
{
	graphics->vertices = get_vertices(&graphics->vertices_n);

	pdebug("creating vertices: %d vertices", graphics->vertices_n);

	VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = (vertex_size() * graphics->vertices_n),
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VkResult res = vkCreateBuffer(graphics->device, &bufferInfo, 0,
				      &graphics->vertexbuffer);

	if(res != VK_SUCCESS)
		goto buffer_create_error;

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(graphics->device, graphics->vertexbuffer,
				      &memRequirements);

	int mem_type =
		find_memory_type(graphics, memRequirements.memoryTypeBits,
				 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
					 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if(mem_type == -1)
		goto buffer_alloc_error;

	VkMemoryAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = mem_type
	};

	res = vkAllocateMemory(graphics->device, &allocInfo, 0,
			       &graphics->vertex_buffer_memory);

	if(res != VK_SUCCESS)
		goto buffer_alloc_error;

	vkBindBufferMemory(graphics->device, graphics->vertexbuffer,
			   graphics->vertex_buffer_memory, 0);

	void *data;

	vkMapMemory(graphics->device, graphics->vertex_buffer_memory, 0,
		    bufferInfo.size, 0, &data);
	memcpy(data, graphics->vertices, bufferInfo.size);
	vkUnmapMemory(graphics->device, graphics->vertex_buffer_memory);

	return 0;
buffer_alloc_error:
	vkDestroyBuffer(graphics->device, graphics->vertexbuffer, 0);
buffer_create_error:
	return -1;
}
static void destroy_swapchain(struct Graphics *graphics)
{
	for (int i = 0; i < graphics->framebuffers_n; i++) {
		vkDestroyFramebuffer(graphics->device, graphics->framebuffers[i], 0);
	}


	for(int i = 0; i < graphics->imageviews_n; i++) {
		vkDestroyImageView(graphics->device, graphics->imageviews[i], 0);
	}

	vkDestroySwapchainKHR(graphics->device, graphics->swapchain, 0);

}

int recreate_swapchain(struct Graphics *graphics)
{
	vkDeviceWaitIdle(graphics->device);
	
	destroy_swapchain(graphics);

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		graphics->physicalDevice, graphics->surface,
		&graphics->swapchain_details.capabilities);

	VkResult res = init_swapchain(graphics);

	if(res != VK_SUCCESS)
		pdebug("swapchain init error");

	uint32_t images_n;

	vkGetSwapchainImagesKHR(graphics->device, graphics->swapchain,
				&images_n, 0);

	if(!images_n)
		goto images_init_error;

	if(images_n != graphics->images_n) {
		
		free(graphics->images);	
		free(graphics->imageviews);
		free(graphics->framebuffers);

		graphics->images_n = images_n;
		graphics->images = malloc(sizeof(VkImage) * graphics->images_n);
		
		if(!graphics->images)
			goto images_malloc_error;

		graphics->imageviews_n = images_n;
		graphics->imageviews =
			malloc(sizeof(VkImageView) * graphics->imageviews_n);

		if(!graphics->imageviews)
			goto imageviews_malloc_error;

		graphics->framebuffers_n = images_n;
		graphics->framebuffers = malloc(sizeof(VkFramebuffer) *
						graphics->framebuffers_n);

		if(!graphics->framebuffers)
			goto framebuffers_malloc_error;
	}


	res = vkGetSwapchainImagesKHR(graphics->device, graphics->swapchain,
				&graphics->images_n, graphics->images);

	if(res != VK_SUCCESS) {
		pdebug("images init error");
		goto images_init_error;
	}

	res = init_imageviews(graphics);

	if(res != VK_SUCCESS) {
		pdebug("images init error");
		goto imageviews_init_error;
	}
	res = init_framebufers(graphics);

	if(res != VK_SUCCESS)
		goto framebuffers_init_error;
	
	return 0;

framebuffers_init_error:
imageviews_init_error:
	for(int i = 0; i < graphics->imageviews_n; i++) {
		vkDestroyImageView(graphics->device, graphics->imageviews[i], 0);
	}

images_init_error:
framebuffers_malloc_error:
	free(graphics->framebuffers);
imageviews_malloc_error:
	free(graphics->imageviews);
images_malloc_error:
	vkDestroySwapchainKHR(graphics->device, graphics->swapchain, 0);
	free(graphics->images);

	graphics->images_n = 0;
	graphics->framebuffers_n = 0;
	graphics->imageviews_n = 0;

	pdebug("recreation error");
		
	return -1;
}



int draw_frame(struct Graphics *graphics)
{
	vkWaitForFences(graphics->device, 1,
			graphics->inflight_fences + graphics->current_frame,
			VK_TRUE, UINT64_MAX);
	if (graphics->flags & graphics_window_resized_flag) {
		graphics->flags &= ~graphics_window_resized_flag;
		goto swapchain_out_of_date;
	}
	uint32_t image_i;

	VkResult res = vkAcquireNextImageKHR(
		graphics->device, graphics->swapchain, UINT64_MAX,
		graphics->image_available_semaphores[graphics->current_frame],
		VK_NULL_HANDLE, &image_i);

	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		goto swapchain_out_of_date;
	}

	if(res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
		return -1;
	}

	vkResetFences(graphics->device, 1,
		      graphics->inflight_fences + graphics->current_frame);



	vkResetCommandBuffer(graphics->commandbuffers[graphics->current_frame],
			     0);


	int r = record_commandbuffer(graphics,
			     graphics->commandbuffers[graphics->current_frame],
			     image_i);

	if(r == -1) {
		pdebug("record buffer error");
		return -1;
	}

	VkPipelineStageFlags wait_stages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = graphics->image_available_semaphores +
				   graphics->current_frame,
		.pWaitDstStageMask = wait_stages,
		.commandBufferCount = 1,
		.pCommandBuffers =
			graphics->commandbuffers + graphics->current_frame,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = graphics->render_finished_semaphores +
				     graphics->current_frame
	};

	res = vkQueueSubmit(
		graphics->queues[queue_families_graphics], 1, &submitInfo,
		graphics->inflight_fences[graphics->current_frame]);


	if(res == VK_ERROR_OUT_OF_DATE_KHR) {
		goto swapchain_out_of_date;
	}

	if(res != VK_SUCCESS) {
		return -1;
	}


	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = graphics->render_finished_semaphores +
				   graphics->current_frame,

		.swapchainCount = 1,
		.pSwapchains = &graphics->swapchain,
		.pImageIndices = &image_i,
		.pResults = 0
	};

	res = vkQueuePresentKHR(graphics->queues[queue_families_present],
				&presentInfo);

	if(res == VK_ERROR_OUT_OF_DATE_KHR) {
		goto swapchain_out_of_date;
	}

	if(res != VK_SUCCESS) {
		return -1;

	}

	graphics->current_frame =
		(graphics->current_frame + 1) % graphics->frames_inflight;

	return 0;

swapchain_out_of_date:
	return recreate_swapchain(graphics);
}


void destroy_syncobjects(struct Graphics *graphics)
{
	for(int i = 0; i < graphics->frames_inflight; i++) {
		vkDestroyFence(graphics->device, graphics->inflight_fences[i],
			       0);

		vkDestroySemaphore(graphics->device,
				   graphics->render_finished_semaphores[i], 0);

		vkDestroySemaphore(graphics->device,
				   graphics->image_available_semaphores[i], 0);
	}

	free(graphics->render_finished_semaphores);
	free(graphics->image_available_semaphores);
	free(graphics->inflight_fences);

}
int create_syncobjects(struct Graphics *graphics)
{
	int i;
	int j;
	int k;

	graphics->image_available_semaphores =
		malloc(sizeof(VkSemaphore) * graphics->frames_inflight);

	if(!graphics->image_available_semaphores)
		goto image_available_semaphores_malloc_error;

	graphics->render_finished_semaphores =
		malloc(sizeof(VkSemaphore) * graphics->frames_inflight);

	if(!graphics->render_finished_semaphores)
		goto render_finished_semaphores_malloc_error;

	graphics->inflight_fences =
		malloc(sizeof(VkFence) * graphics->frames_inflight);

	if(!graphics->inflight_fences)
		goto inflight_fences_malloc_error;

	VkSemaphoreCreateInfo semaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};

	VkFenceCreateInfo fenceInfo = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	for(i = 0; i < graphics->frames_inflight; i++) {
		VkResult res = vkCreateSemaphore(graphics->device, &semaphoreInfo, 0,
					 graphics->image_available_semaphores + i);
		if(res != VK_SUCCESS)
			goto image_available_semaphores_create_error;
	}


	for(j = 0; j < graphics->frames_inflight; j++) {
		VkResult res = vkCreateSemaphore(graphics->device, &semaphoreInfo, 0,
					&graphics->render_finished_semaphores[j]);

		if (res != VK_SUCCESS)
			goto render_finished_semaphores_create_error;

	}

	for(k = 0; k < graphics->frames_inflight; k++) {
		VkResult res = vkCreateFence(graphics->device, &fenceInfo, 0,
			    graphics->inflight_fences + k);

		if(res != VK_SUCCESS)
			goto inflight_fences_create_error;
	}

	return 0;

inflight_fences_create_error:
	while(k--) {
		vkDestroyFence(graphics->device, graphics->inflight_fences[k],
			       0);
	}

render_finished_semaphores_create_error:
	while (j--) {
		vkDestroySemaphore(graphics->device,
				   graphics->render_finished_semaphores[j], 0);
	}

image_available_semaphores_create_error:
	while(i--) {
		vkDestroySemaphore(graphics->device, graphics->image_available_semaphores[i],
			       0);
	}

	free(graphics->inflight_fences);

inflight_fences_malloc_error:
	free(graphics->render_finished_semaphores);

render_finished_semaphores_malloc_error:
	free(graphics->image_available_semaphores);

image_available_semaphores_malloc_error:

	return -1;
}

int record_commandbuffer(struct Graphics *graphics,
			 VkCommandBuffer commandbuffer, uint32_t image_i)
{
	VkCommandBufferBeginInfo beginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = 0,
		.pInheritanceInfo = 0
	};

	VkResult res = vkBeginCommandBuffer(commandbuffer, &beginInfo);

	if(res != VK_SUCCESS)
		return -1;

	VkClearValue clearColor = {
		.color = {0, 0, 0, 0}
	};

	VkRenderPassBeginInfo renderPassInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = graphics->renderpass,
		.framebuffer = graphics->framebuffers[image_i],
		.renderArea = {
			.offset = {0, 0},
			.extent = graphics->swapchain_extent
		},

		.clearValueCount = 1,
		.pClearValues = &clearColor
	};

	vkCmdBeginRenderPass(commandbuffer, &renderPassInfo,
			     VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandbuffer,
			  VK_PIPELINE_BIND_POINT_GRAPHICS, graphics->pipeline);

	VkDeviceSize offsets[] = {0};

	vkCmdBindVertexBuffers(commandbuffer, 0, 1, &graphics->vertexbuffer,
			       offsets);

	VkViewport viewport = {
		.x = 0,
		.y = 0,
		.width = graphics->swapchain_extent.width,
		.height = graphics->swapchain_extent.height,
		.minDepth = 0,
		.maxDepth = 0
	};

	VkRect2D scissor = {
		.offset = {0, 0},
		.extent = graphics->swapchain_extent
	};

	vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandbuffer, 0, 1, &scissor);

	vkCmdDraw(commandbuffer, graphics->vertices_n, 1, 0, 0);
	vkCmdEndRenderPass(commandbuffer);

	res = vkEndCommandBuffer(commandbuffer);

	if(res != VK_SUCCESS)
		return -1;

	return 0;
}
int create_commandbuffers(struct Graphics *graphics)
{
	graphics->commandbuffers =
		malloc(sizeof(VkCommandBuffer) * graphics->frames_inflight);

	if(!graphics->commandbuffers)
		return -1;

	VkCommandBufferAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = graphics->commandpool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = graphics->frames_inflight
	};

	VkResult res = vkAllocateCommandBuffers(graphics->device, &allocInfo,
					       graphics->commandbuffers);

	if(res != VK_SUCCESS) {
		return -1;
	}

	return 0;
}
int create_commandpool(struct Graphics *graphics)
{
	VkCommandPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex =
			graphics->queue_families.indices[queue_families_graphics]
	};

	VkResult res = vkCreateCommandPool(graphics->device, &poolInfo, 0,
					   &graphics->commandpool);

	if(res != VK_SUCCESS)
		return -1;

	return 0;
}

static VkResult init_framebufers(struct Graphics *graphics)
{
	for(int i = 0; i < graphics->imageviews_n; i++) {
		VkFramebufferCreateInfo framebufferCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = graphics->renderpass,
			.attachmentCount = 1,
			.pAttachments = graphics->imageviews + i,
			.width = graphics->swapchain_extent.width,
			.height = graphics->swapchain_extent.height,
			.layers = 1
		};

		VkResult res = vkCreateFramebuffer(graphics->device,
						   &framebufferCreateInfo, 0,
						   graphics->framebuffers + i);

		if(res == VK_SUCCESS)
			continue;
		
		for(int j = 0; j < i; j++) {
			vkDestroyFramebuffer(graphics->device,
					     graphics->framebuffers[i], 0);
		}


		return res;
	}

	return VK_SUCCESS;
}

int create_framebuffers(struct Graphics *graphics)
{
	graphics->framebuffers =
		malloc(sizeof(VkFramebuffer) * graphics->imageviews_n);

	if (!graphics->framebuffers)
		return -1;

	graphics->framebuffers_n = graphics->imageviews_n;

	int res = init_framebufers(graphics);

	if (!res)
		return 0;

	free(graphics->framebuffers);

	return -1;
}

int create_renderpass(struct Graphics *graphics)
{
	VkAttachmentDescription colorAttachment = {
		.format = graphics->swapchain_format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,

		
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};


	VkAttachmentReference colorAttachmentRef = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef
	};

	VkSubpassDependency dependency = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	VkRenderPassCreateInfo renderPassInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &colorAttachment,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency
	};

	VkResult res = vkCreateRenderPass(graphics->device, &renderPassInfo, 0,
					  &graphics->renderpass);

	if(res != VK_SUCCESS)
		return -1;

	return 0;
}
int create_pipeline(struct Graphics *graphics)
{
	VkPipelineShaderStageCreateInfo shader_stages[shaders_n];

	for (int i = 0; i < shaders_n; i++) {
		shader_stages[i] = (VkPipelineShaderStageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.module = graphics->shadermodules[i],
			.pName = "main"
		};
	}

	shader_stages[fragment_shader].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stages[vertex_shader].stage = VK_SHADER_STAGE_VERTEX_BIT;

	VkDynamicState dynamic_states[2] = {
		VK_DYNAMIC_STATE_VIEWPORT,
    		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamic_states
	};

	uint32_t attribute_descriptions_n;

	const VkVertexInputAttributeDescription *attribute_descriptions =
		vertex_vkattribute_descriptions(
			&attribute_descriptions_n);

	uint32_t binding_decriptions_n;
	const VkVertexInputBindingDescription *binding_descriptions = 
		vertex_vkbinding_descriptions(&binding_decriptions_n);

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = binding_decriptions_n,
		.pVertexBindingDescriptions = binding_descriptions,
		.vertexAttributeDescriptionCount = attribute_descriptions_n,
		.pVertexAttributeDescriptions = attribute_descriptions
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE
	};

	VkViewport viewport = {
		.x = 0,
		.y = 0,
		.width = graphics->swapchain_extent.width,
		.height = graphics->swapchain_extent.height,
		.minDepth = 0,
		.maxDepth = 0
	};

	VkRect2D scissor = {
		.offset = {0, 0},
		.extent = graphics->swapchain_extent
	};

	VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};

	VkPipelineRasterizationStateCreateInfo rasterizer = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,		
		.lineWidth = 1.0f,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f
	};

	VkPipelineMultisampleStateCreateInfo multisampling = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.sampleShadingEnable = VK_FALSE,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.minSampleShading = 1.0f,
		.pSampleMask = 0,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE
	};

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {
		.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD
	};

	VkPipelineColorBlendStateCreateInfo colorBlending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
		.blendConstants = {
			0, 0, 0, 0
		}
	};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 0,
		.pSetLayouts = 0,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = 0
	};

	VkResult res = vkCreatePipelineLayout(graphics->device,
					      &pipelineLayoutInfo, 0,
					      &graphics->pipeline_layout);

	if(res != VK_SUCCESS)
		return -1;

	VkGraphicsPipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shader_stages,
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = 0,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicState,
		.layout = graphics->pipeline_layout,
		.renderPass = graphics->renderpass,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};

	res = vkCreateGraphicsPipelines(graphics->device, VK_NULL_HANDLE, 1,
					&pipelineInfo, 0, &graphics->pipeline);

	if(res != VK_SUCCESS) {
		vkDestroyPipelineLayout(graphics->device, graphics->pipeline_layout, 0);
		return -1;
	}

	return 0;
}
int create_shadermodules(struct Graphics *graphics)
{
	for (int i = 0; i < shaders_n; i++) {
		VkShaderModuleCreateInfo createInfo = {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = graphics->shader_sizes[i],
			.pCode = (const uint32_t *) graphics->shaders[i]
		};

		VkResult res =
			vkCreateShaderModule(graphics->device, &createInfo, 0,
					     graphics->shadermodules + i);

		if(res == VK_SUCCESS)
			continue;

		for(int j = 0; j < i; j++) {
			vkDestroyShaderModule(graphics->device, graphics->shadermodules[i], 0);
		}

		return -1;
	}

	return 0;
}

char *read_binary_file(const char *filename, size_t *size)
{
	
	FILE *fp = fopen(filename, "rb");

	if(!fp) {
		pdebug("%s not found", filename);
		return 0;
	}

	fseek(fp, 0, SEEK_END);

	*size = ftell(fp);

	if(!*size) {
		return 0;
	}

	char *buffer = malloc(sizeof(char *) * *size);
	
	if(!buffer) {
		return 0;
	}

	fseek(fp, 0, SEEK_SET);

	fread(buffer, 1, *size, fp);

	return buffer;
}

void swapchain_details_destroy(struct swapchain_details *swapchain_details)
{
	free(swapchain_details->formats);
	free(swapchain_details->presentmodes);
}

static VkResult init_imageviews(struct Graphics *graphics)
{
	for (int i = 0; i < graphics->images_n; i++) {
		VkImageViewCreateInfo createInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = graphics->images[i],

			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = graphics->swapchain_format,

			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			},

			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};

		VkResult res = vkCreateImageView(graphics->device, &createInfo,
						 0, graphics->imageviews + i);

		if (res == VK_SUCCESS)
			continue;

		for (int j = 0; j < i; j++) {
			vkDestroyImageView(graphics->device,
					   graphics->imageviews[j], 0);
		}

		return res;
	}

	return VK_SUCCESS;
}

int create_imageviews(struct Graphics *graphics)
{
	if(!graphics->images_n)
		return 0;

	graphics->imageviews_n = graphics->images_n;

	graphics->imageviews =
		malloc(sizeof(VkImageView) * graphics->imageviews_n);

	if(!graphics->imageviews)
		return -1;

	VkResult res = init_imageviews(graphics);

	if(res == VK_SUCCESS)
		return 0;

	free(graphics->imageviews);

	return -1;
}


static VkResult init_swapchain(struct Graphics *graphics)
{
	VkSurfaceFormatKHR format = get_format(&graphics->swapchain_details);
		
	VkSwapchainCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = graphics->surface,

		.minImageCount = get_images_n(&graphics->swapchain_details),
		.imageFormat = format.format,
		.imageColorSpace = format.colorSpace,
		.imageExtent = get_swapextent(&graphics->swapchain_details),
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,


		.preTransform = graphics->swapchain_details.capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,

		.presentMode = get_presentmode(&graphics->swapchain_details),
		.clipped = VK_TRUE,
		.oldSwapchain = VK_NULL_HANDLE
	};

	if (graphics->queue_families.indices[queue_families_graphics] !=
	    graphics->queue_families.indices[queue_families_present]) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = graphics->queue_families.indices;
	} else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = 0; // Optional
	}

	VkResult res = vkCreateSwapchainKHR(graphics->device, &createInfo, 0,
					    &graphics->swapchain);
	
	graphics->swapchain_extent = createInfo.imageExtent;
	graphics->swapchain_format = createInfo.imageFormat;

	return res;

}

int create_swapchain(struct Graphics *graphics)
{
	VkResult res = init_swapchain(graphics);

	if(res != VK_SUCCESS)
		return -1;

	vkGetSwapchainImagesKHR(graphics->device, graphics->swapchain,
				&graphics->images_n, 0);

	if(graphics->images_n) {
		graphics->images = malloc(sizeof(VkImage) * graphics->images_n);
		
		if(!graphics->images) {
			vkDestroySwapchainKHR(graphics->device, graphics->swapchain, 0);
			return -1;
		}

		vkGetSwapchainImagesKHR(graphics->device, graphics->swapchain,
				&graphics->images_n, graphics->images);
	}
	return 0;
}


int find_swapchain_details(VkPhysicalDevice device, VkSurfaceKHR surface,
			    struct swapchain_details *swapchain_details)
{
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		device, surface, &swapchain_details->capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &swapchain_details->formats_n, 0);

	if (swapchain_details->formats_n) {
		swapchain_details->formats =
			malloc(sizeof(VkSurfaceFormatKHR) *
			       swapchain_details->formats_n);

		if(!swapchain_details->formats)
			goto formats_error;

		vkGetPhysicalDeviceSurfaceFormatsKHR(
			device, surface, &swapchain_details->formats_n,
			swapchain_details->formats);
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(
		device, surface, &swapchain_details->presentmodes_n, 0);

	if (swapchain_details->presentmodes_n) {
		swapchain_details->presentmodes =
			malloc(sizeof(VkPresentModeKHR) *
			       swapchain_details->presentmodes_n);
		if(!swapchain_details->presentmodes)
			goto presentmodes_error;

		vkGetPhysicalDeviceSurfacePresentModesKHR(
			device, surface, &swapchain_details->presentmodes_n,
			swapchain_details->presentmodes);
	}

	return 0;

presentmodes_error:
	free(swapchain_details->formats);
formats_error:
	return -1;
}

int create_logical_device(struct Graphics *graphics)
{
	float queue_priority = 1.0;

	VkDeviceQueueCreateInfo queueCreateInfo[queue_families_n];

	for(int i = 0; i < queue_families_n; i++) {
		queueCreateInfo[i] = (VkDeviceQueueCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = graphics->queue_families.indices[i],
			.queueCount = 1,
			.pQueuePriorities = &queue_priority
		};
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};

	uint32_t extensions_n;
	const char *const *extensions = get_device_exttensions(&extensions_n);

	VkDeviceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = 0,
            .queueCreateInfoCount = queue_families_n,
            .pQueueCreateInfos = queueCreateInfo,
            .pEnabledFeatures = 0, //&deviceFeatures,
            .enabledExtensionCount = extensions_n,
            .ppEnabledExtensionNames = extensions};

	if (vkCreateDevice(graphics->physicalDevice, &createInfo, 0,
			   &graphics->device) != VK_SUCCESS) {
		return -1;
	}

	return 0;
}

int pick_physical_device(struct Graphics *graphics)
{
	uint32_t devices_n;
	vkEnumeratePhysicalDevices(graphics->instance, &devices_n, 0);

	if(devices_n == 0) {
		perror("failed to find GPUs with Vulkan support");
		return -1;
	}

	VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * devices_n);

	if(!devices) {
		return -1;
	}

	vkEnumeratePhysicalDevices(graphics->instance, &devices_n, devices);

	pdebug("physical devices number: %d", devices_n);

	get_suitable_device(graphics, devices_n, devices);

	free(devices);

	if(graphics->physicalDevice == VK_NULL_HANDLE) {
		perror("No suitable device found");
		return -1;
	}

	graphics->swapchain_extent = graphics->swapchain_details.capabilities.currentExtent;

	return 0;
}

int create_instance(struct Graphics *graphics, uint32_t extensions_n, const char *const *extensions)
{
	pdebug("creating instance");

	int res = check_instance_extensions_support(extensions_n, extensions);

	if(res == -1) {
		perror("unsupported exetnsions");
		return -1;
	}

	uint32_t layers_n;
	const char *const *layers = get_layers(&layers_n);
	
	res = check_layers_support(layers_n, layers);
	
	if(res == -1) {
		perror("unsupported layers");
		return -1;
	}

	VkApplicationInfo appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Vulkan Test",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "No Engeine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_0
	};


	VkInstanceCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
		.enabledExtensionCount = extensions_n,
		.ppEnabledExtensionNames = extensions,
		.enabledLayerCount = layers_n,
		.ppEnabledLayerNames = layers
	};

	VkResult result = vkCreateInstance(&createInfo, 0, &graphics->instance);


	if(result != VK_SUCCESS) {
		perror("failed to create instance");
		return -1;
	}

	return 0;
}

void find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface,
			 struct queue_families *queue_families)
{
	uint32_t queue_families_n;
	
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_families_n, 0);

	VkQueueFamilyProperties *properties =
		malloc(sizeof(VkQueueFamilyProperties) * queue_families_n);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_families_n, properties);

	queue_families->state = 0;

	for (int i = 0; i < queue_families_n; i++) {
		if(properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			queue_families->state |= queue_families_graphics_flag;
			queue_families->indices[queue_families_graphics] = i;
		}

		VkBool32 presentSupport = 0;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		
		if(presentSupport) {
			queue_families->state |= queue_families_present_flag;
			queue_families->indices[queue_families_present] = i;
		}
	}

	free(properties);
}

static int is_device_suitable(struct Graphics *graphics)
{
	int res = check_device_extensions_support(graphics->physicalDevice);

	if(res == -1) {
		return 0;
	}

	find_queue_families(graphics->physicalDevice, graphics->surface,
				    &graphics->queue_families);

	res = (graphics->queue_families.state & queue_families_graphics_flag) &&
	      (graphics->queue_families.state & queue_families_present_flag);


	if(!res) {
		return 0;
	}

	res = find_swapchain_details(graphics->physicalDevice,
				     graphics->surface,
				     &graphics->swapchain_details);
	if (res == -1) {
		return -1;
	}


	res = (graphics->swapchain_details.formats_n > 0) &&
	      (graphics->swapchain_details.presentmodes_n > 0);

	return res;
}

static int
rate_physical_device(const VkPhysicalDeviceProperties *deviceProperties,
		     const VkPhysicalDeviceFeatures *deviceFeatures)
{
	int score = 0;

	if (!deviceFeatures->geometryShader) {
		return 0;
	}

	score += (1000 * (deviceProperties->deviceType ==
			  VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU));

	score += deviceProperties->limits.maxImageDimension2D;

	return score;
}
static void
get_suitable_device(struct Graphics *graphics, uint32_t devices_n,
		    const VkPhysicalDevice *devices)
{
	int score = 0;
	int j = -1;

	VkPhysicalDevice device = VK_NULL_HANDLE;

	for(int i = 0; i < devices_n; i++) {
		graphics->physicalDevice = devices[i];

					
		int res = is_device_suitable(graphics);

		if(res == -1) {
			goto swapchain_details_error;
		}

		VkPhysicalDeviceProperties deviceProperties;
    		vkGetPhysicalDeviceProperties(graphics->physicalDevice, &deviceProperties);
		
		if(!res) {
			pdebug("device: %d, name: %s, device unsuitable", i,
			       deviceProperties.deviceName);
			continue;
		}

		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(graphics->physicalDevice, &deviceFeatures);

		int new_score = rate_physical_device(&deviceProperties, &deviceFeatures);

                pdebug("device: %d, name: %s, score: %d", i,
                       deviceProperties.deviceName, new_score);

                if(new_score >= score) {
			score = new_score;
			device = graphics->physicalDevice;
		}

		swapchain_details_destroy(&graphics->swapchain_details);
	}
	
	if(device != VK_NULL_HANDLE) {
		graphics->physicalDevice = device;

		VkPhysicalDeviceProperties deviceProperties;
    		vkGetPhysicalDeviceProperties(graphics->physicalDevice, &deviceProperties);

		pdebug("picked device: %s", deviceProperties.deviceName);
		
		find_queue_families(graphics->physicalDevice, graphics->surface,
				    &graphics->queue_families);

		int res = find_swapchain_details(graphics->physicalDevice,
						 graphics->surface,
						 &graphics->swapchain_details);

		if(res == -1) {
			goto swapchain_details_error;
		}
	}

	return;

swapchain_details_error:
	graphics->device = VK_NULL_HANDLE;

}




static int check_layers_support(const uint32_t layers_n, const char *const *layers)
{
	pdebug("checking layer support");
	
	uint32_t n;
	int res = 0;

	vkEnumerateInstanceLayerProperties(&n, 0);

	pdebug("layers number: %d", n);

	VkLayerProperties *properties = malloc(sizeof(VkLayerProperties) * n);
	vkEnumerateInstanceLayerProperties(&n, properties);
	
	for(int i = 0; i < layers_n; i++) {
		int j = 0;

		for(; j < n && strcmp(properties[j].layerName, layers[i]); j++);

		if(j == n) {
			res =  -1;
			break;
		}
	}
	
	free(properties);

	return res;

}

static int first_missing_extension(uint32_t extensions_n, const char *const *extensions, 
		uint32_t properties_n, const VkExtensionProperties *properties)
{
	uint32_t res = -1;

	for(uint32_t i = 0; i < extensions_n; i++) {
		
		uint32_t j = 0;
		for(; j < properties_n && strcmp(properties[j].extensionName, extensions[i]); j++);

		if(j == properties_n) {
			res = i;
			break;
		}
	}
	
	return res;
}



static int check_device_extensions_support(VkPhysicalDevice device)
{
	uint32_t extensions_n;
	const char *const *extensions = get_device_exttensions(&extensions_n);

	uint32_t properties_n;
	vkEnumerateDeviceExtensionProperties(device, 0, &properties_n, 0);

	VkExtensionProperties *properties =
		malloc(sizeof(VkExtensionProperties) * properties_n);
	vkEnumerateDeviceExtensionProperties(device, 0, &properties_n, properties);

	int res = check_extensions_support(properties_n, properties, extensions_n, extensions);

	free(properties);

	return res;
}

static int check_instance_extensions_support(uint32_t extensions_n,
					     const char *const *extensions)
{
	uint32_t n;
	vkEnumerateInstanceExtensionProperties(0, &n, 0);

	VkExtensionProperties *properties = malloc(sizeof(VkExtensionProperties) * n);
	vkEnumerateInstanceExtensionProperties(0, &n, properties);

	int res = check_extensions_support(n, properties, extensions_n, extensions);

	free(properties);
	
	return res;
}

static int check_extensions_support(uint32_t properties_n,
				    const VkExtensionProperties *properties,
				    uint32_t extensions_n,
				    const char *const *extensions)
{
	uint32_t res = first_missing_extension(extensions_n, extensions,
					       properties_n, properties);

	if(res == -1) {
		return 0;
	}

	do {
		pdebug("unsupported: %s", extensions[res]); 

		res++;

		extensions_n -= res;
		extensions += res;

		res = first_missing_extension(extensions_n, extensions, 
			properties_n, properties);
		
	} while(extensions_n > 0);
		

	return -1;
}

static const char *const *get_device_exttensions(uint32_t *extensions_n)
{
	static const char *const extensions[] = {
		"VK_KHR_swapchain"
	};

	*extensions_n = sizeof(extensions) / sizeof(char *);
	
	return extensions;
}


static const char *const *get_layers(uint32_t *layers_n)
{
#ifdef DEBUG
	pdebug("getting validation layers");
	static const char *const layers[] = {
		"VK_LAYER_KHRONOS_validation"
	};
	
	*layers_n = sizeof(layers) / sizeof(char *);

	return layers;
#else 
	*layers_n = 0;
	return 0;
#endif
}


static uint32_t get_images_n(const struct swapchain_details *swapchain_details)
{
	uint32_t images_n = swapchain_details->capabilities.minImageCount + 1;

	if (images_n > swapchain_details->capabilities.maxImageCount &&
	    swapchain_details->capabilities.minImageCount > 0) {
		images_n = swapchain_details->capabilities.maxImageCount;
	}
		
	return images_n;
}

static VkExtent2D get_swapextent(const struct swapchain_details *swapchain_details)
{
	return swapchain_details->capabilities.currentExtent;
}

static VkPresentModeKHR get_presentmode(const struct swapchain_details *swapchain_details)
{
	for(int i = 0; i != swapchain_details->presentmodes_n; i++) {
		if (swapchain_details->presentmodes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			return VK_PRESENT_MODE_MAILBOX_KHR;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;

}
static VkSurfaceFormatKHR get_format(const struct swapchain_details *swapchain_details)
{
	for(int i = 0; i != swapchain_details->formats_n; i++) {
		if (swapchain_details->formats[i].format ==
			    VK_FORMAT_B8G8R8A8_SRGB &&
		    swapchain_details->formats[i].colorSpace ==
			    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return swapchain_details->formats[i];
		}
	}

	return swapchain_details->formats[0];
}

#include <memory.h>
#include <malloc.h>

#include <unistd.h>
#include <stdint.h>
#include <vulkan/vulkan_core.h>
#include <graphics/setup.h>
#include <window/vksurface.h>
#include <helpers/helpers.h>

#include "vksetup.h"

#define GRAPHICS_DIR "build/src/graphics/"

static void handle_error(int res, Graphics *graphics);
static int init_graphics(Graphics *graphics, Window *window, uint32_t max_frames_inflight);
static const char **get_extensions(uint32_t *extensions_n);
static const char *const *get_shader_names(void);
static int load_shaders(char **shaders, size_t *shader_sizes);

enum vksetup_statuses {
	vksetup_shadermodules_error,
	vksetup_shaders_error,
	vksetup_imageviews_error,
	vksetup_success,
	vksetup_instance_error,
	vksetup_surface_error,
	vksetup_physicalDevice_error,
	vksetup_logicalDevice_error,
	vksetup_queues_error,
	vksetup_swapchain_error,
	vksetup_pipeline_error,
	vksetup_renderpass_error,
	vksetup_framebuffers_error,
	vksetup_commandpool_error,
	vksetup_vertexbuffer_error,
	vksetup_commandbuffer_error,
	vksetup_syncobjects_error,
	vksetup_statuses_n
};

static const char *const errors[vksetup_statuses_n] = {
	[vksetup_vertexbuffer_error] = "vertex buffer error",
	[vksetup_shaders_error] = "shaders setup error",
	[vksetup_imageviews_error] = "imageviews intit error",
	[vksetup_success] = "setup success",
	[vksetup_instance_error] = "instance setup error",
	[vksetup_surface_error] = "surface setup error",
	[vksetup_physicalDevice_error] = "physicalDevice setup error",
	[vksetup_logicalDevice_error] = "logical device setup error",
	[vksetup_queues_error] = "queeus setup error",
	[vksetup_shadermodules_error] = "shader modules setup error",
	[vksetup_pipeline_error] = "pipeline init error",
	[vksetup_framebuffers_error] = "framebuffers creation error",
	[vksetup_renderpass_error] = "renderpass creation error",
	[vksetup_commandpool_error] = "commandpool creation error",
	[vksetup_commandbuffer_error] = "command buffer creation error",
	[vksetup_syncobjects_error] = "failed creating syncobjets",
	[vksetup_swapchain_error] = "swapchain setup error",
};


void graphics_window_resized(struct Graphics *graphics)
{
	graphics->flags |= graphics_window_resized_flag;
}

Graphics *graphics_new(Window *window)
{
	int res;

	Graphics *graphics = malloc(sizeof(Graphics));

	if(!graphics)
		return 0;


	res = init_graphics(graphics, window, 2);

	if(res != vksetup_success) {
		handle_error(res, graphics);
		free(graphics);
		return 0;
	}

	return graphics;
}

void graphics_delete(Graphics *graphics)
{

	vkDeviceWaitIdle(graphics->device);

	destroy_syncobjects(graphics);
	
	vkDestroyCommandPool(graphics->device, graphics->commandpool, 0);

	destroy_vertexbuffer(graphics);

	for(int i = 0; i < graphics->framebuffers_n; i++) {
		vkDestroyFramebuffer(graphics->device,
				     graphics->framebuffers[i], 0);
	}

	free(graphics->framebuffers);

	vkDestroyPipeline(graphics->device, graphics->pipeline, 0);
	vkDestroyPipelineLayout(graphics->device, graphics->pipeline_layout, 0);
	vkDestroyRenderPass(graphics->device, graphics->renderpass, 0);

	for(int i = 0; i < shaders_n; i++) {
		vkDestroyShaderModule(graphics->device,
				      graphics->shadermodules[i], 0);
		free(graphics->shaders[i]);
	}

	for(int i = 0; i < graphics->imageviews_n; i++) {
		vkDestroyImageView(graphics->device, graphics->imageviews[i],
				   0);
	}

	vkDestroySwapchainKHR(graphics->device, graphics->swapchain, 0);
	vkDestroyDevice(graphics->device, 0);
	vkDestroySurfaceKHR(graphics->instance, graphics->surface, 0);
	vkDestroyInstance(graphics->instance, 0);
	
	swapchain_details_destroy(&graphics->swapchain_details);

	free(graphics->images);
	free(graphics->imageviews);

	free(graphics);
}




 

static int init_graphics(Graphics *graphics, Window *window, uint32_t max_frames_inflight)
{
	int res;

	graphics->flags = 0;
	graphics->frames_inflight = max_frames_inflight;

	{
		uint32_t len;
		const char **names = get_extensions(&len);

		res = create_instance(graphics, len, names);

		free(names);
	}

	if(res == -1)
		return vksetup_instance_error;

	res = create_surface(graphics->instance, window, &graphics->surface);

	if(res == -1)
		return vksetup_surface_error;

	res = pick_physical_device(graphics);

	if(res == -1)
		return vksetup_physicalDevice_error;

	res = create_logical_device(graphics);

	if (res == -1)
		return vksetup_logicalDevice_error;

	for(int i = 0; i< queue_families_n; i++) {
		vkGetDeviceQueue(graphics->device,
				 graphics->queue_families.indices[i], 0,
				 &graphics->queues[i]);
	}

	res = create_swapchain(graphics);

	if(res == -1)
		return vksetup_swapchain_error;

	res = create_imageviews(graphics);

	if(res == -1)
		return vksetup_imageviews_error;

	res = load_shaders(graphics->shaders, graphics->shader_sizes);
	
	if(res == -1)
		return vksetup_shaders_error;

	res = create_shadermodules(graphics);

	if(res == -1)
		return vksetup_shadermodules_error;

	res = create_renderpass(graphics);

	if(res == -1)
		return vksetup_renderpass_error;

	res = create_pipeline(graphics);

	if(res == -1)
		return vksetup_pipeline_error;

	res = create_framebuffers(graphics);

	if(res == -1)
		return vksetup_framebuffers_error;

	res = create_commandpool(graphics);

	if(res == -1)
		return vksetup_commandpool_error;

	res = create_vertexbuffer(graphics);

	if(res == -1)
		return vksetup_vertexbuffer_error;

	res = create_commandbuffers(graphics);

	if(res == -1)
		return vksetup_commandbuffer_error;
	
	res = create_syncobjects(graphics);

	if(res == -1)
		return vksetup_syncobjects_error;


	return vksetup_success;
}


static int load_shaders(char **shaders, size_t *shader_sizes)
{
	const char *const *names = get_shader_names();

	for (int i = 0; i < shaders_n; i++) {
		shaders[i] = read_binary_file(names[i], shader_sizes + i);

		if (shaders[i])
			continue;

		for(int j = 0; j < i; j++) {
			free(shaders[j]);
			shader_sizes[j] = 0;
		}

		return -1;
	}

	return 0;
}

static const char *const *get_shader_names(void)
{
	static const char *const names[shaders_n] = {
		[fragment_shader] = GRAPHICS_DIR "shader.frag.spv",
		[vertex_shader] = GRAPHICS_DIR "shader.vert.spv"
	};

	return names;
}

static void handle_error(int res, Graphics *graphics)
{
	switch (res) {
	case vksetup_syncobjects_error:
	case vksetup_commandbuffer_error:
		vkDestroyCommandPool(graphics->device, graphics->commandpool, 0);
	case vksetup_vertexbuffer_error:
		vkDestroyCommandPool(graphics->device, graphics->commandpool, 0);
	case vksetup_commandpool_error:
		for(int i = 0; i < graphics->framebuffers_n; i++) {
			vkDestroyFramebuffer(graphics->device,
					     graphics->framebuffers[i], 0);
		}

		free(graphics->framebuffers);

	case vksetup_framebuffers_error:
		vkDestroyPipeline(graphics->device, graphics->pipeline, 0);
	case vksetup_pipeline_error:
		vkDestroyRenderPass(graphics->device, graphics->renderpass, 0);
	case vksetup_renderpass_error:
		for(int i = 0; i < shaders_n; i++) {
			vkDestroyShaderModule(graphics->device, graphics->shadermodules[i], 0);
			free(graphics->shaders[i]);
		}

	case vksetup_shadermodules_error:
		for(int i = 0; i < shaders_n; i++){
			free(graphics->shaders[i]);
		}
	case vksetup_shaders_error:
		for (int i = 0; i < graphics->imageviews_n; i++) {
			vkDestroyImageView(graphics->device,
					   graphics->imageviews[i], 0);
		}
	case vksetup_imageviews_error:
		vkDestroySwapchainKHR(graphics->device, graphics->swapchain, 0);
	case vksetup_swapchain_error:
		vkDestroyDevice(graphics->device, 0);
	case vksetup_logicalDevice_error: 
		swapchain_details_destroy(&graphics->swapchain_details);
	case vksetup_physicalDevice_error:
		vkDestroySurfaceKHR(graphics->instance, graphics->surface, 0);
	case vksetup_surface_error: 
		vkDestroyInstance(graphics->instance, 0);
	case vksetup_instance_error:
		pdebug("%s:", errors[res]);
		perror("*");	
	}

}

static const char **get_extensions(uint32_t *extensions_n)
{
	uint32_t window_extensions_n;
	const char **window_extensions = get_window_extensions(&window_extensions_n);
	
	*extensions_n = window_extensions_n;

	const char **extensions = malloc(*extensions_n * sizeof(char *));

	memcpy(extensions, window_extensions, window_extensions_n * sizeof(char *));
	
	return extensions;
}

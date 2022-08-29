#include <stdint.h>
#include <window/window.h>
#include <vulkan/vulkan_core.h>

#include "vertex.h"

enum graphics_flags {
	graphics_window_resized_flag = 1
};

enum shader_types {
	fragment_shader,
	vertex_shader,
	shaders_n
};

enum queue_families_flags {
	queue_families_graphics_flag = 1,
	queue_families_present_flag = 2
};

enum queue_families_indices {
	queue_families_graphics,
	queue_families_present,
	queue_families_n
};

struct swapchain_details {
	VkSurfaceCapabilitiesKHR capabilities;
	
	uint32_t formats_n;
	VkSurfaceFormatKHR *formats;

	uint32_t presentmodes_n;
	VkPresentModeKHR *presentmodes;
};

struct queue_families {
	uint32_t indices[queue_families_n];
	int state;
};

struct Graphics {
	VkInstance instance;

	VkSurfaceKHR surface;

	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkQueue queues[queue_families_n];
	struct queue_families queue_families;
	
	VkSwapchainKHR swapchain;
	VkFormat swapchain_format;
	VkExtent2D swapchain_extent;
	struct swapchain_details swapchain_details;

	VkRenderPass renderpass;
	VkPipeline pipeline;
	VkPipelineLayout pipeline_layout;

	VkCommandPool commandpool;

	uint32_t current_frame;
	uint32_t frames_inflight;
	
	VkCommandBuffer *commandbuffers;
	VkSemaphore *image_available_semaphores;
	VkSemaphore *render_finished_semaphores;
	VkFence *inflight_fences;

	uint32_t images_n;
	VkImage *images;

	uint32_t imageviews_n;
	VkImageView *imageviews;


	size_t shader_sizes[shaders_n];
	char *shaders[shaders_n];

	VkShaderModule shadermodules[shaders_n];

	uint32_t framebuffers_n;
	VkFramebuffer *framebuffers;

	uint32_t vertices_n;
	const struct vertex *vertices;

	VkBuffer vertexbuffer;
	VkDeviceMemory vertex_buffer_memory;

	int flags;
};

int create_instance(struct Graphics *graphics, uint32_t ext_n,
		    const char *const *ext);

int pick_physical_device(struct Graphics *graphics);
int create_logical_device(struct Graphics *graphics);
int create_swapchain(struct Graphics *graphics);
int create_imageviews(struct Graphics *graphics);
int create_renderpass(struct Graphics *graphics);
int create_shadermodules(struct Graphics *graphics);
int create_pipeline(struct Graphics *graphics);
int create_framebuffers(struct Graphics *graphics);
int create_vertexbuffer(struct Graphics *graphics);
int create_commandpool(struct Graphics *graphics);
int create_commandbuffers(struct Graphics *graphics);

int recreate_swapchain(struct Graphics *graphics);

int create_syncobjects(struct Graphics *graphics);

void destroy_syncobjects(struct Graphics *graphics);
void destroy_vertexbuffer(struct Graphics *graphics);

int draw_frame(struct Graphics *graphics);

int record_commandbuffer(struct Graphics *graphics,
			 VkCommandBuffer commandbuffer, uint32_t image_i);

char *read_binary_file(const char *filename, size_t *size);

void find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface,
			 struct queue_families *queue_families);

int find_swapchain_details(VkPhysicalDevice device, VkSurfaceKHR surface,
			    struct swapchain_details *swapchain_details);

void swapchain_details_destroy(struct swapchain_details *swapchain_details);

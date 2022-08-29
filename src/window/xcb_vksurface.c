#include <window/vksurface.h>
#include <xcb/xproto.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>

#include "xcb_window.h"

int create_surface(VkInstance instance, Window *window, VkSurfaceKHR *surface)
{
	VkXcbSurfaceCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
		.connection = window->conn,
		.window = window->window_id
	};

	return vkCreateXcbSurfaceKHR(instance, &createInfo, 0, surface);
}

const char **get_window_extensions(uint32_t *extensions_n)
{
	static const char *extensions[] = {
		"VK_KHR_surface",
		"VK_KHR_xcb_surface"
	};

	*extensions_n = sizeof(extensions) / sizeof(char *);
	
	return extensions;
}

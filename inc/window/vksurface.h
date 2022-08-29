#include "window.h"
#include <vulkan/vulkan_core.h>

int create_surface(VkInstance instance, Window *window, VkSurfaceKHR *surface);

const char **get_window_extensions(uint32_t *extensions_n);

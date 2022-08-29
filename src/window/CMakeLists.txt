add_library(window xcb_window.c xcb_window.h xcb_vksurface.c 
	"${INC}/window/window.h"
	"${INC}/window/vksurface.h"
)

find_package(Vulkan REQUIRED)

target_include_directories(window PUBLIC "${Vulkan_INCLUDE_DIRS}")
target_link_libraries(window PRIVATE xcb Vulkan::Vulkan)

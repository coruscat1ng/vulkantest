find_package(Vulkan REQUIRED)

add_library(graphics setup.c "${INC}/graphics/setup.h" vksetup.h vksetup.c vertex.h vertex.c)

target_include_directories(graphics PUBLIC "${Vulkan_INCLUDE_DIRS}")
target_link_libraries(graphics window helpers Vulkan::Vulkan)

compile_shader(graphics
    FORMAT spv
    SOURCES
    "${SHADERS}/shader.vert"
    "${SHADERS}/shader.frag"
)

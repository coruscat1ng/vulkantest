#ifndef VERTEX_H
#define VERTEX_H
#include <stdint.h>
#include <vulkan/vulkan_core.h>

struct vertex;

const VkVertexInputBindingDescription *
vertex_vkbinding_descriptions(uint32_t *binding_descriptions_n);

uint32_t vertex_size(void);

const struct vertex *get_vertices(uint32_t *vertices_n);

const VkVertexInputAttributeDescription *
vertex_vkattribute_descriptions(uint32_t *attributes_n);
#endif

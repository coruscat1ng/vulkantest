#include "vertex.h"
#include <stdint.h>
#include <helpers/helpers.h>

struct vertex {
	float pos[2];
	float color[3];
};

uint32_t vertex_size(void)
{
	return sizeof(struct vertex);
}

const struct vertex *get_vertices(uint32_t *vertices_n)
{
	static const struct vertex vertices[] = {
		{.pos = {0.0, -0.5}, .color = {1.0, 0.0, 1.0}},
		{.pos = {0.5, 0.5}, .color = {1.0, 1.0, 0.0}},
		{.pos = {-0.5, 0.5}, .color = {0.0, 0.0, 1.0}},
		{.pos = {0.5, -0.5}, .color = {0, 1, 0}},
		{.pos = {0.5, 0.5}, .color = {1.0, 1.0, 0.0}},
		{.pos = {-0.5, 0.5}, .color = {0.0, 0.0, 1.0}},
		
	};


	pdebug("sizeof pos: %lu", sizeof(vertices[0].pos));

	*vertices_n = sizeof(vertices) / sizeof(struct vertex);

	return vertices;
}

const VkVertexInputBindingDescription *
vertex_vkbinding_descriptions(uint32_t *binding_descriptions_n)
{
	static VkVertexInputBindingDescription description = {
		.binding = 0,
		.stride = sizeof(struct vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	*binding_descriptions_n = 1;

	return &description;
}

const VkVertexInputAttributeDescription *
vertex_vkattribute_descriptions(uint32_t *attributes_n)
{
	static VkVertexInputAttributeDescription descriptions[2] = {
		{ .binding = 0,
		  .location = 0,
		  .format = VK_FORMAT_R32G32_SFLOAT,
		  .offset = offsetof(struct vertex, pos) },
		{ .binding = 0,
		  .location = 1,
		  .format = VK_FORMAT_R32G32B32_SFLOAT,
		  .offset = offsetof(struct vertex, color) }
	};

	*attributes_n = 2;

	return descriptions;
}

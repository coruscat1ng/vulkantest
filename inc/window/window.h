#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

enum window_event_types {
	resize_event_type,
	close_event_type,
	empty_event_type
};

typedef struct Window Window;

typedef struct window_event {
	int event_type;
	struct window_event_info *info;
} window_event_t;


Window *window_new(int width, int height, const char *name);
void window_delete(Window *window);

void show_window(Window *window);

int window_poll_event(Window *window, window_event_t *event);
void window_event_destroy(window_event_t *window_event);

uint32_t window_get_height(const Window *window);
uint32_t window_get_width(const Window *window);

#endif

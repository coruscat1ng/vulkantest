#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <window/window.h>

typedef struct Graphics Graphics;

Graphics *graphics_new(Window *window);
void graphics_delete(Graphics *graphics);

int draw_frame(Graphics *graphics);

void graphics_window_resized(Graphics *graphics);

#endif

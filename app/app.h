#include <graphics/setup.h>
#include <window/window.h>
#include <window/vksurface.h>
#include <helpers/helpers.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <unistd.h>

typedef struct App {
	Window *window;
	Graphics *graphics;	
} App;

int app_init(App *app);
void app_destroy(App *app);



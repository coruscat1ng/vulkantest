#include "app.h"

int app_init(App *app)
{
	app->window = window_new(600, 600, "test");
	
	if(!app->window)
		return -1;

	app->graphics = graphics_new(app->window);

	if(!app->graphics) {
		window_delete(app->window);
		return -1;
	}

	return 0;
}

void app_destroy(App *app)
{
	window_delete(app->window);
	graphics_delete(app->graphics);
}

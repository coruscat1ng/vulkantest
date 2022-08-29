#include "app.h"
#include "helpers/helpers.h"
#include "window/window.h"

enum app_flags {
	app_running = 1,
	app_poll = 2
};

int main(void) 
{
	pdebug("starting in debug mode");

	App app;

	int res = app_init(&app);
	
	if(res == -1) {
		perror("error during app setup");
		return -1;
	}

	int state = app_running;
	
	do {

		window_event_t event;

		state |= app_poll;

		while(state & app_poll) {


			int res = window_poll_event(app.window, &event);

			if(!res)
				break;

			switch (event.event_type) {
			case resize_event_type:
				graphics_window_resized(app.graphics);
				break;
			case close_event_type:
				state &= ~app_running;
				state &= ~app_poll;
				break;
			}

			window_event_destroy(&event);
		}

		res = draw_frame(app.graphics);

		if(res == -1)
			pdebug("dra frame error");

	} while(state & app_running);

	app_destroy(&app);

	return 0;
}

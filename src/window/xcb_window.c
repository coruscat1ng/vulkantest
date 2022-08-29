#include "window/window.h"
#include <stdint.h>
#include <xcb/xcb.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "xcb_window.h"
#include <xcb/xproto.h>
#include <helpers/helpers.h>

uint32_t window_height(const Window *window);
uint32_t window_width(const Window *window);

int generic_event_type(const Window *window, const window_event_t *window_event)
{

	return empty_event_type;
}

void window_event_destroy(window_event_t *window_event)
{
	free(window_event->info);
}

uint32_t window_get_height(const Window *window)
{
	return window->height;
}

uint32_t window_get_width(const Window *window)
{
	return window->width;
}

static inline void proccess_resize_request(Window *window,
					   xcb_generic_event_t *generic_event)
{
	xcb_resize_request_event_t *event =
		(xcb_resize_request_event_t *)generic_event;

	window->width = event->width;
	window->height = event->height;
}

static inline int is_configure_resize_notify(Window *window,
					     xcb_generic_event_t *generic_event)
{
	xcb_configure_notify_event_t *event =
		(xcb_configure_notify_event_t *)generic_event;

	return (event->width != window->width) ||
	       (event->height != window->height);
}

static inline void
proccess_configure_resize_notify(Window *window,
				 xcb_generic_event_t *generic_event)
{
	xcb_configure_notify_event_t *event =
		(xcb_configure_notify_event_t *)generic_event;

	window->height = event->height;
	window->width = event->width;
}

	static inline int is_close_client_event(
		Window *window, xcb_generic_event_t *generic_event)
{
	xcb_client_message_event_t *client_event =
		(xcb_client_message_event_t *)generic_event;
	return (client_event->data.data32[0] == window->close_reply->atom);
}

int window_poll_event(Window *window, window_event_t *window_event)
{
	window_event->event_type = empty_event_type;
	
	int done = 0;


	do {
		xcb_generic_event_t *event = xcb_poll_for_event(window->conn);

		if (!event)
			return 0;
		switch (event->response_type & ~0x80) {
		case XCB_CONFIGURE_NOTIFY:
			if(is_configure_resize_notify(window, event)) {
				proccess_configure_resize_notify(window, event);
				window_event->event_type = resize_event_type;
			}
			break;

		case XCB_RESIZE_REQUEST:
			proccess_resize_request(window, event);
			window_event->event_type = resize_event_type;
			break;
		case XCB_CLIENT_MESSAGE: 
			if(is_close_client_event(window, event))
				window_event->event_type = close_event_type;
			break;
		}
		
		if(window_event->event_type != empty_event_type) {
			window_event->info = (struct window_event_info *) event;	
			return 1;
		}

		free(event);

	} while (!done);

	return 0;
}

Window *window_new(int width, int height, const char *name)
{

	Window *window = malloc(sizeof(Window));

	if(!window)
		return 0;

	window->conn = xcb_connect(0, 0);
	
	if(xcb_connection_has_error(window->conn)) {
		free(window);
		printf("Error opening display.\n");
		return 0;
  	}
	
	const xcb_setup_t *setup = xcb_get_setup(window->conn);
  	xcb_screen_t *screen = xcb_setup_roots_iterator(setup).data;

  	window->window_id = xcb_generate_id(window->conn);

  	uint32_t prop_name = XCB_CW_EVENT_MASK;
	uint32_t prop_value = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
		//XCB_EVENT_MASK_RESIZE_REDIRECT;
	//XCB_EVENT_MASK_STRUCTURE_NOTIFY; //XCB_EVENT_MASK_EXPOSURE;

	xcb_create_window(window->conn, screen->root_depth,
		window->window_id, screen->root,
		0, 0, width, height, 1, XCB_WINDOW_CLASS_INPUT_OUTPUT,
		screen->root_visual, prop_name, &prop_value);

	xcb_intern_atom_cookie_t protocol_cookie =
		xcb_intern_atom(window->conn, 1, 12, "WM_PROTOCOLS");

	xcb_intern_atom_reply_t *protocol_reply =
		xcb_intern_atom_reply(window->conn, protocol_cookie, 0);
	xcb_intern_atom_cookie_t close_cookie =
		xcb_intern_atom(window->conn, 0, 16, "WM_DELETE_WINDOW");

	window->close_reply =
		xcb_intern_atom_reply(window->conn, close_cookie, 0);

	xcb_change_property(window->conn, XCB_PROP_MODE_REPLACE, window->window_id,
			    protocol_reply->atom, 4, 32, 1,
			    &(window->close_reply->atom));

	free(protocol_reply);

	xcb_map_window(window->conn, window->window_id);
	xcb_flush(window->conn);

	window->height = height;
	window->width = width;

	return window;
}

void window_delete(Window *window)
{
	xcb_disconnect(window->conn);
	free(window->close_reply);
	free(window);
}
/*
int main(void) {

  conn = xcb_connect(NULL, NULL);
  if(xcb_connection_has_error(conn)) {
    printf("Error opening display.\n");
    exit(1);
  }

  setup = xcb_get_setup(conn);
  screen = xcb_setup_roots_iterator(setup).data;

  window_id = xcb_generate_id(conn);
  prop_name = XCB_CW_BACK_PIXEL;
  prop_value = screen->white_pixel;

  xcb_create_window(conn, screen->root_depth, 
     window_id, screen->root, 0, 0, 1000, 1000, 1,
     XCB_WINDOW_CLASS_INPUT_OUTPUT, 
     screen->root_visual, prop_name, &prop_value);

  xcb_map_window(conn, window_id);
  xcb_flush(conn);
	
  	xcb_generic_event_t *event;
	while((event = xcb_wait_for_event(conn))) {
		switch(event->response_type) {
		defaukt:
			break;

		}
	}
  xcb_disconnect(conn);
  return 0;
}*/

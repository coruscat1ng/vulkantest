#include <stdint.h>
#include <window/window.h>
#include <xcb/xproto.h>

struct Window {
	xcb_connection_t *conn;
  	xcb_window_t window_id;
	xcb_intern_atom_reply_t *close_reply;

	uint16_t width;
	uint16_t height;
};

struct window_event_info {
	xcb_generic_event_t event;
};




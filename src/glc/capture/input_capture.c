/**
 * \file glc/capture/input_capture.c
 * \brief input capture from x11
 * \author Sebastian Wick <wick.sebastian@gmail.com>
 * \date 2012-2012
 * For conditions of distribution and use, see copyright notice in glc.h
 */

/**
 * \addtogroup input_capture
 *  \{
 */

#include <stdlib.h>
#include <string.h>

#include <glc/common/glc.h>
#include <glc/common/log.h>
#include <glc/common/state.h>

#include "input_capture.h"

struct input_event {
	Display *dyp;
	XEvent *event;
};

struct input_capture_s {
	glc_t *glc;
	ps_buffer_t *to;
	glc_stream_id_t id;
	glc_state_input_t state_input;

	int capture;
};

int input_capture_init(input_capture_t *input_capture, glc_t *glc) {
	*input_capture = (input_capture_t) malloc(sizeof(struct input_capture_s));
	memset(*input_capture, 0, sizeof(struct input_capture_s));

	(*input_capture)->glc = glc;
	(*input_capture)->capture = 0;
	glc_state_input_new((*input_capture)->glc, &(*input_capture)->id,
			    &(*input_capture)->state_input);

	glc_log((*input_capture)->glc, GLC_INFORMATION, "input_capture", "init");

	return 0;
}

int input_capture_start(input_capture_t input_capture) {
	if(input_capture->to == NULL)
		return EAGAIN;

	if(input_capture->capture)
		glc_log(input_capture->glc, GLC_WARNING, "input_capture", "already started");
	else
		glc_log(input_capture->glc, GLC_INFORMATION, "input_capture", "starting");

	input_capture->capture = 1;
	return 0;
}

int input_capture_stop(input_capture_t input_capture) {
	if(!input_capture->capture)
		glc_log(input_capture->glc, GLC_WARNING, "input_capture", "already stopped");
	else
		glc_log(input_capture->glc, GLC_INFORMATION, "input_capture", "stopping");

	input_capture->capture = 0;
	return 0;
}

int input_capture_destroy(input_capture_t input_capture) {
	free(input_capture);
	return 0;
}

int input_capture_set_buffer(input_capture_t input_capture, ps_buffer_t *buffer) {
	input_capture->to = buffer;
	return 0;
}

int input_capture_event(input_capture_t input_capture, Display *dpy, XEvent *event) {
	if(!input_capture->capture)
		return 0;

	char *serialized;
	glc_input_data_header_t hdr;
	glc_message_header_t msg_hdr;
	ps_packet_t packet;

	ps_packet_init(&packet, input_capture->to);
	msg_hdr.type = GLC_MESSAGE_INPUT_DATA;

	hdr.time = glc_state_time(alsa_capture->glc);
	hdr.size = sizeof(char)*1024;
	hdr.id = input_capture->id;

	serialized = (char*) malloc(hdr.size);
	if (!(ret = input_capture_event_to_string(event, serialized)))
		goto cancel;
	hdr.size = strlen(serialized);

	if ((ret = ps_packet_open(&packet, PS_PACKET_WRITE)))
		goto cancel;
	if ((ret = ps_packet_write(&packet, &msg_hdr, sizeof(glc_message_header_t))))
		goto cancel;
	if ((ret = ps_packet_write(&packet, &hdr, sizeof(glc_input_data_header_t))))
		goto cancel;
	if ((ret = ps_packet_write(&packet, serialized, hdr.size)))
		goto cancel;
	if ((ret = ps_packet_close(&packet)))
		goto cancel;


	ps_packet_destroy(&packet);

cancel:
	glc_log(input_capture->glc, GLC_ERROR, "input_capture", "%s (%d)", strerror(ret), ret);
	if (ret == EINTR)
		break;
	if (ps_packet_cancel(&packet))
		break;
	free(serialized);

	return 0;
}

int input_capture_event_to_string(XEvent *event, char *serialized) {
	switch(event->type) {
		case KeyPress: {
			XKeyPressedEvent *e = (XKeyPressedEvent*)event;
			sprintf(serialized, "kp %lu %d %d %u %u", e->time /* unsigned long */, e->x, e->y, e->state, e->keycode);
			return 0;
		}
		case KeyRelease: {
			XKeyReleasedEvent *e = (XKeyReleasedEvent*)event;
			sprintf(serialized, "kr %lu %d %d %u %u", e->time /* unsigned long */, e->x, e->y, e->state, e->keycode);
			return 0;
		}
		case ButtonPress: {
			XButtonPressedEvent *e = (XButtonPressedEvent*)event;
			sprintf(serialized, "bp %lu %d %d %u %u", e->time /* unsigned long */, e->x, e->y, e->state, e->button);
			return 0;
		}
		case ButtonRelease: {
			XButtonPressedEvent *e = (XButtonReleasedEvent*)event;
			sprintf(serialized, "br %lu %d %d %u %u", e->time /* unsigned long */, e->x, e->y, e->state, e->button);
			return 0;
		}
		case MotionNotify: {
			XMotionEvent *e = (XMotionEvent*)event;
			sprintf(serialized, "mf %lu %d %d %u ", e->time /* unsigned long */, e->x, e->y, e->state/*, e->is_hint /* char */);
			return 0;
		}
	}
	return 1;
}
 
/**  \} */

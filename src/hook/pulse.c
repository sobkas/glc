/**
 * \file hook/pulse.c
 * \brief pulse wrapper
 * \author Sebastian Wick <wick.sebastian@gmail.com>
 * \date 2012-2012
 * For conditions of distribution and use, see copyright notice in glc.h
 */

/**
 * \addtogroup hook
 *  \{
 * \defgroup pulse pulse wrapper
 *  \{
 */

#include <dlfcn.h>
#include <elfhacks.h>
#include <stdio.h>

#include <glc/common/util.h>
#include <glc/common/core.h>
#include <glc/common/log.h>
#include <glc/common/state.h>
#include <pulse/pulseaudio.h>
#include <packetstream.h>

#include "lib.h"

struct pulse_capture_stream_s {
	pa_stream *pulse_stream;

	glc_state_audio_t state_audio;
	glc_stream_id_t id;

	void *buffer;
	size_t buffer_length, buffer_index;
	
	char *device;
	unsigned int channels;
	unsigned int rate;
	pa_sample_format_t format;
	unsigned int fragsize;
	int flags;

	struct pulse_capture_stream_s *next;
};

struct pulse_private_s {
	glc_t *glc;
	
	int started;
	int capture;
	int capturing;

	ps_packet_t packet;
	pa_threaded_mainloop *loop;
	pa_context *context;
	int context_ready;
	int capture_when_context_ready;

	struct pulse_capture_stream_s *capture_stream;
};

__PRIVATE struct pulse_private_s pulse;
__PRIVATE int pulse_loaded = 0;

__PRIVATE int pulse_parse_capture_cfg(const char *cfg);
__PRIVATE void pulse_capture_connect_all();
__PRIVATE void pulse_capture_disconnect_all();
__PRIVATE glc_audio_format_t pulse_glc_format(pa_sample_format_t pcm_fmt);
static void pulse_context_state_callback(pa_context *c, void *userdata);
static void stream_read_callback(pa_stream *s, size_t length, void *userdata);
static void stream_state_callback(pa_stream *s, void *userdata);

int pulse_init(glc_t *glc) {
	pulse.glc = glc;
	pulse.started = pulse.capturing = 0;
	pulse.capture_stream = NULL;
	int ret = 0;

	glc_log(pulse.glc, GLC_DEBUG, "pulse", "initializing");

	if (getenv("GLC_AUDIO") && getenv("GLC_USE_PULSEAUDIO"))
		pulse.capture = atoi(getenv("GLC_AUDIO"));
	else
		pulse.capture = 1;

	if (getenv("GLC_USE_PULSEAUDIO"))
		pulse_parse_capture_cfg(getenv("GLC_AUDIO_DEVICES"));
	
	if(pulse.capture) {
		if (!(pulse.loop = pa_threaded_mainloop_new()))
		    return 1;

		pa_mainloop_api *mainloop_api = pa_threaded_mainloop_get_api(pulse.loop);

		if (!(pulse.context = pa_context_new(mainloop_api, "glc-capture-pulse"))) {
		    return 1;
		}

		pa_context_set_state_callback(pulse.context, pulse_context_state_callback, NULL);

		if (pa_context_connect(pulse.context, NULL, 0, NULL) < 0) {
		    return 1;
		}
	}

	return 0;
}

void pulse_init_streams() {
	pa_context *c = pulse.context;
	struct pulse_capture_stream_s *stream = pulse.capture_stream;

	while(stream != NULL) {

		pa_sample_spec sample_spec = {
			.format = stream->format,
			.rate = stream->rate,
			.channels = stream->channels
		};
		char *name = malloc(sizeof(char)*25);
		sprintf(name, "Audio Stream #%d", stream->id);

		if(!(stream->pulse_stream = pa_stream_new(c, name, &sample_spec, NULL))) {
			glc_log(pulse.glc, GLC_ERROR, "pulse", ("pa_stream_new() failed: %s\n"), pa_strerror(pa_context_errno(c)));
			free(name);
            return;
    	}
    	free(name);

		pa_stream_set_state_callback(stream->pulse_stream, stream_state_callback, (void*)stream);
		pa_stream_set_read_callback(stream->pulse_stream, stream_read_callback, (void*)stream);

		stream = stream->next;
	}
}

static void pulse_context_state_callback(pa_context *c, void *userdata) {
    switch(pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;
        case PA_CONTEXT_READY: {
        	pulse_init_streams();
    		pulse.context_ready = 1;
			if(pulse.capture_when_context_ready) {
				pulse.capture_when_context_ready = 0;
				pulse_capture_connect_all();
			}
            break;
        }

        case PA_CONTEXT_TERMINATED:
            //TODO: quit?
            break;

        case PA_CONTEXT_FAILED:
        default:
            glc_log(pulse.glc, GLC_ERROR, "pulse", "Connection failure: %s\n", pa_strerror(pa_context_errno(c)));
			return;
    }

    return;
}

static void stream_read_callback(pa_stream *s, size_t length, void *userdata) {
    const void *data;

	struct pulse_capture_stream_s *stream = (struct pulse_capture_stream_s*)userdata;

    if (pa_stream_peek(stream->pulse_stream, &data, &length) < 0) {
		glc_log(pulse.glc, GLC_ERROR, "pulse", "pa_stream_peek() failed: %s\n", pa_strerror(pa_context_errno(pulse.context)));
        return;
    }

    if(stream->buffer) {
        stream->buffer = pa_xrealloc(stream->buffer, stream->buffer_length + length);
        memcpy((uint8_t*) stream->buffer + stream->buffer_length, data, length);
        stream->buffer_length += length;
    } else {
        stream->buffer = pa_xmalloc(length);
        memcpy(stream->buffer, data, length);
        stream->buffer_length = length;
        stream->buffer_index = 0;
    }

    pa_stream_drop(stream->pulse_stream);


    while(stream->buffer_length >= stream->fragsize) {

		glc_utime_t time, delay_usec = 0;
	
		glc_audio_data_header_t hdr;
		glc_message_header_t msg_hdr;
		msg_hdr.type = GLC_MESSAGE_AUDIO_DATA;
	
		pa_sample_spec sample_spec = {
			.format = stream->format,
			.rate = stream->rate,
			.channels = stream->channels
		};
	
		time = glc_state_time(pulse.glc);
		delay_usec = 1000000*stream->buffer_length/pa_bytes_per_second(&sample_spec);

		if (delay_usec < time)
			time -= delay_usec;
		hdr.time = time;
		hdr.size = stream->fragsize;
		hdr.id = stream->id;
	
		int ret;
		if ((ret = ps_packet_open(&(pulse.packet), PS_PACKET_WRITE)))
			goto cancel;
		if ((ret = ps_packet_write(&(pulse.packet), &msg_hdr, sizeof(glc_message_header_t))))
			goto cancel;
		if ((ret = ps_packet_write(&(pulse.packet), &hdr, sizeof(glc_audio_data_header_t))))
			goto cancel;

		if ((ret = ps_packet_write(&(pulse.packet), stream->buffer, stream->fragsize)))
			goto cancel;

		size_t newlength = stream->buffer_length - stream->fragsize;
		char *newbuff = malloc(newlength);
		memcpy(newbuff, stream->buffer + stream->fragsize, newlength);
		stream->buffer_length = newlength;
		free(stream->buffer);
		stream->buffer = newbuff;
		

		hdr.size = stream->fragsize * pa_frame_size(&sample_spec);
		if ((ret = ps_packet_setsize(&(pulse.packet), sizeof(glc_message_header_t) +
						     sizeof(glc_audio_data_header_t) +
						     hdr.size)))
			goto cancel;
		if ((ret = ps_packet_close(&(pulse.packet))))
			goto cancel;

		return;

	cancel:
		glc_log(pulse.glc, GLC_ERROR, "pulse", "%s (%d)", strerror(ret), ret);
		if (ret == EINTR)
			return;
		if (ps_packet_cancel(&(pulse.packet)))
			return;
	}

}

static void stream_state_callback(pa_stream *s, void *userdata) {
	struct pulse_capture_stream_s *stream = (struct pulse_capture_stream_s*)userdata;
	switch(pa_stream_get_state(stream->pulse_stream)) {
	    case PA_STREAM_READY: {
	        const pa_buffer_attr *attr = pa_stream_get_buffer_attr(stream->pulse_stream);
	        stream->fragsize = attr->fragsize;
	        break;
        }
	}
}

int pulse_start(ps_buffer_t *buffer) {
	struct pulse_capture_stream_s *stream = pulse.capture_stream;
	int ret;

	if (pulse.started)
		return EINVAL;

	if (pa_threaded_mainloop_start(pulse.loop) < 0) {
	    return 1;
	}
	
	while(stream != NULL) {
		glc_audio_format_message_t fmt_msg;
		glc_message_header_t msg_hdr;
		msg_hdr.type = GLC_MESSAGE_AUDIO_FORMAT;

		stream->flags = GLC_AUDIO_INTERLEAVED;

		fmt_msg.id = stream->id;
		fmt_msg.rate = stream->rate;
		fmt_msg.channels = stream->channels;
		fmt_msg.flags = stream->flags;
		fmt_msg.format = pulse_glc_format(stream->format);

		if (!fmt_msg.format) {
			glc_log(pulse.glc, GLC_ERROR, "pulse", "unsupported audio format 0x%02x", stream->format);
			return ENOTSUP;
		}
		
		ps_packet_t packet;
		ps_packet_init(&packet, buffer);
		ps_packet_open(&packet, PS_PACKET_WRITE);
		ps_packet_write(&packet, &msg_hdr, sizeof(glc_message_header_t));
		ps_packet_write(&packet, &fmt_msg, sizeof(glc_audio_format_message_t));
		ps_packet_close(&packet);
		ps_packet_destroy(&packet);

		stream = stream->next;
	}
	
	ps_packet_init(&pulse.packet, buffer);

	pulse.started = 1;
	return 0;
}

int pulse_close() {
	if (!pulse.started)
		return 0;

	glc_log(pulse.glc, GLC_DEBUG, "pulse", "closing");

	if (pulse.capture) {
		pulse.capture = 0; /* disable capturing */
	}
    if (pulse.context)
        pa_context_unref(pulse.context);

	if(pulse.loop)
        pa_threaded_mainloop_free(pulse.loop);

	ps_packet_destroy(&pulse.packet);

	struct pulse_capture_stream_s *del;

	while(pulse.capture_stream != NULL) {
		del = pulse.capture_stream;
		pulse.capture_stream = pulse.capture_stream->next;
		if(del->device)
			free(del->device);
		free(del);
	}

	return 0;
}

int pulse_capture_start_all() {
	if (!pulse.capture || pulse.capturing)
		return 0;

	if(pulse.context_ready)
		pulse_capture_connect_all();
	else
		pulse.capture_when_context_ready = 1;

	return 0;
}

int pulse_capture_stop_all() {
	if (!pulse.capture)
		return 0;

	if(pulse.capture_when_context_ready)
		pulse.capture_when_context_ready = 0;
	if(pulse.capturing)
		pulse_capture_disconnect_all();

	return 0;
}

int pulse_parse_capture_cfg(const char *cfg) {
	struct pulse_capture_stream_s *stream;
	const char *args, *next, *device = cfg;
	unsigned int channels, rate;
	size_t len;

	if(device == NULL) {
		stream = malloc(sizeof(struct pulse_capture_stream_s));
		memset(stream, 0, sizeof(struct pulse_capture_stream_s));

		stream->device = NULL;

		stream->buffer_length = stream->buffer_index = stream->fragsize = 0;
		stream->buffer = NULL;
		stream->channels = 2;
		stream->rate = 44100;
		stream->format = PA_SAMPLE_S16LE;
		stream->next = pulse.capture_stream;
		glc_state_audio_new(pulse.glc, &(stream->id), &(stream->state_audio));
		pulse.capture_stream = stream;

		glc_log(pulse.glc, GLC_DEBUG, "pulse", "device %s\n", stream->device);
	}

	while (device != NULL) {
		while (*device == ';')
			device++;
		if (*device == '\0')
			break;

		channels = 2;
		rate = 44100;

		/* check if some args have been given */
		if ((args = strstr(device, ",")))
			sscanf(args, ",%u,%u", &rate, &channels);
		next = strstr(device, ";");

		stream = malloc(sizeof(struct pulse_capture_stream_s));
		memset(stream, 0, sizeof(struct pulse_capture_stream_s));

		if (args)
			len = args - device;
		else if (next)
			len = next - device;
		else
			len = strlen(device);

		stream->device = (char *) malloc(sizeof(char) * len);
		memcpy(stream->device, device, len);
		stream->device[len] = '\0';

		stream->buffer_length = stream->buffer_index = stream->fragsize = 0;
		stream->buffer = NULL;
		stream->channels = channels;
		stream->rate = rate;
		stream->format = PA_SAMPLE_S16LE;
		stream->next = pulse.capture_stream;
		glc_state_audio_new(pulse.glc, &(stream->id), &(stream->state_audio));
		pulse.capture_stream = stream;

		glc_log(pulse.glc, GLC_DEBUG, "pulse", "device %s\n", stream->device);

		device = next;
	}

	return 0;
}

glc_audio_format_t pulse_glc_format(pa_sample_format_t pcm_fmt) {
	switch (pcm_fmt) {
		case PA_SAMPLE_S16LE:
			return GLC_AUDIO_S16_LE;
		case PA_SAMPLE_S24LE:
			return GLC_AUDIO_S24_LE;
		case PA_SAMPLE_S32LE:
			return GLC_AUDIO_S32_LE;
		default:
			return 0;
	}
}

void pulse_capture_connect_all() {
	pulse.capturing = 1;
	pa_context *c = pulse.context;
	struct pulse_capture_stream_s *stream = pulse.capture_stream;

	while(stream != NULL) {
		if (pa_stream_connect_record(stream->pulse_stream, stream->device, NULL, 0) < 0) {
			glc_log(pulse.glc, GLC_ERROR, "pulse", "pa_stream_connect_record() failed: %s\n", pa_strerror(pa_context_errno(c)));
			return;
		}

		stream = stream->next;
	}
}

void pulse_capture_disconnect_all() {
	pulse.capturing = 0;
	pa_context *c = pulse.context;
	struct pulse_capture_stream_s *stream = pulse.capture_stream;

	while(stream != NULL) {
		if (pa_stream_disconnect(stream->pulse_stream) < 0) {
			glc_log(pulse.glc, GLC_ERROR, "pulse", "pa_stream_disconnect_record() failed: %s\n", pa_strerror(pa_context_errno(c)));
			return;
		}

		stream = stream->next;
	}
}

/**  \} */
/**  \} */

/**
 * \file glc/export/input.c
 * \brief export to input
 * \author Sebastian Wick <wick.sebastian@gmail.com>
 * \date 2012-2012
 * For conditions of distribution and use, see copyright notice in glc.h
 */

/**
 * \addtogroup input
 *  \{
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <png.h>
#include <packetstream.h>

#include <glc/common/glc.h>
#include <glc/common/core.h>
#include <glc/common/log.h>
#include <glc/common/thread.h>
#include <glc/common/util.h>

#include "input.h"

/*input_t
input_init
input_set_filename
input_set_stream_id
input_process_start
input_process_wait
input_destroy*/

struct input_s {
	glc_t *glc;
	glc_thread_t thread;
	int running;

	glc_stream_id_t id;

	unsigned int file_count;
	const char *filename_format;

	FILE *to;

	glc_utime_t time;
};

int input_read_callback(glc_thread_state_t *state);
void input_finish_callback(void *priv, int err);

int input_write(input_t input, glc_input_data_header_t *input_msg, char *data);

int input_init(input_t *input, glc_t *glc)
{
	*input = (input_t) malloc(sizeof(struct input_s));
	memset(*input, 0, sizeof(struct input_s));

	(*input)->glc = glc;

	(*input)->filename_format = "input%02d";
	(*input)->id = 1;

	(*input)->thread.flags = GLC_THREAD_READ;
	(*input)->thread.ptr = *input;
	(*input)->thread.read_callback = &input_read_callback;
	(*input)->thread.finish_callback = &input_finish_callback;
	(*input)->thread.threads = 1;

	return 0;
}

int input_destroy(input_t input)
{
	free(input);
	return 0;
}

int input_process_start(input_t input, ps_buffer_t *from)
{
	int ret;
	if (input->running)
		return EAGAIN;

	if ((ret = glc_thread_create(input->glc, &input->thread, from, NULL)))
		return ret;

	char *filename = (char *) malloc(1024);
	snprintf(filename, 1023, input->filename_format, ++input->file_count);

	glc_log(input->glc, GLC_INFORMATION, "input", "opening %s for writing", filename);
	input->to = fopen(filename, "w");
	if (!input->to) {
		glc_log(input->glc, GLC_ERROR, "input", "can't open %s", filename);
		free(filename);
		return EINVAL;
	}

	input->running = 1;

	return 0;
}

int input_process_wait(input_t input)
{
	if (!input->running)
		return EAGAIN;

	glc_thread_wait(&input->thread);
	input->running = 0;

	return 0;
}

int input_set_filename(input_t input, const char *filename)
{
	input->filename_format = filename;
	return 0;
}

int input_set_stream_id(input_t input, glc_stream_id_t id)
{
	input->id = id;
	return 0;
}

void input_finish_callback(void *priv, int err)
{
	input_t input = (input_t ) priv;

	if (err)
		glc_log(input->glc, GLC_ERROR, "input", "%s (%d)", strerror(err), err);

	if (input->to) {
		fclose(input->to);
		input->to = NULL;
	}

	input->file_count = 0;
}

int input_read_callback(glc_thread_state_t *state)
{
	input_t input = (input_t ) state->ptr;

	if (state->header.type == GLC_MESSAGE_INPUT_DATA)
		return input_write(input, (glc_input_data_header_t *) state->read_data, &state->read_data[sizeof(glc_input_data_header_t)]);

	return 0;
}

int input_write(input_t input, glc_input_data_header_t *input_hdr, char *data)
{
	size_t need_silence, write_silence;
	unsigned int c;
	size_t samples, s;

	if (input_hdr->id != input->id)
		return 0;

	fwrite(data, sizeof(char), strlen(data), input->to);

	return 0;
}

/**  \} */

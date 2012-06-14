/**
 * \file glc/export/input.h
 * \brief export to input
 * \author Sebastian Wick <wick.sebastian@gmail.com>
 * \date 2012-2012
 * For conditions of distribution and use, see copyright notice in glc.h
 */

/**
 * \addtogroup export
 *  \{
 * \defgroup input export to input
 *  \{
 */

#ifndef _INPUT_H
#define _INPUT_H

#include <packetstream.h>
#include <glc/common/glc.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief input object
 */
typedef struct input_s* input_t;

/**
 * \brief initialize input object
 * \param input input object
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PUBLIC int input_init(input_t *input, glc_t *glc);

/**
 * \brief destroy input object
 * \param input input object
 * \return 0 on success otherwise an error code
 */
__PUBLIC int input_destroy(input_t input);

/**
 * \brief set filename format
 *
 * %d in filename is substituted with counter.
 *
 * Default format is "input%02d"
 * \param input input object
 * \param filename filename format
 * \return 0 on success otherwise an error code
 */
__PUBLIC int input_set_filename(input_t input, const char *filename);

/**
 * \brief set input stream number
 *
 * \param input input object
 * \param id input stream id
 * \return 0 on success otherwise an error code
 */
__PUBLIC int input_set_stream_id(input_t input, glc_stream_id_t id);

/**
 * \brief start input process
 *
 * writes input data from selected input stream into
 * file.
 * \param input input object
 * \param from source buffer
 * \return 0 on success otherwise an error code
 */
__PUBLIC int input_process_start(input_t input, ps_buffer_t *from);

/**
 * \brief block until process has finished
 * \param input input object
 * \return 0 on success otherwise an error code
 */
__PUBLIC int input_process_wait(input_t input);

#ifdef __cplusplus
}
#endif

#endif

/**  \} */
/**  \} */

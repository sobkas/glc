/**
 * \file glc/capture/input_capture.c
 * \brief input capture from x11
 * \author Sebastian Wick <wick.sebastian@gmail.com>
 * \date 2012-2012
 * For conditions of distribution and use, see copyright notice in glc.h
 */

/**
 * \addtogroup capture
 *  \{
 * \defgroup input_capture input capture
 *  \{
 */

#ifndef _INPUT_CAPTURE_H
#define _INPUT_CAPTURE_H

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <packetstream.h>
#include <glc/common/glc.h>

#define INPUT_QUEUE_MAX 10

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief input_capture object
 */
typedef struct input_capture_s* input_capture_t;



/**
 * \brief initialize input_capture object
 * \param input_capture input_capture object
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PUBLIC int input_capture_init(input_capture_t *input_capture, glc_t *glc);


/**
 * \brief start capturing
 * \param input_capture input_capture object
 * \return 0 on success otherwise an error code
 */
__PUBLIC int input_capture_start(input_capture_t input_capture);

/**
 * \brief stop capturing
 * \param input_capture input_capture object
 * \return 0 on success otherwise an error code
 */
__PUBLIC int input_capture_stop(input_capture_t input_capture);

/**
 * \brief destroy input_capture object
 * \param input_capture input_capture object
 * \return 0 on success otherwise an error code
 */
__PUBLIC int input_capture_destroy(input_capture_t input_capture);


/**
 * \brief event to capture
 * \param input_capture input_capture object
 * \param Display X Display object
 * \param XEvent XEvent object
 * \return 0 on success otherwise an error code
 */
__PUBLIC int input_capture_event(input_capture_t input_capture, Display *dpy, XEvent *event);

#ifdef __cplusplus
}
#endif

#endif

/**  \} */
/**  \} */

/*
  Copyright (c) 1999,2003 James Ponder <james@squish.net>

  Permission to use, copy, modify, and distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#define _GNU_SOURCE 1
#define _BSD_SOURCE 1
#define __EXTENSIONS__ 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "mserv.h"
#include "params.h"

static char mserv_rcs_id[] = "$Id: ossaudio.c,v 1.2 2004/03/28 16:06:24 johanwalles Exp $";
MSERV_MODULE(ossaudio, "0.01", "OSS output streaming",
             MSERV_MODFLAG_OUTPUT);

typedef struct _t_ossaudio {
  char *device_name;
  char *mixer_name;
  int  playing;
  int  output_fd;
} t_ossaudio;

/* initialise module */

int ossaudio_init(char *error, int errsize)
{
  (void)this_module;
  (void)error;
  (void)errsize;
  
  return MSERV_SUCCESS;
}

/* finalise module */

int ossaudio_final(char *error, int errsize)
{
  (void)error;
  (void)errsize;
  
  return MSERV_SUCCESS;
}

/* create output stream */

int ossaudio_output_create(t_channel *c, t_channel_outputstream *os,
                          const char *location,
                          t_params *params, void **privateP,
                          char *error, int errsize)
{
  t_ossaudio *ossaudio;
  char *mixer;
  
  (void)c;
  (void)os;
  
  ossaudio = calloc(1, sizeof(t_ossaudio));
  if (ossaudio == NULL) {
    snprintf(error, errsize, "Out of memory");
    return MSERV_FAILURE;
  }
  
  ossaudio->device_name = strdup(location);
  if (ossaudio->device_name == NULL) {
    snprintf(error, errsize, "Out of memory");
    return MSERV_FAILURE;
  }
  
  if (params_get(params, "mixer", &mixer) != MSERV_SUCCESS) {
    mixer = "/dev/mixer";
  }
  ossaudio->mixer_name = strdup(mixer);
  if (ossaudio->mixer_name == NULL) {
    snprintf(error, errsize, "Out of memory");
    return MSERV_FAILURE;
  }
  
  ossaudio->playing = 0;
  
  *privateP = ossaudio;
  
  return MSERV_SUCCESS;
}

/* destroy output stream */

int ossaudio_output_destroy(t_channel *c, t_channel_outputstream *os,
                           void *private, char *error, int errsize)
{
  t_ossaudio *ossaudio = (t_ossaudio *)private;
  (void)ossaudio;
  (void)c;
  (void)os;
  
  snprintf(error, errsize, "%s() unimplemented", __FUNCTION__);
  
  return MSERV_FAILURE;
}

/* synchronise output stream */

int ossaudio_output_sync(t_channel *c, t_channel_outputstream *os,
                        void *private, char *error, int errsize)
{
  t_ossaudio *ossaudio = (t_ossaudio *)private;
  (void)c;
  (void)os;

  if (!ossaudio->playing) {
    // Nothing's playing, we don't need to do anything to be
    // MSERV_SUCCESSful
    return MSERV_SUCCESS;
  }
  
  snprintf(error, errsize, "%s() unimplemented", __FUNCTION__);
  
  return MSERV_FAILURE;
}

/* read/set volume */

int ossaudio_output_volume(t_channel *c, t_channel_outputstream *os,
                          void *private, int *volume,
                          char *error, int errsize)
{
  t_ossaudio *ossaudio = (t_ossaudio *)private;
  (void)ossaudio;
  (void)c;
  (void)os;
  (void)volume;
  
  snprintf(error, errsize, "%s() unimplemented", __FUNCTION__);
  
  return MSERV_FAILURE;
}

/* start output stream */

int ossaudio_output_start(t_channel *c, t_channel_outputstream *os,
                         void *private, char *error, int errsize)
{
  t_ossaudio *ossaudio = (t_ossaudio *)private;
  int output_fd;
  
  (void)c;
  (void)os;

  if (ossaudio->playing) {
    return MSERV_SUCCESS;
  }
  
  // FIXME: Set an alarm() that times out unless the open() call has
  // returned within a couple of seconds
  
  if ((output_fd = open(ossaudio->device_name, O_WRONLY, 0)) == -1) {
    snprintf(error, errsize,
	     "failed opening audio device '%s' for output: %s",
	     ossaudio->device_name,
	     strerror(errno));
    return MSERV_FAILURE;
  }
  
  // FIXME: OSS devices default to 8kHz sample rate.  We need to ask
  // for something better than that.
  
  ossaudio->output_fd = output_fd;
  ossaudio->playing = 1;
  
  return MSERV_SUCCESS;
}

/* stop output stream */

int ossaudio_output_stop(t_channel *c, t_channel_outputstream *os,
                        void *private, char *error, int errsize)
{
  t_ossaudio *ossaudio = (t_ossaudio *)private;
  (void)c;
  (void)os;

  if (!ossaudio->playing) {
    return MSERV_SUCCESS;
  }

  if (close(ossaudio->output_fd) == 0) {
    return MSERV_SUCCESS;
  } else {
    snprintf(error, errsize,
	     "failed closing audio device '%s': %s",
	     ossaudio->device_name,
	     strerror(errno));
    return MSERV_FAILURE;
  }
}

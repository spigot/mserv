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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <alloca.h>

#include "mserv.h"
#include "mserv-soundcard.h"
#include "params.h"

static char mserv_rcs_id[] = "$Id: ossaudio.c,v 1.4 2004/03/28 18:53:03 johanwalles Exp $";
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
  unsigned int sample;
  unsigned int channel;
  int i;
  int written;
  int buffer_size = sizeof(unsigned char) * os->channels * os->samplerate;
  unsigned char *buffer = alloca(buffer_size);
  
  if (!ossaudio->playing) {
    // Nothing's playing, we don't need to do anything to be
    // MSERV_SUCCESSful
    return MSERV_SUCCESS;
  }

  // Create a one second sample for the sound card
  i = 0;
  for (sample = 0; sample < os->samplerate; sample++) {
    for (channel = 0; channel < os->channels; channel++) {
      // The output contains floats in the range -1.0 - 1.0
      buffer[i++] = (unsigned char)((os->output[sample * os->channels + channel] + 1.0f) * 127.0f);
    }
  }

  if ((written = write(ossaudio->output_fd,
		       buffer,
		       buffer_size)) != buffer_size)
  {
    if (written == -1) {
      snprintf(error, errsize,
	       "failed to send data to soundcard: %s",
	       strerror(errno));
    } else {
      snprintf(error, errsize,
	       "attempted to send %d bytes to soundcard, but only %d bytes were written",
	       buffer_size,
	       written);
    }
    
    return MSERV_FAILURE;
  }
  
  return MSERV_SUCCESS;
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
  int format;
  int stereo;
  int samplerate;
  
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

  // Set the sample format
  
  // FIXME: What should we set it to?  Until I have a good answer to
  // that question I'll be using eight bits unsigned data.
  format = AFMT_U8;
  if (ioctl(output_fd, SNDCTL_DSP_SETFMT, &format) == -1) {
    snprintf(error, errsize,
	     "failed setting audio format to AFMT_U8");
    goto failed;
  }
  if (format != AFMT_U8) {
    snprintf(error, errsize,
	     "audio format AFMT_U8 not supported by sound card (got %d when trying)",
	     format);
    goto failed;
  } 
  
  // Set stereo output
  if (os->channels != 1 && os->channels != 2) {
    snprintf(error, errsize,
	     "the OSS output plugin supports only mono or stereo, %d channels is neither",
	     os->channels);
    goto failed;
  }
  stereo = (os->channels == 2 ? 1 : 0);
  if (ioctl(output_fd, SNDCTL_DSP_STEREO, &stereo) == -1) {
    snprintf(error, errsize,
	     "failed asking soundcard for %s output",
	     stereo ? "stereo" : "mono");
    goto failed;
  }
  if (stereo != (os->channels == 2 ? 1 : 0)) {
    snprintf(error, errsize,
	     "sound card doesn't support %s output",
	     os->channels == 2 ? "stereo" : "mono");
    goto failed;
  }
  
  // Set the sample rate to whatever we get in os->samplerate.
  samplerate = os->samplerate;
  if (ioctl(output_fd, SNDCTL_DSP_SPEED, &samplerate) == -1) {
    snprintf(error, errsize,
	     "failed asking soundcard for %dHz output", samplerate);
    goto failed;
  } 
  if (samplerate != (signed)os->samplerate) {
    snprintf(error, errsize,
	     "setting output sample rate to %dHz failed, got %dHz",
	     os->samplerate,
	     samplerate);
    goto failed;
  }
  
  ossaudio->output_fd = output_fd;
  ossaudio->playing = 1;
  
  return MSERV_SUCCESS;

 failed:
  close(output_fd);
  return MSERV_FAILURE;
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

  if (close(ossaudio->output_fd) != 0) {
    snprintf(error, errsize,
	     "failed closing audio device '%s': %s",
	     ossaudio->device_name,
	     strerror(errno));
    return MSERV_FAILURE;
  }
  
  ossaudio->playing = 0;
  return MSERV_SUCCESS;
}

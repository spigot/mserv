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

static char mserv_rcs_id[] = "$Id: ossaudio.c,v 1.9 2004/06/22 21:06:56 johanwalles Exp $";
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

/* synchronise output stream */

int ossaudio_output_sync(t_channel *c, t_channel_outputstream *os,
                        void *private, char *error, int errsize)
{
  t_ossaudio *ossaudio = (t_ossaudio *)private;
  (void)c;
  int written;
  static int buffer_size;
  static signed short *buffer;
  
  if (buffer == NULL) {
    buffer_size = sizeof(signed short) * os->channels * os->samplerate;
    buffer = (signed short *)malloc(buffer_size);
  }
  
  if (!ossaudio->playing) {
    // Nothing's playing, we don't need to do anything to be
    // MSERV_SUCCESSful
    return MSERV_SUCCESS;
  }
  
  if (os->bytesLeft == -1) {
    // New data is available.  Create a one second sample for the
    // sound card.
    int i = 0;
    unsigned int sample_number;
    for (sample_number = 0;
	 sample_number < os->samplerate;
	 sample_number++)
    {
      unsigned int channel;
      for (channel = 0; channel < os->channels; channel++) {
	float sample = os->output[sample_number * os->channels + channel];
	
	// The output contains floats in the range -1.0 - 1.0
	buffer[i++] = (signed short)(sample * 32767.0f);
      }
    }
    
    os->bytesLeft = buffer_size;
  }

  if (os->bytesLeft == 0) {
    return MSERV_SUCCESS;
  }

  written = write(ossaudio->output_fd,
		  ((char*)buffer) + (buffer_size - os->bytesLeft),
		  os->bytesLeft);
  if (written == -1) {
    snprintf(error, errsize,
	     "failed to send data to soundcard: %s",
	     strerror(errno));
    return MSERV_FAILURE;
  }
  
  os->bytesLeft -= written;
  
  return os->bytesLeft;
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

  int mixer_fd;
  int is_getter = (*volume == -1);
  
  if ((mixer_fd = open(ossaudio->mixer_name,
		       O_NONBLOCK | is_getter ? O_RDONLY : O_WRONLY,
		       0)) == -1)
  {
    snprintf(error, errsize,
	     "failed opening mixer device '%s' for %s: %s",
	     ossaudio->mixer_name,
	     is_getter ? "reading" : "writing",
	     strerror(errno));
    return MSERV_FAILURE;
  }
  
  if (is_getter) {
    if (ioctl(mixer_fd,
	      SOUND_MIXER_READ_PCM,
	      volume) == -1)
    {
      snprintf(error, errsize,
	       "failed reading volume from mixer device '%s': %s",
	       ossaudio->mixer_name,
	       strerror(errno));
      goto failed;
    }

    // The volume is encoded using one byte per channel
    switch (os->channels) {
    case 1:
      *volume = *volume & 0xff;
      break;
      
    case 2:
      // Calculate the average of the left and the right channel
      *volume = ((*volume & 0xff) +
		 ((*volume >> 8) & 0xff)) / 2;
      break;
    }
  } else {
    int local_volume = *volume;

    // The volume is encoded using one byte per channel
    if (os->channels == 2) {
      local_volume |= local_volume << 8;
    }
    
    if (ioctl(mixer_fd,
	      SOUND_MIXER_WRITE_PCM,
	      &local_volume) == -1)
    {
      snprintf(error, errsize,
	       "failed setting volume to %d%% using mixer device '%s': %s",
	       *volume,
	       ossaudio->mixer_name,
	       strerror(errno));
      goto failed;
    }
  }

  close(mixer_fd);
  return MSERV_SUCCESS;

 failed:
  close(mixer_fd);
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
  
  if ((output_fd =
       open(ossaudio->device_name, O_NONBLOCK | O_WRONLY, 0)) == -1)
  {
    snprintf(error, errsize,
	     "failed opening audio device '%s' for output: %s",
	     ossaudio->device_name,
	     strerror(errno));
    return MSERV_FAILURE;
  }
  
  // Set the sample format to 16 bits signed data.  We support nothing
  // else.
  format = AFMT_S16_NE;
  if (ioctl(output_fd, SNDCTL_DSP_SETFMT, &format) == -1) {
    snprintf(error, errsize,
	     "failed setting audio format to AFMT_S16_NE");
    goto failed;
  }
  if (format != AFMT_S16_NE) {
    snprintf(error, errsize,
	     "audio format AFMT_S16_NE not supported by sound card (got %d when trying)",
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

/* destroy output stream */

int ossaudio_output_destroy(t_channel *c, t_channel_outputstream *os,
                           void *private, char *error, int errsize)
{
  t_ossaudio *ossaudio = (t_ossaudio *)private;
  (void)ossaudio;
  (void)c;
  (void)os;

  // Stop before destroying so that we don't leak any resources
  ossaudio_output_stop(c, os, private, error, errsize);
  
  free(ossaudio->device_name);
  free(ossaudio->mixer_name);
  free(ossaudio);
  
  return MSERV_SUCCESS;
}

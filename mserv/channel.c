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

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include "mserv.h"
#include "misc.h"
#include "channel.h"
#include "module.h"
#include "params.h"

#if HAVE_LIBSAMPLERATE
#include <samplerate.h>
#endif

/*
 *
 *   buffer_char/buffer_float/buffer_bytes/buffer_size
 *     the raw PCM data (buffer_char) from the player we have a as child
 *     process, we try and maintain this as having a full second's worth of
 *     data.  buffer_float is the same as buffer_char after being converted
 *     to floats and modified in volume
 *
 */

/*
 * A channel stream is created with channel_create, setting up the necessary
 * parameters and connections.
 *
 * Inputs are passed to channel_addinput in the form of a file handle
 * containing a stream of raw PCM data.  These file handles are placed in a
 * link list so that we could potentially have a new stream before the old one
 * has finished.
 *
 * Stopping or pausing makes the channel routines generate silence.
 *
 */

static t_channel_list *channel_list;

/* initialise */

int channel_init(char *error, int errsize)
{
  (void)error;
  (void)errsize;
  return MSERV_SUCCESS;
}

/* finalise */

int channel_final(char *error, int errsize)
{
  (void)error;
  (void)errsize;
  return MSERV_SUCCESS;
}

/* create channel */

int channel_create(t_channel **channel, const char *name,
                   char *error, int errsize)
{
  t_channel *c;
  t_channel_list *cl, *cl_end;

  if (strlen(name) >= sizeof(c->name)) {
    snprintf(error, errsize, "name too long");
    return MSERV_FAILURE;
  }
  if ((c = malloc(sizeof(t_channel))) == NULL) {
    snprintf(error, errsize, "out of memory");
    return MSERV_FAILURE;
  }
  strncpy(c->name, name, sizeof(c->name));
  c->name[sizeof(c->name) - 1] = '\0';
  c->playing.track = NULL;
  c->input = NULL;
  c->output = NULL;
  c->paused = 0;
  c->stopped = 1;
  c->channels = 2;
  c->samplerate = 44100;
  c->buffer_samples = c->samplerate * c->channels;
  c->buffer_size = c->buffer_samples * 2;
  if ((c->buffer_char = malloc(c->buffer_samples * 2)) == NULL) {
    free(c);
    snprintf(error, errsize, "out of memory");
    return MSERV_FAILURE;
  }
  if ((c->buffer = malloc(c->buffer_samples * sizeof(float))) == NULL) {
    free(c->buffer_char);
    free(c);
    snprintf(error, errsize, "out of memory");
    return MSERV_FAILURE;
  }
  c->buffer_bytes = 0;
  c->lasttime.tv_sec = 0;

  if ((cl = malloc(sizeof(t_channel_list))) == NULL) {
    free(c->buffer);
    free(c->buffer_char);
    free(c);
    snprintf(error, errsize, "out of memory creating channel list structure");
    return MSERV_FAILURE;
  }
  cl->channel = c;
  cl->created = time(NULL);
  cl->next = NULL;

  if (channel_list) {
    /* find end of module list and append our new entry */
    for (cl_end = channel_list; cl_end->next; cl_end = cl_end->next) ;
    cl_end->next = cl;
  } else {
    /* start the list */
    channel_list = cl;
  }
  mserv_log("channel %s: created", c->name);

  *channel = c;
  return MSERV_SUCCESS;
}

/* add output stream to channel */

int channel_addoutput(t_channel *c, const char *modname, const char *location,
                     const char *params, char *error, int errsize)
{
  t_channel_outputstream *os, **osend;
  t_modinfo *mi;
  int resampletype;
  int src_errno;
  char *val;
 
  if ((mi = module_find(modname)) == NULL) {
    snprintf(error, errsize, "module %s not loaded", modname);
    return MSERV_FAILURE;
  }
  if (strlen(location) >= sizeof(os->location)) {
    snprintf(error, errsize, "destination location too long");
    return MSERV_FAILURE;
  }
  if ((os = malloc(sizeof(t_channel_outputstream))) == NULL) {
    snprintf(error, errsize, "out of memory");
    return MSERV_FAILURE;
  }
  memset(os, 0, sizeof(t_channel_outputstream));
  os->next = NULL;
  os->modinfo = mi;
  strncpy(os->location, location, sizeof(os->location));
  os->location[sizeof(os->location) - 1] = '\0';
  if (params_parse(&(os->params), params, error, errsize) != MSERV_SUCCESS)
    goto failed;
  if (mi->output_create == NULL) {
    snprintf(error, errsize, "module '%s' has no output creation function",
             modname);
    goto failed;
  }
  if (params_get(os->params, "downmix", &val) != MSERV_SUCCESS)
    val = "0";
  os->channels = atoi(val) ? 1 : 2;
  if (params_get(os->params, "samplerate", &val) != MSERV_SUCCESS)
    val = "44100";
  os->samplerate = atoi(val);
  if (params_get(os->params, "resampletype", &val) != MSERV_SUCCESS)
    val = "1"; /* SRC_SINC_MEDIUM_QUALITY */
  resampletype = atoi(val);
  if (c->samplerate != os->samplerate) {
#if HAVE_LIBSAMPLERATE
    os->resampler = (void *)src_new(resampletype, c->channels, &src_errno);
    if (os->resampler == NULL) {
      snprintf(error, errsize, "libsamplerate failed to initialise: %s",
               src_strerror(src_errno));
      goto failed;
    }
    /* allocate based on output sample rate but input channels, this is
     * because we do the samplerate conversion first, before we downmix */
    if ((os->output = malloc(sizeof(float) * c->channels *
                                       os->samplerate)) == NULL) {
      snprintf(error, errsize, "out of memory");
      goto failed;
    }
#else
    snprintf(error, errsize, "libsamplerate is not enabled");
    goto failed;
#endif
  } else {
    os->resampler = NULL;
  }
  if (mserv_debug)
    mserv_log("calling module '%s' output_create function", modname);
  if (mi->output_create(c, os, location, os->params, &os->private,
                        error, errsize) != MSERV_SUCCESS)
    goto failed;
  if (mserv_debug)
    mserv_log("output_create function returned success");
  if (c->output) {
    for (osend = &c->output; (*osend)->next; osend = &((*osend)->next)) ;
    (*osend)->next = os;
  } else {
    c->output = os;
  }
  return MSERV_SUCCESS;
failed:
  if (os->resampler)
    src_delete((SRC_STATE *)os->resampler);
  if (os->output)
    free(os->output);
  if (os->params)
    free(os->params);
  free(os);
  return MSERV_FAILURE;
}

/* remove output from channel stream (modname or uri can be NULL) */

int channel_removeoutput(t_channel *c, const char *modname,
                        const char *location, char *error, int errsize)
{
  t_channel_outputstream *os, *os_last;
  t_channel_outputstream **osp, **osp_last;
  t_modinfo *mi;
 
  /* search for stream, keeping pointer so we can remove it later */
  os_last = NULL;
  osp_last = NULL;
  for (osp = &c->output, os = c->output; os; osp = &(os->next), os = os->next) {
    if (modname == NULL || stricmp(os->modinfo->module->name, modname) == 0) {
      if (location == NULL || stricmp(os->location, location) == 0) {
        os_last = os;
        osp_last = osp;
      }
    }
  }
  if (os_last == NULL || osp_last == NULL) {
    snprintf(error, errsize, "could not find output stream");
    return MSERV_FAILURE;
  }
  if ((mi = module_find(os_last->modinfo->module->name)) == NULL) {
    snprintf(error, errsize, "module not loaded");
    return MSERV_FAILURE;
  }
  if (mi->output_destroy(c, os, &os_last->private,
                         error, errsize) != MSERV_SUCCESS)
    return MSERV_FAILURE;
  if (os_last->resampler)
    src_delete((SRC_STATE *)os_last->resampler);
  *osp_last = os_last->next;
  free(os_last);
  return MSERV_SUCCESS;
}

/* TODO: volume setter/getter for each output stream */

/* get or set current volume */

int channel_volume(t_channel *c, int *volume, char *error, int errsize)
{
  t_channel_outputstream *os;
  int matches = 0;

  /* look for primary vosume controsler */
  for (os = c->output; os; os = os->next) {
    if (os->modinfo->output_volume) {
      if (os->modinfo->flags | MSERV_MODFLAG_OUTPUT_VOLUME_PRIMARY)
        return os->modinfo->output_volume(c, os, os->private, volume,
                                          error, errsize);
      matches++;
    }
  }
  /* if there was only one volume able output stream, or we're reading, use
   * the first able output stream */
  if (matches == 1 || *volume == -1) {
    for (os = c->output; os; os = os->next) {
      if (os->modinfo->output_volume)
        return os->modinfo->output_volume(c, os, os->private, volume,
                                          error, errsize);
    }
  }
  /* writing to multiple output streams */
  for (os = c->output; os; os = os->next) {
    if (os->modinfo->output_volume)
      os->modinfo->output_volume(c, os, os->private, volume, error, errsize);
  }
  return MSERV_SUCCESS;
}

/* close channel stream */

int channel_close(t_channel *c, char *error, int errsize)
{
  int ret;
  t_channel_list *cl, *cl_last;

  /* close all output streams */
  while (c->output) {
    ret = channel_removeoutput(c, NULL, NULL, error, errsize);
    if (ret != MSERV_SUCCESS)
      return ret;
  }

  /* stop channel (and clear up inputs) */
  if ((ret = channel_stop(c, error, errsize)) != MSERV_SUCCESS)
    return ret;

  /* remove from list */
  for (cl_last = NULL, cl = channel_list; cl; cl_last = cl, cl = cl->next) {
    if (cl->channel == c) {
      if (cl_last)
        cl_last->next = cl->next;
      else
        channel_list = cl->next;
      break;
    }
  }

  /* free up memory */
  free(c->buffer_char);
  free(c->buffer);
  free(c);
  return MSERV_SUCCESS;
}

/* add an input stream (file handle)
 *   track is passed so that mserv_setplaying can be called when song reached
 *   if delay is given (in ms), then there will be a delay before/after play
 */

int channel_addinput(t_channel *c, int fd, t_trkinfo *trkinfo,
                     unsigned int samplerate, unsigned int channels, 
                     double delay_start, double delay_end,
                     char *error, int errsize)
{
  t_channel_inputstream *i, **tail;

  if (c->channels != channels || c->samplerate != samplerate) {
    /* TODO: perhaps we could support both mono->stereo / stereo->mono and
     *       22050->44100 / 44100->22050 conversion */
    snprintf(error, errsize, "channels/samplerate not supported");
    return MSERV_FAILURE;
  }
  if ((i = malloc(sizeof(t_channel_inputstream))) == NULL) {
    snprintf(error, errsize, "out of memory");
    return MSERV_FAILURE;
  }
  i->next = NULL;
  i->fd = fd;
  i->silence_start = delay_start * c->samplerate;
  i->silence_end = delay_end * c->samplerate;
  i->announced = 0;
  i->timer_started = 0;
  i->trkinfo = *trkinfo;
  mserv_log("channel %s: added %d/%d to stream", c->name,
            i->trkinfo.track->album->id, i->trkinfo.track->n_track);
  /* add on end of linked list */
  for (tail = &c->input; *tail; tail = &(*tail)->next) ;
  *tail = i;
  return MSERV_SUCCESS;
}

/* channel_inputfinished removes the current entry in the input queue */

int channel_inputfinished(t_channel *c)
{
  t_channel_inputstream *i = c->input;

  if (i == NULL)
    return MSERV_SUCCESS;
  mserv_log("channel %s: decoding of %d/%d finished", c->name,
            i->trkinfo.track->album->id, i->trkinfo.track->n_track);
  if (i->fd != -1)
    close(i->fd);
  c->input = i->next;
  free(i);
  if (c->input)
    mserv_log("channel %s: decoding of %d/%d started", c->name,
              i->trkinfo.track->album->id, i->trkinfo.track->n_track);
  return MSERV_SUCCESS;
}

/* sync things */

static int channel_input_sync(t_channel *c, char *error, int errsize)
{
  int ret;
  unsigned int ui;
  short int *sp;
  float *fp;
  unsigned int words;
  t_channel_outputstream *os;

  (void)error;
  (void)errsize;
  
  /* if we haven't got a full buffer, read in more from the player */
  while (c->buffer_bytes < (c->buffer_samples * 2)) {
    /* try and read more from the input stream */
    if (c->paused || c->stopped) {
      if (mserv_debug) {
        mserv_log("channel %s: %s%s: filling in silence", c->name,
                  c->paused ? "paused" : "",
                  c->stopped ? "stopped" : "");
      }
      /* we're paused, use some zeros for silence */
      memset((char *)c->buffer + c->buffer_bytes, 0,
             (c->buffer_samples * sizeof(float)) - c->buffer_bytes);
      c->buffer_bytes = c->buffer_samples * sizeof(float);
      break;
    }
    if (c->input == NULL) {
      /* no available input stream at the moment, this is different to being
       * stopped or paused, this usually means we've run out of input and a
       * new player hasn't been spawned yet */
      break;
    }
    if (c->input->silence_start > 0) {
      /* we need to create some silence (GAP) to start with */
      if (mserv_debug) {
        mserv_log("in silence - %d samples (start)", c->input->silence_start);
        mserv_log("buffer_bytes = %d (pre)", c->buffer_bytes);
        mserv_log("buf left = %d", (c->buffer_samples * 2) - c->buffer_bytes);
      }
      channel_align(c);
      /* this is somewhat confusing because buffer_size/buffer_bytes refers
       * to our 16-bit raw buffer, but we're filling our float buffer */
      ui = mserv_MIN((c->buffer_size - c->buffer_bytes) / (2 * c->channels),
                     c->input->silence_start);
      memset((char *)c->buffer + c->buffer_bytes, 0,
             ui * sizeof(float) * c->channels);
      c->input->silence_start-= ui;
      c->buffer_bytes+= ui * 2 * c->channels;
      if (mserv_debug)
        mserv_log("buffer_bytes = %d (post)", c->buffer_bytes);
      /* we may have only had a bit of silence and can fill with some data */
      continue;
    }
    if (!c->input->announced) {
      /* announce this song to users in channel, if we haven't already */
      mserv_setplaying(c, c->playing.track ? &c->playing : NULL,
		       &c->input->trkinfo);
    }
    if (!c->input->timer_started) {
      c->lastStartTime = mserv_getMSecsSinceEpoch();
      c->timeUntilLastStop = 0;
      c->input->timer_started = 1;
    }
    if (c->input->fd != -1) {
      /* read PCM data from input stream */
      ret = read(c->input->fd, c->buffer_char + c->buffer_bytes,
                 (c->buffer_samples * 2) - c->buffer_bytes);
      if (ret == -1) {
        if (errno != EAGAIN && errno != EINTR) {
          mserv_log("channel %s: failure reading on input socket for %d/%d: %s",
                    c->name, c->input->trkinfo.track->album->id,
                    c->input->trkinfo.track->n_track, strerror(errno));
        }
        break;
      } else if (ret == 0) {
        /* end of song */
        channel_align(c);
        if (mserv_debug)
          mserv_log("channel %s: EOF on input stream", c->name);
        close(c->input->fd);
        c->input->fd = -1;
      } else {
        if (mserv_debug)
          mserv_log("channel %s: %d bytes read from input stream",
                    c->name, ret);
        /* if we had a left over byte from before, and we now have the byte,
         * add one to the number of words we can now convert to floats */
        words = (ret / 2) + (((c->buffer_bytes & 1) && (ret & 1)) ? 1 : 0);
        sp = (signed short *)c->buffer_char + (c->buffer_bytes / 2);
        fp = (float *)c->buffer + (c->buffer_bytes / 2);
        for (ui = 0; ui < words; ui++)
          fp[ui] = (sp[ui] / 32768.f) *
              ((float)c->input->trkinfo.track->volume / 100);
        c->buffer_bytes += ret;
      }
    } else {
      /* we must be in the silence at the end part */
      if (c->input->silence_end <= 0) {
        channel_align(c);
        channel_inputfinished(c);
      } else {
        /* we need to create some silence (GAP) to end with */
        if (mserv_debug) {
          mserv_log("in silence - %d samples (end)", c->input->silence_end);
          mserv_log("buffer_bytes = %d (pre)", c->buffer_bytes);
          mserv_log("buf left = %d", c->buffer_size - c->buffer_bytes);
        }
        channel_align(c);
        /* this is somewhat confusing because buffer_size/buffer_bytes refers
         * to our 16-bit raw buffer, but we're filling our float buffer */
        ui = mserv_MIN((c->buffer_size - c->buffer_bytes) / (2 * c->channels),
                       c->input->silence_end);
        memset((char *)c->buffer + c->buffer_bytes, 0,
               ui * sizeof(float) * c->channels);
        c->input->silence_end-= ui;
        c->buffer_bytes+= ui * 2 * c->channels;
        if (mserv_debug)
          mserv_log("buffer_bytes = %d (post)", c->buffer_bytes);
      }
    }
  }

  /* although the data was written to c->buffer_char, we converted to floats
   * and modified according to track volume in to c->buffer */
  
  /* Prepare the output */
  for (os = c->output; os; os = os->next) {
    if (os->modinfo->output_sync) {
#ifdef HAVE_LIBSAMPLERATE
      if (os->samplerate != c->samplerate) {
        /* resample from channel buffer to output stream */
	SRC_DATA src_data;
	
        src_data.data_in = c->buffer;
        src_data.data_out = os->output;
        src_data.input_frames = c->samplerate;
        src_data.output_frames = os->samplerate;
        src_data.end_of_input = 0;
        src_data.src_ratio =
            (double)src_data.output_frames / src_data.input_frames;
        if (mserv_debug)
          mserv_log("channel %s: %s resampling: in %d, "
                    "out %d, ratio %.4f", c->name,
                    os->modinfo->module->name, src_data.input_frames,
                    src_data.output_frames, src_data.src_ratio);
        if ((ret = src_process((SRC_STATE *)os->resampler, &src_data)) != 0) {
          mserv_log("channel %s: %s output sync: "
                    "failed to resample: %s", c->name,
                    os->modinfo->module->name, src_strerror(ret));
          continue;
        }
        if ((unsigned int)src_data.input_frames_used != c->samplerate &&
            (unsigned int)src_data.output_frames_gen != os->samplerate) {
          mserv_log("channel %s: %s resampling: wanted %d->%d, got %d->%d",
                    c->samplerate, os->samplerate, src_data.input_frames_used,
                    src_data.output_frames_gen);
          continue;
        }
      } else {
#endif
        /* no modifications needed */
        os->output = c->buffer;
#ifdef HAVE_LIBSAMPLERATE
      }
#endif
      if (os->channels != c->channels) {
        /* downmix? */
        if (c->channels != 2 && os->channels != 1) {
          mserv_log("channel %s: %s output sync: unsupported channel "
                    "combination (can't downmix/upmix)", c->name,
                    os->modinfo->module->name);
          continue;
        }
        if (mserv_debug)
          mserv_log("channel %s: %s downmixing from %d channels to %d",
                    c->name, os->modinfo->module->name, c->channels,
                    os->channels);
        /* use source buffer as destination, there's no overlap */
        for (ui = 0; ui < os->samplerate; ui++) {
          os->output[ui] = (os->output[ui * 2 + 0] +
                            os->output[ui * 2 + 1]) / 2.0;
        }
      }

      // Signal the output plugin that new data has been written to
      // the output buffer
      os->bytesLeft = -1;
    }
  }
  
  return MSERV_SUCCESS;
}

int channel_sync(t_channel *c, char *error, int errsize)
{
  char ierror[256];
  struct timeval now, ago;
  int moreInputWanted = 1;
  int synchronizedOutputWanted = 1;
  t_channel_outputstream *os;
  
  /* poll modules */
  for (os = c->output; os; os = os->next) {
    if (os->modinfo->output_poll) {
      if (os->modinfo->output_poll(c, os, os->private, 
                                   ierror, sizeof(ierror)) != MSERV_SUCCESS)
        mserv_log("channel %s: %s output poll: %s", c->name,
                  os->modinfo->module->name, ierror);
    }
  }
  
  /* Are all output channels ready for more data? */
  for (os = c->output; os; os = os->next) {
    if (os->bytesLeft != 0) {
      moreInputWanted = 0;
    }
  }
  
  if (moreInputWanted) {
    int ret;
    c->buffer_bytes = 0;

    ret = channel_input_sync(c, error, errsize);
    if (ret == MSERV_FAILURE) {
      // We failed to read input
      return ret;
    }
    
    /* All output channels have received new data */
    for (os = c->output; os; os = os->next) {
      os->bytesLeft = -1;
    }
  }

  /* Have all output channels recently received fresh data? */
  for (os = c->output; os; os = os->next) {
    if (os->bytesLeft != -1) {
      // Nope
      synchronizedOutputWanted = 0;
    }
  }
  
  if (synchronizedOutputWanted) {
    if (gettimeofday(&now, NULL) == -1) {
      snprintf(error, errsize, "failed gettimeofday: %s", strerror(errno));
      return MSERV_FAILURE;
    }
  
    if (c->lasttime.tv_sec != 0) {
      mserv_timersub(&now, &c->lasttime, &ago);
      if (ago.tv_sec == 0) /* a second hasn't passed */
	return MSERV_SUCCESS;
    }
    if (mserv_debug)
      mserv_log("channel %s: next interval", c->name);

    if (c->lasttime.tv_sec == 0) {
      /* this is the first buffer we've had - set lasttime to be half
       * a second in the past - this keeps us half a second ahead of
       * the data required */
      c->lasttime.tv_sec = now.tv_sec;
      if ((c->lasttime.tv_usec = now.tv_usec - 500000) < 0) {
	c->lasttime.tv_sec-= 1;
	c->lasttime.tv_usec+= 1000000;
      }
    } else {
      c->lasttime.tv_sec+= 1;
    }
  }
  
  for (os = c->output; os; os = os->next) {
    if (os->modinfo->output_sync) {
      int result;
      
      if (mserv_debug)
        mserv_log("calling module '%s' output_sync function",
                  os->modinfo->module->name);
      result = os->modinfo->output_sync(c, os, os->private, 
					ierror, sizeof(ierror));
      if (result >= 0) {
	os->bytesLeft = result;

	if (mserv_debug) {
	  mserv_log("output_sync function returned success");
	}
      } else {
        mserv_log("channel %s: %s output sync: %s", c->name,
                  os->modinfo->module->name, ierror);
      }
    }
  }
  
  return MSERV_SUCCESS;
}

/* channel_align - align stream */

void channel_align(t_channel *c)
{
  unsigned int offset = c->buffer_bytes % (c->channels * 2);

  if (offset & 1) {
    c->buffer[c->buffer_bytes / 2] = 0.f;
    c->buffer_bytes++;
    offset--;
  }
  while (offset >= 2) {
    c->buffer[c->buffer_bytes / 2] = 0.f;
    c->buffer_bytes+= 2;
    offset-= 2;
  }
}

/* channel_replacetrack - replace track references, if any */

void channel_replacetrack(t_channel *c, t_track *track, t_track *newtrack)
{
  t_channel_inputstream *i, *next;

  for (i = c->input; i; i = next) {
    next = i->next;
    if (i->trkinfo.track == track)
      i->trkinfo.track = newtrack;
  }
  if (c->playing.track == track)
    c->playing.track = newtrack;
  return;
}

/* channel_stop clears the input queue and marks channel as stopped */

int channel_stop(t_channel *c, char *error, int errsize)
{
  t_channel_inputstream *i, *next;
  t_channel_outputstream *os;

  (void)error;
  (void)errsize;
  if (c == NULL || c->stopped)
    return MSERV_SUCCESS;
  mserv_setplaying(c, c->playing.track ? &c->playing : NULL, NULL);
  /* call output stream modules stop methods */
  for (os = c->output; os; os = os->next) {
    if (os->modinfo->output_stop)
      os->modinfo->output_stop(c, os, os->private, error, errsize);
  }
  mserv_log("channel %s: stopped playing", c->name);
  for (i = c->input; i; i = next) {
    next = i->next;
    close(i->fd);
    free(i);
  }
  c->playing.track = NULL;
  c->input = NULL;
  c->stopped = 1;
  c->paused = 0;
  return MSERV_SUCCESS;
}

/* channel_start - start play */

int channel_start(t_channel *c, char *error, int errsize)
{
  t_channel_outputstream *os;
  int ret;

  (void)error;
  (void)errsize;
  if (!c->stopped)
    return MSERV_SUCCESS;
  /* call output stream modules start methods */
  for (os = c->output; os; os = os->next) {
    if (os->modinfo->output_start) {
      ret = os->modinfo->output_start(c, os, os->private, error, errsize);
      if (ret != MSERV_SUCCESS)
        return ret;
    }
  }
  mserv_log("channel %s: started playing", c->name);
  c->stopped = 0;
  c->playing = c->input->trkinfo;
  return MSERV_SUCCESS;
}

/* channel_pause */

int channel_pause(t_channel *c, char *error, int errsize)
{
  (void)error;
  (void)errsize;

  if (c->paused)
    return MSERV_SUCCESS;
  mserv_log("channel %s: paused", c->name);
  c->paused = 1;
  // Remember for how long we played before pausing
  c->timeUntilLastStop += mserv_getMSecsSinceEpoch() - c->lastStartTime;
  return MSERV_SUCCESS;
}

/* channel_unpause */

int channel_unpause(t_channel *c, char *error, int errsize)
{
  (void)error;
  (void)errsize;
  if (!c->paused)
    return MSERV_SUCCESS;
  mserv_log("channel %s: resumed", c->name);
  c->paused = 0;
  c->lastStartTime = mserv_getMSecsSinceEpoch();
  return MSERV_SUCCESS;
}

/* channel_stopped - read whether stopped or not */

int channel_stopped(t_channel *c)
{
  return c->stopped;
}

/* channel_paused - read whether paused or not */

int channel_paused(t_channel *c)
{
  return c->paused;
}

/* channel_find - find channel structure from name */

t_channel *channel_find(const char *name)
{
  t_channel_list *cl;

  for (cl = channel_list; cl; cl = cl->next) {
    if (stricmp(cl->channel->name, name) == 0)
      return cl->channel;
  }
  return NULL;
}

/* channel_playing - get currently playing track */

t_trkinfo *channel_getplaying(t_channel *c)
{
  if (c->playing.track)
    return &c->playing;
  return NULL;
}

/* How many milliseconds has the current track been playing? */
long channel_getplaying_msecs(t_channel *c)
{
  if (c
      && c->input
      && c->input->trkinfo.track == mserv_player_playing.track
      && c->input->timer_started)
  {
    if (c->stopped) {
      return 0;
    } else if (c->paused) {
      return c->timeUntilLastStop;
    } else /* playing */ {
      return
	c->timeUntilLastStop +
	(mserv_getMSecsSinceEpoch() - c->lastStartTime);
    }
  }
  
  return 0;
}

/* How many milliseconds of sound does the channel with the smallest
 * buffer buffer? */
int channel_getSoundBufferMs(void)
{
  t_channel_list *iterator;
  int returnMe = 0;
  
  for (iterator = channel_list;
       iterator != NULL;
       iterator = iterator->next)
  {
    t_channel_outputstream *os;
    for (os = iterator->channel->output; os != NULL; os = os->next) {
      if (os->modinfo->get_buffer_ms != NULL) {
	int thisGuysBufferSize = os->modinfo->get_buffer_ms(os->private);
	if (returnMe == 0 ||
	    (thisGuysBufferSize > 0 && thisGuysBufferSize < returnMe))
	{
	  returnMe = thisGuysBufferSize;
	}
      }
    }
  }

  if (returnMe == 0) {
    // 0 means "don't know", which makes us assume all output channels
    // can actually swallow the whole second that we keep feeding
    // them.
    returnMe = 1000;
  }

  return returnMe;
}

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
  t_output_list *ol, **olend;
  t_modinfo *mi;
 
  if ((mi = module_find(modname)) == NULL) {
    snprintf(error, errsize, "module %s not loaded", modname);
    return MSERV_FAILURE;
  }
  if (strlen(location) >= sizeof(ol->location)) {
    snprintf(error, errsize, "destination location too long");
    return MSERV_FAILURE;
  }
  if ((ol = malloc(sizeof(t_output_list))) == NULL) {
    snprintf(error, errsize, "out of memory");
    return MSERV_FAILURE;
  }
  ol->next = NULL;
  ol->modinfo = mi;
  strncpy(ol->location, location, sizeof(ol->location));
  ol->location[sizeof(ol->location) - 1] = '\0';
  if (params_parse(&(ol->params), params, error, errsize) != MSERV_SUCCESS)
    return MSERV_FAILURE;
  if (mi->output_create == NULL) {
    snprintf(error, errsize, "module '%s' has no output creation function",
             modname);
    return MSERV_FAILURE;
  }
  if (mserv_debug)
    mserv_log("calling module '%s' output_create function", modname);
  if (mi->output_create(c, location, ol->params, &ol->private,
                        error, errsize) != MSERV_SUCCESS)
    return MSERV_FAILURE;
  if (mserv_debug)
    mserv_log("output_create function returned success");
  if (c->output) {
    for (olend = &c->output; (*olend)->next; olend = &((*olend)->next)) ;
    (*olend)->next = ol;
  } else {
    c->output = ol;
  }
  return MSERV_SUCCESS;
}

/* remove output from channel stream (modname or uri can be NULL) */

int channel_removeoutput(t_channel *c, const char *modname,
                        const char *location, char *error, int errsize)
{
  t_output_list *ol, *ol_last;
  t_output_list **olp, **olp_last;
  t_modinfo *mi;
 
  /* search for stream, keeping pointer so we can remove it later */
  ol_last = NULL;
  olp_last = NULL;
  for (olp = &c->output, ol = c->output; ol; olp = &(ol->next), ol = ol->next) {
    if (modname == NULL || stricmp(ol->modinfo->module->name, modname) == 0) {
      if (location == NULL || stricmp(ol->location, location) == 0) {
        ol_last = ol;
        olp_last = olp;
      }
    }
  }
  if (ol_last == NULL || olp_last == NULL) {
    snprintf(error, errsize, "could not find output stream");
    return MSERV_FAILURE;
  }
  if ((mi = module_find(ol_last->modinfo->module->name)) == NULL) {
    snprintf(error, errsize, "module not loaded");
    return MSERV_FAILURE;
  }
  if (mi->output_destroy(c, &ol->private, error, errsize) != MSERV_SUCCESS)
    return MSERV_FAILURE;
  *olp_last = ol_last->next;
  free(ol_last);
  return MSERV_SUCCESS;
}

/* TODO: volume setter/getter for each output stream */

/* get or set current volume */

int channel_volume(t_channel *c, int *volume, char *error, int errsize)
{
  t_output_list *ol;
  int matches = 0;

  /* look for primary volume controller */
  for (ol = c->output; ol; ol = ol->next) {
    if (ol->modinfo->output_volume) {
      if (ol->modinfo->flags | MSERV_MODFLAG_OUTPUT_VOLUME_PRIMARY)
        return ol->modinfo->output_volume(c, ol->private, volume,
                                          error, errsize);
      matches++;
    }
  }
  /* if there was only one volume able output stream, or we're reading, use
   * the first able output stream */
  if (matches == 1 || *volume == -1) {
    for (ol = c->output; ol; ol = ol->next) {
      if (ol->modinfo->output_volume)
        return ol->modinfo->output_volume(c, ol->private, volume,
                                          error, errsize);
    }
  }
  /* writing to multiple output streams */
  for (ol = c->output; ol; ol = ol->next) {
    if (ol->modinfo->output_volume)
      ol->modinfo->output_volume(c, ol->private, volume, error, errsize);
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

int channel_addinput(t_channel *c, int fd, t_supinfo *track_supinfo,
                     unsigned int samplerate, unsigned int channels, 
                     double delay_start, double delay_end,
                     char *error, int errsize)
{
  t_output_inputstream *i, **tail;

  if (c->channels != channels || c->samplerate != samplerate) {
    /* TODO: perhaps we could support both mono->stereo / stereo->mono and
     *       22050->44100 / 44100->22050 conversion */
    snprintf(error, errsize, "channels/samplerate not supported");
    return MSERV_FAILURE;
  }
  if ((i = malloc(sizeof(t_output_inputstream))) == NULL) {
    snprintf(error, errsize, "out of memory");
    return MSERV_FAILURE;
  }
  i->next = NULL;
  i->fd = fd;
  i->zeros_start = delay_start * c->samplerate * c->channels * 2;
  i->zeros_end = delay_end * c->samplerate * c->channels * 2;
  i->announced = 0;
  i->supinfo = *track_supinfo;
  if (c->input == NULL)
    mserv_log("channel %s: decoding of %d/%d started", c->name,
              i->supinfo.track->n_album, i->supinfo.track->n_track);
  /* add on end of linked list */
  for (tail = &c->input; *tail; tail = &(*tail)->next) ;
  *tail = i;
  return MSERV_SUCCESS;
}

/* channel_inputfinished removes the current entry in the input queue */

int channel_inputfinished(t_channel *c)
{
  t_output_inputstream *i = c->input;

  if (i == NULL)
    return MSERV_SUCCESS;
  mserv_log("channel %s: decoding of %d/%d finished", c->name,
            i->supinfo.track->n_album, i->supinfo.track->n_track);
  if (i->fd != -1)
    close(i->fd);
  c->input = i->next;
  free(i);
  if (c->input)
    mserv_log("channel %s: decoding of %d/%d started", c->name,
              i->supinfo.track->n_album, i->supinfo.track->n_track);
  return MSERV_SUCCESS;
}

/* sync things */

int channel_sync(t_channel *c, char *error, int errsize)
{
  char ierror[256];
  struct timeval now, ago;
  t_output_list *ol;
  int ret;
  unsigned int ui;
  short int *sp;
  float *fp;
  unsigned int words;

  /* poll modules */
  for (ol = c->output; ol; ol = ol->next) {
    if (ol->modinfo->output_poll) {
      if (ol->modinfo->output_poll(c, ol->private, 
                                   ierror, sizeof(ierror)) != MSERV_SUCCESS)
        mserv_log("channel %s: %s output poll: %s", c->name,
                  ol->modinfo->module->name, ierror);
    }
  }

  /* if we haven't got a full buffer, read in more from the player */
  while (c->buffer_bytes < (c->buffer_samples * 2)) {
    /* try and read more from the input stream */
    if (c->paused || c->stopped) {
      if (mserv_debug) {
        mserv_log("channel %s: %s%s: filling in silence", c->name,
                  c->paused ? "paused" : "",
                  c->stopped ? "stopped" : "");
      }
      /* we're paused, channel some zeros */
      memset(c->buffer + c->buffer_bytes, 0,
             (c->buffer_samples * 2) - c->buffer_bytes);
      c->buffer_bytes = c->buffer_samples * 2;
      break;
    }
    if (c->input == NULL) {
      /* no available input stream at the moment, this is different to being
       * stopped or paused, this usually means we've run out of input and a
       * new player hasn't been spawned yet */
      break;
    }
    if (c->input->zeros_start > 0) {
      if (mserv_debug) {
        mserv_log("in silence - %d bytes to go", c->input->zeros_start);
        mserv_log("buffer_bytes = %d (pre)", c->buffer_bytes);
        mserv_log("buf left = %d", (c->buffer_samples * 2) - c->buffer_bytes);
      }
      /* we need to channel some silence (GAP) to start with */
      ui = mserv_MIN(c->buffer_size - c->buffer_bytes, c->input->zeros_start);
      memset(c->buffer_char + c->buffer_bytes, 0, ui);
      c->input->zeros_start-= ui;
      c->buffer_bytes+= ui;
      if (mserv_debug)
        mserv_log("buffer_bytes = %d (post)", c->buffer_bytes);
      /* we may have only had a bit of silence and can fill with some data */
      continue;
    }
    if (!c->input->announced) {
      /* announce this song to users in channel, if we haven't already */
      mserv_setplaying(&c->input->supinfo);
      c->input->announced = 1;
    }
    if (c->input->fd != -1) {
      /* read PCM data from input stream */
      ret = read(c->input->fd, c->buffer_char + c->buffer_bytes,
                 (c->buffer_samples * 2) - c->buffer_bytes);
      if (ret == -1) {
        if (errno != EAGAIN || errno != EINTR) {
          mserv_log("channel %s: failure reading on input socket for %d/%d: %s",
                    c->name, c->input->supinfo.track->n_album,
                    c->input->supinfo.track->n_track, strerror(errno));
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
        words = (ret / 2) + ((c->buffer_bytes & 1) && (ret & 1) ? 1 : 0);
        sp = (signed short *)c->buffer_char + (c->buffer_bytes / 2);
        fp = (float *)c->buffer + (c->buffer_bytes / 2);
        for (ui = 0; ui < words; ui++)
          fp[ui] = (sp[ui] / 32768.f) *
              ((float)c->input->supinfo.track->volume / 100);
        c->buffer_bytes += ret;
      }
    } else {
      /* we must be in the silence at the end part */
      if (c->input->zeros_end <= 0) {
        channel_align(c);
        channel_inputfinished(c);
      } else {
        if (mserv_debug) {
          mserv_log("in silence - %d bytes to go", c->input->zeros_end);
          mserv_log("buf left = %d", c->buffer_size - c->buffer_bytes);
          mserv_log("buffer_bytes = %d (pre)", c->buffer_bytes);
        }
        /* we need to channel some silence (GAP) to end with */
        ui = mserv_MIN(c->buffer_size - c->buffer_bytes, c->input->zeros_end);
        memset(c->buffer + c->buffer_bytes, 0, ui);
        c->input->zeros_end-= ui;
        c->buffer_bytes+= ui;
        if (mserv_debug)
          mserv_log("buffer_bytes = %d (post)", c->buffer_bytes);
      }
    }
  }
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
    /* this is the first buffer we've had - set lasttime to be half a second
     * in the past - this keeps us half a second ahead of the data required */
    c->lasttime.tv_sec = now.tv_sec;
    if ((c->lasttime.tv_usec = now.tv_usec - 500000) < 0) {
      c->lasttime.tv_sec-= 1;
      c->lasttime.tv_usec+= 1000000;
    }
  } else {
    c->lasttime.tv_sec+= 1;
  }
  for (ol = c->output; ol; ol = ol->next) {
    if (ol->modinfo->output_sync) {
      if (mserv_debug)
        mserv_log("calling module '%s' output_sync function",
                  ol->modinfo->module->name);
      if (ol->modinfo->output_sync(c, ol->private, 
                                   ierror, sizeof(ierror)) != MSERV_SUCCESS)
        mserv_log("channel %s: %s output sync: %s", c->name,
                  ol->modinfo->module->name, ierror);
      if (mserv_debug)
        mserv_log("output_sync function returned success");
    }
  }
  c->buffer_bytes = 0;
  return MSERV_SUCCESS;
}

/* channel_align - align stream */

void channel_align(t_channel *c)
{
  int offset = c->buffer_bytes % (c->channels * 2);

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
  t_output_inputstream *i, *next;

  for (i = c->input; i; i = next) {
    next = i->next;
    if (i->supinfo.track == track)
      i->supinfo.track = newtrack;
  }
  return;
}

/* channel_stop clears the input queue and marks channel as stopped */

int channel_stop(t_channel *c, char *error, int errsize)
{
  t_output_inputstream *i, *next;
  t_output_list *ol;

  (void)error;
  (void)errsize;
  if (c->stopped)
    return MSERV_SUCCESS;
  /* call output stream modules stop methods */
  for (ol = c->output; ol; ol = ol->next) {
    if (ol->modinfo->output_stop)
      ol->modinfo->output_stop(c, ol->private, error, errsize);
  }
  mserv_log("channel %s: stopped playing", c->name);
  for (i = c->input; i; i = next) {
    next = i->next;
    close(i->fd);
    free(i);
  }
  c->input = NULL;
  c->stopped = 1;
  c->paused = 0;
  mserv_setplaying(NULL);
  return MSERV_SUCCESS;
}

/* channel_start - start play */

int channel_start(t_channel *c, char *error, int errsize)
{
  t_output_list *ol;

  (void)error;
  (void)errsize;
  if (!c->stopped)
    return MSERV_SUCCESS;
  /* call output stream modules start methods */
  for (ol = c->output; ol; ol = ol->next) {
    if (ol->modinfo->output_start)
      ol->modinfo->output_start(c, ol->private, error, errsize);
  }
  mserv_log("channel %s: started playing", c->name);
  c->stopped = 0;
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

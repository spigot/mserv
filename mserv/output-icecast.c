/*
All of the documentation and software included in the Mserv releases is
copyrighted by James Ponder <james@squish.net>.

Copyright 1999-2003 James Ponder.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* All advertising materials mentioning features or use of this software,
  must display the following acknowledgement:
  "This product includes software developed by James Ponder."

* Neither the name of myself nor the names of its contributors may be used
  to endorse or promote products derived from this software without
  specific prior written permission.

* THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
  OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _GNU_SOURCE 1
#define _BSD_SOURCE 1
#define __EXTENSIONS__ 1

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <shout/shout.h>
#include <vorbis/vorbisenc.h>

#include "config.h"
#include "mserv.h"
#include "output-icecast.h"

#define DEBUG_OUTPUT

/*
 *
 * There are two buffers:
 *
 *   buffer_char/buffer_float/buffer_bytes/buffer_size
 *     the raw PCM data (buffer_char) from the player we have a as child
 *     process, we try and maintain this as having a full second's worth of
 *     data.  buffer_float is the same as buffer_char after being converted
 *     to floats and modified in volume
 *
 *   buffer_ready/buffer_ready_bytes/buffer_ready_size
 *     the dynamically resizing ogg vorbis encoded data.  we fill this up
 *     with the data from the other buffer, via libvorbisenc.  We only do
 *     this once, when the PCM data buffer is full, so this should be
 *     one second's worth of encoded data.
 *
 *   We use libshout's delay mechanism to tell us when more data is required,
 *   and when it is, we send it all of the ogg vorbis blocks we've got, 
 *   which is therefore on average a second's worth of data.  We then let
 *   libshout tell us to wait a while before throwing more data at it.
 */

/*
 * An output stream is created with output_create, setting up the necessary
 * parameters and connections.
 *
 * Inputs are passed to output_addinput in the form of a file handle
 * containing a stream of raw PCM data.  These file handles are placed in a
 * link list so that we could potentially have a new stream before the old one
 * has finished.
 *
 * Stopping or pausing makes the output routines generate silence.
 *
 */

/* init output sub-system */

int output_init(void)
{
  shout_init();
  return 0;
}

/* create output stream
 * returns t_output object on success, -1 (and errno) on failure */

t_output *output_create(const char *destination, const char *parameters,
                        char **error)
{
  t_output *o;
  char *user, *pass, *host, *port;
  char mount[128];
  char splitbuf[128];
  char *p;

  if ((o = malloc(sizeof(t_output))) == NULL)
    return NULL;
  o->input = NULL;
  o->paused = 0;
  o->stopped = 0;
  o->channels = 2;
  o->samplerate = 44100;
  o->bitrate = atoi(parameters);
  o->buffer_size = o->samplerate * o->channels * 2;
  if ((o->buffer_char = malloc(o->buffer_size)) == NULL) {
    free(o);
    return NULL;
  }
  /* buffer_float isn't 16 bit, it's probably 32 bit */
  if ((o->buffer_float = malloc(sizeof(float) *
                                (o->buffer_size / 2))) == NULL) {
    free(o);
    return NULL;
  }
  o->buffer_bytes = 0;
  o->volume = 50;
  o->buffer_ready = NULL;
  o->buffer_ready_bytes = 0;
  o->buffer_ready_size = 0;
  if (!(o->shout = shout_new())) {
    mserv_log("Failed to allocate shout object");
    *error = "Failed to allocate shout object";
    goto failed;
  }
  /* now take a copy of destination in o->url and into splitbuf for splitting */
  if (strlen(destination) >= sizeof(o->url) ||
      strlen(destination) >= sizeof(splitbuf)) {
    *error = "Output destination URL too long";
    goto failed;
  }
  strncpy(o->url, destination, sizeof(o->url));
  o->url[sizeof(o->url) - 1] = '\0';
  strncpy(splitbuf, destination, sizeof(splitbuf));
  splitbuf[sizeof(splitbuf) - 1] = '\0';
  if (strncmp(o->url, "http://", strlen("http://")) != 0) {
    *error = "Only http:// Icecast URLs are supported";
    goto failed;
  }
  p = splitbuf + strlen("http://");
  user = p; while (*p && *p != ':') p++; *p++ = '\0';
  pass = p; while (*p && *p != '@') p++; *p++ = '\0';
  host = p; while (*p && *p != ':') p++; *p++ = '\0';
  port = p; while (*p && *p != '/') p++; *p++ = '\0';
  if ((strlen(p) + 1) >= sizeof(mount)) {
    *error = "Mount portion of URL too long";
    goto failed;
  }
  snprintf(mount, sizeof(mount), "/%s", p);
  mserv_log("Request to icecast connect to %s:%s", host, port);
  if (!*user || !*pass || !*host || !*port || !mount[1]) {
    *error = "Icecast location invalid, use http://user:pass@host:port/mount";
    goto failed;
  }
  if (shout_set_host(o->shout, host) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast hostname: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast hostname";
    goto failed;
  }
  if (shout_set_protocol(o->shout, SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast protocol: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast protocol";
    goto failed;
  }
  if (shout_set_port(o->shout, atoi(port)) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast port: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast port";
    goto failed;
  }
  if (shout_set_password(o->shout, pass) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast password: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast password";
    goto failed;
  }
  if (shout_set_mount(o->shout, mount) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast hostname: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast hostname";
    goto failed;
  }
  if (shout_set_user(o->shout, user) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast user: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast user";
    goto failed;
  }
  if (shout_set_format(o->shout, SHOUT_FORMAT_VORBIS) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast format: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast format";
    goto failed;
  }
  if (shout_open(o->shout) != SHOUTERR_SUCCESS) {
    mserv_log("Failed opening Icecast connection: %s",
              shout_get_error(o->shout));
    *error = "Failed opening Icecast connection";
    goto failed;
  }
  mserv_log("Successfully connected to Icecast host '%s@%s' for mount '%s'",
            host, port, mount);

  vorbis_info_init(&o->vi);
  if (vorbis_encode_init(&o->vi, o->channels, o->samplerate, -1,
                         o->bitrate, -1) != 0) {
    *error = "Failed to initialise vorbis engine";
    goto failed;
  }
  vorbis_comment_init(&o->vc);
  vorbis_comment_add_tag(&o->vc, "ENCODER", "Mserv " VERSION);
  vorbis_analysis_init(&o->vd, &o->vi);
  vorbis_block_init(&o->vd, &o->vb);
  ogg_stream_init(&o->os, rand());
  {
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    vorbis_analysis_headerout(&o->vd, &o->vc,
                              &header, &header_comm, &header_code);
    ogg_stream_packetin(&o->os, &header);
    ogg_stream_packetin(&o->os, &header_comm);
    ogg_stream_packetin(&o->os,&header_code);
    for (;;) {
      int result = ogg_stream_flush(&o->os, &o->og);
      if (result == 0)
        break;
      if (shout_send(o->shout, o->og.header,
                     o->og.header_len) != SHOUTERR_SUCCESS) {
        mserv_log("Failed to send to shout: %s", shout_get_error(o->shout));
        *error = "Failed to send Ogg header";
        goto failed;
      }
      if (shout_send(o->shout, o->og.body,
                     o->og.body_len) != SHOUTERR_SUCCESS) {
        mserv_log("Failed to send to shout: %s", shout_get_error(o->shout));
        *error = "Failed to send Ogg header";
        goto failed;
      }
    }
  }
  /*
  if (gettimeofday(&o->lasttime, NULL) == -1) {
    *error = "Failed to gettimeofday()";
    return NULL;
  }
  */
  return o;
failed:
  free(o->buffer_char);
  free(o->buffer_float);
  free(o);
  return NULL;
}

/* get current volume */

int output_getvolume(t_output *o)
{
  return o->volume;
}

/* set volume, returns new volume */

int output_setvolume(t_output *o, int volume)
{
  o->volume = volume;
  return o->volume;
}

/* close output stream */

void output_close(t_output *o)
{
  output_stop(o);
  free(o->buffer_char);
  free(o->buffer_float);
  shout_close(o->shout);
}

/* finalise output system */

void output_final(void)
{
  shout_shutdown();
}

/* add an input file handle
 * track is passed so that mserv_setplaying can be called when song reached
 * if continuation is not set, clears queue
 * if delay is given (in milliseconds), then there will be a delay before play
 * returns -1 on error, 0 on success
 */

int output_addinput(t_output *o, int fd, t_supinfo *track_supinfo,
                    int samplerate, int channels, int continuation,
                    double delay_start, double delay_end)
{
  t_input *i, **tail;

  if (o->channels != channels || o->samplerate != samplerate) {
    /* TODO: perhaps we could support both mono->stereo / stereo->mono and
     *       22050->44100 / 44100->22050 conversion */
    mserv_log("Channels and sample rate of input do not match output");
    return -1;
  }
  if (!continuation) {
    /* we're not continuing on from current songs, clear everything out */
    output_stop(o);
  }
  if ((i = malloc(sizeof(t_input))) == NULL) {
    mserv_log("Failed to malloc for new input stream");
    return -1;
  }
  i->next = NULL;
  i->fd = fd;
  i->zeros_start = delay_start * o->samplerate * o->channels * 2;
  i->zeros_end = delay_end * o->samplerate * o->channels * 2;
  i->announced = 0;
  i->supinfo = *track_supinfo;
  /* find tail of linked list */
  for (tail = &o->input; *tail; tail = &(*tail)->next) ;
  /* store new input on end of list */
  *tail = i;
  return 0;
}

/* output_inputfinished removes the current entry in the input queue,
 * probably because we've finished it
 * returns 0 if there was nothing playing, 1 if something was cleared */

int output_inputfinished(t_output *o)
{
  t_input *i;

  if (o->input == NULL)
    return 0;
  i = o->input;
  mserv_log("Decoding of %d/%d finished", i->supinfo.track->n_album,
            i->supinfo.track->n_track);
  if (i->fd != -1)
    close(i->fd);
  o->input = i->next;
  free(i);
  i = o->input;
  if (i) {
    mserv_log("Decoding of %d/%d started", i->supinfo.track->n_album,
              i->supinfo.track->n_track);
  }
  return 1;
}

/* sync things */

void output_sync(t_output *o)
{
  int ret;
  unsigned int chan, i;
  unsigned int pages;
  int datasize, reqbufsize;
  char *newbuf;
  short int *sp;
  float *fp;
  float **vorbbuf;
  int words;

  /* if it's time to send data, send everything we have */
  if (shout_delay(o->shout) <= 0) {
#ifdef DEBUG_OUTPUT
    mserv_log("delay sync");
#endif
    if (o->buffer_ready_bytes <= 0) {
      mserv_log("Output underrun for mount '%s'!", o->url);
    } else {
      if (shout_send(o->shout, o->buffer_ready,
                     o->buffer_ready_bytes) != SHOUTERR_SUCCESS) {
        mserv_log("Failed to send to shout: %s", shout_get_error(o->shout));
      }
    }
    o->buffer_ready_bytes = 0;
  }
  /* if we have no ready buffer, and we have a full normal buffer,
   * send it to vorbis */
  if (o->buffer_ready_bytes == 0 && o->buffer_bytes == o->buffer_size) {
    vorbbuf = vorbis_analysis_buffer(&o->vd,
                                     o->buffer_size / (o->channels * 2));
#ifdef DEBUG_OUTPUT
    mserv_log("another second of data, time to analyse block with vorbis");
#endif
    /* buffer_size is in bytes - divide by 2 for 16-bit samples */
    for (i = 0; i < o->buffer_size / (2 * o->channels); i++) {
      /* apply current volume setting (0-100, where 50 is normal) */
      for (chan = 0; chan < o->channels; chan++) {
        vorbbuf[chan][i] = o->buffer_float[i * o->channels + chan] *
            ((float)o->volume / 50);
        if (vorbbuf[chan][i] > 1.0f)
          vorbbuf[chan][i] = 1.0f;
        if (vorbbuf[chan][i] < -1.0f)
          vorbbuf[chan][i] = -1.0f;
      }
    }
    vorbis_analysis_wrote(&o->vd, o->buffer_size / (o->channels * 2));
    o->buffer_bytes = 0;
  }
  /* if we haven't got a full buffer, read in more from the player */
  while (o->buffer_bytes < o->buffer_size) {
    /* try and read more from the input stream */
    if (o->paused || o->stopped) {
#ifdef DEBUG_OUTPUT
      mserv_log("filling in silence, reason: %s %s",
                o->paused ? "paused" : "not-paused",
                o->stopped ? "stopped" : "not-stopped");
#endif
      /* we're paused, output some zeros */
      memset(o->buffer_float + o->buffer_bytes, 0,
             o->buffer_size - o->buffer_bytes);
      o->buffer_bytes = o->buffer_size;
      break;
    }
    if (o->input == NULL) {
      /* no available input stream at the moment */
      /* this is different to being stopped or paused, this usually means we've
       * run out of input and a new player hasn't been spawned yet */
      break;
    }
    if (o->input->zeros_start > 0) {
#ifdef DEBUG_OUTPUT
      mserv_log("in silence - %d bytes to go", o->input->zeros_start);
      mserv_log("buf left = %d", o->buffer_size - o->buffer_bytes);
      mserv_log("buffer_bytes = %d", o->buffer_bytes);
#endif
      /* we need to output some silence (GAP) to start with */
      i = mserv_MIN(o->buffer_size - o->buffer_bytes, o->input->zeros_start);
      memset(o->buffer_float + o->buffer_bytes, 0, i);
      o->input->zeros_start-= i;
      o->buffer_bytes+= i;
      mserv_log("buffer_bytes = %d", o->buffer_bytes);
      /* we may have only had a bit of silence and can fill with some data */
      continue;
    }
    if (!o->input->announced) {
      mserv_setplaying(&o->input->supinfo);
      o->input->announced = 1;
    }
    if (o->input->fd != -1) {
      /* read PCM data from input stream */
      ret = read(o->input->fd, o->buffer_char + o->buffer_bytes,
                 o->buffer_size - o->buffer_bytes);
      if (ret == -1) {
        if (errno != EAGAIN || errno != EINTR) {
          mserv_log("Failure reading on input socket for mount '%s': %s",
                    o->url, strerror(errno));
        }
        break;
      } else if (ret == 0) {
        /* end of song */
        mserv_log("End of file properly reached in input stream");
        close(o->input->fd);
        o->input->fd = -1;
        if (o->buffer_bytes & 1) {
          mserv_log("Odd number of bytes in 16 bit input stream!");
          o->buffer_float[o->buffer_bytes / 2] = 0.f;
          o->buffer_bytes++;
        }
      } else {
        mserv_log("%d bytes read from input stream", ret);
        /* if we had a left over byte from before, and we now have the byte,
         * add one to the number of words we can now convert to floats */
        words = (ret / 2) + ((o->buffer_bytes & 1) && (ret & 1) ? 1 : 0);
        mserv_log("%d words", words);
        sp = (signed short *)o->buffer_char + (o->buffer_bytes / 2);
        fp = (float *)o->buffer_float + (o->buffer_bytes / 2);
        for (i = 0; i < words; i++)
          fp[i] = (sp[i] / 32768.f) *
              ((float)o->input->supinfo.track->volume / 100);
        o->buffer_bytes += ret;
      }
    } else {
      /* we must be in the silence at the end part */
      if (o->input->zeros_end <= 0) {
        output_inputfinished(o);
      } else {
#ifdef DEBUG_OUTPUT
        mserv_log("in silence - %d bytes to go", o->input->zeros_end);
        mserv_log("buf left = %d", o->buffer_size - o->buffer_bytes);
        mserv_log("buffer_bytes = %d", o->buffer_bytes);
#endif
        /* we need to output some silence (GAP) to end with */
        i = mserv_MIN(o->buffer_size - o->buffer_bytes, o->input->zeros_end);
        memset(o->buffer_float + o->buffer_bytes, 0, i);
        o->input->zeros_end-= i;
        o->buffer_bytes+= i;
        mserv_log("buffer_bytes = %d", o->buffer_bytes);
      }
    }
  }
  /* does vorbis have anything decoded from what we've already sent it via
   * vorbis_analysis_wrote above? */
  pages = 0;
  while (vorbis_analysis_blockout(&o->vd, &o->vb) == 1) {
    vorbis_analysis(&o->vb, NULL);
    vorbis_bitrate_addblock(&o->vb);
    while (vorbis_bitrate_flushpacket(&o->vd, &o->op)) {
      ogg_stream_packetin(&o->os, &o->op);
      while (!ogg_page_eos(&o->og)) {
        if (ogg_stream_pageout(&o->os, &o->og) == 0) {
          if (pages || ogg_stream_flush(&o->os, &o->og) == 0)
            break;
        }
        pages++;
        datasize = o->og.header_len + o->og.body_len;
        reqbufsize = o->buffer_ready_bytes + datasize;
        reqbufsize = ((reqbufsize / 8192) + 1) * 8192; /* add granularity */
        if (reqbufsize > o->buffer_ready_size) {
          newbuf = realloc(o->buffer_ready, reqbufsize);
          mserv_log("Extending output buffer size from %d to %d bytes",
                    o->buffer_ready_size, reqbufsize);
          if (newbuf == NULL) {
            mserv_log("Out of memory extending output buffer");
            exit(1);
          }
          o->buffer_ready = newbuf;
          o->buffer_ready_size = reqbufsize;
        }
        memcpy(o->buffer_ready + o->buffer_ready_bytes,
               o->og.header, o->og.header_len);
        memcpy(o->buffer_ready + o->buffer_ready_bytes + o->og.header_len,
               o->og.body, o->og.body_len);
        o->buffer_ready_bytes+= datasize;
      }
    }
  }
#ifdef DEBUG_OUTPUT
  if (pages)
    mserv_log("Wrote %d Ogg pages (buffer now %d)", pages,
              o->buffer_ready_bytes);
#endif
}

/* end of stream: vorbis_analysis_wrote(&o->vd, 0); */
/* maybe we never do that? - close of room/chan? */

/* output_delay - return the ideal delay until next call to output_sync */

int output_delay(t_output *o)
{
  int delay = shout_delay(o->shout);
  return (delay < 0) ? 0 : delay;
}

/* output_replacetrack - replace track references, if any */

void output_replacetrack(t_output *o, t_track *track, t_track *newtrack)
{
  t_input *i, *next;

  if (o->input == NULL)
    return;
  for (i = o->input; i; i = next) {
    next = i->next;
    if (i->supinfo.track == track)
      i->supinfo.track = newtrack;
  }
  return;
}

/* output_stop clears the input queue and marks output as stopped
 * returns 0 if there was nothing playing, 1 if something was cleared */

int output_stop(t_output *o)
{
  t_input *i, *next;
  int old = o->stopped;

  for (i = o->input; i; i = next) {
    next = i->next;
    close(i->fd);
    free(i);
  }
  o->input = NULL;
  o->stopped = 1;
  o->paused = 0;
  if (old != 1)
    mserv_setplaying(NULL);
  return 1;
}

/* output_start - start play, returns whether stopped (1) or not (0) */

int output_start(t_output *o)
{
  int old = o->stopped;
  o->stopped = 0;
  return old;
}

/* output_pause - set pause flag (0 or 1), returns previous */

int output_pause(t_output *o, int pause)
{
  int old = o->paused;

  o->paused = pause;
  mserv_log("paused");
  return old;
}

/* output_stopped - read whether stopped or not */

int output_stopped(t_output *o)
{
  return o->stopped;
}

/* output_paused - read whether paused or not */

int output_paused(t_output *o)
{
  return o->paused;
}

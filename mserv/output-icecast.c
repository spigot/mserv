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
 *   buffer/buffer_bytes
 *     the raw PCM data from the player we have a as child process, we try
 *     and maintain this as having a full second's worth of data
 *
 *   buffer_ready/buffer_ready_bytes/buffer_ready_size
 *     the dynamically resizing ogg vorbis encoded data.  we fill this up
 *     with the data from the other buffer, via libvorbisenc.  We only do
 *     this once, when the PCM data buffer is full, so this should be about
 *     one second's worth of encoded data.
 *
 *   We use libshout's delay mechanism to tell us when more data is required,
 *   and when it is, we send it all of the ogg vorbis blocks we've got, 
 *   which is therefore on average a second's worth of data.  We then let
 *   libshout tell us to wait a while before throwing more data at it.
 */

/* init output sub-system */

int output_init(void)
{
  shout_init();
  return 0;
}

/* create output stream
 * returns t_output object on success, -1 (and errno) on failure */


t_output *output_create(const char *destination, char **error)
{
  t_output *o;
  char *user, *pass, *host, *port;
  char mount[128];
  char splitbuf[128];
  char *p;

  if ((o = malloc(sizeof(t_output))) == NULL)
    return NULL;
  o->input = -1;
  o->volume = 0;
  o->buffer_ready = NULL;
  o->buffer_bytes = 0;
  if (!(o->shout = shout_new())) {
    mserv_log("Failed to allocate shout object");
    *error = "Failed to allocate shout object";
    return NULL;
  }
  /* now take a copy of destination in o->url and into splitbuf for splitting */
  if (strlen(destination) >= sizeof(o->url) ||
      strlen(destination) >= sizeof(splitbuf)) {
    *error = "Output destination URL too long";
    return NULL;
  }
  strncpy(o->url, destination, sizeof(o->url));
  o->url[sizeof(o->url) - 1] = '\0';
  strncpy(splitbuf, destination, sizeof(splitbuf));
  splitbuf[sizeof(splitbuf) - 1] = '\0';
  if (strncmp(o->url, "http://", strlen("http://")) != 0) {
    *error = "Only http:// Icecast URLs are supported";
    return NULL;
  }
  p = splitbuf + strlen("http://");
  user = p; while (*p && *p != ':') p++; *p++ = '\0';
  pass = p; while (*p && *p != '@') p++; *p++ = '\0';
  host = p; while (*p && *p != ':') p++; *p++ = '\0';
  port = p; while (*p && *p != '/') p++; *p++ = '\0';
  if ((strlen(p) + 1) >= sizeof(mount)) {
    *error = "Mount portion of URL too long";
    return NULL;
  }
  snprintf(mount, sizeof(mount), "/%s", p);
  mserv_log("Request to icecast connect to %s:%s", host, port);
  if (!*user || !*pass || !*host || !*port || !mount[1]) {
    *error = "Icecast location invalid, use http://user:pass@host:port/mount";
    return NULL;
  }
  if (shout_set_host(o->shout, host) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast hostname: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast hostname";
    return NULL;
  }
  if (shout_set_protocol(o->shout, SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast protocol: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast protocol";
    return NULL;
  }
  if (shout_set_port(o->shout, atoi(port)) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast port: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast port";
    return NULL;
  }
  if (shout_set_password(o->shout, pass) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast password: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast password";
    return NULL;
  }
  if (shout_set_mount(o->shout, mount) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast hostname: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast hostname";
    return NULL;
  }
  if (shout_set_user(o->shout, user) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast user: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast user";
    return NULL;
  }
  if (shout_set_format(o->shout, SHOUT_FORMAT_VORBIS) != SHOUTERR_SUCCESS) {
    mserv_log("Failed setting Icecast format: %s", shout_get_error(o->shout));
    *error = "Failed setting Icecast format";
    return NULL;
  }
  if (shout_open(o->shout) != SHOUTERR_SUCCESS) {
    mserv_log("Failed opening Icecast connection: %s",
              shout_get_error(o->shout));
    *error = "Failed opening Icecast connection";
    return NULL;
  }
  mserv_log("Successfully connected to Icecast host '%s@%s' for mount '%s'",
            host, port, mount);

  vorbis_info_init(&o->vi);
  if (vorbis_encode_init(&o->vi, 2, MSERV_SAMPLERATE, -1, 48000, -1) != 0) {
    *error = "Failed to initialise vorbis engine";
    return NULL;
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
        return NULL;
      }
      if (shout_send(o->shout, o->og.body,
                     o->og.body_len) != SHOUTERR_SUCCESS) {
        mserv_log("Failed to send to shout: %s", shout_get_error(o->shout));
        *error = "Failed to send Ogg header";
        return NULL;
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
  shout_close(o->shout);
}

/* finalise output system */

void output_final(void)
{
  shout_shutdown();
}

/* set input file handle */

void output_setinput(t_output *o, int input)
{
  if (o->input != -1)
    close(o->input);
  o->input = input;
}

/* close input stream */

int output_closeinput(t_output *o)
{
  if (o->input != -1) {
    close(o->input);
    o->input = -1;
    return 1;
  }
  return 0;
}

/* sync things */

void output_sync(t_output *o)
{
  int ret;
  unsigned int i;
  unsigned int pages;
  int datasize, reqbufsize;
  char *newbuf;
  float **vorbbuf;

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
  if (o->buffer_ready_bytes == 0 && o->buffer_bytes == sizeof(o->buffer)) {
    vorbbuf = vorbis_analysis_buffer(&o->vd, MSERV_SAMPLERATE);
#ifdef DEBUG_OUTPUT
    mserv_log("another second, time to analyse block with vorbis");
#endif
    for (i = 0; i < sizeof(o->buffer) / 4; i++) {
      vorbbuf[0][i] = ((o->buffer[i * 4 + 1] << 8) |
                       (0x00ff & o->buffer[i * 4])) / 32768.f;
      vorbbuf[1][i] = ((o->buffer[i * 4 + 3] << 8) |
                       (0x00ff & o->buffer[i * 4 + 2])) / 32768.f;
    }
    vorbis_analysis_wrote(&o->vd, sizeof(o->buffer) / 4);
    o->buffer_bytes = 0;
  }
  /* if we haven't got a full buffer, read in more from the player */
  if (o->buffer_bytes < sizeof(o->buffer)) {
    /* try and read more from the input stream */
    if (o->input == -1) {
#ifdef DEBUG_OUTPUT
      mserv_log("filling buffer with nothing");
#endif
      /* there's no input stream right now - so make stream silent */
      memset(o->buffer + o->buffer_bytes, 0,
             sizeof(o->buffer) - o->buffer_bytes);
      o->buffer_bytes = sizeof(o->buffer);
    } else {
      /* read PCM data from input stream */
      ret = read(o->input, o->buffer + o->buffer_bytes,
                 sizeof(o->buffer) - o->buffer_bytes);
      if (ret == -1) {
        if (errno != EAGAIN || errno != EINTR) {
          mserv_log("Failure reading on input socket for mount '%s': %s",
                    o->url, strerror(errno));
        }
      } else if (ret == 0) {
        mserv_log("end of song");
        /* end of song */
        o->input = -1;
      }
      o->buffer_bytes += ret;
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

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

#include <shout/shout.h>
#include <vorbis/vorbisenc.h>

#include "mserv.h"

static char mserv_rcs_id[] = "$Id: icecast.c,v 1.2 2003/11/11 00:11:43 squish Exp $";
MSERV_MODULE(icecast, "0.01", "Icecast output streaming",
             MSERV_MODFLAG_OUTPUT);

typedef struct _t_icecast {
  int channels;            /* 1 for mono, 2 for stereo, etc. */
  int samplerate;          /* samples per second (16 bit) */
  int bitrate;             /* bitrate */
  int volume;              /* volume level */
  int persist;             /* persist streaming when stopped */
  int connected;           /* connected? */
  shout_t *shout;          /* shout output object */
  vorbis_info vi;          /* vorbis object */
  vorbis_comment vc;       /* vorbis comment object */
  vorbis_dsp_state vd;     /* PCM decoder object */
  vorbis_block vb;         /* PCM decoder local working space */
  ogg_stream_state os;     /* Take physical pages, weld into logical stream */
  ogg_page og;             /* one Ogg bitstream page containing Vorbis pkts */
  ogg_packet op;           /* one raw packet of data for decode */
} t_icecast;

/* initialise module */

int icecast_init(char *error, int errsize)
{
  (void)error;
  (void)errsize;
  (void)this_module;
  shout_init();
  return MSERV_SUCCESS;
}

/* finalise module */

int icecast_final(char *error, int errsize)
{
  (void)error;
  (void)errsize;
  shout_shutdown();
  return MSERV_SUCCESS;
}

/* connect to stream */

static int icecast_internal_connect(t_channel *c, t_icecast *icecast,
                                    char *error, int errsize)
{
  ogg_packet header;
  ogg_packet header_comm;
  ogg_packet header_code;

  if (icecast->connected)
    return MSERV_SUCCESS;

  if (shout_open(icecast->shout) != SHOUTERR_SUCCESS) {
    snprintf(error, errsize, "icecast: failed opening Icecast connection: %s",
             shout_get_error(icecast->shout));
    goto failed;
  }
  mserv_log("Successfully connected to Icecast server '%s:%s'"
            " for mount '%s'",
            shout_get_host(icecast->shout), shout_get_port(icecast->shout),
            shout_get_mount(icecast->shout));
  icecast->connected = 1;
  vorbis_info_init(&icecast->vi);
  if (vorbis_encode_init(&icecast->vi, icecast->channels, 
                         icecast->samplerate, -1,
                         icecast->bitrate, -1) != 0) {
    snprintf(error, errsize, "icecast: failed to initialise vorbis engine");
    goto failed;
  }
  vorbis_comment_init(&icecast->vc);
  vorbis_comment_add_tag(&icecast->vc, "ENCODER", "mserv " VERSION);
  vorbis_analysis_init(&icecast->vd, &icecast->vi);
  vorbis_block_init(&icecast->vd, &icecast->vb);
  ogg_stream_init(&icecast->os, rand());
  vorbis_analysis_headerout(&icecast->vd, &icecast->vc,
                            &header, &header_comm, &header_code);
  ogg_stream_packetin(&icecast->os, &header);
  ogg_stream_packetin(&icecast->os, &header_comm);
  ogg_stream_packetin(&icecast->os, &header_code);
  for (;;) {
    if (ogg_stream_flush(&icecast->os, &icecast->og) == 0)
      break;
    if (shout_send(icecast->shout, icecast->og.header,
                   icecast->og.header_len) != SHOUTERR_SUCCESS ||
        shout_send(icecast->shout, icecast->og.body,
                   icecast->og.body_len) != SHOUTERR_SUCCESS) {
      snprintf(error, errsize, "icecast: failed to send starter to "
               "shout: %s", shout_get_error(icecast->shout));
      vorbis_block_clear(&icecast->vb);
      vorbis_dsp_clear(&icecast->vd);
      vorbis_info_clear(&icecast->vi);
      goto failed;
    }
  }
  return MSERV_SUCCESS;
failed:
  if (icecast->connected)
    shout_close(icecast->shout);
  icecast->connected = 0;
  return MSERV_FAILURE;
}

/* disconnect to stream */

static int icecast_internal_disconnect(t_channel *c, t_icecast *icecast,
                                       char *error, int errsize)
{
  if (!icecast->connected)
    return MSERV_SUCCESS;

  vorbis_analysis_wrote(&icecast->vd, 0);
  vorbis_block_clear(&icecast->vb);
  vorbis_dsp_clear(&icecast->vd);
  vorbis_info_clear(&icecast->vi);
  shout_close(icecast->shout);
  icecast->connected = 0;
}

/* create output stream */

int icecast_output_create(t_channel *c, const char *location,
                           t_params *params, void **private,
                           char *error, int errsize)
{
  char *user, *pass, *host, *port;
  char mount[128];
  char splitbuf[128];
  char *p;
  char *val;
  t_icecast *icecast;

  if ((icecast = malloc(sizeof(t_icecast))) == NULL) {
    snprintf(error, errsize, "out of memory");
    return MSERV_FAILURE;
  }
  icecast->connected = 0;
  icecast->channels = c->channels;
  icecast->samplerate = c->samplerate;
  if (icecast->channels != 2) {
    snprintf(error, errsize, "channels must be 2");
    return MSERV_FAILURE;
  }
  if (icecast->samplerate != 44100) {
    snprintf(error, errsize, "sample rate must be 44100");
    return MSERV_FAILURE;
  }
  if (params_get(params, "bitrate", &val) != MSERV_SUCCESS)
    val = "32768";
  icecast->bitrate = atoi(val);
  if (params_get(params, "volume", &val) != MSERV_SUCCESS)
    val = "50";
  icecast->volume = atoi(val);
  if (params_get(params, "persist", &val) != MSERV_SUCCESS)
    val = "1";
  icecast->persist = atoi(val) ? 1 : 0;
  if (!(icecast->shout = shout_new())) {
    snprintf(error, errsize, "failed to allocate shout object");
    return MSERV_FAILURE;
  }
  /* now take a copy of destination into splitbuf for splitting */
  if (strlen(location) >= sizeof(splitbuf)) {
    snprintf(error, errsize, "URI too long");
    goto failed;
  }
  strncpy(splitbuf, location, sizeof(splitbuf));
  splitbuf[sizeof(splitbuf) - 1] = '\0';
  if (strncmp(splitbuf, "http://", strlen("http://")) != 0) {
    snprintf(error, errsize, "only http:// Icecast URIs are supported");
    goto failed;
  }
  p = splitbuf + strlen("http://");
  user = p; while (*p && *p != ':') p++; *p++ = '\0';
  pass = p; while (*p && *p != '@') p++; *p++ = '\0';
  host = p; while (*p && *p != ':') p++; *p++ = '\0';
  port = p; while (*p && *p != '/') p++; *p++ = '\0';
  if ((strlen(p) + 1) >= sizeof(mount)) {
    snprintf(error, errsize, "icecast: mount portion of URL too long");
    goto failed;
  }
  snprintf(mount, sizeof(mount), "/%s", p);
  mserv_log("Request to icecast connect to %s:%s", host, port);
  if (!*user || !*pass || !*host || !*port || !mount[1]) {
    snprintf(error, errsize, "icecast: location invalid, use "
             "http://user:pass@host:port/mount");
    goto failed;
  }
  if (shout_set_host(icecast->shout, host) != SHOUTERR_SUCCESS) {
    snprintf(error, errsize, "icecast: failed setting Icecast hostname: %s",
             shout_get_error(icecast->shout));
    goto failed;
  }
  if (shout_set_protocol(icecast->shout,
                         SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS) {
    snprintf(error, errsize, "icecast: failed setting Icecast protocol: %s",
             shout_get_error(icecast->shout));
    goto failed;
  }
  if (shout_set_port(icecast->shout, atoi(port)) != SHOUTERR_SUCCESS) {
    snprintf(error, errsize, "icecast: failed setting Icecast port: %s",
             shout_get_error(icecast->shout));
    goto failed;
  }
  if (shout_set_password(icecast->shout, pass) != SHOUTERR_SUCCESS) {
    snprintf(error, errsize, "icecast: failed setting Icecast password: %s",
             shout_get_error(icecast->shout));
    goto failed;
  }
  if (shout_set_mount(icecast->shout, mount) != SHOUTERR_SUCCESS) {
    snprintf(error, errsize, "icecast: failed setting Icecast hostname: %s",
             shout_get_error(icecast->shout));
    goto failed;
  }
  if (shout_set_user(icecast->shout, user) != SHOUTERR_SUCCESS) {
    snprintf(error, errsize, "icecast: failed setting Icecast user: %s",
             shout_get_error(icecast->shout));
    goto failed;
  }
  if (shout_set_format(icecast->shout,
                       SHOUT_FORMAT_VORBIS) != SHOUTERR_SUCCESS) {
    snprintf(error, errsize, "icecast: failed setting Icecast format: %s",
             shout_get_error(icecast->shout));
    goto failed;
  }
  /* does the user want us to connect now? */
  if (params_get(params, "connect", &val) == MSERV_SUCCESS && atoi(val) == 1) {
    if (icecast_internal_connect(c, icecast, error, errsize) != MSERV_SUCCESS)
      goto failed;
  }
  *private = (void *)icecast;
  return MSERV_SUCCESS;
failed:
  if (icecast->shout)
    shout_free(icecast->shout);
  free(icecast);
  return MSERV_FAILURE;
}

/* destroy output stream */

int icecast_output_destroy(t_channel *c, void *private,
                            char *error, int errsize)
{
  t_icecast *icecast = (t_icecast *)private;

  (void)c;
  (void)error;
  (void)errsize;

  if (icecast->connected) {
    if (icecast_internal_disconnect(c, icecast, error,
                                    errsize) != MSERV_SUCCESS)
      return MSERV_FAILURE;
  }
  return MSERV_SUCCESS;
}

/* synchronise output stream */

int icecast_output_sync(t_channel *c, void *private, char *error, int errsize)
{
  t_icecast *icecast = (t_icecast *)private;
  float **vorbbuf;
  unsigned int chan, i, pages, bytes;

  /* take one second sample buffer and send to libvorbis */

  vorbbuf = vorbis_analysis_buffer(&icecast->vd,
                                   c->buffer_samples / c->channels);
  for (i = 0; i < c->buffer_samples / c->channels; i++) {
    for (chan = 0; chan < c->channels; chan++)
      vorbbuf[chan][i] = c->buffer[i * c->channels + chan];
  }
  if (mserv_debug)
    mserv_log("icecast: wrote %d samples to libvorbis",
              c->buffer_samples / c->channels);
  vorbis_analysis_wrote(&icecast->vd, c->buffer_samples / c->channels);

  /* get encoded data back from libvorbis */

  pages = 0;
  bytes = 0;
  while (vorbis_analysis_blockout(&icecast->vd, &icecast->vb) == 1) {
    vorbis_analysis(&icecast->vb, NULL);
    vorbis_bitrate_addblock(&icecast->vb);
    while (vorbis_bitrate_flushpacket(&icecast->vd, &icecast->op)) {
      ogg_stream_packetin(&icecast->os, &icecast->op);
      while (!ogg_page_eos(&icecast->og)) {
        if (ogg_stream_pageout(&icecast->os, &icecast->og) == 0) {
          if (pages || ogg_stream_flush(&icecast->os, &icecast->og) == 0)
            break;
        }
        pages++;
        if (shout_send(icecast->shout, icecast->og.header,
                       icecast->og.header_len) != SHOUTERR_SUCCESS) {
          snprintf(error, errsize, "icecast: failed to send ogg header to "
                   "shout: %s", shout_get_error(icecast->shout));
          return MSERV_FAILURE;
        }
        if (shout_send(icecast->shout, icecast->og.body,
                       icecast->og.body_len) != SHOUTERR_SUCCESS) {
          snprintf(error, errsize, "icecast: failed to send ogg body to "
                   "shout: %s", shout_get_error(icecast->shout));
          return MSERV_FAILURE;
        }
        bytes+= icecast->og.header_len + icecast->og.body_len;
      }
    }
  }
  if (mserv_debug) {
    mserv_log("icecast: received %d ogg pages from libvorbis", pages);
    mserv_log("icecast: sent %d bytes to server", bytes);
  }
  return MSERV_SUCCESS;
}

/* read/set volume */

int icecast_output_volume(t_channel *c, void *private, int *volume,
                           char *error, int errsize)
{
  t_icecast *icecast = (t_icecast *)private;

  (void)c;
  (void)error;
  (void)errsize;

  if (*volume == -1) {
    *volume = icecast->volume;
    return MSERV_SUCCESS;
  }
  icecast->volume = *volume;
  return MSERV_SUCCESS;
}

/* start output stream */

int icecast_output_start(t_channel *c, void *private, char *error, int errsize)
{
  t_icecast *icecast = (t_icecast *)private;

  if (!icecast->connected)
    return icecast_internal_connect(c, icecast, error, errsize);
  return MSERV_SUCCESS;
}

/* stop output stream */

int icecast_output_stop(t_channel *c, void *private, char *error, int errsize)
{
  t_icecast *icecast = (t_icecast *)private;

  if (icecast->connected && !icecast->persist)
    return icecast_internal_disconnect(c, icecast, error, errsize);
  return MSERV_SUCCESS;
}

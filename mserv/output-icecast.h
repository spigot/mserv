#ifndef _MSERV_OUTPUT_ICECAST_H
#define _MSERV_OUTPUT_ICECAST_H

#include <shout/shout.h>
#include <vorbis/vorbisenc.h>

#include "global.h"

#define MSERV_SAMPLERATE 44100

typedef struct _t_input {
  struct _t_input *next;   /* next input stream */
  int fd;                  /* input file descriptor */
  t_supinfo supinfo;       /* track sup. info. */
  int zeros_start;         /* number of zero bytes left for delay */
  int zeros_end;           /* number of zero bytes left for delay */
  int announced;           /* have we announced the play of this track? */
} t_input;

typedef struct {
  /* struct timeval lasttime; */ /* interval timer */
  int paused;              /* are we currently paused? */
  int stopped;             /* are we currently stopped? */
  t_input *input;          /* structure containing file descriptor of input */
  int channels;            /* 1 for mono, 2 for stereo, etc. */
  int samplerate;          /* samples per second (16 bit) */
  int bitrate;             /* bitrate */
  shout_t *shout;          /* shout output object */
  int volume;              /* volume level */
  char url[128];           /* http://user:pass@hostname:port/thing */
  vorbis_info vi;          /* vorbis object */
  vorbis_comment vc;       /* vorbis comment object */
  vorbis_dsp_state vd;     /* PCM decoder object */
  vorbis_block vb;         /* PCM decoder local working space */
  ogg_stream_state os;     /* Take physical pages, weld into logical stream */
  ogg_page og;             /* one Ogg bitstream page containing Vorbis pkts */
  ogg_packet op;           /* one raw packet of data for decode */
  int buffer_ready_bytes;  /* bytes we have so far */
  int buffer_ready_size;   /* size of buffer_ready buffer */
  char *buffer_ready;      /* previous buffer that's ready to send or NULL */
  int buffer_bytes;        /* bytes we have so far */
  int buffer_size;         /* one second buffer for this output stream */
  char *buffer;            /* one second buffer for this output stream */
} t_output;

int output_init(void);
t_output *output_create(const char *destination, const char *parameters,
                        char **error);
int output_getvolume(t_output *o);
int output_setvolume(t_output *o, int volume);
void output_close(t_output *o);
void output_final(void);
int output_addinput(t_output *o, int fd, t_supinfo *track_supinfo,
                    int samplerate, int channels, int continuation,
                    double delay_start, double delay_end);
int output_clear(t_output *o);
int output_inputfinished(t_output *o);
void output_sync(t_output *o);
int output_delay(t_output *o);
void output_replacetrack(t_output *o, t_track *track, t_track *newtrack);
int output_stop(t_output *o);
int output_start(t_output *o);
int output_pause(t_output *o, int pause);
int output_stopped(t_output *o);
int output_paused(t_output *o);

#endif

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

#ifndef _MSERV_GLOBAL_H
#define _MSERV_GLOBAL_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>

#include "config.h"
#include "defines.h"

#define MSERV_APIVER 1

/* timercmp() is broken on Solaris, and isn't particularly standard */
#define mserv_timercmp(a, b, CMP)                                       \
  (((a)->tv_sec == (b)->tv_sec) ?                                       \
    ((a)->tv_usec CMP (b)->tv_usec) : ((a)->tv_sec CMP (b)->tv_sec))

/* timersub() replacement */
#define mserv_timersub(a, b, res)                                       \
  do {                                                                  \
    (res)->tv_sec = (a)->tv_sec - (b)->tv_sec;                          \
    (res)->tv_usec = (a)->tv_usec - (b)->tv_usec;                       \
    if ((res)->tv_usec < 0) {                                           \
      (res)->tv_sec--;                                                  \
      (res)->tv_usec += 1000000;                                        \
    }                                                                   \
  } while (0)


#define mserv_MIN(a,b) ( ((a) < (b)) ? (a) : (b) )
#define mserv_MAX(a,b) ( ((a) > (b)) ? (a) : (b) )

#define MSERV_MODULE(name, version, desc, flags) \
  t_module name ## _module = { \
    MSERV_APIVER, flags, #name, desc, version, mserv_rcs_id \
  }; \
  static t_module *this_module = &name ## _module;

#define MSERV_FAILURE -1
#define MSERV_SUCCESS 0

#define MSERV_NAN (0.0/0.0)

#define MSERV_STRUCTOFFSET(s,m) ((unsigned int)(&(((s*)(0))->m)))

#define MSERV_MODFLAG_OUTPUT 1
#define MSERV_MODFLAG_OUTPUT_VOLUME_PRIMARY 2

typedef enum {
  st_wait,
  st_closed
} t_state;

typedef enum {
  lst_normal,
  lst_cr
} t_lstate;

typedef enum {
  mode_human,
  mode_computer,
  mode_rtcomputer
} t_mode;

typedef enum {
  level_guest,
  level_user,
  level_priv,
  level_master
} t_userlevel;

typedef enum {
  stack_false,
  stack_true,
  stack_not,
  stack_open,
  stack_close,
  stack_and,
  stack_or
} t_stack;

typedef struct _t_client {
  struct _t_client *next;
  struct sockaddr_in sin;
  t_state state;
  t_lstate lstate;
  int socket;
  time_t lastread;
  unsigned int linebuflen;
  struct iovec outbuf[OUTVECTORS];
  unsigned char linebuf[LINEBUFLEN];
  char user[USERNAMELEN+1];
  t_userlevel userlevel;
  t_mode mode;
  double weight;
  int loggedinatsong;
  time_t last_unrated;          /* When the user last heard an unrated song */
  int bored;                    /* True if the user hasn't heard anything new in an hour */
  char channel[CHANNELNAMELEN+1]; /* current channel */
  unsigned int authed:1;
  unsigned int quitme:1;
} t_client;

typedef struct _t_cmdparams {
  const char *ru;               /* remote user (or current user), always set */
  const char *line;             /* command line, always set */
  const char *channel;          /* for CHANNEL commands, the channel name */
} t_cmdparams;

typedef struct _t_cmds {
  unsigned int authed;
  t_userlevel userlevel;
  char *name;
  struct _t_cmds *sub_cmds;
  void (*function)(t_client *cl, t_cmdparams *cp);
  char *help;
  char *syntax;
} t_cmds;

typedef struct _t_acl {
  struct _t_acl *next;
  char user[USERNAMELEN+1];
  char pass[PASSWORDLEN+1];
  time_t nexttime;
  t_userlevel userlevel;
} t_acl;

typedef struct _t_rating {
  struct _t_rating *next;
  char *user;
  int rating;
  time_t when;
} t_rating;

struct _t_album;
typedef struct _t_track {
  struct _t_track *next;
  unsigned int id;
  struct _t_album *album;
  unsigned int n_track;
  unsigned int year;
  unsigned int modified:1;
  unsigned int norandom:1;
  unsigned int filterok:1;
  int volume;                       /* percentage (100 is normal) */
  double prating;                   /* calculated rating of track */
  double rating;                    /* temporally, filtered adjusted rating */
  unsigned long int duration;       /* song duration in 1/100 seconds */
  char *author;
  char *name;
  char *filename;
  char *genres;
  char *miscinfo;
  time_t lastplay;
  time_t mtime;
  t_rating *ratings;
} t_track;

/* This struct keeps track of a user's average rating of the tracks in
 * a certain album.  It is used to compute an expected value for
 * unrated tracks of the same album. */
typedef struct _t_average {
  struct _t_average *next;
  char *user;
  double sum;
  int n_ratings;
} t_average;

typedef struct _t_album {
  struct _t_album *next;
  unsigned int id;
  unsigned int modified;
  unsigned int tracks_size;
  unsigned int ntracks;
  char *author;
  char *name;
  char *filename;
  time_t mtime;
  t_track **tracks;
  t_average *averages;
} t_album;

typedef struct _t_author {
  struct _t_author *next;
  unsigned int id;
  unsigned int tracks_size;
  unsigned int ntracks;
  char *name;
  t_track **tracks;
} t_author;

typedef struct _t_genre {
  struct _t_genre *next;
  unsigned int id;
  unsigned int tracks_size;
  unsigned int ntracks;
  char *name;
  t_track **tracks;
} t_genre;

typedef struct _t_lang {
  struct _t_lang *next;
  int code;
  char *token;
  char *text;
} t_lang;

typedef struct _t_trkinfo {
  t_track *track;
  char user[USERNAMELEN+1];
} t_trkinfo;

typedef struct _t_historyentry {
  t_track *track;
  char user[USERNAMELEN+1];
  int durationMsecs;
} t_historyentry;

typedef struct _t_queue {
  struct _t_queue *next;
  t_trkinfo trkinfo;
} t_queue;

/* input stream structure */

typedef struct _t_channel_inputstream {
  struct _t_channel_inputstream *next; /* next input in stream */
  int fd;                     /* input file descriptor */
  t_trkinfo trkinfo;          /* track + info. */
  unsigned int silence_start; /* number of zero samples (all chans) left */
  unsigned int silence_end;   /* number of zero samples (all chans) left */
  unsigned int announced;     /* have we announced the play of this track? */
  unsigned int timer_started; /* have we started timing this track? */
} t_channel_inputstream;

typedef struct _t_module {
  int apiver;
  int flags;
  char *name;
  char *desc;
  char *version;
  char *rcs_id;
} t_module;

typedef struct _t_params_list {
  struct _t_params_list *next;   /* next entry in linked list */
  char *key;                /* key=val */
  char *val;
} t_params_list;

typedef struct _t_params {
  char *memory;             /* memory allocated */
  t_params_list *list;      /* parameter list */
} t_params;

struct _t_modinfo;
typedef struct _t_modinfo t_modinfo;

/* output stream structure */

typedef struct _t_channel_outputstream {
  struct _t_channel_outputstream *next; /* next output stream */
  char location[256];           /* http://user:pass@hostname:port/thing */
  t_params *params;             /* parameters (bitrate, etc.) */
  t_modinfo *modinfo;           /* the module itself */
  void *private;                /* private module info for this stream */
  unsigned int channels;        /* output stream channels */
  unsigned int samplerate;      /* output stream sample rate */
  void *resampler;              /* resampler state (SRC_STATE) */
  float *output;                /* output for resampler / output for module */
  int bytesLeft;                /* how many bytes haven't yet been output */
} t_channel_outputstream;

struct _t_channel;
typedef struct _t_channel t_channel;

typedef int (*t_module_init)(char *error, int errsize);
typedef int (*t_module_final)(char *error, int errsize);
typedef int (*t_module_output_create)(t_channel *c, t_channel_outputstream *os,
                                      const char *location,
                                      t_params *params, void **private,
                                      char *error, int errsize);
typedef int (*t_module_output_destroy)(t_channel *c, t_channel_outputstream *os,
                                       void *private,
                                       char *error, int errsize);
typedef int (*t_module_output_poll)(t_channel *c, t_channel_outputstream *os,
                                    void *private,
                                    char *error, int errsize);
/* This function returns the number of bytes yet to be written or
   MSERV_FAILURE on failure. */
typedef int (*t_module_output_sync)(t_channel *c, t_channel_outputstream *os,
                                    void *private,
                                    char *error, int errsize);
typedef int (*t_module_output_volume)(t_channel *c, t_channel_outputstream *os,
                                      void *private, int *volume,
                                      char *error, int errsize);
typedef int (*t_module_output_start)(t_channel *c, t_channel_outputstream *os,
                                     void *private,
                                     char *error, int errsize);
typedef int (*t_module_output_stop)(t_channel *c, t_channel_outputstream *os,
                                    void *private,
                                    char *error, int errsize);
typedef int (*t_module_get_buffer_ms)(void *private);

struct _t_modinfo {
  struct _t_modinfo *next;
  void *dlh;
  t_module *module;
  int flags;
  t_module_init init;
  t_module_final final;
  t_module_output_create output_create;
  t_module_output_destroy output_destroy;
  t_module_output_poll output_poll;
  t_module_output_sync output_sync;
  t_module_output_volume output_volume;
  t_module_output_start output_start;
  t_module_output_stop output_stop;
  t_module_get_buffer_ms get_buffer_ms;
};

struct _t_channel {
  char name[CHANNELNAMELEN + 1];  /* channel name */
  struct timeval lasttime;        /* interval timer */
  unsigned int paused;            /* are we currently paused? */
  unsigned int stopped;           /* are we currently stopped? */
  t_trkinfo playing;              /* currently playing track */
  long timeUntilLastStop;         /* msecs of play until current song
				   * was first paused*/
  long lastStartTime;             /* when this song last resumed
				   * playing in msecs since the
				   * epoch */
  t_channel_inputstream *input;    /* stream of inputs */
  t_channel_outputstream *output; /* outputs (simultaneous) */
  unsigned int channels;          /* 1 for mono, 2 for stereo, etc. */
  unsigned int samplerate;        /* samples per second (16 bit) */
  unsigned int buffer_size;       /* samplerate * channels * bytes per sample */
  unsigned int buffer_bytes;      /* bytes we have so far */
  char *buffer_char;              /* one second input buffer */
  unsigned int buffer_samples;    /* samps in buffer and buffer_char (16 bit) */
  float *buffer;                  /* same as buffer_char, but float and vol'd */
};

typedef struct _t_channel_list {
  struct _t_channel_list *next; /* next in list */
  unsigned int created;        /* time the user created the channel */
  t_channel *channel;
} t_channel_list;

#endif

#ifndef _MSERV_GLOBAL_H
#define _MSERV_GLOBAL_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>

#include "config.h"
#include "defines.h"

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
  unsigned int authed:1;
  unsigned int quitme:1;
} t_client;

typedef struct _t_cmds {
  unsigned int authed;
  t_userlevel userlevel;
  char *name;
  void (*function)(t_client *cl, const char *ru, const char *line);
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

typedef struct _t_track {
  struct _t_track *next;
  unsigned int id;
  unsigned int n_album;
  unsigned int n_track;
  unsigned int year;
  unsigned int random;
  unsigned int modified:1;
  unsigned int norandom:1;
  unsigned int filterok:1;
  int volume;                       /* percentage (100 is normal) */
  double prating;                   /* calculated rating of track */
  double rating;                    /* temporally, filtered adjusted rating */
  unsigned long int duration;
  char *author;
  char *name;
  char *filename;
  char *genres;
  char *miscinfo;
  time_t lastplay;
  time_t mtime;
  t_rating *ratings;
} t_track;

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

typedef struct _t_supinfo {
  t_track *track;
  char user[USERNAMELEN+1];
} t_supinfo;

typedef struct _t_queue {
  struct _t_queue *next;
  t_supinfo supinfo;
} t_queue;

#endif

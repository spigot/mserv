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
  double prating;
  double rating;
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
  char *author;
  char *name;
  char *filename;
  time_t mtime;
  t_track *(tracks[TRACKSPERALBUM]);
} t_album;

typedef struct _t_author {
  struct _t_author *next;
  unsigned int id;
  char *name;
  t_track *(tracks[TRACKSPERALBUM]);
} t_author;

typedef struct _t_genre {
  struct _t_genre *next;
  unsigned int id;
  unsigned int size;
  unsigned int total;
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

void mserv_log(const char *text, ...);
void mserv_response(t_client *cl, const char *token, const char *fmt, ...);
void mserv_responsent(t_client *cl, const char *token, const char *fmt, ...);
void mserv_broadcast(const char *token, const char *fmt, ...);
void mserv_send(t_client *cl, const char *data, unsigned int len);
t_lang *mserv_gettoken(const char *token);
int mserv_split(char *str[], int nelements, char *line, const char *sep);
t_track *mserv_gettrack(unsigned int n_album, unsigned int n_track);
t_album *mserv_getalbum(unsigned int n_album);
t_rating *mserv_getrate(const char *user, t_track *track);
int mserv_addqueue(t_client *cl, t_track *track);
int mserv_playnext(void);
void mserv_pauseplay(t_client *cl);
void mserv_abortplay(void);
void mserv_resumeplay(void);
void mserv_recalcratings(void);
t_track *mserv_altertrack(t_track *track, const char *author,
                          const char *name, const char *genres,
                          const char *miscinfo);
t_album *mserv_alteralbum(t_album *album, const char *author,
			  const char *name);
int mserv_savechanges(void);
t_rating *mserv_ratetrack(t_client *cl, t_track **track, unsigned int val);
const char *mserv_ratestr(t_rating *rate);
t_userlevel *mserv_strtolevel(const char *level);
const char *mserv_levelstr(t_userlevel userlevel);
const char *mserv_stndrdth(int day);
int mserv_setmixer(t_client *cl, int what, const char *line);
int mserv_flush(void);
void mserv_closedown(int exitcode);
char *mserv_idletxt(time_t idletime);
int mserv_acl_checkpassword(const char *user, const char *password,
                            t_acl **acl);
char *mserv_crypt(const char *password);
void mserv_saveacl(void);
int mserv_idea(const char *text);
void mserv_reset(void);
void mserv_close(t_client *cl);
int mserv_setgap(int gap);
int mserv_setfilter(const char *filter);
int mserv_checkgenre(const char *genres);
int mserv_checkauthor(const char *author);
int mserv_checkname(const char *name);
char *mserv_getfilter(void);
int mserv_strtorate(const char *str);
t_track *mserv_checkdisk_track(t_track *track);
t_album *mserv_checkdisk_album(t_album *album);
void mserv_ensuredisk(void);
int mserv_checklevel(t_client *cl, t_userlevel level);

extern char *progname;
extern int mserv_verbose;
extern t_client *mserv_clients;
extern t_track *mserv_tracks;
extern t_album *mserv_albums;
extern t_queue *mserv_queue;
extern t_supinfo *mserv_history[];
extern t_supinfo mserv_playing;
extern int mserv_paused;
extern time_t mserv_playingstart;
extern t_acl *mserv_acl;
extern int mserv_shutdown;
extern int mserv_random;
extern double mserv_factor;
extern t_author *mserv_authors;
extern t_genre *mserv_genres;
extern unsigned int mserv_filter_ok;
extern unsigned int mserv_filter_notok;
extern unsigned int mserv_playnext_waiting;
extern struct timeval mserv_playnext_at;

extern char *mserv_path_acl;
extern char *mserv_path_webacl;
extern char *mserv_path_conf;

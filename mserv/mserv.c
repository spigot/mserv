/*
All of the documentation and software included in the Mserv releases is
copyrighted by James Ponder <james@squish.net>.

Copyright 1999, 2000 James Ponder.  All rights reserved.

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
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pwd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <sys/ioctl.h>

#include "mserv.h"
#include "misc.h"
#include "cmd.h"
#include "acl.h"
#include "mp3info.h"
#include "soundcard.h"
#include "defconf.h"
#include "conf.h"
#include "opt.h"
#include "filter.h"

#define MAXFD 64

#ifndef MIN
# define MIN(X,Y) (((X) < (Y)) ? (X) : (Y))
#endif

extern char *optarg;
extern int optind;
/* extern int getopt(int, char *const *, const char *); */ /* sunos ;( */

/*** file-scope (static) globals ***/

static FILE *mserv_logfile = NULL;
static int mserv_socket = 0;
static int mserv_nextid_album = 1;
static int mserv_nextid_track = 1;
static int mserv_playingpid = 0;
static int mserv_started = 0;
static char mserv_filter[FILTERLEN+1] = "";
static char mserv_pausedby[USERNAMELEN+1];
static unsigned int mserv_gap = 0;
static t_lang *mserv_language;

/*** externed variables ***/

char *progname = NULL;
int mserv_verbose = 0;
t_client *mserv_clients = NULL;
t_track *mserv_tracks = NULL;
t_album *mserv_albums = NULL;
t_queue *mserv_queue = NULL;
t_supinfo *mserv_history[HISTORYLEN];
t_supinfo mserv_playing;
int mserv_paused = 0;
time_t mserv_playingstart;
t_acl *mserv_acl = NULL;
int mserv_shutdown = 0;
int mserv_random = 0;
double mserv_factor = 0.6;
t_author *mserv_authors = NULL;
t_genre *mserv_genres = NULL;
unsigned int mserv_filter_ok = 0;
unsigned int mserv_filter_notok = 0;
time_t mserv_playnextat = (time_t)0;

/*** file-scope (static) function declarations ***/

static void mserv_mainloop(void);
static void mserv_checkchild(void);
static void mserv_pollwrite(t_client *cl);
static void mserv_pollclient(t_client *cl);
static void mserv_endline(t_client *cl, char *line);
static int mserv_loadlang(const char *pathname);
static void mserv_vresponse(t_client *cl, const char *token, const char *fmt,
			    va_list ap);
static void mserv_scandir(void);
static void mserv_scandir_recurse(const char *pathname);
static t_track *mserv_loadtrk(const char *filename);
static t_album *mserv_loadalbum(const char *filename, int onlyifexists);
static int album_insertsort(t_album *album);
static t_author *mserv_authorlist(void);
static int author_insertsort(t_author **list, t_author *author);
static t_genre *mserv_genrelist(void);
static int genre_insertsort(t_genre **list, t_genre *genre);
static int mserv_argsubs(char *outbuf, const int length,
			 const char * const *str, const unsigned int params,
			 const char *fmt);
static int mserv_trackcompare_filename(const void *a, const void *b);
static int mserv_trackcompare_name(const void *a, const void *b);
static int mserv_trackcompare_rating(const void *a, const void *b);
static void mserv_kill(t_client *cl);
static void mserv_checkshutdown(void);
static void mserv_replacetrack(t_track *track, t_track *newtrack);
static void mserv_replacealbum(t_album *album, t_album *newalbum);
static int mserv_createdir(const char *path);
static void mserv_sendwrap(t_client *cl, const char *string, int wrapwidth,
			   const char *indentstr, int nextwidth);
static void mserv_strtoprintable(char *string);
#ifdef SOUNDCARD
static int mserv_setvolume(int vol);
static int mserv_readvolume(void);
#endif

/*** functions ***/

static RETSIGTYPE mserv_sighandler(int signum)
{
  signal(signum, mserv_sighandler);

  switch(signum) {
  case SIGINT:
    mserv_log("Signal received, closing down.");
    mserv_closedown(0);
    break;
  case SIGCHLD:
    /* wake up select */
  default:
    break;
  }
}

/* General initialisiation to be done after
   starting mserv and also after a reset */
static void mserv_init()
{
  int i;
  t_album *album;

  /* load up defaults */
  mserv_factor = opt_factor;
  mserv_random = opt_random;
  mserv_gap = opt_gap;

  /* load albums and tracks */
  mserv_scandir();

  /* re-number albums */
  mserv_nextid_album = 1;

  for (album = mserv_albums; album; album = album->next) {
    for (i = 0; i < TRACKSPERALBUM; i++) {
      if (album->tracks[i])
        album->tracks[i]->n_album = mserv_nextid_album;
    }
    album->id = mserv_nextid_album++;
  }

  /* extract author information */
  mserv_authors = mserv_authorlist();

  /* extract genre information */
  mserv_genres = mserv_genrelist();

  mserv_savechanges();

  /* setting the filter must be done after all the tracks are loaded, if
     there is no filter then don't try to set it, the default install won't
     have any tracks and this will cause it to fail */
  if (*opt_filter && mserv_setfilter(opt_filter)) {
    mserv_log("Unable to set default filter, ignoring");
  } else {
    mserv_recalcratings();
  }
}

int main(int argc, char *argv[])
{
  int i;
  struct protoent *protocol;
  struct sockaddr_in sin;
  int so_int;
  int flags;
  char *mserv_root = NULL;
  char *mserv_conf = NULL;
  unsigned int mserv_port = 0;
  struct passwd *ps;
  int l;
  int f_acl, f_webacl, f_conf;
  int populate;
  struct stat statbuf;
  char *m = NULL;
  int badparams = 0;

  progname = argv[0];

  if (getuid() == 0) {
    fprintf(stderr, "%s: fool attempted to run server as root\n", progname);
    exit(1);
  }

  while ((i = getopt(argc, argv, "c:p:r:v")) != -1) {
    switch(i) {
    case 'c':
      mserv_conf = optarg;
      break;
    case 'p':
      mserv_port = atoi(optarg);
      break;
    case 'r':
      mserv_root = optarg;
      break;
    case 'v':
      mserv_verbose = 1;
      break;
    default:
      fputs("mserv: parameters not understood\n", stderr);
      badparams = 1;
    }
  }
  argc-= optind;
  argv+= optind;

  if (badparams || argc != 0) {
    fputs("Syntax: mserv [-r <mserv root>]\n"
	  "              [-c <config file>]\n"
	  "              [-p <port>]\n"
	  "              [-v] (verbose mode)\n"
	  "        This is mserv version " VERSION "\n",
	  stderr);
    exit(2);
  }    

  if (mserv_verbose)
    printf("Mserv version " VERSION " (c) James Ponder 1999-2000\n");

  if (!mserv_root) {
    if ((ps = getpwuid(getuid())) == NULL || ps->pw_dir[0] == '\0') {
      fprintf(stderr, "%s: could not determine home directory\n", progname);
      exit(1);
    }
    if ((m = malloc(strlen(ps->pw_dir)+16)) == NULL) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
    sprintf(m, "%s%s.mserv", ps->pw_dir,
	     ps->pw_dir[strlen(ps->pw_dir)-1] == '/' ? "" : "/");
  } else {
    /* copy out of environment */
    if ((m = malloc(strlen(mserv_root))) == NULL) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
    strcpy(m, mserv_root);
  }
  mserv_root = m;
  /* strip superfluous slashes */
  l = strlen(mserv_root);
  while (l > 2 && mserv_root[l-1] == '/')
    l--;
  mserv_root[l] = '\0';
  if (!mserv_conf) {
    if ((m = malloc(strlen(mserv_root)+sizeof("/config"))) == NULL) {
      fprintf(stderr, "%s: out of memory\n", progname);
      exit(1);
    }
    sprintf(m, "%s/config", mserv_root);
    mserv_conf = m;
  }
  if (stat(mserv_root, &statbuf) == -1) {
    if (errno != ENOENT) {
      fprintf(stderr, "%s: mserv root %s: %s\n", progname, mserv_root,
	      strerror(errno));
      exit(1);
    }
    populate = 1;
  } else {
    if (!S_ISDIR(statbuf.st_mode)) {
      fprintf(stderr, "%s: mserv root %s is not a directory!\n", progname,
	      mserv_root);
      exit(1);
    }
    if (conf_load(mserv_conf)) {
      fprintf(stderr, "%s: failed to read conf file\n", progname);
      exit(1);
    }
    populate = 0;
  }
  if (opt_read(mserv_root)) {
    fprintf(stderr, "%s: failed to read options\n", progname);
    exit(1);
  }
  if (populate) {
    /* no directory, populate! */
    if (mkdir(mserv_root, 0755) == -1) {
      fprintf(stderr, "%s: unable to mkdir '%s': %s\n", progname,
	      mserv_root, strerror(errno));
      exit(1);
    }
    if (mkdir(opt_path_tracks, 0755) == -1) {
      fprintf(stderr, "%s: unable to mkdir '%s': %s\n", progname,
	      opt_path_tracks, strerror(errno));
      exit(1);
    }
    if (mkdir(opt_path_trackinfo, 0755) == -1) {
      fprintf(stderr, "%s: unable to mkdir '%s': %s\n", progname,
	      opt_path_trackinfo, strerror(errno));
      exit(1);
    }
    if ((f_acl = open(opt_path_acl, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC,
		      0600)) == -1) {
      fprintf(stderr, "%s: unable to make %s: %s\n", progname,
	      opt_path_acl, strerror(errno));
      exit(1);
    }
    if ((f_webacl = open(opt_path_webacl, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC,
			 0644)) == -1) {
      fprintf(stderr, "%s: unable to make %s: %s\n", progname,
	      opt_path_webacl, strerror(errno));
      exit(1);
    }
    if (write(f_acl, "root:a16aKMw/UDpfc:MASTER\n", 26) == -1) {
      fprintf(stderr, "%s: unable to write to %s: %s\n", progname,
	      opt_path_acl, strerror(errno));
      exit(1);
    }
    if (write(f_acl, "guest:ax/jGzB/YyIVk:GUEST\n", 26) == -1) {
      fprintf(stderr, "%s: unable to write to %s: %s\n", progname,
	      opt_path_acl, strerror(errno));
      exit(1);
    }
    if (write(f_webacl, "root:a16aKMw/UDpfc\n", 19) == -1) {
      fprintf(stderr, "%s: unable to write to %s: %s\n", progname,
	      opt_path_webacl, strerror(errno));
      exit(1);
    }
    if (write(f_webacl, "guest:ax/jGzB/YyIVk\n", 20) == -1) {
      fprintf(stderr, "%s: unable to write to %s: %s\n", progname,
	      opt_path_webacl, strerror(errno));
      exit(1);
    }
    if (close(f_acl) == -1) {
      fprintf(stderr, "%s: unable to close %s: %s\n", progname,
	      opt_path_acl, strerror(errno));
      exit(1);
    }
    if (close(f_webacl) == -1) {
      fprintf(stderr, "%s: unable to close %s: %s\n", progname,
	      opt_path_webacl, strerror(errno));
      exit(1);
    }
    if ((f_conf = open(mserv_conf, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC,
		       0644)) == -1) {
      fprintf(stderr, "%s: unable to make %s: %s\n", progname,
	      mserv_conf, strerror(errno));
      exit(1);
    }
    if (write(f_conf, defconf_file, defconf_size) == -1) {
      fprintf(stderr, "%s: unable to write to %s: %s\n", progname,
	      mserv_conf, strerror(errno));
      exit(1);
    }
    if (close(f_conf) == -1) {
      fprintf(stderr, "%s: unable to close %s: %s\n", progname,
	      mserv_conf, strerror(errno));
      exit(1);
    }
  }
  if (acl_load()) {
    mserv_log("Unable to load ACL file");
    exit(1);
  }
  if (mserv_loadlang(opt_path_language)) {
    mserv_log("Corrupt language file");
    exit(1);
  }
  for (i = 0; i < HISTORYLEN; i++) {
    mserv_history[i] = NULL;
  }

  srand(time(NULL));
 
  if (mserv_verbose && mserv_port)
    printf("Port set via command line options to %d\n", mserv_port);
  protocol = getprotobyname("IP");
  mserv_socket = socket(AF_INET, SOCK_STREAM, protocol->p_proto);
  if (mserv_socket == -1) {
    mserv_log("Socket error '%s'", strerror(errno));
    mserv_closedown(1);
  }
  so_int = 1;
  if (setsockopt(mserv_socket, SOL_SOCKET, SO_REUSEADDR,
                 (void *)&so_int, sizeof(so_int)) == -1) {
    mserv_log("setsockopt error '%s'", strerror(errno));
    mserv_closedown(1);
  }    
  sin.sin_family = AF_INET;
  sin.sin_port = htons(mserv_port ? mserv_port : opt_port);
  sin.sin_addr.s_addr = INADDR_ANY;
  if (bind(mserv_socket, (struct sockaddr *)&sin,
	   sizeof(sin)) == -1) {
    mserv_log("Socket bind error '%s'", strerror(errno));
    mserv_closedown(1);
  }
  flags = fcntl(mserv_socket, F_GETFL, 0);
  flags|= O_NONBLOCK | O_APPEND;
  if (fcntl(mserv_socket, F_SETFL, flags) == -1) {
    mserv_log("Socket fcntl error '%s'", strerror(errno));
    mserv_closedown(1);
  }
  if (listen(mserv_socket, BACKLOG) == -1) {
    mserv_log("Socket listen error '%s'", strerror(errno));
    mserv_closedown(1);
  }    

  mserv_log("*** Server started!");

  /* do initialization */
  mserv_init();

  mserv_started = 1;

  signal(SIGINT, mserv_sighandler);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, mserv_sighandler);

  if (opt_play) {
    if (mserv_playnext())
      mserv_broadcast("FINISH", NULL); /* who are we telling? :) */
  }

  mserv_mainloop();

  return(0);
}

static void mserv_mainloop(void)
{
  t_client *cl, *last;
  fd_set fd_reads, fd_writes;
  int client_socket;
  int maxfd;
  struct timeval timeout;
  struct sockaddr_in sin_client;
  int sin_client_len = sizeof(sin_client);
  int flags;
  int i;

  for(;;) {
    /* check all connections */
    for (cl = mserv_clients; cl; cl = cl->next) {
      mserv_pollclient(cl);
    }
    if (mserv_playing.track)
      mserv_checkchild();
    /* check all output buffers, clients may have caused buffers of
       other clients to fill, or checkchild() might have sent something */
    for (cl = mserv_clients; cl; cl = cl->next) {
      mserv_pollwrite(cl);
    }
    /* free closed connections */
    for (cl = mserv_clients, last = NULL; cl; ) {
      if (cl->state == st_closed && cl->outbuf[0].iov_len == 0) {
	mserv_kill(cl);
        if (last == NULL) {
	  mserv_clients = cl->next;
          free(cl);
          cl = mserv_clients;
          continue;
        } else {
	  last->next = cl->next;
          free(cl);
          cl = last->next;
          continue;
        }
      }
      last = cl;
      cl = cl->next;
    }
    /* setup select */
    FD_ZERO(&fd_reads);
    FD_ZERO(&fd_writes);
    FD_SET(mserv_socket, &fd_reads);
    maxfd = mserv_socket+1;
    for (cl = mserv_clients; cl; cl = cl->next) {
      if (cl->socket != -1) {
        FD_SET(cl->socket, &fd_reads);
        if (cl->outbuf[0].iov_len)
	  FD_SET(cl->socket, &fd_writes);
        if (cl->socket >= maxfd)
	  maxfd = cl->socket+1;
      }
    }
    if (!mserv_playnextat ||
	(timeout.tv_sec = difftime(mserv_playnextat, time(NULL))) > 60)
      timeout.tv_sec = 60;
    timeout.tv_usec = 0;
    /* wait for something to happen or for a second to ellapse */
    /* mserv_log("SELECT %d", timeout.tv_sec); */
    select(maxfd, &fd_reads, &fd_writes, NULL, &timeout);
    /* mserv_log("END SELECT"); */
    /* check for time to play next song */
    if (mserv_playnextat && difftime(mserv_playnextat, time(NULL)) <= 0) {
      mserv_playnextat = 0;
      if (!mserv_playing.track) {
	if (mserv_playnext())
	  mserv_broadcast("FINISH", NULL);
      }
    }
    /* check for incoming connections */
    client_socket = accept(mserv_socket, (struct sockaddr *)&sin_client,
			   &sin_client_len);
    if (client_socket == -1) {
      if (errno != EWOULDBLOCK) {
	/* error occured */
	mserv_log("Cannot accept socket: '%s'", strerror(errno));
	mserv_closedown(1);
      }
      continue;
    }
    /* new connection */
    mserv_log("Connection from %s:%d...", inet_ntoa(sin_client.sin_addr),
	      htons(sin_client.sin_port));
    if (fcntl(client_socket, F_SETFD, 1) == -1) {
      mserv_log("fcntl close-on-exec error: '%s'", strerror(errno));
      close(client_socket);
      continue;
    }
    flags = fcntl(client_socket, F_GETFL, 0);
    flags|= O_NONBLOCK;
    if (fcntl(client_socket, F_SETFL, flags) == -1) {
      mserv_log("Socket fcntl error: '%s'", strerror(errno));
      close(client_socket);
      continue;
    }
    if ((cl = malloc(sizeof(t_client))) == NULL) {
      mserv_log("Out of memory creating client structure: %s",
		strerror(errno));
      close(client_socket);
      continue;
    }
    cl->next = mserv_clients;
    mserv_clients = cl;
    cl->sin = sin_client;
    cl->state = st_wait;
    cl->lstate = lst_normal;
    cl->socket = client_socket;
    DEBUG(mserv_log("%s: Allocated socket %x", inet_ntoa(cl->sin.sin_addr)));
    for (i = 0; i < OUTVECTORS; i++) {
      cl->outbuf[i].iov_base = NULL;
      cl->outbuf[i].iov_len = 0;
    }
    if ((cl->outbuf[0].iov_base = malloc(OUTBUFLEN)) == NULL) {
      mserv_log("Out of memory creating output buffer: %s", strerror(errno));
      close(client_socket);
      continue;
    }
    cl->linebuflen = 0;
    cl->lastread = time(NULL);
    cl->authed = 0;
    cl->mode = mode_computer;
    cl->user[0] = '\0';
    mserv_send(cl, "200 Mserv " VERSION " (c) James Ponder 2000 - "
	       "Type: USER <username>\r\n", 0);
    mserv_send(cl, ".\r\n", 0);
  }
}

static void mserv_checkchild(void)
{
  int st, pid;
  t_rating *rate;
  t_client *cl;

  if ((pid = waitpid(mserv_playingpid, &st,
		     WNOHANG | WUNTRACED)) == mserv_playingpid) {
    if (WIFSTOPPED(st)) {
      mserv_broadcast("PAUSE", "%s\t%d\t%d\t%s\t%s", mserv_pausedby,
		      mserv_playing.track->n_album,
		      mserv_playing.track->n_track,
		      mserv_playing.track->author, mserv_playing.track->name);
      mserv_paused = 1;
      strcpy(mserv_pausedby, "unknown");
    } else {
      if (WIFEXITED(st)) {
	if (WEXITSTATUS(st)) {
	  mserv_log("Child process exited (%d)", WEXITSTATUS(st));
	  mserv_broadcast("NOSPAWN", "%d\t%d\t%s\t%s",
			  mserv_playing.track->n_album,
			  mserv_playing.track->n_track,
			  mserv_playing.track->author,
			  mserv_playing.track->name);
	  mserv_playing.track = NULL;
	  mserv_playingpid = 0;
	  mserv_playingstart = 0;
	  mserv_checkshutdown();
	  return;
	}
      } else if (WIFSIGNALED(st)) {
	mserv_log("Child process received signal %d%s",
		  WTERMSIG(st), WCOREDUMP(st) ? " (core dumped)" : "");
      }
      for (cl = mserv_clients; cl; cl = cl->next) {
        if (!cl->authed)
          continue;
        if (cl->mode == mode_human) {
          if ((rate = mserv_getrate(cl->user, mserv_playing.track)) == NULL) {
	    if (mserv_ratetrack(cl, &mserv_playing.track, 0) == NULL)
	      mserv_broadcast("MEMORY", NULL);
	  }
	}
      }
      mserv_checkdisk_track(mserv_playing.track);
      mserv_playing.track->lastplay = time(NULL);
      mserv_playing.track->modified = 1;
      mserv_playing.track = NULL;
      mserv_playingpid = 0;
      mserv_playingstart = 0;
      mserv_recalcratings(); /* recalc ratings now lastplay has changed */
      mserv_savechanges();
      mserv_checkshutdown();
      if (!mserv_gap) {
	if (mserv_playnext())
	  mserv_broadcast("FINISH", NULL);
      }
      mserv_playnextat = time(NULL) + mserv_gap;
    }
  } else if (pid == -1) {
    mserv_log("waitpid failure (%d): %s", errno, strerror(errno));
    exit(1);
  }
}

static void mserv_pollwrite(t_client *cl)
{
  int bytes;
  unsigned int i, size;

  if (cl->socket < 0)
    return;
  /* find last block with stuff in it */
  for (i = 0; i < OUTVECTORS; i++) {
    /* mserv_log("%s: output %d = %x/%d", inet_ntoa(cl->sin.sin_addr),
       i, cl->outbuf[i].iov_base, cl->outbuf[i].iov_len); */
    if (!cl->outbuf[i].iov_base)
      break;
  }
  if (!i) {
    /* there are no vector blocks allocated! */
    mserv_log("%s: Internal error in output buffers",
	      inet_ntoa(cl->sin.sin_addr));
    mserv_kill(cl);
    return;
  }
  /* mserv_log("%s: WRITING %i blocks", inet_ntoa(cl->sin.sin_addr), i); */
  bytes = writev(cl->socket, cl->outbuf, i);
  if (bytes == -1) {
    if (errno == EPIPE) {
      mserv_log("%s: Lost connection", inet_ntoa(cl->sin.sin_addr));
      mserv_kill(cl);
    } else if (errno != EAGAIN) {
      mserv_log("%s: Write error '%s'", inet_ntoa(cl->sin.sin_addr),
		strerror(errno));
      mserv_kill(cl);
    }
    return;
  }
  DEBUG(mserv_log("%s: write %d bytes", inet_ntoa(cl->sin.sin_addr), bytes));
  while(bytes) {
    if (cl->outbuf[0].iov_len == 0) {
      mserv_log("%s: Internal output buffer error",
		inet_ntoa(cl->sin.sin_addr));
      mserv_kill(cl);
      return;
    }
    if ((size_t)bytes < (size_t)cl->outbuf[0].iov_len) {
      DEBUG(mserv_log("memmove 2: %x->%x %d",cl->outbuf[0].iov_base,
		      (char *)cl->outbuf[0].iov_base + bytes,
		      cl->outbuf[0].iov_len - bytes));
      memmove(cl->outbuf[0].iov_base,
	      (char *)cl->outbuf[0].iov_base + bytes,
	      cl->outbuf[0].iov_len - bytes);
      cl->outbuf[0].iov_len-= bytes;
      /* now shuffle things around so that the first blocks are full */
      for (i = 1; i < OUTVECTORS; i++) {
        if (!cl->outbuf[i].iov_base)
          break;
	size = OUTBUFLEN - cl->outbuf[i-1].iov_len; /* amount to fill */
	DEBUG(mserv_log("memmove 3: %x->%x %d",
			(char *)cl->outbuf[i-1].iov_base +
			cl->outbuf[i-1].iov_len,
			cl->outbuf[i].iov_base, size));
	memmove((char *)cl->outbuf[i-1].iov_base + cl->outbuf[i-1].iov_len,
		cl->outbuf[i].iov_base, size);
	DEBUG(mserv_log("memmove 4: %x->%x %d", cl->outbuf[i].iov_base,
			(char *)cl->outbuf[i].iov_base + size,
			cl->outbuf[i-1].iov_len));
	memmove(cl->outbuf[i].iov_base, (char *)cl->outbuf[i].iov_base + size,
		cl->outbuf[i-1].iov_len);
	if ((size_t)cl->outbuf[i].iov_len < (size_t)size)
	  size = cl->outbuf[i].iov_len;
	cl->outbuf[i-1].iov_len+= size;
	if ((cl->outbuf[i].iov_len-= size) == 0) {
	  free(cl->outbuf[i].iov_base);
	  cl->outbuf[i].iov_base = NULL;
	  cl->outbuf[i].iov_len = 0;
	  break;
	}
      }
      bytes = 0;
      break;
    }
    bytes-= cl->outbuf[0].iov_len;
    if (cl->outbuf[1].iov_base) {
      free(cl->outbuf[0].iov_base);
      DEBUG(mserv_log("memmove 1: %x->%x %d", &cl->outbuf, &cl->outbuf[1],
		sizeof(struct iovec)*(OUTVECTORS-1)));
      memmove(&cl->outbuf, &cl->outbuf[1],
	      sizeof(struct iovec)*(OUTVECTORS-1));
      cl->outbuf[OUTVECTORS-1].iov_base = NULL;
      cl->outbuf[OUTVECTORS-1].iov_len = 0;
    } else {
      cl->outbuf[0].iov_len = 0;
    }
  }
}

static void mserv_pollclient(t_client *cl)
{
  int bytes;
  int i;
  unsigned char c;
  char inbuf[INBUFLEN];

  DEBUG(mserv_log("%s: poll %x state %d",
		  inet_ntoa(cl->sin.sin_addr), cl, cl->state));

  mserv_pollwrite(cl);

  if (cl->state == st_closed)
    return;

  bytes = INBUFLEN;
  while (cl->socket >= 0 && bytes == INBUFLEN) {
    /* try reading any data there is */
    bytes = read(cl->socket, inbuf, INBUFLEN);
    if (bytes == -1) {
      if (errno != EAGAIN) {
	mserv_log("Read error '%s'", strerror(errno));
      }
    } else if (bytes == 0) {
      mserv_log("%s: Client closed connection", inet_ntoa(cl->sin.sin_addr));
      mserv_close(cl);
      return;
    } else {
      DEBUG(mserv_log("%s: read %d bytes", inet_ntoa(cl->sin.sin_addr),
		      bytes));
      cl->lastread = time(NULL);
      for (i = 0; i < bytes; i++) {
	c = inbuf[i];
        DEBUG(mserv_log("%s: byte %d state %d", inet_ntoa(cl->sin.sin_addr),
			c, cl->lstate));
	switch(cl->lstate) {
	case lst_cr:
	  cl->lstate = lst_normal;
	  if (c == 0 || c == 10) {
	    DEBUG(mserv_log("%s: EOL received", inet_ntoa(cl->sin.sin_addr)));
	    break;
	  }
	case lst_normal:
	  if (c == 13) {
	    cl->linebuf[cl->linebuflen] = '\0';
	    mserv_endline(cl, (char *)cl->linebuf);
	    cl->linebuflen = 0;
	    cl->lstate = lst_cr;
	  } else if (c == 10) {
	    cl->linebuf[cl->linebuflen] = '\0';
	    mserv_endline(cl, (char *)cl->linebuf);
	    cl->linebuflen = 0;
	    cl->lstate = lst_normal;
	  } else if (c >= 32) {
	    if (cl->linebuflen < LINEBUFLEN-1) {
	      cl->linebuf[cl->linebuflen++] = c;
	    }
	  }
	  break;
	}
      }
    }
  }
}

void mserv_kill(t_client *cl)
{
  int i;
  if (cl->socket == -1)
    return;
  if (cl->state != st_closed)
    mserv_close(cl);
  for (i = 0; i < OUTVECTORS; i++) {
    if (cl->outbuf[i].iov_base)
      free(cl->outbuf[i].iov_base);
    cl->outbuf[i].iov_base = NULL;
    cl->outbuf[i].iov_len = 0;
  }
  close(cl->socket);
  cl->socket = -1;
}

void mserv_close(t_client *cl)
{
  t_queue *a, *b, *c;

  if (cl->state == st_closed)
    return;
  mserv_log("%s: Closing connection to %s", inet_ntoa(cl->sin.sin_addr),
	    cl->authed ? cl->user : "unknown");
  cl->state = st_closed;
  if (cl->authed && cl->mode != mode_computer)
    mserv_broadcast("DISCONN", "%s", cl->user);
  if (cl->userlevel != level_guest && cl->mode != mode_computer)
    mserv_recalcratings();
  if (cl->mode != mode_computer) {
    a = NULL;
    b = mserv_queue;
    c = mserv_queue ? mserv_queue->next : NULL;
    while (b) {
      if (!stricmp(b->supinfo.user, cl->user)) {
	mserv_broadcast("UNQA", "%d\t%d\t%s\t%s",
			b->supinfo.track->n_album, b->supinfo.track->n_track,
			b->supinfo.track->author, b->supinfo.track->name);
	free(b);
	if (a == NULL)
	  mserv_queue = c;
	else
	  a->next = c;
      } else {
	a = b;
      }
      b = c;
      c = c ? c->next : NULL;
    }
  }
}

void mserv_send(t_client *cl, const char *data, unsigned int len)
{
  int bytes, i, size;

  (void)bytes;

  if (cl->state == st_closed || cl->socket < 0)
    return;
  if (!len)
    len = strlen(data);
  if (cl->outbuf[1].iov_len)
    /* this is outbuf[1] and not [0] so that it writes in chunks */
    mserv_pollwrite(cl);
  if (len == 0)
    return; /* all data written */
  for (i = 0; len && i < OUTVECTORS; i++) {
    if (!cl->outbuf[i].iov_base) {
      if ((cl->outbuf[i].iov_base = malloc(OUTBUFLEN)) == NULL) {
	mserv_log("Out of memory allocating output buffer");
	mserv_close(cl);
	return;
      }
      cl->outbuf[i].iov_len = 0;
    }
    if (cl->outbuf[i].iov_len < OUTBUFLEN) {
      /* space in this buffer */
      size = MIN(len, (unsigned int)(OUTBUFLEN - cl->outbuf[i].iov_len));
      memmove(((char *)cl->outbuf[i].iov_base) + cl->outbuf[i].iov_len, data,
	      size);
      cl->outbuf[i].iov_len+= size;
      len-= size;
      data+= size;
    }
  }
  if (i == OUTVECTORS) {
    /* output buffer overflow */
    mserv_log("%s: Output buffer overflow (needed %d bytes)",
	      inet_ntoa(cl->sin.sin_addr), len);
    for (i = 0; i < OUTVECTORS; i++) {
      mserv_log("%d: %x %d", i, cl->outbuf[i].iov_base, cl->outbuf[i].iov_len);
    }
    mserv_close(cl);
    return;
  }
}

char *mserv_idletxt(time_t idletime)
{
  static char buffer[64];
  int mins = (idletime/60) % 60;
  int hours = (idletime/3600);

  if (hours)
    sprintf(buffer, "%d:%02d", hours, mins);
  else
    sprintf(buffer, "%d", mins);

  return buffer;
}

/* mserv_endline - a line has been received from the client */

static void mserv_endline(t_client *cl, char *line)
{
  t_cmds *cmdsptr;
  int len;
  char *p = line;
  char *q;
  char ru[USERNAMELEN+1];

  if (cl->mode == mode_human && !*line)
    return;

  switch(cl->state) {
  case st_wait:
    while (*p == ' ')
      p++;
    /* check for [user] before command */
    if (*p == '[') {
      p++;
      if ((q = strchr(p, ']')) == NULL) {
	mserv_response(cl, "BADCOM", NULL);
	break;
      }
      len = MIN((q-p), USERNAMELEN);
      strncpy(ru, p, len);
      ru[len] = '\0';
      p = q+1;
      while (*p == ' ')
	p++;
    } else {
      strcpy(ru, cl->user);
    }
    for (cmdsptr = mserv_cmds; cmdsptr->name; cmdsptr++) {
      if (!mserv_checklevel(cl, cmdsptr->userlevel))
	continue;
      len = strlen(cmdsptr->name);
      if (strnicmp(p, cmdsptr->name, len) == 0) {
	if (p[len] != '\0' && p[len] != ' ')
	  continue;
	p+= len;
	while(*p == ' ')
	  p++;
	while(*p && p[strlen(p)-1] == ' ')
	  p[strlen(p)-1] = '\0';
	if (cmdsptr->authed && !cl->authed)
	  mserv_response(cl, "NOTAUTH", NULL);
	else
	  cmdsptr->function(cl, ru, p);
	return;
      }
    }
    mserv_response(cl, "BADCOM", NULL);
    break;
  default:
    mserv_response(cl, "STATE", NULL);
  }
}

int mserv_checklevel(t_client *cl, t_userlevel level)
{
  /* stop guests */
  if (cl->userlevel == level_guest && level != level_guest)
    return 0;
  /* only master can do master commands */
  if (level == level_master && cl->userlevel != level_master)
    return 0;
  /* only master and priv users can do priv commands */
  if (level == level_priv && (cl->userlevel != level_master &&
			      cl->userlevel != level_priv))
    return 0;
  return 1;
}

/* 1 <= val <= 5, 0 = heard */

t_rating *mserv_ratetrack(t_client *cl, t_track **track, unsigned int val)
{
  t_rating *rate, *r, *p;

  *track = mserv_checkdisk_track(*track);
  (*track)->modified = 1; /* save information */

  if ((rate = mserv_getrate(cl->user, *track)) != NULL) {
    rate->rating = val;
    rate->when = time(NULL);
    return rate;
  }
  if ((rate = malloc(sizeof(t_rating)+strlen(cl->user)+1)) == NULL) {
    mserv_log("Out of memory creating rating");
    return NULL;
  }
  rate->user = (char *)(rate+1);
  strcpy(rate->user, cl->user);
  rate->rating = val;
  rate->when = time(NULL);
  rate->next = NULL;
  for (p = NULL, r = (*track)->ratings; r; p = r, r = r->next) ;
  if (p)
    p->next = rate;
  else
    (*track)->ratings = rate;
  return rate;
}

t_track *mserv_altertrack(t_track *track, const char *author,
			  const char *name, const char *genres,
			  const char *miscinfo)
{
  t_track *newtrack;
  const char *new_author = author;
  const char *new_name = name;
  const char *new_genres = genres;
  const char *new_miscinfo = miscinfo;

  track = mserv_checkdisk_track(track);

  if (!new_author)
    new_author = track->author;
  if (!new_name)
    new_name = track->name;
  if (!new_genres)
    new_genres = track->genres;
  if (!new_miscinfo)
    new_miscinfo = track->miscinfo;
  mserv_log("Track '%s'/'%s'/'%s'/'%s' altered to '%s'/'%s'/'%s'/'%s'",
	    track->name, track->author, track->genres, track->miscinfo,
	    new_name, new_author, new_genres, new_miscinfo);

  if ((newtrack = malloc(sizeof(t_track)+strlen(new_author)+strlen(new_name)+
			 strlen(new_genres)+strlen(track->filename)+
			 strlen(new_miscinfo)+5)) == NULL) {
    mserv_log("Out of memory altering '%s'", track->filename);
    return NULL;
  }
  memcpy(newtrack, track, sizeof(t_track));
  newtrack->author = (char *)(newtrack+1);
  newtrack->name = newtrack->author+strlen(new_author)+1;
  newtrack->filename = newtrack->name+strlen(new_name)+1;
  newtrack->genres = newtrack->filename+strlen(track->filename)+1;
  newtrack->miscinfo = newtrack->genres+strlen(new_genres)+1;
  strcpy(newtrack->author, new_author);
  strcpy(newtrack->name, new_name);
  strcpy(newtrack->filename, track->filename);
  strcpy(newtrack->genres, new_genres);
  strcpy(newtrack->miscinfo, new_miscinfo);
  newtrack->modified = 1;
  mserv_replacetrack(track, newtrack);
  strcpy(track->name, "*");
  free(track);
  mserv_savechanges();
  return newtrack;
}

t_album *mserv_alteralbum(t_album *album, const char *author,
			   const char *name)
{
  t_album *newalbum;
  const char *new_author = author;
  const char *new_name = name;

  album = mserv_checkdisk_album(album);

  if (!new_author)
    new_author = album->author;
  if (!new_name)
    new_name = album->name;

  mserv_log("Album '%s'/'%s' altered to '%s'/'%s'", album->name,
	    album->author, new_name, new_author);

  if ((newalbum = malloc(sizeof(t_album)+strlen(new_author)+strlen(new_name)+
			 strlen(album->filename)+3)) == NULL) {
    mserv_log("Out of memory renaming '%s'", album->filename);
    exit(1);
  }
  memcpy(newalbum, album, sizeof(t_album));
  newalbum->author = (char *)(newalbum+1);
  newalbum->name = newalbum->author+strlen(new_author)+1;
  newalbum->filename = newalbum->name+strlen(new_name)+1;
  strcpy(newalbum->author, new_author);
  strcpy(newalbum->name, new_name);
  strcpy(newalbum->filename, album->filename);
  newalbum->modified = 1;
  mserv_replacealbum(album, newalbum);
  strcpy(album->name, "*");
  free(album);
  mserv_savechanges();
  return newalbum;
}

static void mserv_replacetrack(t_track *track, t_track *newtrack)
{
  t_queue *queue;
  t_track *tr;
  t_album *album;
  t_author *au;
  t_genre *gen;
  unsigned int ui;

  newtrack->next = track->next;
  newtrack->id = track->id;
  newtrack->n_album = track->n_album;
  newtrack->n_track = track->n_track;

  /* change album track pointers */
  for (album = mserv_albums; album; album = album->next) {
    for (ui = 0; ui < TRACKSPERALBUM; ui++) {
      if (album->tracks[ui] == track)
	album->tracks[ui] = newtrack;
    }
  }
  /* change currently playing track pointer */
  if (mserv_playing.track == track)
    mserv_playing.track = newtrack;
  /* change queue track pointers */
  for (queue = mserv_queue; queue; queue = queue->next) {
    if (queue->supinfo.track == track)
      queue->supinfo.track = newtrack;
  }
  /* change track linked list pointer */
  for (tr = mserv_tracks; tr; tr = tr->next) {
    if (tr->next == track)
      tr->next = newtrack;
  }
  /* change history track pointers */
  for (ui = 0; ui < HISTORYLEN; ui++) {
    if (mserv_history[ui] && mserv_history[ui]->track == track)
      mserv_history[ui]->track = newtrack;
  }
  for (au = mserv_authors; au; au = au->next) {
    for (ui = 0; ui < TRACKSPERALBUM; ui++) {
      if (au->tracks[ui] == track) {
	au->tracks[ui] = newtrack;
      }
    }
  }
  for (gen = mserv_genres; gen; gen = gen->next) {
    for (ui = 0; ui < gen->total; ui++) {
      if (gen->tracks[ui] == track) {
	gen->tracks[ui] = newtrack;
      }
    }
  }
  if (mserv_tracks == track)
    mserv_tracks = newtrack;
}

static void mserv_replacealbum(t_album *album, t_album *newalbum)
{
  t_album *al;

  newalbum->id = album->id;
  newalbum->next = album->next;
  memcpy(newalbum->tracks, album->tracks, TRACKSPERALBUM*sizeof(t_track *));

  /* change album linked list pointer */
  for (al = mserv_albums; al; al = al->next) {
    if (al->next == album) {
      al->next = newalbum;
      break;
    }
  }
  if (mserv_albums == album)
    mserv_albums = newalbum;
}

void mserv_closedown(int exitcode)
{
  signal(SIGINT, SIG_IGN); /* ignore bad signals */
  if (mserv_logfile)
    fclose(mserv_logfile);
  exit(exitcode);
}

static int mserv_loadlang(const char *pathname)
{
  char buffer[LANGLINELEN];
  FILE *fd;
  char *s;
  int line = 0;
  t_lang *lang;
  char *tab[10];
  int i;

  if ((fd = fopen(pathname, "r")) == NULL) {
    perror("fopen");
    mserv_log("Unable to fopen language file for reading");
    return 1;
  }

  while(fgets(buffer, LANGLINELEN, fd)) {
    line++;
    if (buffer[strlen(buffer)-1] != '\n') {
      mserv_log("Line %d too long in language file.", line);
      fclose(fd);
      return 1;
    } else {
      buffer[strlen(buffer)-1] = '\0';
      if (*buffer == ';' || !*buffer)
	continue;
      for (i = 0; i < 10; i++)
	tab[i] = NULL;
      s = buffer;
      for (i = 0; i < 10 && (tab[i] = strsep(&s, "\t")); i++)
	;
      if (!tab[0] || !tab[1] || !tab[2]) {
	mserv_log("Invalid line in language file at line %d.", line);
	fclose(fd);
	return 1;
      }
      if ((lang = malloc(sizeof(t_lang)+strlen(tab[0])+
			 strlen(tab[2])+2)) == NULL) {
	mserv_log("Out of memory.");
	return 1;
      }
      lang->code = atoi(tab[1]);
      lang->token = (char *)(lang+1);
      strcpy(lang->token, tab[0]);
      lang->text = lang->token+strlen(lang->token)+1;
      strcpy(lang->text, tab[2]);
      lang->next = mserv_language;
      mserv_language = lang;
    }
  }
  if (ferror(fd)) {
    perror("fgets");
    return 1;
  }
  fclose(fd);
  return 0;
}

t_lang *mserv_gettoken(const char *token)
{
  t_lang *lang;

  for (lang = mserv_language; lang; lang = lang->next) {
    if (!strcmp(token, lang->token)) {
      break;
    }
  }
  return lang;
}

/* mserv_broadcast("VOLUME", "%s", user); */

void mserv_broadcast(const char *token, const char *fmt, ...)
{
  t_lang *lang;
  char outputcomp[1024];
  char output[1024];
  char *str[21];
  int params;
  va_list ap;
  t_client *cl;
  int i;

  va_start(ap, fmt);

  if ((lang = mserv_gettoken(token)) == NULL) {
    mserv_log("Failed to find language token %s", token);
    exit(1);
  }
  if (fmt) {
    vsnprintf(outputcomp, 1024, fmt, ap);
    if ((params = mserv_split(str, 20, outputcomp, "\t")) >= 20) {
      mserv_log("Internal error - too many parameters passed to vresponse");
      exit(1);
    }
  } else {
    params = 0;
  }
  for (cl = mserv_clients; cl; cl = cl->next) {
    if (!cl->authed)
      continue;
    if (cl->mode == mode_rtcomputer) {
      snprintf(output, 1024, "=%d", lang->code);
      mserv_send(cl, output, 0);
      if (fmt) {
	for (i = 0; i < params; i++) {
	  mserv_send(cl, "\t", 1);
	  mserv_send(cl, str[i], 0);
	}
      }
      mserv_send(cl, "\r\n", 0);
    } else if (cl->mode == mode_human) {
      mserv_argsubs(output, 1024, (const char * const *)str, params,
		    lang->text);
      mserv_send(cl, "[] ", 0);
      mserv_sendwrap(cl, output, 76, "[]   ", 74);
    }
  }
  va_end(ap);
}

/* mserv_response(cl, "VOLUME", "%s", user); */

void mserv_response(t_client *cl, const char *token, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  mserv_vresponse(cl, token, fmt, ap);
  va_end(ap);
  if (cl->mode != mode_human)
    mserv_send(cl, ".\r\n", 0);
}

void mserv_responsent(t_client *cl, const char *token, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  mserv_vresponse(cl, token, fmt, ap);
  va_end(ap);
}


static void mserv_vresponse(t_client *cl, const char *token, const char *fmt,
		     va_list ap)
{
  t_lang *lang;
  char outputcomp[1024];
  char output[1024];
  char *str[21];
  int params;
  int i;

  if ((lang = mserv_gettoken(token)) == NULL) {
    mserv_log("Failed to find language token %s", token);
    exit(1);
  }
  if (fmt) {
    vsnprintf(outputcomp, 1024, fmt, ap);
    if ((params = mserv_split(str, 20, outputcomp, "\t")) >= 20) {
      mserv_log("Internal error - too many parameters passed to vresponse");
      exit(1);
    }
  } else {
    params = 0;
  }
  if (cl->mode == mode_rtcomputer || cl->mode == mode_computer) {
    snprintf(output, 1024, "%d ", lang->code);
    mserv_send(cl, output, 0);
    mserv_argsubs(output, 1024, (const char * const *)str, params, lang->text);
    mserv_send(cl, output, 0);
    mserv_send(cl, "\r\n", 0);
    if (fmt) {
      if (params) {
	mserv_send(cl, str[0], 0);
	for (i = 1; i < params; i++) {
	  mserv_send(cl, "\t", 1);
	  mserv_send(cl, str[i], 0);
	}
      }
      mserv_send(cl, "\r\n", 0);
    }
  } else if (cl->mode == mode_human) {
    mserv_send(cl, "[] ", 0);
    mserv_argsubs(output, 1024, (const char * const *)str, params,
		  lang->text);
    mserv_sendwrap(cl, output, 76, "[]     ", 72);
  }
}

#define BREAK(c) ((c) == ' ' || (c) == '&' || (c) == '|')

static void mserv_sendwrap(t_client *cl, const char *string, int wrapwidth,
			   const char *indentstr, int nextwidth) {
  const char *end, *p;
  int i, x;
  end = string + strlen(string);
  p = string;
  while (end > p && BREAK(*(end-1)))
    /* strip end spaces, ensures word wrap code won't end up printing
       a blank line at the end */
    end--;
  while (end-p > wrapwidth) {
    if (BREAK(p[wrapwidth])) {
      /* wrapwidth+1 char is a breakable character */
      for (i = wrapwidth-1; i >= 0; i--) {
	if (p[i] != ' ')
	  break;
      }
      if (i >= 0) {
	/* there is text in the first wrapwidth chars */
	mserv_send(cl, p, i+1);
	mserv_send(cl, "\r\n", 0);
	mserv_send(cl, indentstr, 0);
	p+= wrapwidth+1; /* wrapwidth char is a space f'sure */
	wrapwidth = nextwidth;
      } else {
	/* only spaces in entire line, remove */
	p+= wrapwidth+1; /* wrapwidth char is a space f'sure */
      }
    } else {
      /* wrapwidth+1 char is not a space */
      for (i = wrapwidth-1; i >= 0; i--) {
	if (BREAK(p[i]))
	  break;
      }
      if (i >= 0) {
	/* there is a break in the first wrapwidth chars */
	if (end-(p+i+1) > wrapwidth) {
	  for (x = i+1; p[x] ; x++) {
	    if (BREAK(p[x]))
	      break;
	  }
	  if (x-(i+1) > wrapwidth) {
	    /* no point wrapping when the next line can't even fit
	       on a line anyway, so break at wrapwidth */
	    mserv_send(cl, p, wrapwidth);
	    mserv_send(cl, "\r\n", 0);
	    mserv_send(cl, indentstr, 0);
	    p+= wrapwidth;
	    wrapwidth = nextwidth;
	    goto next;
	  }
	}
	for (; i >= 0; i--) {
	  if (p[i] != ' ')
	    break;
	}
	if (i >= 0) {
	  /* there is text before the break */
	  if (i < wrapwidth/2) {
	    /* lets not wrap if the blank is less than half way
	       across the line */
	    mserv_send(cl, p, wrapwidth);
	    mserv_send(cl, "\r\n", 0);
	    mserv_send(cl, indentstr, 0);
	    p+= wrapwidth;
	    wrapwidth = nextwidth;
	  } else {
	    mserv_send(cl, p, i+1);
	    mserv_send(cl, "\r\n", 0);
	    mserv_send(cl, indentstr, 0);
	    p+= i+1;
	    wrapwidth = nextwidth;
	  }
	} else {
	  /* there is a load of blank and then lots of text, always
	     preserve blank bits at start of lines */
	  mserv_send(cl, p, wrapwidth);
	  mserv_send(cl, "\r\n", 0);
	  mserv_send(cl, indentstr, 0);
	  p+= wrapwidth;
	  wrapwidth = nextwidth;
	}
      } else {
	/* there is no break in the first wrapwidth chars */
	mserv_send(cl, p, wrapwidth);
	mserv_send(cl, "\r\n", 0);
	mserv_send(cl, indentstr, 0);
	p+= wrapwidth;
	wrapwidth = nextwidth;
      }
    next:
      while (*p == ' ')
	p++;
    }
  }
  mserv_send(cl, p, end-p);
  mserv_send(cl, "\r\n", 0);
}

void mserv_log(const char *text, ...)
{
  char tmp[800], tmp2[1024];
  char timetmp[64];
  struct tm *timeptr;
  time_t tloc;
  va_list ap;

  va_start(ap, text);
  vsnprintf(tmp, 799, text, ap);
  va_end(ap);

  if (!mserv_logfile) {
    if (!(mserv_logfile = fopen(opt_path_logfile, "a"))) {
      fprintf(stderr, "Could not open logfile: %s\n", strerror(errno));
      mserv_closedown(1);
    }
  }

  tloc = time(NULL);
  timeptr = localtime(&tloc);

  strftime(timetmp, 63, "%m/%d %H:%M:%S", timeptr);
  snprintf(tmp2, 1023, "%s [%06d] %s\n", timetmp, getpid(), tmp);

  fputs(tmp2, mserv_logfile);
  if (mserv_verbose || !mserv_started)
    fputs(tmp2, stdout);
  fflush(mserv_logfile);
}

static int mserv_argsubs(char *outbuf, const int length,
			 const char * const *str, const unsigned int params,
			 const char *fmt)
{
  const char *cp = fmt;
  char * const end = outbuf+length;
  unsigned int argno;
  int len;
  char *out = outbuf;

  if (!length)
    return -1;

  do {
    switch(*cp) {
    case 0:
      *out = '\0';
      return (out-outbuf);
    case '%':
      cp++; /* skip % char */
      if (isdigit((int)*cp) && (isdigit((int)*(cp+1)))) {
	argno = 10*(*cp - '0') + (*(cp+1) - '0');
	cp+= 2;
	if (argno < 1 || argno > params) {
	  mserv_log("Reference to unknown parameter %d in language "
		    "string '%s'", argno, fmt);
	  *out = '\0';
	  return -1;
	}
	len = strlen(str[argno-1]);
	if ((end-out) < len) {
	  strncpy(out, str[argno-1], end-out);
	  /* output truncated */
	  *(end-1) = '\0';
	  return -1;
	}
	strcpy(out, str[argno-1]);
	out+= len;
      } else if (*cp == '%') {
	*out++ = '%';
	cp++;
      } else {
	mserv_log("Invalid argument substitution in language string '%s'",
		  fmt);
	*out = '\0';
	return -1;
      }
      break;
    default:
      *out++ = *cp++;
    }
  } while (out < end);
  /* output truncated */
  *(end-1) = '\0';
  return -1;
}

static void mserv_scandir(void)
{
  mserv_log("Scanning '%s'...", opt_path_tracks);
  mserv_scandir_recurse("");
  mserv_log("Finished scanning.");
  mserv_recalcratings();
}

static void mserv_scandir_recurse(const char *pathname)
{
  DIR *dir;
  struct dirent *ent;
  char fullpath[1024];
  char *filename;
  int llen;
  int tnum, toomany;
  t_track *(tracks[TRACKSPERALBUM]);
  struct stat buf;
  int i;
  t_album *album;
  int flag = 0;

  /* pathname is "" or "directory/" or "directory/directory/..." */

  if (mserv_verbose)
    mserv_log("Scan directory: %s", pathname);

  for (tnum = 0; tnum < TRACKSPERALBUM; tnum++)
    tracks[tnum] = NULL;
  tnum = 0;
  toomany = 0;

  /* find .mp3 files and use .trk files if they exist */

  if ((unsigned int)snprintf(fullpath, 1024, "%s/%s",
			     opt_path_tracks, pathname) >= 1024) {
    mserv_log("fullpath buffer too small");
    return;
  }
  if ((dir = opendir(fullpath)) == NULL) {
    perror("opendir");
    mserv_log("Unable to opendir '%s'", fullpath);
    return;
  }
  while((ent = readdir(dir))) {
    llen = strlen(ent->d_name);
    if (ent->d_name[0] == '.')
      continue;
    if ((unsigned int)snprintf(fullpath, 1024, "%s/%s%s", opt_path_tracks,
			       pathname, ent->d_name) >= 1024) {
      mserv_log("fullpath buffer too small");
      return;
    }
    if (stat(fullpath, &buf)) {
      perror("stat");
      mserv_log("Unable to stat '%s'", fullpath);
      continue;
    }
    filename = fullpath+strlen(opt_path_tracks)+1;
    if (S_ISDIR(buf.st_mode)) {
      strcat(filename, "/");
      mserv_scandir_recurse(filename);
    } else if (!toomany && S_ISREG(buf.st_mode)) {
      if (llen > 4 && !stricmp(".mp3", ent->d_name+llen-4)) {
	if (tnum >= TRACKSPERALBUM) {
	  mserv_log("The limit of tracks per album was reached, and some "
		    "tracks were discarded. To increase this limit "
		    "recompile with TRACKSPERALBUM set higher.  A better "
		    "solution is to sub-divide your tracks into more "
		    "directories.");
	  toomany = 1;
	  break;
	}
	if (mserv_verbose)
	  mserv_log("Track file: %s", fullpath);
	if ((tracks[tnum] = mserv_loadtrk(filename)) == NULL) {
	  mserv_log("Unable to add track '%s'", fullpath);
	} else {
	  tracks[tnum]->id = mserv_nextid_track++;
	  tracks[tnum]->n_album = mserv_nextid_album;
	  tracks[tnum]->n_track = tnum+1;
	  tracks[tnum]->next = mserv_tracks;
	  mserv_tracks = tracks[tnum++];
	}
	flag = 1; /* there is at least one track in this directory */
      }
    }
  }
  closedir(dir);
  /* load album, but only if there is an album file or flag is set */
  if ((album = mserv_loadalbum(pathname, flag ? 0 : 1)) == NULL)
    return;
  qsort(tracks, TRACKSPERALBUM, sizeof(t_track *),
	mserv_trackcompare_filename);
  /* we've sorted so we need to renumber tracks */
  for (i = 0; i < TRACKSPERALBUM; i++) {
    album->tracks[i] = tracks[i];
    if (album->tracks[i])
      album->tracks[i]->n_track = i+1;
  }
  if (mserv_verbose)
    mserv_log("Added album: '%s' by %s", album->name, album->author);
  if (album_insertsort(album)) {
    free(album);
    return;
  }
  return;
}

static int album_insertsort(t_album *album)
{
  t_album *al, *p;
  int a;

  for (p = NULL, al = mserv_albums; al; al = al->next) {
    if ((a = strcmp(al->author, album->author)) > 0)
      break;
    if (a == 0) {
      if ((a = strcmp(al->name, album->name)) > 0)
        break;
      if (a == 0) {
	mserv_log("Duplicate album for author '%s' name '%s' (%s and %s)",
		  album->author, album->name, album->filename, al->filename);
        return 1;
      }
    }
    p = al;
  }
  album->next = al;
  if (!p)
    mserv_albums = album;
  else
    p->next = album;
  return 0;
}

static int mserv_trackcompare_filename(const void *a, const void *b)
{
  /* sorts in ascending order */
  if (*(const t_track * const *)a == NULL) {
    if (*(const t_track *const *)b == NULL)
      return 0;
    else
      return 1;
  }
  if (*(const t_track * const *)b == NULL)
    return -1;
  return stricmp((*(const t_track * const *)a)->filename,
		 (*(const t_track * const *)b)->filename);
}

static int mserv_trackcompare_name(const void *a, const void *b)
{
  /* sorts in ascending order */
  if (*(const t_track * const *)a == NULL) {
    if (*(const t_track *const *)b == NULL)
      return 0;
    else
      return 1;
  }
  if (*(const t_track * const *)b == NULL)
    return -1;
  return stricmp((*(const t_track * const *)a)->name,
		 (*(const t_track * const *)b)->name);
}

static int mserv_trackcompare_rating(const void *a, const void *b)
{
  /* sorts in descending order */
  if (*(const t_track * const *)a == NULL) {
    if (*(const t_track *const *)b == NULL)
      return 0;
    else
      return 1;
  }
  if (*(const t_track * const *)b == NULL)
    return -1;
  if ((*(const t_track * const *)a)->rating ==
      (*(const t_track * const *)b)->rating) {
    if ((*(const t_track * const *)a)->lastplay ==
	(*(const t_track * const *)b)->lastplay) {
      if ((*(const t_track * const *)a)->random ==
	  (*(const t_track * const *)b)->random) {
	return 0;
      } else if ((*(const t_track * const *)a)->random <
		 (*(const t_track * const *)b)->random) {
	return 1;
      } else {
	return -1;
      }
    } else if ((*(const t_track * const *)a)->lastplay >
	       (*(const t_track * const *)b)->lastplay) {
      return 1;
    } else {
      return -1;
    }
  } else if ((*(const t_track * const *)a)->rating <
	     (*(const t_track * const *)b)->rating) {
    return 1;
  } else {
    return -1;
  }
}

static t_album *mserv_loadalbum(const char *filename, int onlyifexists)
{
  FILE *fd;
  char fullpath[MAXFNAME];
  char buffer[1024];
  char author[AUTHORLEN+1] = "";
  char name[NAMELEN+1] = "";
  char token[64];
  char *value;
  t_album *album;
  struct stat buf;
  int alen;
  const char *p, *q;
  int line = 0;
  int l;
  time_t mtime;

  if ((unsigned int)snprintf(fullpath, MAXFNAME, "%s/%s",
			     opt_path_trackinfo, filename) >= MAXFNAME) {
    mserv_log("fullpath buffer too small");
    return NULL;
  }
  if (mserv_createdir(fullpath)) {
    mserv_log("Unable to create dir '%s'", fullpath);
    return NULL;
  }
  if ((unsigned int)snprintf(fullpath, MAXFNAME, "%s/%salbum",
			     opt_path_trackinfo, filename) >= MAXFNAME) {
    mserv_log("fullpath buffer too small");
    return NULL;
  }
  if (stat(fullpath, &buf) != -1 && S_ISREG(buf.st_mode)) {
    if ((fd = fopen(fullpath, "r")) == NULL) {
      perror("fopen");
      mserv_log("Unable to fopen '%s' for reading", fullpath);
      return NULL;
    }
    /* there is an album file - get details */
    if (mserv_verbose)
      mserv_log("Album info file: %s", filename);
    while(fgets(buffer, 1024, fd)) {
      line++;
      alen = strlen(buffer);
      if (buffer[alen-1] != '\n') {
	mserv_log("Line %d too long in '%s'", line, fullpath);
	continue;
      }
      buffer[--alen] = '\0';
      if (!(l = strcspn(buffer, "=")) || l >= 64) {
	mserv_log("Invalid album line %d in '%s'", line, fullpath);
	continue;
      }
      strncpy(token, buffer, l);
      token[l] = '\0';
      value = buffer+l+1;
      if (token[0] == '_') {
	if (!strcmp(token, "_author")) {
	  if (strlen(value) > AUTHORLEN) {
	    mserv_log("Author too long for album '%s', truncating",
		      fullpath);
	  }
	  strncpy(author, value, AUTHORLEN+1);
	  author[AUTHORLEN] = '\0';
	} else if (!strcmp(token, "_name")) {
	  if (strlen(value) > NAMELEN) {
	    mserv_log("Name too long for album '%s', truncating", fullpath);
	  }
	  strncpy(name, value, NAMELEN);
	  name[NAMELEN] = '\0';
	}
      } else {
	mserv_log("Invalid line %d in album %s", line, fullpath);
	continue;
      }
    }
    fclose(fd);
    /* got details */
    mtime = buf.st_mtime;
  } else {
    /* there is no album file */
    if (onlyifexists)
      return NULL;
    mtime = time(NULL);
  }
  if (!*author)
    strcpy(author, "!-Unindexed");
  if (!*name) {
    if (!*filename || !*(filename+1)) {
      strcpy(name, "rootdir");
    } else {
      p = q = filename+strlen(filename)-1; /* point at last char, a slash */
      while (p > filename) {
	if (*(p-1) == '\\')
	  break;
	p--;
      }
      if (q-p > NAMELEN) {
	/* not important enough to tell user */
	strncpy(name, p, NAMELEN);
	name[NAMELEN] = '\0';
      } else {
	strncpy(name, p, q-p);
	name[q-p] = '\0';
      }
    }
  }
  if ((album = malloc(sizeof(t_album)+strlen(author)+strlen(name)+
		      strlen(filename)+3)) == NULL) {
    mserv_log("Out of memory creating album structure for '%s'", filename);
    return NULL;
  }
  album->author = (char *)(album+1);
  album->name = album->author+strlen(author)+1;
  album->filename = album->name+strlen(name)+1;
  strcpy(album->author, author);
  strcpy(album->name, name);
  strcpy(album->filename, filename);
  album->id = mserv_nextid_album++;
  album->modified = 0;
  album->mtime = mtime;
  return album;
}

static void mserv_strtoprintable(char *string)
{
  char *p = string;
  char *r = string;

  while (*p) {
    if (isprint(*p))
      *r++ = *p;
    p++;
  }
  *r = '\0';
}

static t_track *mserv_loadtrk(const char *filename)
{
  FILE *fd;
  int line = 0;
  char buffer[1024];
  int l, alen;
  char token[64];
  char *value;
  t_track *track;
  t_rating *ratings = NULL;
  t_rating *ratingsl = NULL;
  t_rating *arate;
  char author[AUTHORLEN+1] = "";
  char name[NAMELEN+1] = "";
  char genres[GENRESLEN+1] = "";
  char miscinfo[MISCINFOLEN+1] = "";
  time_t lastplay = 0;
  char *p;
  int year = 0;
  int modified = 0;
  int duration = 0;
  char fullpath_trk[MAXFNAME];
  char fullpath_file[MAXFNAME];
  int len;
  time_t mtime;
  struct stat buf;
  int bitrate;
  t_id3tag id3tag;
  int newinfofile = 0;

  if ((unsigned int)snprintf(fullpath_file, MAXFNAME, "%s/%s",
			     opt_path_tracks, filename) >= MAXFNAME) {
    mserv_log("MAXFNAME too small for loading trk '%s'", filename);
    return NULL;
  }
  if ((unsigned int)snprintf(fullpath_trk, MAXFNAME, "%s/%s",
			     opt_path_trackinfo, filename) >= MAXFNAME) {
    mserv_log("MAXFNAME too small for loading trk '%s'", filename);
    return NULL;
  }
  (*strrchr(fullpath_trk, '/')) = '\0';
  if (mserv_createdir(fullpath_trk)) {
    mserv_log("Unable to create dir '%s'", fullpath_trk);
    return NULL;
  }
  if ((unsigned int)snprintf(fullpath_trk, MAXFNAME, "%s/%s.trk",
			     opt_path_trackinfo, filename) >= MAXFNAME) {
    mserv_log("MAXFNAME too small for loading trk '%s'", filename);
    return NULL;
  }
  if ((fd = fopen(fullpath_trk, "r")) == NULL) {
    modified = 1; /* cause .trk file to be saved for this file */
    strcpy(author, "Unknown");
    if ((p = strrchr(filename, '/')))
      strncpy(name, p+1, NAMELEN+1);
    else
      strncpy(name, filename, NAMELEN+1);
    name[NAMELEN] = '\0';
    mtime = time(NULL);
    newinfofile = 1;
    /* no track information */
  } else {
    newinfofile = 0;
    while(fgets(buffer, 1024, fd)) {
      line++;
      alen = strlen(buffer);
      if (buffer[alen-1] != '\n') {
	mserv_log("Line %d too long in '%s'", line, fullpath_trk);
	return NULL;
      }
      buffer[--alen] = '\0';
      if (!(l = strcspn(buffer, "=")) || l >= 64) {
	mserv_log("Invalid track line %d in '%s'", line, fullpath_trk);
	return NULL;
      }
      strncpy(token, buffer, l);
      token[l] = '\0';
      value = buffer+l+1;
      if (token[0] == '_') {
	if (!stricmp(token, "_author")) {
	  strncpy(author, value, AUTHORLEN+1);
	  author[AUTHORLEN] = '\0';
	} else if (!stricmp(token, "_name")) {
	  strncpy(name, value, NAMELEN+1);
	  name[NAMELEN] = '\0';
	} else if (!stricmp(token, "_genres")) {
	  strncpy(genres, value, GENRESLEN+1);
	  genres[GENRESLEN] = '\0';
	} else if (!stricmp(token, "_norandom")) {
	  /* ignore old line */
	  modified = 1;
	} else if (!stricmp(token, "_lastplay")) {
	  lastplay = atol(value);
	} else if (!stricmp(token, "_duration")) {
	  duration = atol(value);
	} else if (!stricmp(token, "_miscinfo")) {
	  strncpy(miscinfo, value, MISCINFOLEN+1);
	  miscinfo[MISCINFOLEN] = '\0';
	} else if (!stricmp(token, "_year")) {
	  year = atoi(value);
	}
      } else {
	if (strlen(value) > USERNAMELEN) {
	  mserv_log("Username too long in ratings for '%s'", fullpath_trk);
	  continue;
	}
	if ((arate = malloc(sizeof(t_rating)+strlen(token)+1)) == NULL) {
	  mserv_log("Out of memory creating ratings for '%s'", fullpath_trk);
	  return NULL;
	}
	memset(arate, 0, sizeof(t_rating));
	arate->rating = atoi(value);
	if ((p = strchr(value, ':'))) {
	  arate->when = atol(p+1);
	}
	if (!p || arate->when == 0) {
	  arate->when = time(NULL); /* backwards compatibility */
	  modified = 1; /* cause track information to be saved */
	}
	arate->user = (char *)(arate+1);
	strcpy(arate->user, token);
	arate->next = NULL;
	if (!ratings)
	  ratings = arate;
	if (ratingsl)
	  ratingsl->next = arate;
	ratingsl = arate;
      }
    }
    if (!*author) {
      mserv_log("No author specified in '%s'", fullpath_trk);
      return NULL;
    }
    if (!*name) {
      mserv_log("No name specified in '%s'", fullpath_trk);
      return NULL;
    }
    if (fstat(fileno(fd), &buf) == -1) {
      perror("fstat");
      mserv_log("Unable to stat '%s': %s", filename, strerror(errno));
      return NULL;
    }
    mtime = buf.st_mtime;
    fclose(fd);
  }
  if (duration == 0 && !*miscinfo) {
    len = strlen(fullpath_file);
    if (len > 4 && !stricmp(".mp3", fullpath_file+len-4)) {
      duration = mserv_mp3info_readlen(fullpath_file, &bitrate, &id3tag);
      if (duration == -1) {
	mserv_log("Unable to determine details of mp3 '%s': %s",
		  filename, strerror(errno));
	duration = 0;
	miscinfo[0] = '\0';
      } else {
	if (id3tag.present) {
	  if (newinfofile) {
	    strncpy(author, id3tag.artist, AUTHORLEN);
	    author[AUTHORLEN] = '\0';
	    strncpy(name, id3tag.title, NAMELEN);
	    name[NAMELEN] = '\0';
	    year = atoi(id3tag.year);
	    strncpy(genres, id3tag.genre, GENRESLEN);
	    genres[GENRESLEN] = '\0';
	    mserv_strtoprintable(author);
	    mserv_strtoprintable(name);
	    mserv_strtoprintable(genres);
	  }
	  if (*id3tag.comment) {
	    snprintf(miscinfo, MISCINFOLEN, "%dkbps; %s", bitrate,
		     id3tag.comment);
	    mserv_strtoprintable(miscinfo);
	  } else {
	    sprintf(miscinfo, "%dkbps", bitrate);
	  }

	} else {
	  sprintf(miscinfo, "%dkbps", bitrate);
	}
	modified = 1;
      }
    }
  }
  len = sizeof(t_track)+strlen(author)+strlen(name)+strlen(filename)+
    strlen(genres)+strlen(miscinfo)+5;
  if ((track = malloc(len)) == NULL) {
    mserv_log("Out of memory creating track structure for '%s'", fullpath_trk);
    return NULL;
  }
  memset(track, 0, len);
  track->author = (char *)(track+1);
  track->name = track->author+strlen(author)+1;
  track->filename = track->name+strlen(name)+1;
  track->genres = track->filename+strlen(filename)+1;
  track->miscinfo = track->genres+strlen(genres)+1;
  strcpy(track->author, author);
  strcpy(track->name, name);
  strcpy(track->filename, filename);
  strcpy(track->genres, genres);
  strcpy(track->miscinfo, miscinfo);
  track->ratings = ratings;
  track->modified = modified;
  track->lastplay = lastplay;
  track->year = year;
  track->duration = duration;
  track->random = rand();
  track->mtime = mtime;
  return track;
}

int mserv_addqueue(t_client *cl, t_track *track)
{
  t_queue *q, *p;
  t_queue *newq;
  int total;

  if (mserv_shutdown)
    return -1;

  if ((newq = malloc(sizeof(t_queue))) == NULL) {
    mserv_log("Out of memory adding to queue");
    return -1;
  }
  newq->supinfo.track = track;
  strcpy(newq->supinfo.user, cl->user);
  newq->next = NULL;

  total = 0;
  for (q = mserv_queue; q; q = q->next) {
    if (q->supinfo.track == track) {
      /* can't enter the same track into the queue twice */
      free(newq);
      return -1;
    }
    if (!stricmp(q->supinfo.user, cl->user))
      total++;
  }
  if (total >= 100) { /* don't allow a user to queue more than 100 tracks */
    free(newq);
    return -1;
  }
  /* add a track at the first instance of two tracks queued by the same
     person */
  for (p = NULL, q = mserv_queue; q; p = q, q = q->next) {
    if (p && !stricmp(q->supinfo.user, p->supinfo.user) &&
	stricmp(cl->user, q->supinfo.user))
      break;
  }
  if (p) {
    if (q)
      newq->next = q;
    p->next = newq;
  } else {
    mserv_queue = newq;
  }
  return 0;
}

/* w = (weight - 0.5)*RFACT, x^(w+1), x^(1/(1-w)) */

int mserv_playnext(void)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  char fullpath[MAXFNAME];
  char bit[32];
  int pid;
  int fd;
  t_queue *q;
  t_track *track;
  int total, tnum, n;
  double lin,lnn;
  t_client *cl;
  t_rating *rate;
  int i;
  t_lang *lang;
  t_supinfo *history;
  char *str[21];
  char *strbuf;

  if (mserv_playing.track) {
    mserv_abortplay();
  }
  if (mserv_queue == NULL) { /* no more tracks to play! */
    if (!mserv_random)
      return 1;
    total = 0;
    for (track = mserv_tracks; track; track = track->next) {
      if (track->filterok)
	total++;
    }
    lin = rand()/(RAND_MAX+1.0);
    if (mserv_factor < 0.5)
      lnn = 1-pow(1-lin, ((0.5-mserv_factor)*RFACT)+1);
    else
      lnn = 1-pow(1-lin, 1.0/(1.0-((0.5-mserv_factor)*RFACT)));
    tnum = 1+(int)(total*lnn);
    DEBUG(mserv_log("%f -> %f", lin, lnn));
    n = 1;
    for (track = mserv_tracks; track; track = track->next) {
      if (!track->filterok)
	continue;
      if (n == tnum)
	break;
      n++;
    }
    if (track) {
      mserv_playing.track = track;
      mserv_log("Randomly picking track %d of %d (%d/%d)", tnum, total,
		track->n_album, track->n_track);
      strcpy(mserv_playing.user, "random");
    } else {
      mserv_log("Could not randomise song");
    }
  } else {
    q = mserv_queue;
    memcpy(&mserv_playing, &q->supinfo, sizeof(t_supinfo));
    mserv_queue = mserv_queue->next;
    free(q);
  }
  mserv_paused = 0;
  /* recalc the ratings so that the current playing song plummits */
  mserv_recalcratings();
  mserv_savechanges();
  if (mserv_playing.track == NULL) {
    mserv_log("No track to play");
    return 1;
  }
  if ((lang = mserv_gettoken("NOWPLAY")) == NULL) {
    mserv_log("Failed to find language token NOWPLAY");
    exit(1);
  }
  for (cl = mserv_clients; cl; cl = cl->next) {
    if (!cl->authed)
      continue;
    rate = mserv_getrate(cl->user, mserv_playing.track);
    if (cl->mode == mode_human) {
      mserv_send(cl, "[] ", 0);
      mserv_send(cl, lang->text, 0);
      mserv_send(cl, "\r\n", 0);
      sprintf(bit, "%d/%d", mserv_playing.track->n_album,
	      mserv_playing.track->n_track);
      sprintf(buffer, "[] \x1B[1m%-10.10s %6.6s %-1.1s %-20.20s "
	      "%-30.30s%2ld:%02ld\x1B[0m\r\n",
	      mserv_playing.user, bit,
	      rate && rate->rating ? mserv_ratestr(rate) : "-",
	      mserv_playing.track->author, mserv_playing.track->name,
	      (mserv_playing.track->duration / 100) / 60,
	      (mserv_playing.track->duration / 100) % 60);
      mserv_send(cl, buffer, 0);
      if (!mserv_playing.track->genres[0])
	mserv_response(cl, "GENREME", NULL); /* set my genre! */
      if (!mserv_playing.track->ratings) {
	mserv_response(cl, "FPLAY", NULL);
      } else if (!rate) {
	mserv_response(cl, "RATEME2", NULL); /* unheard */
      } else if (!rate->rating) {
	mserv_response(cl, "RATEME1", NULL); /* unrated */
      }
    } else if (cl->mode == mode_rtcomputer) {
      sprintf(buffer, "=%d\t%s\t%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n", lang->code,
	      mserv_playing.user, mserv_playing.track->n_album,
	      mserv_playing.track->n_track, mserv_playing.track->author,
	      mserv_playing.track->name, mserv_ratestr(rate),
	      (mserv_playing.track->duration / 100) / 60,
	      (mserv_playing.track->duration / 100) % 60);
      mserv_send(cl, buffer, 0);
    }
  }
  snprintf(fullpath, MAXFNAME, "%s/%s", opt_path_tracks,
	   mserv_playing.track->filename);
  if ((strbuf = malloc(strlen(opt_player)+1)) == NULL) {
    mserv_broadcast("MEMORY", NULL);
    return 1;
  }
  strcpy(strbuf, opt_player);
  if ((i = mserv_split(str, 21, strbuf, " ")) >= 21) {
    mserv_log("Internal error - too many parameters for player");
    exit(1);
  }
  str[i] = fullpath; /* max str[20] */
  str[i+1] = NULL;   /* max str[21] */
  if (!(pid = fork())) {
    if ((fd = open(opt_path_playout,
		   O_RDWR|O_CREAT|O_TRUNC|O_APPEND, 0700)) == -1) {
      mserv_log("Open of '%s' failed: %s\n", opt_path_playout,
		strerror(errno));
      exit(0);
    }
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
      close(fd);
    /* close file descriptors, ikky but suggested in unix network programming
       volume 1 second edition, although I seem to remember a better way.. */
    for (i = 3; i < MAXFD; i++)
      close(i);
    setsid();
    for (i = 0; str[i] && i < 20; i++) {
      printf("%d: %s\n", i, str[i]);
    }
    execv(str[0], str);
    fprintf(stderr, "%s: Unable to execv '%s': '%s'", progname,
	    opt_player, strerror(errno));
    exit(0);
  } else if (pid == -1) {
    perror("fork");
    mserv_log("Unable to fork: '%s'", strerror(errno));
  }
  free(strbuf);
  mserv_playingpid = pid;
  mserv_playingstart = time(NULL);
  if ((history = malloc(sizeof(t_supinfo))) == NULL) {
    mserv_log("Out of memory adding item to history");
    mserv_broadcast("MEMORY", NULL);
  } else {
    if (mserv_history[HISTORYLEN-1])
      free(mserv_history[HISTORYLEN-1]);
    memmove((char *)mserv_history+sizeof(t_supinfo *), mserv_history,
	    (HISTORYLEN-1)*sizeof(t_supinfo *));
    mserv_history[0] = history;
    memcpy(mserv_history[0], &mserv_playing, sizeof(t_supinfo));
  }
  return 0;
}

void mserv_abortplay(void)
{
  int pid, st;
  int vol = -1;
  int x;

  if (!mserv_playing.track)
    return;

  if (mserv_paused) {
#ifdef SOUNDCARD
    vol = mserv_readvolume();
    mserv_setvolume(0);
#else
  (void)vol;
#endif                                                                    
    mserv_resumeplay();
  }
  if (kill(mserv_playingpid, 0) == 0) {
    /* our process exists */
    if (kill(-mserv_playingpid, SIGTERM) == -1) {
      perror("kill");
      mserv_log("Unable to kill currently playing song: %s", strerror(errno));
    } else {
      for (x = 0; kill(mserv_playingpid, 0) == 0 && x < 10; x++) {
	usleep(100*1000);
	if ((pid = waitpid(mserv_playingpid, &st,
			   WNOHANG)) == -1) {
	  mserv_log("waitpid failure (%d): %s", errno, strerror(errno));
	}
      }
    }
    if (kill(mserv_playingpid, 0) == 0) {
      /* our process STILL exists */
      if (kill(-mserv_playingpid, SIGKILL) == -1) {
	perror("kill");
	mserv_log("Unable to kill currently playing song: %s",
		  strerror(errno));
      } else {
	for (x = 0; kill(mserv_playingpid, 0) == 0 && x < 10; x++) {
	  usleep(100*1000);
	  if ((pid = waitpid(mserv_playingpid, &st,
			     WNOHANG)) == -1) {
	    mserv_log("waitpid failure (%d): %s", errno, strerror(errno));
	  }
	}
      }
    }
  }
#ifdef SOUNDCARD
  if (vol != -1)
    mserv_setvolume(vol);
#endif                                                                    
  mserv_checkdisk_track(mserv_playing.track);
  mserv_playing.track->lastplay = time(NULL);
  mserv_playing.track->modified = 1;
  mserv_playing.track = NULL;
  mserv_playingpid = 0;
  mserv_playingstart = 0;
  mserv_recalcratings(); /* recalc ratings now lastplay has changed */
  mserv_savechanges();
  mserv_checkshutdown();
}

static void mserv_checkshutdown(void)
{
  if (mserv_shutdown) {
    mserv_flush();
    sleep(1);
    mserv_flush();
    sleep(1);
    mserv_flush();
    mserv_closedown(0);
  }
}

void mserv_pauseplay(t_client *cl)
{
  int i;

  if (!mserv_playing.track) {
    if (mserv_playnextat) {
      /* we're between songs - start up next one, a bit of a fudge this */
      mserv_playnextat = 0;
      mserv_playnext(); /* ignore return */
    }
  }  
  if (cl) {
    strncpy(mserv_pausedby, cl->user, USERNAMELEN);
    mserv_pausedby[USERNAMELEN] = '\0';
  }
  if (kill(-mserv_playingpid, SIGSTOP) == -1) {
    perror("kill");
    mserv_log("Unable to pause currently playing song: %s", strerror(errno));
    return;
  }
  for (i = 0; !mserv_paused && i < 5*5; i++) {
    mserv_checkchild();
    usleep(200*1000);
  }
  if (!mserv_paused)
    mserv_broadcast("SUSERR", NULL);
}

void mserv_resumeplay(void)
{
  if (!mserv_playing.track)
    return;

  if (kill(-mserv_playingpid, SIGCONT) == -1) {
    perror("kill");
    mserv_log("Unable to resume currently playing song: %s", strerror(errno));
  }
  mserv_paused = 0;
}

void mserv_recalcratings(void)
{
  t_track *track;
  t_rating *rate;
  double rating;
  t_client *cl;
  time_t off;
  int totalusers = 0;
  int ntracks;
  t_track **sbuf;
  int i;

  DEBUG(mserv_log("recalc ratings..."));

  mserv_filter_ok = 0;
  mserv_filter_notok = 0;
  for (cl = mserv_clients; cl; cl = cl->next) {
    if (cl->authed && cl->state != st_closed && cl->mode != mode_computer
	&& cl->userlevel != level_guest)
      totalusers++;
  }
  ntracks = 0;
  for (track = mserv_tracks; track; track = track->next) {
    ntracks++;
    track->filterok = (filter_check(mserv_filter, track) == 1) ? 1 : 0;
    if (track->filterok)
      mserv_filter_ok++;
    else
      mserv_filter_notok++;
    if (!totalusers) {
      track->prating = 0;
    } else if (track->ratings) {
      rating = 0;
      for (cl = mserv_clients; cl; cl = cl->next) {
	if (!cl->authed || cl->state == st_closed ||
	    cl->mode == mode_computer || cl->userlevel == level_guest)
	  continue;
	if ((rate = mserv_getrate(cl->user, track)) != NULL) {
	  if (rate->rating == 0)
	    rating+= RATE_HEARD; /* heard but not rated - NEUTRAL */
	  else
	    rating+= rate->rating;
	} else { /* user has not rated song */
	  rating+= RATE_NOTHEARD; /* GOOD */
	}
      }
      /* convert accumulative ratings (1-5) to (0-4) to %age */
      track->prating = (rating-totalusers)/(totalusers*4);
    } else {
      track->prating = ((double)RATE_NOTHEARD-1)/4;
    }
    track->rating = track->prating;
    off = time(NULL) - track->lastplay;
    if (off < 60*10) /* within ten minutes */
      track->rating = track->rating*0.1;
    else if (off < 60*60) /* within an hour */
      track->rating = track->rating*0.2;
    else if (off < 60*60*24) /* within a day */
      track->rating = track->rating*0.4;
    else if (off < 60*60*24*3) /* within three days */
      track->rating = track->rating*0.6;
    else if (off < 60*60*24*7) /* within a week */
      track->rating = track->rating*0.7;
    else if (off < 60*60*24*7*2) /* within two week */
      track->rating = track->rating*0.8;
    else if (track->lastplay != 0) /* played */
      track->rating = track->rating*0.95;
    if (track == mserv_playing.track)
      track->rating = 0; /* currently playing */
    if (!track->filterok)
      track->rating = 0; /* filtered out */
  }
  if (ntracks != mserv_nextid_track-1) {
    mserv_log("Track list has become correct (ntracks!=nextid-1)");
    exit(1);
  }
  DEBUG(mserv_log("sort..."));
  if (ntracks) {
    if ((sbuf = malloc(sizeof(t_track *)*ntracks)) == NULL) {
      mserv_log("Out of memory creating sort buffer");
      mserv_broadcast("MEMORY", NULL);
    } else {
      for (i = 0, track = mserv_tracks; track; track = track->next, i++) {
	if (i >= ntracks) {
	  mserv_log("Internal error in sort buffer");
	  exit(1);
	}
	sbuf[i] = track;
      }
      if (i != ntracks) {
	mserv_log("Internal error in sort buffer");
	exit(1);
      }
      qsort(sbuf, ntracks, sizeof(t_track *), mserv_trackcompare_rating);
      for (i = 0; i < ntracks-1; i++) {
	sbuf[i]->next = sbuf[i+1];
      }
      sbuf[ntracks-1]->next = NULL;
      mserv_tracks = sbuf[0];
      free(sbuf);
    }
  }
  DEBUG(mserv_log("end"));
  return;
}

/* splits a string into NULL terminated list of nelements, the last of which
   will always contain the remainding string, e.g. if the string was
   "hello there this is cool" and nelements is 3, you will get "hello",
   "there", "this is cool" and NULL. */

int mserv_split(char *str[], int nelements, char *line, const char *sep)
{
  int i, j;
  char *p = line;
  int space = !stricmp(" ", sep);

  str[0] = strsep(&p, sep);
  for (i = 1; i < (nelements-1) && (str[i] = strsep(&p, sep)); ) {
    if (!space || *str[i] != '\0')
      i++;
  }
  if (str[i-1] == NULL) {
    i--;
  } else {
    if (p && *p != '\0') {
      while (space && *p == ' ')
	p++;
      if (*p != '\0')
	str[i++] = p;
    }
    str[i] = NULL;
  }
  /* blank until end of list */
  for (j = i+1; j < nelements; j++)
    str[j] = NULL;
  return i;
}

t_track *mserv_checkdisk_track(t_track *track)
{
  t_track *newtrack;
  char filename[MAXFNAME];
  struct stat buf;
  t_rating *rating, *r_n;

  if (track->modified)
    /* we rule, not the disk!... besides, this probably means we've changed
       stuff in the past but have been unable to save the information to
       disk */
    return track;
  if ((unsigned int)snprintf(filename, MAXFNAME,
			     "%s/%s.trk", opt_path_trackinfo,
			     track->filename) >= MAXFNAME) {
    mserv_log("MAXFNAME too small for checking '%s'", track->filename);
    return track;
  }
  if (stat(filename, &buf) == -1) {
    perror("fstat");
    mserv_log("Unable to stat '%s': %s", filename, strerror(errno));
    return track;
  }
  if (buf.st_mtime == track->mtime)
    return track;
  mserv_log("'%s' updated from disk", track->filename);
  newtrack = mserv_loadtrk(track->filename);
  mserv_replacetrack(track, newtrack);
  for (rating = track->ratings; rating; rating = r_n) {
    r_n = rating->next;
    free(rating);
  }
  strcpy(track->name, "*");
  free(track);
  return newtrack;
}

t_album *mserv_checkdisk_album(t_album *album)
{
  t_album *newalbum;
  char filename[MAXFNAME];
  struct stat buf;

  if (album->modified)
    /* we rule, not the disk!... besides, this probably means we've changed
       stuff in the past but have been unable to save the information to
       disk */
    return album;
  if ((unsigned int)snprintf(filename, MAXFNAME,
			     "%s/%salbum", opt_path_trackinfo,
			     album->filename) >= MAXFNAME) {
    mserv_log("MAXFNAME too small for checking '%salbum'", album->filename);
    return album;
  }
  if (stat(filename, &buf) == -1) {
    perror("fstat");
    mserv_log("Unable to stat '%s': %s", filename, strerror(errno));
    return album;
  }
  if (buf.st_mtime == album->mtime)
    return album;
  if ((newalbum = mserv_loadalbum(album->filename, 1)) == NULL) {
    mserv_log("Unable to re-load '%s'", album->filename);
    return album;
  }
  mserv_replacealbum(album, newalbum);
  strcpy(album->name, "*");
  free(album);
  return newalbum;
}

int mserv_savechanges(void)
{
  t_track *track;
  t_album *album;
  t_rating *rate;
  FILE *fd;
  char filename[MAXFNAME];
  int error = 0;
  struct stat buf;

  /* returns 0 if no errors, 1 if there was an error somewhere */

  for (track = mserv_tracks; track; track = track->next) {
    if (!track->modified)
      continue;
    if ((unsigned int)snprintf(filename, MAXFNAME, "%s/%s.trk",
			       opt_path_trackinfo,
			       track->filename) >= MAXFNAME) {
      mserv_log("MAXFNAME too small for saving '%s'", track->filename);
      error = 1;
      continue;
    }
    if ((fd = fopen(filename, "w")) == NULL) {
      perror("fopen");
      mserv_log("Unable to fopen '%s' for writing: %s", filename,
		strerror(errno));
      error = 1;
      continue;
    }
    fprintf(fd, "_author=%s\n", track->author);
    fprintf(fd, "_name=%s\n", track->name);
    fprintf(fd, "_year=%d\n", track->year);
    fprintf(fd, "_genres=%s\n", track->genres);
    fprintf(fd, "_lastplay=%lu\n", (unsigned long int)track->lastplay);
    fprintf(fd, "_duration=%lu\n", track->duration);
    fprintf(fd, "_miscinfo=%s\n", track->miscinfo);
    for (rate = track->ratings; rate; rate = rate->next) {
      fprintf(fd, "%s=%d:%ld\n", rate->user, rate->rating,
	      (unsigned long int)rate->when);
    }
    if (fclose(fd) == EOF) {
      perror("fclose");
      mserv_log("Unable to close '%s': %s", filename, strerror(errno));
      error = 1;
      continue;
    }
    mserv_log("Saved changes to '%s'", filename);
    track->modified = 0;
    /* stat the closed file, must be stat and not fstat */
    if (stat(filename, &buf) == -1) {
      perror("stat");
      mserv_log("Unable to stat '%s': %s", filename, strerror(errno));
      error = 1;
      continue;
    }
    track->mtime = buf.st_mtime;
  }
  for (album = mserv_albums; album; album = album->next) {
    if (!album->modified)
      continue;
    if ((unsigned int)snprintf(filename, MAXFNAME, "%s/%salbum",
			       opt_path_trackinfo,
			       album->filename) == MAXFNAME) {
      mserv_log("MAXFNAME too small for saving '%s'", album->filename);
      error = 1;
      continue;
    }
    if ((fd = fopen(filename, "w")) == NULL) {
      perror("fopen");
      mserv_log("Unable to fopen '%s' for writing: %s", filename,
		strerror(errno));
      error = 1;
      continue;
    }
    fprintf(fd, "_author=%s\n", album->author);
    fprintf(fd, "_name=%s\n", album->name);
    if (fclose(fd) == EOF) {
      perror("fclose");
      mserv_log("Unable to close '%s': %s", filename, strerror(errno));
      error = 1;
      continue;
    }
    mserv_log("Saved changes to '%s'", filename);
    album->modified = 0;
    /* stat the closed file, must be stat and not fstat */
    if (stat(filename, &buf) == -1) {
      perror("stat");
      mserv_log("Unable to stat '%s': %s", filename, strerror(errno));
      error = 1;
      continue;
    }
    album->mtime = buf.st_mtime;
  }
  return error;
}

t_rating *mserv_getrate(const char *user, t_track *track)
{
  t_rating *rate;
  for (rate = track->ratings; rate; rate = rate->next) {
    if (!stricmp(rate->user, user)) {
      return rate;
    }
  }
  return NULL;
}

t_track *mserv_gettrack(unsigned int n_album, unsigned int n_track)
{
  t_album *album;
  t_track *track;

  if (n_track > TRACKSPERALBUM)
    return NULL;
  track = NULL;
  for (album = mserv_albums; album; album = album->next) {
    if (album->id != n_album)
      continue;
    if ((track = album->tracks[n_track-1]) == NULL)
      return NULL;
    return track;
  }
  return NULL;
}

t_album *mserv_getalbum(unsigned int n_album)
{
  t_album *album;

  for (album = mserv_albums; album; album = album->next) {
    if (album->id == n_album)
      return album;
  }
  return NULL;
}

t_userlevel *mserv_strtolevel(const char *level)
{
  static t_userlevel ul;

  if (!stricmp(level, "GUEST"))
    ul = level_guest;
  else if (!stricmp(level, "USER"))
    ul = level_user;
  else if (!stricmp(level, "PRIV"))
    ul = level_priv;
  else if (!stricmp(level, "MASTER"))
    ul = level_master;
  else
    return NULL;
  return &ul;
}

const char *mserv_levelstr(t_userlevel userlevel)
{
  switch(userlevel) {
  case level_guest:
    return "GUEST";
  case level_user:
    return "USER";
  case level_priv:
    return "PRIV";
  case level_master:
    return "MASTER";
  default:
    return NULL;
  }
}

const char *mserv_ratestr(t_rating *rate)
{
  int rating;

  if (!rate)
    return "UNHEARD";

  rating = rate->rating;

  if (rating == 0)
    return "HEARD";
  if (rating == 1)
    return "AWFUL";
  if (rating == 2)
    return "BAD";
  if (rating == 3)
    return "NEUTRAL";
  if (rating == 4)
    return "GOOD";
  if (rating == 5)
    return "SUPERB";
  return NULL;
}

int mserv_strtorate(const char *str)
{
  if (!stricmp(str, "AWFUL"))
    return 1;
  else if (!stricmp(str, "BAD"))
    return 2;
  else if (!stricmp(str, "NEUTRAL"))
    return 3;
  else if (!stricmp(str, "GOOD"))
    return 4;
  else if (!stricmp(str, "SUPERB"))
    return 5;
  return -1;
}

static t_author *mserv_authorlist(void)
{
  t_author *authorlist = NULL;
  t_author *author;
  t_track *track, *strack;
  int i;

  for (track = mserv_tracks; track; track = track->next) {
    for (author = authorlist; author; author = author->next) {
      if (!stricmp(track->author, author->name)) {
	break;
      }
    }
    if (author)
      continue;
    /* we have an author that hasn't been indexed */
    if ((author = malloc(sizeof(t_author)+strlen(track->author)+1)) == NULL) {
      mserv_log("Out of memory creating author structure");
      mserv_broadcast("MEMORY", NULL);
      return authorlist;
    }
    author->name = (char *)(author+1);
    strcpy(author->name, track->author);
    i = 0;
    for (strack = track; i < TRACKSPERALBUM && strack; strack = strack->next) {
      if (!stricmp(strack->author, track->author)) {
	author->tracks[i++] = strack;
      }
    }
    while (i < TRACKSPERALBUM) {
      author->tracks[i++] = NULL;
    }
    if (author_insertsort(&authorlist, author)) {
      mserv_log("Internal authorlist error");
    }
    qsort(author->tracks, TRACKSPERALBUM, sizeof(t_track *),
	  mserv_trackcompare_name);
  }
  i = 1;
  for (author = authorlist; author; author = author->next) {
    author->id = i++;
  }
  return authorlist;
}

static int author_insertsort(t_author **list, t_author *author)
{
  t_author *au, *p;
  int a;

  for (p = NULL, au = *list; au; au = au->next) {
    if ((a = stricmp(au->name, author->name)) > 0)
      break;
    if (a == 0)
      return 1;
    p = au;
  }
  author->next = au;
  if (!p)
    *list = author;
  else
    p->next = author;
  return 0;
}

const char *mserv_stndrdth(int day)
{
  if (day == 1 || day == 21 || day == 31) {
    return "st";
  } else if (day == 2 || day == 22) {
    return "nd";
  } else if (day == 3 || day == 23) {
    return "rd";
  } else {
    return "th";
  }
}

int mserv_flush(void)
{
  t_client *cl;
  int notempty = 0;

  for (cl = mserv_clients; cl; cl = cl->next) {
    if (cl->state != st_closed) {
      mserv_pollwrite(cl);
      if (cl->outbuf[0].iov_len)
	notempty = 1;
    }    
  }
  return notempty;
}

int mserv_idea(const char *text)
{
  FILE *fd;

  if ((fd = fopen(opt_path_idea, "a")) == NULL) {
    mserv_log("Unable to open idea file for writing: %s", strerror(errno));
    return -1;
  }
  fwrite(text, 1, strlen(text), fd);
  fwrite("\n", 1, 1, fd);
  if (fclose(fd)) {
    mserv_log("Unable to close idea file: %s", strerror(errno));
    return -1;
  }
  return 0;
}

void mserv_reset(void)
{
  t_queue *q, *q_n;
  t_album *a, *a_n;
  t_track *t, *t_n;
  t_author *auth, *auth_n;
  t_genre *genre, *genre_n;
  int i;

  /* clear queue */
  for (q = mserv_queue; q; q = q_n) {
    q_n = q->next;
    free(q);
  }
  mserv_queue = NULL;

  /* clear albums */
  for (a = mserv_albums; a; a = a_n) {
    a_n = a->next;
    free(a);
  }
  mserv_albums = NULL;

  /* clear tracks */
  for (t = mserv_tracks; t; t = t_n) {
    t_n = t->next;
    free(t);
  }
  mserv_tracks = NULL;

  /* clear authors */
  for (auth = mserv_authors; auth; auth = auth_n) {
    auth_n = auth->next;
    free(auth);
  }
  mserv_authors = NULL;

  /* clear genres */
  for (genre = mserv_genres; genre; genre = genre_n) {
    genre_n = genre->next;
    free(genre);
  }
  mserv_genres = NULL;

  /* clear history */
  for (i = 0; i < HISTORYLEN; i++) {
    free(mserv_history[i]);
    mserv_history[i] = NULL;
  }

  /* other stuff */
  mserv_nextid_album = 1;
  mserv_nextid_track = 1;
  mserv_playing.track = NULL;
  mserv_playingpid = 0;
  mserv_paused = 0;
  mserv_shutdown = 0;

  /* ok, lets get everything back */
  mserv_init();

}

int mserv_setgap(int gap)
{
  if (gap != -1)
    mserv_gap = gap;
  return mserv_gap;
}

int mserv_setfilter(const char *filter)
{
  if (!mserv_tracks)
    return -1;
  if (strlen(filter) > FILTERLEN)
    return -1;
  /* run the filter on the first track in the mserv_tracks list, filter_check
     returns true/false or -1 for parse error, so we can check validity */
  if (*filter && filter_check(filter, mserv_tracks) == -1)
    return -1;
  strcpy(mserv_filter, filter);
  return 0;
}

char *mserv_getfilter(void)
{
  return mserv_filter;
}

int mserv_checkgenre(const char *genres)
{
  const char *p;
  int count = 1;

  for (p = genres; *p; p++) {
    if (*p == ',') {
      count++;      
      continue;
    }
    if (!isalnum((int)*p))
      return -1;     
  }             
  if (count > MAXNGENRE)
    return -1;          
  return 0;
}

int mserv_checkauthor(const char *author)
{
  const char *p;

  for (p = author; *p; p++) {
    /* newline and tab are really the only chars not allowed */
    if (!(isprint((int)*p) && *p != '\n' && *p != '\t'))
      return -1;
  }
  return 0;
}

int mserv_checkname(const char *name)
{
  const char *p;

  for (p = name; *p; p++) {
    /* newline and tab are really the only chars not allowed */
    if (!(isprint((int)*p) && *p != '\n' && *p != '\t'))
      return -1;
  }
  return 0;
}

static t_genre *mserv_genrelist(void)
{
  char *genres_str[MAXNGENRE+2];
  char genres_spl[GENRESLEN+1];
  int genres_n;
  t_genre *genrelist = NULL;
  t_genre *genre;
  t_track *track;
  int gn, i;
  unsigned int ui;

  for (track = mserv_tracks; track; track = track->next) {
    strncpy(genres_spl, track->genres, GENRESLEN);
    genres_spl[GENRESLEN] = '\0'; /* shouldn't ever overflow */
    if ((genres_n = mserv_split(genres_str, MAXNGENRE+1, genres_spl,
				",")) >= MAXNGENRE+1) {
      /* too many genres should never happen */
      mserv_log("Track %s has too many genres!", track->filename);
      genres_n = MAXNGENRE;
    }
    for (gn = 0; gn < genres_n; gn++) {
      for (genre = genrelist; genre; genre = genre->next) {
	if (!stricmp(genres_str[gn], genre->name)) {
	  break;
	}
      }
      if (!genre) {
	/* create genrelist entry */
	if ((genre = malloc(sizeof(t_genre)+strlen(genres_str[gn])+1)) == NULL
	    || (genre->tracks = malloc(64*sizeof(t_track *))) == NULL) {
	  mserv_log("Out of memory creating genre structure");
	  mserv_broadcast("MEMORY", NULL);
	  return genrelist;
	}
	genre->name = (char *)(genre+1);
	strcpy(genre->name, genres_str[gn]);
	genre->size = 64;
	genre->total = 0;
	for (ui = 0; ui < genre->size; ui++) {
	  genre->tracks[ui] = NULL;
	}
	if (genre_insertsort(&genrelist, genre)) {
	  mserv_log("Internal genrelist error (insert)");
	  return genrelist;
	}
      }
      if (genre->size == genre->total) {
	/* we need a bigger block */
	if ((genre->tracks = realloc(genre->tracks, (genre->size+64)*
				     sizeof(t_track *))) == NULL) {
	  mserv_log("Out of memory increasing size of genre structure");
	  mserv_broadcast("MEMORY", NULL);
	  return genrelist;
	}
	for (ui = genre->size; ui < genre->size+64; ui++) {
	  genre->tracks[ui] = NULL;
	}
	genre->size+= 64;
      }
      /* add track to genrelist entry */
      genre->tracks[genre->total++] = track;
    }
  }
  i = 1;
  for (genre = genrelist; genre; genre = genre->next) {
    genre->id = i++;
  }
  return genrelist;
}

static int genre_insertsort(t_genre **list, t_genre *genre)
{
  t_genre *g, *p;
  int a;

  for (p = NULL, g = *list; g; p = g, g = g->next) {
    if ((a = stricmp(g->name, genre->name)) > 0)
      break;
    if (a == 0)
      return 1;
  }
  genre->next = g;
  if (!p)
    *list = genre;
  else
    p->next = genre;
  return 0;
}

static int mserv_createdir(const char *path)
{
  char *mypath;
  char *p, *r;
  struct stat buf;
  unsigned int len = strlen(path);

  if (!*path)
    return 1;
  if ((mypath = malloc(len+2)) == NULL)
    return 1;
  strcpy(mypath, path);
  if (!(len && mypath[len-1] == '/')) {
    mypath[len++] = '/';
    mypath[len++] = '\0';
  }
  for (r = mypath+1; (p = strchr(r, '/')) != NULL; r = p+1) {
    *p = '\0';
    if (stat(mypath, &buf)) {
      if (errno == ENOENT) {
        if (mkdir(mypath, 0755))
          return 1;
      } else {
        return 1;
      }
    } else {
      if (!S_ISDIR(buf.st_mode))
        return 1;
    }
    *p = '/';
  }
  return 0;
}

void mserv_ensuredisk(void)
{
  t_track *track;
  t_album *album;

  for (track = mserv_tracks; track; track = track->next) {
    track = mserv_checkdisk_track(track);
  }
  for (album = mserv_albums; album; album = album->next) {
    album = mserv_checkdisk_album(album);
  }
}

#ifdef SOUNDCARD

static int mserv_readvolume(void)
{
  int mixer_fd;
  int vol;

  if (!(mixer_fd = open(opt_path_mixer, O_RDWR, 0)))
    return -1;
  if (ioctl(mixer_fd, MIXER_READ(SOUND_MIXER_VOLUME), &vol) == -1) {
    close(mixer_fd); /* ignore errors */
    return -1;
  }
  close(mixer_fd);
  return vol;
}

static int mserv_setvolume(int vol)
{
  int mixer_fd;
  int newval = vol;

  if (!(mixer_fd = open(opt_path_mixer, O_RDWR, 0)))
    return -1;
  if (ioctl(mixer_fd, MIXER_WRITE(SOUND_MIXER_VOLUME), &newval) == -1) {
    close(mixer_fd); /* ignore errors */
    return -1;
  }
  close(mixer_fd); /* ignore errors */
  return newval;
}

int mserv_setmixer(t_client *cl, int what, const char *line)
{
  int curval, param, newval, attempt;
  int mixer_fd;
  char *end;
  const char *p;
  int type;
  int multi = 1;
  
  if (!(mixer_fd = open(opt_path_mixer, O_RDWR, 0))) {
    mserv_response(cl, "MIXER", 0);
    return -1;
  }
  if (ioctl(mixer_fd, MIXER_READ(what), &curval) == -1) {
    close(mixer_fd);
    perror("iotcl read");
    mserv_response(cl, "IOCTLRD", 0);
    return -1;
  }
  curval = curval & 0xff;
  if (!*line)
    return curval;
  if (*line == '+' || *line == '-') {
    type = *line == '+' ? 1 : -1;
    param = 1;
    p = line+1;
    if (*line == *p) {
      while(*p++ == *line)
	param+= 1;
      if (*p)
	goto badnumber;
    } else {
      if (*p) {
	param = strtol(p, &end, 10);
	if (*end)
	  goto badnumber;
      } else {
	param = 1;
      }
    }
  } else {
    type = 0;
    param = 0;
    newval = strtol(line, &end, 10);
    if (*end)
      goto badnumber;
  }
  for (; multi; multi--) {
    for (attempt = 0; attempt < 10; attempt++, param++) {
      if (type)
	newval = curval + type*param;
      if (newval > 100)
	newval = 100;
      if (newval < 0)
	newval = 0;
      newval = newval | (newval<<8);
      if (ioctl(mixer_fd, MIXER_WRITE(what), &newval) == -1) {
	close(mixer_fd);
	perror("iotcl write");
	mserv_response(cl, "IOCTLWR", NULL);
	return -1;
      }
      newval = newval & 0xFF;
      if (type == 0 || newval != curval)
	break;
      param++;
    }
    if (attempt == 10) {
      mserv_response(cl, "IOCTLEE", NULL);
      close(mixer_fd);
      return -1;
    }
    curval = newval; /* for multi +/-, start from this value */
  }
  close(mixer_fd);
  return newval;
 badnumber:
  close(mixer_fd);
  mserv_response(cl, "NAN", NULL);
  return -1;
}

#endif

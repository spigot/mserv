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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pwd.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <time.h>

#include "mserv.h"
#include "misc.h"
#include "cmd.h"
#include "acl.h"
#include "mp3info.h"
#include "mserv-soundcard.h"
#include "defconf.h"
#include "conf.h"
#include "opt.h"
#include "filter.h"
#include "channel.h"
#include "module.h"

#define MAXFD 64

extern char *optarg;
extern int optind;
/* extern int getopt(int, char *const *, const char *); */ /* sunos ;( */

/*** file-scope (static) globals ***/

static FILE *mserv_logfile = NULL;
static int mserv_socket = 0;
static int mserv_nextid_album = 1;
static int mserv_nextid_track = 1;
static int mserv_player_pid = 0;
static int mserv_player_pipe = -1;
static int mserv_started = 0;
static char mserv_filter[FILTERLEN+1] = "";
static double mserv_gap = 0;
static t_lang *mserv_language;

/*** externed variables ***/

char *progname = NULL;
int mserv_verbose = 0;
int mserv_debug = 0;
t_client *mserv_clients = NULL;
t_track *mserv_tracks = NULL;
t_album *mserv_albums = NULL;
t_queue *mserv_queue = NULL;
t_historyentry *mserv_history[HISTORYLEN];
t_acl *mserv_acl = NULL;
int mserv_shutdown = 0;
int mserv_random = 0;
double mserv_factor = 0.6;
t_author *mserv_authors = NULL;
t_genre *mserv_genres = NULL;
unsigned int mserv_filter_ok = 0;
unsigned int mserv_filter_notok = 0;
t_channel *mserv_channel = NULL;
t_trkinfo mserv_player_playing = { NULL, "" };
int mserv_n_songs_started = 0;

/*** file-scope (static) function declarations ***/

static void mserv_mainloop(void);
static void mserv_checkchild(void);
static void mserv_pollwrite(t_client *cl);
static void mserv_pollclient(t_client *cl);
static void mserv_endline(t_client *cl, char *line);
static int mserv_loadlang(const char *pathname);
static void mserv_vresponse(int asynchronous,
			    t_client *cl,
			    const char *token,
			    const char *fmt,
			    va_list ap);
static void mserv_scandir(void);
static void mserv_scandir_recurse(const char *pathname);
static t_track *mserv_loadtrk(const char *filename);
static t_album *mserv_loadalbum(const char *filename, int onlyifexists);
static t_average *mserv_forced_getaverage(t_album *album, const char *user);
static void album_compute_averages(t_album *album);
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
static const char *mserv_getplayer(char *fname);

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

static void mserv_init(void)
{
  unsigned int i;
  t_album *album;
  char line[256];
  char error[256];
  const char *config_execline;
  t_client cl;

  /* load up defaults */
  mserv_factor = opt_factor;
  mserv_random = opt_random;
  mserv_gap = opt_gap;

  /* load albums and tracks */
  mserv_scandir();

  /* re-number albums */
  mserv_nextid_album = 1;

  for (album = mserv_albums; album; album = album->next) {
    album->id = mserv_nextid_album++;
  }

  /* extract author information */
  mserv_authors = mserv_authorlist();

  /* extract genre information */
  mserv_genres = mserv_genrelist();

  mserv_savechanges();

  /* create default channel */
  if (channel_create(&mserv_channel, "default",
                     error, sizeof(error)) != MSERV_SUCCESS) {
    mserv_log("Failed to create default channel: %s", error);
    mserv_closedown(1);
  }

  /* setting the filter must be done after all the tracks are loaded, if
     there is no filter then don't try to set it, the default install won't
     have any tracks and this will cause it to fail */
  if (*opt_filter && mserv_setfilter(opt_filter)) {
    mserv_log("Unable to set default filter, ignoring");
  }

  /* must be done when there is a default channel */
  mserv_recalcratings();

  memset(&cl, 0, sizeof(t_client));
  cl.userlevel = level_master;
  cl.authed = 1;
  cl.state = st_wait;
  cl.mode = mode_human;
  strcpy(cl.user, "root");
  strcpy(cl.channel, "default");
  cl.socket = fileno(mserv_logfile); /* TODO: startup.out */
  fflush(mserv_logfile);

  /* execute commands in options file */
  for (i = 0;; i++) {
    if ((config_execline = conf_getvalue_n("exec", i)) == NULL)
      break;
    if (strlen(config_execline) >= sizeof(line)) {
      mserv_log("Line too long: %s", config_execline);
      continue;
    }
    strncpy(line, config_execline, sizeof(line));
    line[sizeof(line) - 1] = '\0';

    if (mserv_debug)
      mserv_log("executing: %s", line);
    mserv_endline(&cl, line);
  }
  mserv_pollwrite(&cl);
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
  char error[256];
  int badparams = 0;

  progname = argv[0];

  if (getuid() == 0) {
    fprintf(stderr, "%s: fool attempted to run server as root\n", progname);
    exit(1);
  }

  while ((i = getopt(argc, argv, "c:p:r:vd")) != -1) {
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
    case 'd':
      mserv_debug++;
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
	  "              [-d] (debug mode)\n"
	  "        This is mserv version " VERSION "\n",
	  stderr);
    exit(2);
  }    

  if (mserv_verbose)
    printf("Mserv version " VERSION " (c) James Ponder 1999-2003\n");

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
    if ((m = malloc(strlen(mserv_root) + 1)) == NULL) {
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
  if ((f_conf = open(opt_path_distconf,
                     O_WRONLY|O_CREAT|O_TRUNC, 0644)) != -1) {
    if (write(f_conf, defconf_file, defconf_size) == -1)
      fprintf(stderr, "%s: unable to write to %s: %s\n", progname,
              opt_path_distconf, strerror(errno));
    close(f_conf);
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

  if (conf_getvalue("player")) {
    mserv_log("You have an old configuration file format, please take a look "
              "at config.dist and merge in the player configuration lines");
    mserv_closedown(1);
  }

  mserv_log("*** Server started!");

  /* init module part */
  if (module_init(error, sizeof(error)) != MSERV_SUCCESS) {
    mserv_log("Failed to initialise module sub-system: %s", error);
    mserv_closedown(1);
  }

  /* init channel part */
  if (channel_init(error, sizeof(error)) != MSERV_SUCCESS) {
    mserv_log("Failed to initialise channel sub-system: %s", error);
    mserv_closedown(1);
  }

  /* do initialization */
  mserv_init();

  /* TODO: write startup.out to stdout (log?) if verbose (debug?) on */

  signal(SIGINT, mserv_sighandler);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, mserv_sighandler);

  if (opt_play) {
    if (mserv_player_playnext())
      mserv_log("Unable to start to play - nothing in queue and random off?");
  }

  mserv_started = 1;

  mserv_mainloop();

  if (channel_final(error, sizeof(error)) != MSERV_SUCCESS)
    mserv_log("Failed to finalise channel sub-system: %s", error);
  if (module_final(error, sizeof(error)) != MSERV_SUCCESS)
    mserv_log("Failed to finalise module sub-system: %s", error);

  return(0);
}

static void mserv_mainloop(void)
{
  char error[256];
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
    if (mserv_player_pid)
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
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    /* mserv_log("SELECT %d %d", timeout.tv_sec, timeout.tv_usec); */
    select(maxfd, &fd_reads, &fd_writes, NULL, &timeout);
    /* mserv_log("END SELECT"); */
    /* output any sound that needs to be */
    channel_sync(mserv_channel, error, sizeof(error));
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
    cl->userlevel = level_guest;
    cl->lstate = lst_normal;
    cl->socket = client_socket;
    if (mserv_debug)
      mserv_log("%s: Allocated socket %x", inet_ntoa(cl->sin.sin_addr));
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
    strcpy(cl->channel, "default");
    mserv_send(cl, "200 Mserv " VERSION " (c) James Ponder 1999-2003 - "
	       "Type: USER <username>\r\n", 0);
    mserv_send(cl, ".\r\n", 0);
  }
}

/* Return true if called more than three times in five minutes */
static int mserv_toooften(void)
{
  static time_t lastTime;
  static int numberOfTimes;

  time_t now = time(NULL);

  /* Reset the counter after 5 minutes */
  if (now - lastTime > 5 * 60) {
    lastTime = now;
    numberOfTimes = 0;
  }

  numberOfTimes += 1;
  
  return numberOfTimes > 3;
}

static void mserv_setHistoricalDuration(const t_track *track,
					int durationmsecs)
{
  int i;
  t_historyentry *entry = NULL;
  
  /* Find the most recent history entry matching the track */
  for (i = 0; i < HISTORYLEN; i++) {
    if (mserv_history[i] != NULL &&
	mserv_history[i]->track == track)
    {
      entry = mserv_history[i];
      break;
    }
  }
  
  if (entry != NULL) {
    entry->durationMsecs = durationmsecs;
  }
}

static void mserv_checkchild(void)
{
  int status, pid, trouble = 0, duration;
  t_trkinfo *playing = channel_getplaying(mserv_channel);
  
  pid = waitpid(mserv_player_pid, &status, WNOHANG | WUNTRACED);
  
  if (pid == -1) {
    mserv_log("waitpid failure (%d): %s", errno, strerror(errno));
    exit(1);
  }
  
  if (pid == 0) {
    /* Nothing's happened */
    return;
  }
  
  /* Invariant: pid is now mserv_player_pid */
  
  if (WIFSTOPPED(status)) {
    mserv_log("Child process stopped");
    return;
  }
  
  /* Invariant: player has terminated */

  duration = channel_getplaying_msecs(mserv_channel);
  mserv_setHistoricalDuration(playing->track, duration);
  
  mserv_player_pid = 0;
  if (abs(mserv_player_playing.track->duration - duration / 10) > 250) {
    /* The recorded duration is more than 2.5 seconds off.  Update it
     * with how long it really took to play the track. */
    mserv_player_playing.track->duration = duration / 10;
  }
  mserv_player_playing.track->lastplay = time(NULL);
  mserv_player_playing.track->modified = 1;
  mserv_player_playing.track = NULL;
  
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    /* Player done, exit code 0. */
    trouble = 0;
  } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
    /* Player done, non-zero exit code. */
    trouble = 1;
    
    mserv_log("Child process exited with status (%d)", WEXITSTATUS(status));
    if (playing) {
      mserv_broadcast("PLTRBL", "%d\t%d\t%s\t%s\t%d",
		      playing->track->album->id, playing->track->n_track,
		      playing->track->author, playing->track->name,
                      WEXITSTATUS(status));
    } else {
      mserv_broadcast("PLTRBL", "");
    }
  } else if (WIFSIGNALED(status)) {
    /* Player terminated by signal */
    trouble = 1;
    
    mserv_log("Child process received signal %s (%d)%s",
              strsignal(WTERMSIG(status)), WTERMSIG(status),
              WCOREDUMP(status) ? " (core dumped)" : "");
    if (playing) {
      mserv_broadcast("PLTRBL2", "%d\t%d\t%s\t%s\t%s",
		      playing->track->album->id, playing->track->n_track,
                      playing->track->author, playing->track->name,
                      strsignal(WTERMSIG(status)));
    } else {
      mserv_broadcast("PLTRBL2", "");
    }
  }

  /* Invariant: Any information about player trouble has now been
   * logged */
  mserv_checkshutdown();
  
  if (trouble && mserv_toooften()) {
    /* If we run into trouble more than three times in five minutes,
     * stop playing. */
    mserv_log("Too many player problems, playback stopped");
    mserv_broadcast("NOSPAWN", "");
    return;
  }
  
  mserv_player_playnext(); /* may or may not start a new track */
  /* if nothing playing, output stream will run out of input and print
   * the finished message */
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
  if (mserv_debug >= 3)
    mserv_log("%s: write %d bytes", inet_ntoa(cl->sin.sin_addr), bytes);
  while (bytes) {
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
  /* clear queue of user if appropriate */
  if ((cl->mode == mode_human && opt_queue_clear_human) ||
      (cl->mode == mode_computer && opt_queue_clear_computer) ||
      (cl->mode == mode_rtcomputer && opt_queue_clear_rtcomputer)) {
    a = NULL;
    b = mserv_queue;
    c = mserv_queue ? mserv_queue->next : NULL;
    while (b) {
      if (!stricmp(b->trkinfo.user, cl->user)) {
	mserv_broadcast("UNQA", "%d\t%d\t%s\t%s",
			b->trkinfo.track->album->id, b->trkinfo.track->n_track,
			b->trkinfo.track->author, b->trkinfo.track->name);
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
      size = mserv_MIN(len, (unsigned int)(OUTBUFLEN - cl->outbuf[i].iov_len));
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
  t_cmdparams cp;

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
      len = mserv_MIN((q-p), USERNAMELEN);
      strncpy(ru, p, len);
      ru[len] = '\0';
      p = q+1;
      while (*p == ' ')
	p++;
    } else {
      strcpy(ru, cl->user);
    }
    cmdsptr = cmd_cmds;
again:
    for (; cmdsptr->name; cmdsptr++) {
      if (!mserv_checklevel(cl, cmdsptr->userlevel))
	continue;
      len = strlen(cmdsptr->name);
      if (strnicmp(p, cmdsptr->name, len) == 0) {
	if (p[len] != '\0' && p[len] != ' ')
	  continue;
	p+= len;
	while (*p == ' ')
	  p++;
	while (*p && p[strlen(p)-1] == ' ')
	  p[strlen(p)-1] = '\0';
	if (cmdsptr->authed && !cl->authed) {
          mserv_response(cl, "NOTAUTH", NULL);
          break;
        }
        if (cmdsptr->sub_cmds && cmdsptr->function == NULL) {
          /* this is not a real command but a command container */
          cmdsptr = cmdsptr->sub_cmds;
          goto again;
        }
        if (cmdsptr->function == NULL)
          /* this shouldn't happen */
          break;
        memset(&cp, 0, sizeof(cp));
        cp.ru = ru;
        cp.line = p;
        cmdsptr->function(cl, &cp);
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

static void album_updateaverage(t_album *album, const char *user,
				int oldrating, int newrating)
{
  t_average *average;

  if (oldrating == 0 && newrating == 0) {
    return;
  }
  
  average = mserv_forced_getaverage(album, user);
  
  /* Remove the previous rating from the average */
  if (oldrating > 0) {
    average->n_ratings--;
    average->sum -= ((double)(oldrating - 1)) / 4.0;
  }
  
  /* Add the current rating to the average */
  if (newrating > 0) {
    average->n_ratings++;
    average->sum += ((double)(newrating - 1)) / 4.0;
  }
}

/* 1 <= val <= 5, 0 = heard */
t_rating *mserv_ratetrack(t_client *cl, t_track **track, unsigned int val)
{
  t_rating *rate, *r, *p;
  
  *track = mserv_checkdisk_track(*track);
  (*track)->modified = 1; /* save information */
  
  if ((rate = mserv_getrate(cl->user, *track)) == NULL) {
    if ((rate = malloc(sizeof(t_rating)+strlen(cl->user)+1)) == NULL) {
      mserv_log("Out of memory creating rating");
      return NULL;
    }
    rate->user = (char *)(rate+1);
    rate->rating = 0; /* 0 means HEARD */
    strcpy(rate->user, cl->user);
    rate->next = NULL;
    for (p = NULL, r = (*track)->ratings; r; p = r, r = r->next) ;
    if (p)
      p->next = rate;
    else
      (*track)->ratings = rate;
  }
  
  /* Invariant: rate->rating can be modified when we get here */
  album_updateaverage((*track)->album,
		      cl->user,
		      rate != NULL ? rate->rating : 0,
		      val);
  rate->rating = val;
  rate->when = time(NULL);
  
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
  newtrack->album = track->album;
  newtrack->n_track = track->n_track;

  /* change album track pointers */
  for (album = mserv_albums; album; album = album->next) {
    for (ui = 0; ui < album->ntracks; ui++) {
      if (album->tracks[ui] == track) {
	t_acl *user;
	
	/* Keep the album averages up to date */
	for (user = mserv_acl; user != NULL; user = user->next) {
	  t_rating *oldrating = mserv_getrate(user->user, track);
	  t_rating *newrating = mserv_getrate(user->user, newtrack);
	  
	  album_updateaverage(album, user->user,
			      oldrating ? oldrating->rating : 0,
			      newrating ? newrating->rating : 0);
	}
	
	album->tracks[ui] = newtrack;
      }
    }
  }
  /* change currently decoding track pointer */
  if (mserv_player_playing.track == track)
    mserv_player_playing.track = newtrack;
  /* change queue track pointers */
  for (queue = mserv_queue; queue; queue = queue->next) {
    if (queue->trkinfo.track == track)
      queue->trkinfo.track = newtrack;
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
    for (ui = 0; ui < au->ntracks; ui++) {
      if (au->tracks[ui] == track) {
	au->tracks[ui] = newtrack;
      }
    }
  }
  for (gen = mserv_genres; gen; gen = gen->next) {
    for (ui = 0; ui < gen->ntracks; ui++) {
      if (gen->tracks[ui] == track) {
	gen->tracks[ui] = newtrack;
      }
    }
  }
  if (mserv_tracks == track)
    mserv_tracks = newtrack;
  channel_replacetrack(mserv_channel, track, newtrack);
}

static void mserv_replacealbum(t_album *album, t_album *newalbum)
{
  t_album *al;

  newalbum->id = album->id;
  newalbum->next = album->next;
  newalbum->tracks_size = album->tracks_size;
  newalbum->ntracks = album->ntracks;
  newalbum->tracks = album->tracks;
  newalbum->averages = album->averages;

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
  mserv_shutdown = 2; /* 2 = shutdown in progress */

  /* As mserv_abortplay() will log that a song has been stopped, it
   * must be called before closing the log file (below). */
  mserv_abortplay();

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

  while (fgets(buffer, LANGLINELEN, fd)) {
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
  mserv_vresponse(0, cl, token, fmt, ap);
  va_end(ap);
  if (cl->mode != mode_human)
    mserv_send(cl, ".\r\n", 0);
}

void mserv_responsent(t_client *cl, const char *token, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  mserv_vresponse(0, cl, token, fmt, ap);
  va_end(ap);
}

/* Inform a user about something that was caused by somebody else */
void mserv_informnt(t_client *cl, const char *token, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  mserv_vresponse(1, cl, token, fmt, ap);
  va_end(ap);
}

static void mserv_vresponse(int asynchronous,
			    t_client *cl,
			    const char *token,
			    const char *fmt,
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
  if (asynchronous && cl->mode == mode_computer) {
    /* mode_computer clients aren't interested in asynchronous
     * messages.  If they were, they'd be mode_rtcomputer clients. */
    return;
  }
  if (cl->mode == mode_rtcomputer && asynchronous) {
    /* Asynchronous messages to rtcomputer clients are prefixed with "=" */
    mserv_send(cl, "=", 0);
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
  va_list ap;
  struct timeval tv;

  va_start(ap, text);
  vsnprintf(tmp, 799, text, ap);
  va_end(ap);

  if (!mserv_logfile) {
    if (!(mserv_logfile = fopen(opt_path_logfile, "a"))) {
      fprintf(stderr, "Could not open logfile: %s\n", strerror(errno));
      mserv_closedown(1);
    }
  }

  gettimeofday(&tv, NULL);
  timeptr = localtime(&tv.tv_sec);

  strftime(timetmp, 63, "%m/%d %H:%M:%S", timeptr);
  snprintf(tmp2, 1023, "%s.%03d [%06d] %s\n", timetmp,
           (int)(tv.tv_usec / 1000), getpid(), tmp);

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
}

static void mserv_scandir_recurse(const char *pathname)
{
  DIR *dir;
  struct dirent *ent;
  char fullpath[1024];
  char *filename;
  int llen;
  struct stat buf;
  t_album *album;
  t_track **tracks;
  unsigned int ntracks;
  unsigned int tracks_size;
  unsigned int ui;
  int flag = 0;

  /* pathname is "" or "directory/" or "directory/directory/..." */

  if (mserv_verbose)
    mserv_log("Scan directory: %s", pathname);

  /* create initial tracks array, 64 entries */

  tracks_size = 64;
  ntracks = 0;
  if ((tracks = malloc(tracks_size * sizeof(t_track *))) == NULL) {
    mserv_log("Out of memory creating album structure");
    return;
  }
  for (ui = 0; ui < tracks_size; ui++)
    tracks[ui] = NULL;

  /* find music files and use .trk files if they exist */

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
  while ((ent = readdir(dir))) {
    llen = strlen(ent->d_name);
    /* ignore dot files */
    if (ent->d_name[0] == '.')
      continue;
    /* construct full path */
    if ((unsigned int)snprintf(fullpath, 1024, "%s/%s%s", opt_path_tracks,
			       pathname, ent->d_name) >= 1024) {
      mserv_log("fullpath buffer too small");
      closedir(dir);
      return;
    }
    /* get details about this file */
    if (stat(fullpath, &buf)) {
      mserv_log("Unable to stat '%s': %s", fullpath, strerror(errno));
      continue;
    }
    filename = fullpath+strlen(opt_path_tracks)+1;
    if (S_ISDIR(buf.st_mode)) {
      /* recurse into directories */
      strcat(filename, "/");
      mserv_scandir_recurse(filename);
      continue;
    }
    if (!S_ISREG(buf.st_mode))
      /* ignore files if they aren't regular */
      continue;
    /* we have found a normal track - check there is a player for it */
    if (mserv_getplayer(ent->d_name) == NULL)
      continue; /* extension not found */
    if (tracks_size == ntracks) {
      /* we need a bigger block */
      if ((tracks = realloc(tracks, (tracks_size + 64) *
                            sizeof(t_track *))) == NULL) {
        mserv_log("Out of memory increasing size of album structure");
        return;
      }
      for (ui = tracks_size; ui < tracks_size + 64; ui++)
        tracks[ui] = NULL;
      tracks_size+= 64;
    }
    if (mserv_verbose)
      mserv_log("Track file: %s", fullpath);
    if ((tracks[ntracks] = mserv_loadtrk(filename)) == NULL) {
      mserv_log("Unable to add track '%s'", fullpath);
      continue;
    }
    tracks[ntracks]->id = mserv_nextid_track++;
    tracks[ntracks]->n_track = ntracks + 1; /* 1... */
    tracks[ntracks]->next = mserv_tracks;
    mserv_tracks = tracks[ntracks];
    ntracks++;
    flag = 1; /* there is at least one track in this directory */
  }
  closedir(dir);
  /* load album, but only if there is an album file or flag is set */
  if ((album = mserv_loadalbum(pathname, flag ? 0 : 1)) == NULL)
    return;
  qsort(tracks, ntracks, sizeof(t_track *), mserv_trackcompare_filename);
  /* we've sorted so we need to renumber tracks */
  for (ui = 0; ui < ntracks; ui++) {
    tracks[ui]->n_track = ui+1;
    tracks[ui]->album = album;
  }
  album->tracks = tracks;
  album->tracks_size = tracks_size;
  album->ntracks = ntracks;
  if (mserv_verbose)
    mserv_log("Added album: '%s' by %s", album->name, album->author);
  if (album_insertsort(album)) {
    free(album);
    return;
  }
  album_compute_averages(album);
  return;
}

static void album_free_averages(t_album *album)
{
  while (album->averages != NULL) {
    t_average *average = album->averages;
    album->averages = album->averages->next;
    free(average);
  }
}

static t_average *mserv_getaverage(t_album *album, const char *user)
{
  t_average *average;
  
  for (average = album->averages;
       average != NULL;
       average = average->next)
  {
    if (strcmp(average->user, user) == 0) {
      return average;
    }
  }
  
  return NULL;
}

/* This function never returns NULL.  If there is no average for this
 * user, a new one is created */
static t_average *mserv_forced_getaverage(t_album *album,
					  const char *user)
{
  t_average *average;
  
  average = mserv_getaverage(album, user);
  if (average != NULL) {
    return average;
  }
  
  /* Allocate space for the struct and the user name in one chunk */
  average = calloc(1, sizeof(t_average) + strlen(user) + 1);
  if (average == NULL) {
    mserv_log("Failed to allocate memory for user average, terminating.");
    exit(1);
  }
  
  /* Put the user name right after the struct */
  average->user = (char *)(average + 1);
  strcpy(average->user, user);
  
  average->next = album->averages;
  album->averages = average;
  
  return average;
}

static void mserv_setaverage(t_album *album,
			     const char *user,
			     int n_ratings,
			     double sum)
{
  t_average *average = mserv_forced_getaverage(album, user);

  average->n_ratings = n_ratings;
  average->sum = sum;
}

/* Re-compute the per-user average ratings for this album. */
static void album_compute_averages(t_album *album)
{
  t_acl *user;
  
  album_free_averages(album);

  /* For all users... */
  for (user = mserv_acl; user != NULL; user = user->next) {
    int n_ratings = 0;
    double sum = 0.0;
    
    unsigned int track_number;
    
    /* For all tracks of this album... */
    for (track_number = 0; track_number < album->ntracks; track_number++) {
      t_track *track = album->tracks[track_number];
      t_rating *rating = mserv_getrate(user->user, track);
      
      /* If the user has rated this track... */
      if (rating != NULL && rating->rating > 0) {
	n_ratings++;
	
	/* Convert the integer rating 1-5 to 0.0-1.0 */
	sum += ((double)(rating->rating - 1)) / 4.0;
      }
    }

    if (n_ratings > 0) {
      mserv_setaverage(album, user->user, n_ratings, sum);
    }
  }
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

/*
 * This function accepts a value in the range 0.0-0.1 and returns the
 * same value +-0.05.
 */
static inline double mserv_fuzz_rating(double rating)
{
  static double diff = -0.05;
  
  diff += 0.03141;
  if (diff > 0.05) {
    diff -= 0.10;
  }
  
  return rating + diff;
}

static int mserv_trackcompare_rating(const void *a, const void *b)
{
  const t_track *atrack = *(const t_track * const *)a;
  const t_track *btrack = *(const t_track * const *)b;
  
  double fuzzed_arating;
  
  /* sorts in descending order */
  if (atrack == NULL) {
    if (btrack == NULL) {
      return 0;
    } else {
      return 1;
    }
  }
  
  if (btrack == NULL)
    return -1;
  
  /* Invariant: Both tracks have a rating each */

  /* When comparing the ratings of two songs, we do a fuzzy compare.
   * The fuzziness consists of that if the ratings are close enough
   * (determined by mserv_fuzz_rating()), the songs will sometimes be
   * considered to be in the wrong order.
   *
   * So at the time of writing, 0.55 will often be considered to be
   * larger than 0.56, seldom be considered to be larger than 0.59,
   * and never considered to be larger than 0.6.
   *
   * The point is that to a human, 0.55 and 0.56 are almost the same
   * value.  This way, mserv thinks so as well.
   */
  fuzzed_arating = mserv_fuzz_rating(atrack->rating);
  if (fuzzed_arating < btrack->rating) {
    return 1;
  } else if (fuzzed_arating > btrack->rating) {
    return -1;
  }
  
  /* Invariant: Both tracks have the same rating */
  
  if (atrack->lastplay > btrack->lastplay) {
    return 1;
  } else if (atrack->lastplay < btrack->lastplay) {
    return -1;
  }
  
  /* Invariant: Both tracks were last played at the same time (!) */
  
  /* I really can't order these two tracks */
  return 0;
}

/* Load album from disk, but don't populate it with tracks */
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
    while (fgets(buffer, 1024, fd)) {
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
    strcpy(author, "Unnamed");
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
  album->averages = NULL;
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
  int volume = 100;
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
    while (fgets(buffer, 1024, fd)) {
      line++;
      alen = strlen(buffer);
      if (buffer[alen-1] != '\n') {
        mserv_log("Line %d too long in '%s'", line, fullpath_trk);
        fclose(fd);
	return NULL;
      }
      buffer[--alen] = '\0';
      if (!(l = strcspn(buffer, "=")) || l >= 64) {
	mserv_log("Invalid track line %d in '%s'", line, fullpath_trk);
        fclose(fd);
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
	} else if (!stricmp(token, "_volume")) {
	  volume = atol(value);
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
          fclose(fd);
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
    if (fstat(fileno(fd), &buf) == -1) {
      perror("fstat");
      mserv_log("Unable to stat '%s': %s", filename, strerror(errno));
      fclose(fd);
      return NULL;
    }
    mtime = buf.st_mtime;
    fclose(fd);
    if (!*author) {
      mserv_log("No author specified in '%s'", fullpath_trk);
      strncpy(author, "Unknown", AUTHORLEN+1);
      author[AUTHORLEN] = '\0';
    }
    if (!*name) {
      char *basename;
      
      mserv_log("No name specified in '%s'", fullpath_trk);

      /* Fall back on the filename in fullpath_trk... */
      basename = strrchr(fullpath_trk, '/');
      if (basename == NULL) {
	basename = fullpath_trk;
      } else {
	/* Skip the '/' character */
	basename++;
      }
      
      /* ... but without the ".trk". */
      strncpy(name, basename, NAMELEN+1);
      if (strlen(basename) - 4 < NAMELEN) {
	name[strlen(basename) - 4] = '\0';
      } else {
	name[NAMELEN] = '\0';
      }
    }
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
  track->mtime = mtime;
  track->volume = volume;
  return track;
}

int mserv_addqueue(t_client *cl, t_track *track)
{
  t_queue *q, *p, *last, *newpos_last;
  t_queue *newq;
  int total;
  int qpos, ppos, newpos;

  if (mserv_shutdown)
    return -1;

  if ((newq = malloc(sizeof(t_queue))) == NULL) {
    mserv_log("Out of memory adding to queue");
    mserv_broadcast("MEMORY", NULL);
    return -1;
  }
  newq->trkinfo.track = track;
  strcpy(newq->trkinfo.user, cl->user);
  newq->next = NULL;

  total = 0;
  for (q = mserv_queue; q; q = q->next) {
    if (q->trkinfo.track == track) {
      /* can't enter the same track into the queue twice */
      free(newq);
      return -1;
    }
    if (!stricmp(q->trkinfo.user, cl->user))
      total++;
  }
  if (total >= 100) { /* don't allow a user to queue more than 100 tracks */
    free(newq);
    return -1;
  }
  /* find our last queued song */
  last = NULL;
  for (q = mserv_queue; q; q = q->next) {
    if (stricmp(q->trkinfo.user, cl->user) == 0)
      last = q;
  }
  /* now we're going to loop around all the tracks in the queue, looking for
   * users who have two tracks queued.  newpos is going to record the lowest
   * position we've found where this occurs */
  q = last ? last->next : mserv_queue;
  newpos = -1;
  newpos_last = NULL;
  for (qpos = 0; q && (newpos == -1 || newpos < qpos); q = q->next, qpos++) {
    /* lets look at this song and see if this user has queued another song */
    for (ppos = qpos + 1, last = q, p = q->next;
         p;
         last = p, p = p->next, ppos++) {
      if (stricmp(q->trkinfo.user, p->trkinfo.user) == 0) {
        /* found second song */
        if (newpos == -1 || ppos < newpos) {
          newpos = ppos;
          newpos_last = last;
        }
        break;
      }
    }
  }
  if (newpos == -1) {
    /* we didn't find any users who have two songs queued, so stick track
     * at the end of the queue */
    for (p = NULL, q = mserv_queue; q; p = q, q = q->next) ;
    if (p)
      p->next = newq;
    else
      mserv_queue = newq;
  } else {
    /* insert at position we just located */
    newq->next = newpos_last->next;
    newpos_last->next = newq;
  }
  return 0;
}

/* w = (weight - 0.5)*RFACT, x^(w+1), x^(1/(1-w)) */

/*
 * May or may not start a new track.  If nothing playing, output
 * stream will run out of input and print the finished message.
 */
int mserv_player_playnext(void)
{
  char fullpath[MAXFNAME];
  int pid;
  int fd;
  t_queue *q;
  t_track *track;
  int total, tnum, n;
  double lin,lnn;
  int i;
  char *str[21];
  char *strbuf;
  const char *player;
  int playerpipe[2];
  char error[256];

  /* If experimental fairness is enabled, the users' satisfaction will
   * impact the song selection.  The number of milliseconds the
   * current song has been playing affects all users' satisfaction.
   * Thus, to make sure the top list is up to date before choosing the
   * next track, we need to recalculate it here. */
  if (opt_experimental_fairness) {
    mserv_recalcratings();
  }
  
  if (mserv_queue == NULL) { /* no more tracks to play! */
    if (!mserv_random) {
      if (channel_stop(mserv_channel, error, sizeof(error)) != MSERV_SUCCESS)
        mserv_log("Failed to stop channel %s: %s", mserv_channel->name, error);
      return 1;
    }
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
      mserv_player_playing.track = track;
      mserv_log("Randomly picking track %d of %d (%d/%d)", tnum, total,
		track->album->id, track->n_track);
      strcpy(mserv_player_playing.user, "random");
    } else {
      mserv_log("Could not randomise song");
    }
  } else {
    q = mserv_queue;
    memcpy(&mserv_player_playing, &q->trkinfo, sizeof(t_trkinfo));
    mserv_queue = mserv_queue->next;
    free(q);
  }
  if (mserv_player_playing.track == NULL) {
    mserv_log("No track to play");
    return 1;
  }
  if (pipe(playerpipe) == -1) {
    mserv_log("Failed to create pipes with pipe() call: %s\n", strerror(errno));
    exit(1);
  }
  snprintf(fullpath, MAXFNAME, "%s/%s", opt_path_tracks,
	   mserv_player_playing.track->filename);
  player = mserv_getplayer(fullpath);
  if ((strbuf = malloc(strlen(player)+1)) == NULL) {
    mserv_broadcast("MEMORY", NULL);
    return 1;
  }
  strcpy(strbuf, player);
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
    if (fd != STDIN_FILENO)
      dup2(fd, STDIN_FILENO);
    if (fd != STDERR_FILENO)
      dup2(fd, STDERR_FILENO);
    if (playerpipe[1] != STDOUT_FILENO)
      dup2(playerpipe[1], STDOUT_FILENO);
    /* close file descriptors, ikky but suggested in unix network programming
       volume 1 second edition, although I seem to remember a better way.. */
    for (i = 3; i < MAXFD; i++)
      close(i);
    setsid();
    fprintf(stderr, "Arguments:\n");
    for (i = 0; str[i] && i < 20; i++) {
      fprintf(stderr, "%d: %s\n", i, str[i]);
    }
    execvp(str[0], str);
    fprintf(stderr, "%s: Unable to execvp '%s': '%s'", progname,
	    player, strerror(errno));
    exit(1);
  } else if (pid == -1) {
    perror("fork");
    mserv_log("Unable to fork: '%s'", strerror(errno));
  }
  free(strbuf);
  close(playerpipe[1]);
  mserv_player_pipe = playerpipe[0];
  mserv_player_pid = pid;
  if (channel_addinput(mserv_channel, mserv_player_pipe, &mserv_player_playing,
                       44100, 2, 0, mserv_gap,
                       error, sizeof(error)) != MSERV_SUCCESS) {
    mserv_log("Failed to add input to channel %s: %s", mserv_channel->name,
              error);
    /* continue anyway? */
  }

  /* If the input we just added was adapted right away, we announce it
   * at once.  Otherwise it will be announced by channel_sync() when
   * the new input is reached. */
  if (mserv_channel->input->trkinfo.track ==
      mserv_player_playing.track)
  {
    mserv_setplaying(mserv_channel,
		     mserv_channel->playing.track ? &mserv_channel->playing : NULL,
		     &mserv_channel->input->trkinfo);
  }
  
  return 0;
}

/* get player string from filename or NULL for no player */

static const char *mserv_getplayer(char *fname)
{
  char *dot;
  char playername[32];
  const char *player;

  if ((dot = strrchr(fname, '.')) == NULL)
    return NULL;
  snprintf(playername, 32, "player_%s", dot+1);
  if ((player = conf_getvalue(playername)) == NULL)
    return NULL;
  /* now convert player name (e.g. "mpg123") to full path and options */
  return conf_getvalue(player); /* NULL or player invocation string */
}

/* Return true if the pid / everything in that process group was
 * killed.  Return false if a given process group is empty when we get
 * here or we're unable to signal the requested process. */
static int mserv_killplayer(pid_t pid)
{
  /* Give processes 5us to die nicely */
  static int deathTimeout = 1;
  int status;
  int waitedFor;
  int killHasFailed = 0;
  int nWaits = 0;
  
  /* Does wait() say the pid is already dead? */
  while ((waitedFor = waitpid(pid, &status, WNOHANG)) > 0) {
    nWaits++;
  }
  if (waitedFor == -1) {
    if (nWaits == 0 && pid < 0) {
      /* The process group was empty when we got here */
      return 0;
    }
    
    /* No more processes */
    return 1;
  }
  
  /* Invariant: The pid represents at least one live process */
  
  /* Kill it! */
  if (kill(pid, SIGTERM) == -1) {
    if (errno != ESRCH) {
      /* We can't signal that process */
      return 0;
    }
    
    /* ESRCH might mean the process has died all by itself */
    killHasFailed = 1;
  }
  
  /* Does wait() say the pid died? */
  while ((waitedFor = waitpid(pid, &status, WNOHANG)) > 0) {
    /* This block intentionally left blank */
  }
  if (waitedFor == -1) {
    /* No process left! */
    return 1;
  }
  
  if (killHasFailed) {
    /* The process didn't die by itself, and we can't signal it. */
    return 0;
  }
  
  /* Invariant: We have signalled all processes represented by the
   * pid */
  
  /* Sleep a while to give the process a chance to die. */
  usleep(deathTimeout);
  /* So, is it dead now? */
  while ((waitedFor = waitpid(pid, &status, WNOHANG)) > 0) {
    /* This block intentionally left blank */
  }
  if (waitedFor == -1) {
    /* No process left! */
    deathTimeout /= 2;
    if (deathTimeout < 1) {
      deathTimeout = 1;
    }
    return 1;
  }
  
  /* The process didn't die!  Try harder. */
  kill(pid, SIGKILL);
  
  /* Wait until the process is gone */
  while ((waitedFor = waitpid(pid, NULL, WNOHANG)) >= 0) {
    if (waitedFor == 0) {
      waitpid(pid, NULL, 0);
    }
  }
  
  deathTimeout *= 3;
  return 1;
}

void mserv_abortplay()
{
  char error[256];
  t_track *stop_me = NULL;
  unsigned int duration = 0;
  
  if (mserv_player_playing.track) {
    duration = channel_getplaying_msecs(mserv_channel);
    mserv_setHistoricalDuration(mserv_player_playing.track, duration);
  }
  
  /* channel_stop probably will cause a SIGPIPE in the child */
  if (channel_stop(mserv_channel, error, sizeof(error)) != MSERV_SUCCESS)
    mserv_log("Failed to stop channel %s: %s\n", mserv_channel->name, error);
  
  /* kill the player */
  if (mserv_player_playing.track) {
    stop_me = mserv_player_playing.track;
    
    if (!mserv_killplayer(mserv_player_pid))
    {
      mserv_log("could not kill player process %d! Help.", mserv_player_pid);
    }

    /* If any processes remain in the player's process group, kill
     * them as well.  If no such processes exist, that's fine.  Thus,
     * we ignore the return code from this operation. */
    mserv_killplayer(-mserv_player_pid);
  }
  mserv_player_pid = 0;
  
  /* Mark played track done */
  if (mserv_player_playing.track) {
    mserv_checkdisk_track(mserv_player_playing.track);
    if (mserv_player_playing.track->duration < duration / 10) {
      /* The track to be stopped has already been playing for longer
       * than its total recorded duration.  This must mean the
       * recorded duration is too short. */
      mserv_player_playing.track->duration = duration / 10;
    }
    mserv_player_playing.track->lastplay = time(NULL);
    mserv_player_playing.track->modified = 1;
    mserv_player_playing.track = NULL;
  }
  
  /* recalc ratings now lastplay has changed */
  if (stop_me != NULL) {
    mserv_recalcrating(stop_me);
  }
  mserv_savechanges();
  mserv_checkshutdown();
}

static void mserv_checkshutdown(void)
{
  /* 1 == shutdown requested, 2 == shutdown in progress */
  if (mserv_shutdown == 1) {
    mserv_flush();
    sleep(1);
    mserv_flush();
    sleep(1);
    mserv_flush();
    mserv_closedown(0);
  }
}

/*
 * Calculate a value 0.0-1.0 that will be used as this user's opinion
 * when determining what track to play next.
 *
 * Called from mserv_recalcratings().
 */
static double mserv_getcookedrate(const char *user, t_track *track)
{
  t_rating *rate;
  double fallback;
  t_average *average;
  
  int n_ratings;
  double ratings_sum;
  double mean;
  
  /* Threshold value for the True Bayesian Estimate (see below) */
  int threshold;
  
  rate = mserv_getrate(user, track);
  
  /* Simple case, the user has rated the track */
  if (rate != NULL && rate->rating != 0) {
    return ((double)(rate->rating - 1)) / 4.0; /* 1-5 -> percentage */
  }
  
  if (rate == NULL) {
    fallback = opt_rate_unheard;
  } else {
    fallback = opt_rate_unrated;
  }
  
  average = mserv_getaverage(track->album,
			     user);
  if (average != NULL) {
    n_ratings = average->n_ratings;
    ratings_sum = average->sum;
  } else {
    n_ratings = 0;
    ratings_sum = 0.0;
  }
  
  if (n_ratings == 0) {
    return fallback;
  }
  
  mean = ratings_sum / n_ratings;

  threshold = track->album->ntracks / 2;
  if (threshold < 1) {
    threshold = 1;
  }
  
  /* Require at least THRESHOLD / 2 ratings before allowing lowering the
   * default track scores */
  if (n_ratings < threshold / 2 && mean < fallback) {
    return fallback;
  }
  
  /* Calculate the True Bayesian Estimate as defined by the Internet
   * Movie Database's Top 250 list (I mean it!)  What this says is
   * basically that to get very high or very low values, they have to
   * be backed by a lot of ratings.  Few ratings will give you values
   * close to the fallback value.
   *
   * To be more specific, the TBE is a weighted average between
   * fallback and mean.  The weight for fallback is THRESHOLD, and the
   * weight for mean is n_ratings.
   *
   * Thus, the exact meaning of the THRESHOLD value is that if you
   * have exactly THRESHOLD ratings, the final value will be the
   * average of the fallback and the average rating. */

  /* Note that "n_ratings * mean" could be replaced by ratings_sum,
   * but the formula is more readable this way.  I'll leave that to
   * the C compiler's optimizer if it's interested. */
  return
    ((threshold * fallback) + (n_ratings * mean)) /
    (threshold + n_ratings);
}

/* Recalculate the weights of all logged in users.  Returns the sum of
 * all weights. */
static double mserv_recalcweights(void)
{
  t_client *cl;
  double total_weight = 0.0;
  int totalusers = 0;
  
  for (cl = mserv_clients; cl; cl = cl->next) {
    if (cl->authed && cl->state != st_closed && cl->mode != mode_computer
	&& cl->userlevel != level_guest)
    {
      totalusers++;

      if (opt_experimental_fairness) {
	double satisfaction;
      
	if (!mserv_getsatisfaction(cl, &satisfaction)) {
	  /* No satisfaction value available for this user, use the
	   * neutral value 0.5 */
	  satisfaction = 0.5;
	}
	/* The user's weight will become 101% minus the user's
	 * satisfaction.  Thus, if a user has heard only SUPERB songs,
	 * that user's weight will be 0.01.  If a user has heard only
	 * AWFUL songs, the weight will be 1.01.  This way nobody ever
	 * gets their opinion entirely ignored (i.e. a weight of
	 * 0.0). */
	cl->weight = 1.01 - satisfaction;
      } else {
	/* Experimental fairness disabled; all users get a weight of
	 * 1.0. */
	cl->weight = 1.0;
      }
      
      total_weight += cl->weight;
    }
  }
  
  return total_weight;
}

/* Compute a common rating for a song that takes everything except
 * when the song was last played into account.
 *
 * If a song is filtered out, it gets a rating of 0.0.
 */
static inline double mserv_recalcprating(t_track *track)
{
  double total_weight = 0.0;
  double rating = 0.0;
  t_client *cl;
  
  for (cl = mserv_clients; cl; cl = cl->next) {
    if (!cl->authed || cl->state == st_closed ||
	cl->mode == mode_computer || cl->userlevel == level_guest)
      continue;
    
    /* Note that if the experimental fairness is disabled,
     * cl->weight will always be 1.0. */
    rating += cl->weight * mserv_getcookedrate(cl->user, track);
    total_weight += cl->weight;
  }
  
  /* If the experimental fairness is disabled, total_weight will
   * be equal to totalusers. */
  if (total_weight > 0.0) {
    return rating / total_weight;
  } else {
    /* A total weight of 0.0 should mean nobody is listening, and the
     * ratings should really be 0.0 as well. */
    return 0.0;
  }
}

/* Return a score penalty for a song that was last played seconds_ago
 * seconds ago. */
static double mserv_timepenalty(time_t seconds_ago)
{
  if (seconds_ago < 60*10) /* within ten minutes */
    return 0.1;
  else if (seconds_ago < 60*60) /* within an hour */
    return 0.2;
  else if (seconds_ago < 60*60*24) /* within a day */
    return 0.4;
  else if (seconds_ago < 60*60*24*3) /* within three days */
    return 0.6;
  else if (seconds_ago < 60*60*24*7) /* within a week */
    return 0.7;
  else if (seconds_ago < 60*60*24*7*2) /* within two weeks */
    return 0.8;
  else if (seconds_ago < 60*60*24*7*4*6) /* within half a year */
    return 0.95;
  else /* more than half a year ago */
    return 1.0;
}

void mserv_recalcratings(void)
{
  t_track *track;
  time_t now;
  int ntracks;
  t_track **sbuf;
  t_trkinfo *playing = channel_getplaying(mserv_channel);
  long time_functionEntry;
  long time_afterScoring;
  long time_afterSorting;
  
  time_functionEntry = mserv_getMSecsSinceEpoch();
  
  if (mserv_debug)
    mserv_log("Calculating ratings of tracks...");

  mserv_recalcweights();
  
  mserv_filter_ok = 0;
  mserv_filter_notok = 0;
  ntracks = 0;
  now = time(NULL);
  for (track = mserv_tracks; track; track = track->next) {
    ntracks++;
    track->filterok = (filter_check(mserv_filter, track) == 1) ? 1 : 0;
    if (track->filterok)
      mserv_filter_ok++;
    else
      mserv_filter_notok++;
    
    track->prating = mserv_recalcprating(track);
    
    if (playing && track == playing->track) {
      /* currently playing */
      track->rating = 0;
    } else {
      track->rating =
	mserv_timepenalty(now - track->lastplay) *
	track->prating;
    }
  }
  
  if (ntracks != mserv_nextid_track-1) {
    mserv_log("Track list has become incorrect (ntracks!=nextid-1)");
    exit(1);
  }

  time_afterScoring = mserv_getMSecsSinceEpoch();
  
  if (mserv_debug)
    mserv_log("Sorting tracks for top listing");
  
  if (ntracks) {
    if ((sbuf = malloc(sizeof(t_track *) * ntracks)) == NULL) {
      mserv_log("Out of memory creating sort buffer");
      mserv_broadcast("MEMORY", NULL);
    } else {
      int i;
      
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

  if (mserv_debug)
    mserv_log("Finished recalculation");
  time_afterSorting = mserv_getMSecsSinceEpoch();

  if (time_afterSorting - time_functionEntry > channel_getSoundBufferMs()) {
    mserv_log("Warning: mserv_recalcratings() took %ldms (scoring) + %ldms (sorting) = %ldms, which is > %dms (sound buffer).  Expect sound skips.",
	      time_afterScoring - time_functionEntry,
	      time_afterSorting - time_afterScoring,
	      time_afterSorting - time_functionEntry,
	      channel_getSoundBufferMs());
  }
  
  return;
}

/* Move one single song to its correct location in the top scores
 * list.  This is a lot faster than doing a full
 * mserv_recalcratings(), but misses some interactions between logged
 * in users and between songs in the same album. */
void mserv_recalcrating(t_track *track)
{
  t_trkinfo *playing = channel_getplaying(mserv_channel);
  double rating;
  
  /* Update the track's rating */
  mserv_recalcweights();
  track->filterok = (filter_check(mserv_filter, track) == 1) ? 1 : 0;
  track->prating = mserv_recalcprating(track);
  if (playing && track == playing->track) {
    /* currently playing */
    track->rating = 0;
  } else {
    track->rating =
      mserv_timepenalty(time(NULL) - track->lastplay) *
      track->prating;
  }
  
  /* Unlink the track from the tracks list */
  if (mserv_tracks == track) {
    mserv_tracks = track->next;
  } else {
    t_track *previous;
    
    for (previous = mserv_tracks;
	 previous != NULL;
	 previous = previous->next)
    {
      if (previous->next == track) {
	break;
      }
    }
    
    if (previous == NULL) {
      fprintf(stderr, "Internal error, attempt to recalc() non-existant track\n");
      exit(1);
    }
    
    previous->next = track->next;
  }
  track->next = NULL;
  
  /* Move the track to its correct location in the tracks list */
  rating = mserv_fuzz_rating(track->rating);
  if (rating > mserv_tracks->rating) {
    /* Put the track first */
    track->next = mserv_tracks->next;
    mserv_tracks = track;
  } else {
    t_track *previous;

    for (previous = mserv_tracks;
	 previous != NULL;
	 previous = previous->next)
    {
      if (previous->next == NULL ||
	  rating > previous->next->rating)
      {
	track->next = previous->next;
	previous->next = track;
	break;
      }
    }

    if (previous == NULL) {
      fprintf(stderr, "Internal error, unable to place single track in list\n");
      exit(1);
    }
  }
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
    fprintf(fd, "_volume=%d\n", track->volume);
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

/* Retrieves the user's average rating of the last 15 tracks (which is
 * supposed to be a bit less than an hour) heard by the user.  UNHEARD
 * tracks count as HEARD.  The average is stored in *satisfaction.
 * Returns the number of milliseconds on which the average is based.
 *
 * If no heard songs have been played yet, 0 is returned and the value
 * stored in *satisfaction is undefined. */
int mserv_getsatisfaction(const t_client *cl, double *satisfaction)
{
  int i;
  int totalDuration = 0;
  double sum = 0.0;
  int maxsongs;
  
  maxsongs = mserv_n_songs_started - cl->loggedinatsong;
  if (maxsongs > HISTORYLEN) {
    maxsongs = HISTORYLEN;
  }
  if (maxsongs > 15) {
    maxsongs = 15;
  }
  
  for (i = 0; mserv_history[i] && i < maxsongs; i++) {
    double songscore = mserv_getcookedrate(cl->user, mserv_history[i]->track);
    int songduration = mserv_history[i]->durationMsecs;

    /* If this is the currently playing track... */
    if (i == 0 &&
	mserv_history[i]->track == mserv_player_playing.track)
    {
      /* ... the history entry will contain a duration of 0.  Check
       * manually for how long it has been playing. */
      songduration = channel_getplaying_msecs(mserv_channel);
    }

    sum += songscore * songduration;
    totalDuration += songduration;
  }
  
  if (totalDuration > 0) {
    *satisfaction = sum / totalDuration;
  }
  
  return totalDuration;
}

/* If a user by that name is logged in, return a corresponding
 * t_client*.  If the user isn't logged in, return NULL.  */
t_client *mserv_user2client(const char *username)
{
  t_client *cl;
  
  for (cl = mserv_clients; cl; cl = cl->next) {
    if (cl->authed &&
	cl->state != st_closed &&
	cl->mode != mode_computer &&
	cl->userlevel != level_guest &&
	strncmp(cl->user, username, USERNAMELEN) == 0)
    {
      return cl;
    }
  }
  
  return NULL;
}

t_track *mserv_gettrack(unsigned int n_album, unsigned int n_track)
{
  t_album *album;
  t_track *track;

  if (n_track == 0)
    return NULL;
  track = NULL;
  for (album = mserv_albums; album; album = album->next) {
    if (album->id != n_album)
      continue;
    if (n_track > album->ntracks)
      return NULL;
    return album->tracks[n_track - 1];
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
  if (!rate)
    return "UNHEARD";
  switch(rate->rating) {
  case 0: return "HEARD";
  case 1: return "AWFUL";
  case 2: return "BAD";
  case 3: return "NEUTRAL";
  case 4: return "GOOD";
  case 5: return "SUPERB";
  }
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
  unsigned int ui;

  /* TODO: errors should free up autorlist created so far */

  for (track = mserv_tracks; track; track = track->next) {
    for (author = authorlist; author; author = author->next) {
      if (!stricmp(track->author, author->name))
	break;
    }
    if (author)
      continue;
    /* we have an author that hasn't been indexed */
    if ((author = malloc(sizeof(t_author)+strlen(track->author)+1)) == NULL) {
      mserv_log("Out of memory creating author structure");
      mserv_broadcast("MEMORY", NULL);
      return NULL;
    }
    author->name = (char *)(author + 1);
    strcpy(author->name, track->author);
    author->ntracks = 0;
    author->tracks_size = 64;
    if ((author->tracks = malloc(author->tracks_size * 
                                 sizeof(t_track *))) == NULL) {
      mserv_log("Out of memory creating author structure");
      return NULL;
    }
    for (ui = 0; ui < author->tracks_size; ui++)
      author->tracks[ui] = NULL;
    for (strack = track; strack; strack = strack->next) {
      if (!stricmp(strack->author, track->author)) {
        /* we've got one */
        if (author->tracks_size == author->ntracks) {
          /* we need a bigger block */
          if ((author->tracks = realloc(author->tracks,
                                        (author->tracks_size + 64) *
                                        sizeof(t_track *))) == NULL) {
            mserv_log("Out of memory increasing size of author structure");
            return NULL;
          }
          for (ui = author->tracks_size; ui < author->tracks_size + 64; ui++)
            author->tracks[ui] = NULL;
          author->tracks_size+= 64;
        }
	author->tracks[author->ntracks++] = strack;
      }
    }
    if (author_insertsort(&authorlist, author)) {
      mserv_log("Internal authorlist error");
      return NULL;
    }
    qsort(author->tracks, author->tracks_size, sizeof(t_track *),
	  mserv_trackcompare_name);
  }
  /* number authors */
  ui = 1;
  for (author = authorlist; author; author = author->next) {
    author->id = ui++;
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
  char error[256];

  /* clear queue */
  for (q = mserv_queue; q; q = q_n) {
    q_n = q->next;
    free(q);
  }
  mserv_queue = NULL;

  /* clear albums */
  for (a = mserv_albums; a; a = a_n) {
    a_n = a->next;
    album_free_averages(a);
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
  mserv_player_playing.track = NULL;
  mserv_player_pid = 0;
  mserv_shutdown = 0;

  /* remove default channel */
  if (mserv_channel) {
    if (channel_close(mserv_channel, error, sizeof(error)) != MSERV_SUCCESS)
      mserv_log("Failed to close default channel during reset: %s", error);
  }
  mserv_channel = NULL;

  /* ok, lets get everything back */
  mserv_init();
}

void mserv_setgap(double gap)
{
  mserv_gap = gap;
}

double mserv_getgap(void)
{
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
	genre->ntracks = 0;
	genre->tracks_size = 64;
	for (ui = 0; ui < genre->tracks_size; ui++) {
	  genre->tracks[ui] = NULL;
	}
	if (genre_insertsort(&genrelist, genre)) {
	  mserv_log("Internal genrelist error (insert)");
	  return genrelist;
	}
      }
      if (genre->tracks_size == genre->ntracks) {
	/* we need a bigger block */
	if ((genre->tracks = realloc(genre->tracks, (genre->tracks_size + 64) *
				     sizeof(t_track *))) == NULL) {
	  mserv_log("Out of memory increasing size of genre structure");
	  mserv_broadcast("MEMORY", NULL);
	  return genrelist;
	}
	for (ui = genre->tracks_size; ui < (genre->tracks_size + 64); ui++) {
	  genre->tracks[ui] = NULL;
	}
	genre->tracks_size+= 64;
      }
      /* add track to genrelist entry */
      genre->tracks[genre->ntracks++] = track;
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
  char *mypath = NULL;
  char *p, *r;
  struct stat buf;
  unsigned int len = strlen(path);
  int returnme = 1;
  
  if (!*path)
    goto fail;
  if ((mypath = malloc(len+2)) == NULL)
    goto fail;
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
          goto fail;
      } else {
        goto fail;
      }
    } else {
      if (!S_ISDIR(buf.st_mode))
        goto fail;
    }
    *p = '/';
  }

  returnme = 0;
  
 fail:
  if (mypath != NULL) {
    free(mypath);
  }
  return returnme;
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

void mserv_setplaying(t_channel *c, t_trkinfo *wasplaying,
                      t_trkinfo *nowplaying)
{
  t_lang *lang;
  t_client *cl;
  t_rating *rate;
  char buffer[USERNAMELEN+AUTHORLEN+NAMELEN+64];
  (void)c;
  int recalculate = 0;

  if (wasplaying) {
    mserv_checkdisk_track(wasplaying->track);
    for (cl = mserv_clients; cl; cl = cl->next) {
      if (!cl->authed)
        continue;
      if (cl->mode == mode_human) {
        if ((rate = mserv_getrate(cl->user, wasplaying->track)) == NULL) {
          if (mserv_ratetrack(cl, &wasplaying->track, 0) == NULL)
            mserv_broadcast("MEMORY", NULL);
	  
	  /* The just stopped track has been marked as HEARD.  It needs to
	   * be moved to the correct location on the top list. */
	  recalculate = 1; 
        }
      }
    }
    mserv_checkshutdown();
  }
  if (nowplaying) {
    mserv_checkdisk_track(nowplaying->track);
    if ((lang = mserv_gettoken("NOWPLAY")) == NULL) {
      mserv_log("Failed to find language token NOWPLAY");
      exit(1);
    }
    for (cl = mserv_clients; cl; cl = cl->next) {
      if (!cl->authed)
        continue;
      rate = mserv_getrate(cl->user, nowplaying->track);
      if (cl->mode == mode_human) {
        mserv_send(cl, "[] ", 0);
        mserv_send(cl, lang->text, 0);
        mserv_send(cl, "\r\n", 0);
        mserv_send_trackinfo(cl, nowplaying->track, rate, 1,
                             nowplaying->user);
        if (!nowplaying->track->genres[0])
          if (opt_human_alert_nogenre)
            mserv_response(cl, "GENREME", NULL); /* set my genre! */
        if (!nowplaying->track->ratings) {
          if (opt_human_alert_firstplay)
            mserv_response(cl, "FPLAY", NULL);
        } else if (!rate) {
          if (opt_human_alert_unheard)
            mserv_response(cl, "RATEME2", NULL); /* unheard */
        } else if (!rate->rating) {
          if (opt_human_alert_unrated)
            mserv_response(cl, "RATEME1", NULL); /* unrated */
        }
      } else if (cl->mode == mode_rtcomputer) {
        sprintf(buffer, "=%d\t%s\t%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
                lang->code, nowplaying->user, nowplaying->track->album->id,
                nowplaying->track->n_track, nowplaying->track->author,
                nowplaying->track->name, mserv_ratestr(rate),
                (nowplaying->track->duration / 100) / 60,
                (nowplaying->track->duration / 100) % 60);
        mserv_send(cl, buffer, 0);
      }
    }
    c->input->announced = 1;
    mserv_addtohistory(nowplaying);
    c->playing = *nowplaying;
    /* recalc ratings now that the new track has started playing */
    recalculate += 2;
  } else {
    if (mserv_player_playing.track == NULL) {
      /* reached end of input stream, and no more upcoming tracks... */
      mserv_broadcast("FINISH", NULL);
    }
  }
  
  if (recalculate == 1) {
    mserv_recalcrating(wasplaying->track);
    mserv_savechanges();
  } else if (recalculate >= 2) {
    /* Stuff has changed, recalculate all ratings */
    mserv_recalcratings(); 
    mserv_savechanges();
  }
}

/* mserv_send_trackinfo(cl, track, rate, bold, info, percentage)
 * print information about track to the client (who should be human)
 * the info string is displayed if given (NULL if not supplied)
 * bold makes the string bold
 * rate is shown in the information string as a single character */

#define TRACKINFO_MAX_INFO 16
#define TRACKINFO_MAX_TRACK 16
#define TRACKINFO_MAX_AUTHOR AUTHORLEN
#define TRACKINFO_MAX_NAME NAMELEN
#define TRACKINFO_MAX_DURATION 16

void mserv_send_trackinfo(t_client *cl, t_track *track, t_rating *rate,
                          unsigned int bold, const char *info)
{
  char buffer[TRACKINFO_MAX_INFO + 1 + TRACKINFO_MAX_TRACK + strlen(" G ") +
      TRACKINFO_MAX_AUTHOR + strlen(", ") + TRACKINFO_MAX_NAME + 1 +
      TRACKINFO_MAX_DURATION + 64];
  char bit[64];
  char *p = buffer;
  int w;
  unsigned int width_info = opt_human_display_info;
  unsigned int width_author = opt_human_display_author;
  unsigned int width_track = opt_human_display_track;
  unsigned int width_screen = opt_human_display_screen;
  unsigned int display_align = opt_human_display_align;
  unsigned int display_bold = opt_human_display_bold;

  if (width_info > TRACKINFO_MAX_INFO || width_info < 1)
    width_info = TRACKINFO_MAX_INFO;
  if (width_track > TRACKINFO_MAX_TRACK || width_track < 1)
    width_track = TRACKINFO_MAX_TRACK;
  if (width_author > TRACKINFO_MAX_AUTHOR || width_author < 3)
    width_author = TRACKINFO_MAX_AUTHOR;

  if (info)
    p+= sprintf(p, "%*.*s ", -width_info, TRACKINFO_MAX_INFO, info);
  sprintf(bit, "%d/%d", track->album->id, track->n_track);
  p+= sprintf(p, "%*.*s ", width_track, TRACKINFO_MAX_TRACK, bit);
  p+= sprintf(p, "%-1.1s ", rate && rate->rating ? mserv_ratestr(rate) : "-");
  if (display_align) {
    /* pad out to author width */
    p+= sprintf(p, "%-*.*s ", width_author, width_author, track->author);
  } else {
    /* bring track name in to author area to maximise information */
    if (strlen(track->author) > width_author) {
      p+= sprintf(p, "%.*s.., ", width_author - 2, track->author);
    } else {
      p+= sprintf(p, "%s, ", track->author);
    }
  }
  if (track->duration > 100*(59+(60*999))) {
    sprintf(bit, " ++:++");
  } else {
    sprintf(bit, " %ld:%02ld", (track->duration / 100) / 60,
            (track->duration / 100) % 60);
  }
  w = (width_screen - strlen("[] ")) - strlen(bit) - (p - buffer);
  if (w > 1)
    p+= sprintf(p, "%*.*s%.*s", -w, w, track->name,
                TRACKINFO_MAX_DURATION, bit);
  if (bold && display_bold) {
    mserv_send(cl, "[] \x1B[1m", 0);
    mserv_send(cl, buffer, 0);
    mserv_send(cl, "\x1B[0m", 0);
  } else {
    mserv_send(cl, "[] ", 0);
    mserv_send(cl, buffer, 0);
  }
  mserv_send(cl, "\r\n", 0);
}

void mserv_addtohistory(const t_trkinfo *addme)
{
  t_historyentry *newentry;

  if ((newentry = calloc(1, sizeof(t_historyentry))) == NULL) {
    mserv_log("Out of memory adding item to history");
    mserv_broadcast("MEMORY", NULL);
  } else {
    if (mserv_history[HISTORYLEN-1])
      free(mserv_history[HISTORYLEN-1]);
    
    memmove((char *)mserv_history+sizeof(t_historyentry *),
	    mserv_history,
            (HISTORYLEN-1)*sizeof(t_historyentry *));
    
    newentry->track = addme->track;
    strncpy(newentry->user, addme->user,
	    strnlen(addme->user, USERNAMELEN+1));
    
    mserv_history[0] = newentry;
  }
  mserv_n_songs_started++;
}

const char *mserv_clientmodetext(t_client *cl)
{
  if (cl->mode == mode_human)
    return "human";
  if (cl->mode == mode_computer)
    return "computer";
  if (cl->mode == mode_rtcomputer)
    return "rtcomputer";
  return "unknown";
}

int mserv_channelvolume(t_client *cl, const char *line)
{
  int curval, param, newval;
  char *end;
  const char *p;
  int type;
  char error[256];

  curval = -1;
  if (channel_volume(mserv_channel, &curval,
                     error, sizeof(error)) != MSERV_SUCCESS)
    goto badnumber;
  if (!*line)
    return curval;
  if (*line == '+' || *line == '-') {
    type = *line == '+' ? 1 : -1;
    param = 1;
    p = line+1;
    if (*line == *p) {
      while (*p == *line) {
        p++;
        param+= 1;
      }
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
  if (type)
    newval = curval + type*param;
  if (newval > 100)
    newval = 100;
  if (newval < 0)
    newval = 0;
  if (channel_volume(mserv_channel, &newval,
                     error, sizeof(error)) != MSERV_SUCCESS)
    goto badnumber;
  return newval;
 badnumber:
  mserv_response(cl, "NAN", NULL);
  return -1;
}

/* How many milliseconds have passed since the epoch? */
long mserv_getMSecsSinceEpoch(void)
{
  struct timeval now;

  if (gettimeofday(&now, NULL) == -1) {
    mserv_log("Couldn't get time of day, closing down.");
    mserv_closedown(1);
  }

  return now.tv_sec * 1000 + now.tv_usec / 1000;
}

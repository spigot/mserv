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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h> /* inet_ntoa on freebsd */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include "mserv.h"
#include "misc.h"
#include "soundcard.h"
#include "acl.h"
#include "filter.h"
#include "cmd.h"

/*** file-scope (static) function declarations ***/

static void mserv_cmd_moo(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_quit(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_help(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_user(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_pass(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_status(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_userinfo(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_who(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_say(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_emote(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_create(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_remove(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_level(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_password(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_albums(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_tracks(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_ratings(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_queue(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_unqueue(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_play(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_stop(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_next(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_clear(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_repeat(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_random(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_filter(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_factor(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_top(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_pause(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_volume(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_bass(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_treble(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_history(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_rate(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_check(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_search(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_searchf(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_idea(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_info(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_date(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_kick(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_reset(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_sync(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_shutdown(t_client *cl, const char *ru,
			       const char *line);
static void mserv_cmd_gap(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_set(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_x(t_client *cl, const char *ru, const char *line);
static void mserv_cmd_set_author(t_client *cl, const char *ru,
				 const char *line);
static void mserv_cmd_set_name(t_client *cl, const char *ru,
			       const char *line);
static void mserv_cmd_set_genre(t_client *cl, const char *ru,
				const char *line);
static void mserv_cmd_set_year(t_client *cl, const char *ru,
			       const char *line);
static void mserv_cmd_set_albumauthor(t_client *cl, const char *ru,
				      const char *line);
static void mserv_cmd_set_albumname(t_client *cl, const char *ru,
				    const char *line);
static void mserv_cmd_x_authors(t_client *cl, const char *ru,
				const char *line);
static void mserv_cmd_x_authorid(t_client *cl, const char *ru,
				 const char *line);
static void mserv_cmd_x_authorinfo(t_client *cl, const char *ru,
				   const char *line);
static void mserv_cmd_x_authortracks(t_client *cl, const char *ru,
				     const char *line);
static void mserv_cmd_x_genres(t_client *cl, const char *ru,
			       const char *line);
static void mserv_cmd_x_genreid(t_client *cl, const char *ru,
				const char *line);
static void mserv_cmd_x_genreinfo(t_client *cl, const char *ru,
				  const char *line);
static void mserv_cmd_x_genretracks(t_client *cl, const char *ru,
				    const char *line);

static int mserv_cmd_queue_sub(t_client *cl, t_album *album, int n_track,
			       int header);

/*** file-scope (static) globals ***/

/*** externed variables ***/

t_cmds mserv_cmds[] = {
  /* authed_flag, level, name, command, help, syntax */
  { 0, level_guest, "QUIT", mserv_cmd_quit,
    "Quits you from this server",
    "" },
  { 0, level_guest, "HELP", mserv_cmd_help,
    "Displays help",
    "[<command> [<sub-command>]]" },
  { 0, level_guest, "USER", mserv_cmd_user,
    "Informs server of your username",
    "<username>" },
  { 0, level_guest, "PASS", mserv_cmd_pass,
    "Informs server of your password",
    "<password> [<mode: HUMAN, COMPUTER, RTCOMPUTER>]" },
  { 0, level_guest, "DATE", mserv_cmd_date,
    "Display the time and date",
    "" },
  { 1, level_guest, "STATUS", mserv_cmd_status,
    "Displays the current status of the server",
    "" },
  { 1, level_guest, "USERINFO", mserv_cmd_userinfo,
    "Displays information about a particular user",
    "<username>" },
  { 1, level_user, "WHO", mserv_cmd_who,
    "Displays who is currently connected",
    "" },
  { 1, level_user, "SAY", mserv_cmd_say,
    "Say something to the rest of the server",
    "<text>" },
  { 1, level_user, "EMOTE", mserv_cmd_emote,
    "Emote something to the rest of the server",
    "<text>" },
  { 1, level_guest, "ALBUMS", mserv_cmd_albums,
    "List all albums",
    "" },
  { 1, level_guest, "TRACKS", mserv_cmd_tracks,
    "List all tracks belonging to an album",
    "[<album>]" },
  { 1, level_guest, "RATINGS", mserv_cmd_ratings,
    "List ratings for a given track or current track",
    "[<album> <track>]" },
  { 1, level_guest, "QUEUE", mserv_cmd_queue,
    "Show queue contents, add an album (optionally random order) or a track",
    "[<album> [<track>|RANDOM]]" },
  { 1, level_user, "UNQUEUE", mserv_cmd_unqueue,
    "Remove track from the queue",
    "<album> <track>" },
  { 1, level_user, "PLAY", mserv_cmd_play,
    "Start playing queued tracks",
    "" },
  { 1, level_user, "STOP", mserv_cmd_stop,
    "Stop playing current song",
    "" },
  { 1, level_user, "NEXT", mserv_cmd_next,
    "Stop playing current song and play next in queue",
    "" },
  { 1, level_user, "PAUSE", mserv_cmd_pause,
    "Pause current song, use PLAY to resume",
    "" },
  { 1, level_user, "CLEAR", mserv_cmd_clear,
    "Clear the queue",
    "" },
  { 1, level_user, "REPEAT", mserv_cmd_repeat,
    "Repeat the current track by putting it in the queue",
    "" },
  { 1, level_guest, "RANDOM", mserv_cmd_random,
    "Enable, disable or show current random song selection mode",
    "[off|on]" },
  { 1, level_user, "FILTER", mserv_cmd_filter,
    "Limit random play to tracks matching filter",
    "off|<filter>" },
  { 1, level_guest, "FACTOR", mserv_cmd_factor,
    "Set random factor (default 0.6)",
    "<0=play worst, 0.5=play any, 0.99=play best>" },
  { 1, level_guest, "TOP", mserv_cmd_top,
    "Show most likely to be played tracks, default 20 lines",
    "[<lines>]" },
  { 1, level_guest, "VOLUME", mserv_cmd_volume,
    "Display or set the volume (+/- for relative)",
    "[[+|-][<0-100>]]" },
  { 1, level_guest, "BASS", mserv_cmd_bass,
    "Display or set the bass level (+/- for relative)",
    "[[+|-][<0-100>]]" },
  { 1, level_guest, "TREBLE", mserv_cmd_treble,
    "Display or set the treble level (+/- for relative)",
    "[[+|-][<0-100>]]" },
  { 1, level_guest, "HISTORY", mserv_cmd_history,
    "View last 20 tracks",
    "" },
  { 1, level_user, "RATE", mserv_cmd_rate,
    "Rate the current track, a specified track or all tracks in an album",
    "[<album> [<track>]] <AWFUL|BAD|NEUTRAL|GOOD|SUPERB>" },
  { 1, level_user, "CHECK", mserv_cmd_check,
    "Check ratings of same-named tracks and inform of inconsitencies",
    "" },
  { 1, level_guest, "SEARCH", mserv_cmd_search,
    "Search for a track based on string",
    "<string>" },
  { 1, level_guest, "SEARCHF", mserv_cmd_searchf,
    "Search for a track based on filter",
    "<filter>" },
  { 1, level_guest, "INFO", mserv_cmd_info,
    "Display information on the current track, a specified track or an album",
    "[<album> [<track>]]" },
  { 1, level_user, "PASSWORD", mserv_cmd_password,
    "Change your password",
    "<old password> <new password>" },
#ifdef IDEA
  { 1, level_user, "IDEA", mserv_cmd_idea,
    "Store your idea in a file",
    "<string>" },
#endif
  { 1, level_master, "MOO", mserv_cmd_moo,
    "Moo!",
    "" },
  { 1, level_master, "CREATE", mserv_cmd_create,
    "Create a user, or change details if it exists",
    "<username> <password> <GUEST|USER|PRIV|MASTER>" },
  { 1, level_master, "REMOVE", mserv_cmd_remove,
    "Removes a user from the system - WILL PURGE ALL RATINGS!",
    "<username>" },
  { 1, level_master, "LEVEL", mserv_cmd_level,
    "Set a user's user level",
    "<username> <GUEST|USER|PRIV|MASTER>" },
  { 1, level_priv, "KICK", mserv_cmd_kick,
    "Kick a user off the server and prohibit from re-connecting for a bit",
    "<user> [0|<minutes>]" },
  { 1, level_priv, "RESET", mserv_cmd_reset,
    "Clear everything and reload tracks",
    "" },
  { 1, level_priv, "SYNC", mserv_cmd_sync,
    "Ensure track information on disk and in memory are synchronised",
    "" },
  { 1, level_priv, "GAP", mserv_cmd_gap,
    "Set or display the gap between each track, in seconds",
    "[<delay>]" },
  { 1, level_master, "SHUTDOWN", mserv_cmd_shutdown,
    "Shutdown server when server isn't playing a song",
    "" },
  { 1, level_priv, "SET", mserv_cmd_set,
    "Commands for setting track and album information",
    "<params>" },
  { 1, level_guest, "X", mserv_cmd_x,
    "Extra commands for advanced users or computers",
    "<params>" },
  { 0, level_guest, NULL, NULL, NULL, NULL }
};

t_cmds mserv_x_cmds[] = {
  /* authed_flag, level, name, command, help, syntax */
  { 1, level_guest, "AUTHORS", mserv_cmd_x_authors,
    "List all authors on the system",
    "" },
  { 1, level_guest, "AUTHORID", mserv_cmd_x_authorid,
    "Given an author returns the corresponding id",
    "<author name>" },
  { 1, level_guest, "AUTHORINFO", mserv_cmd_x_authorinfo,
    "Returns information about the given author id",
    "<author id>" },
  { 1, level_guest, "AUTHORTRACKS", mserv_cmd_x_authortracks,
    "Displays the tracks written by a given author",
    "<author id>" },
  { 1, level_guest, "GENRES", mserv_cmd_x_genres,
    "Display list of existing genres set in tracks",
    "" },
  { 1, level_guest, "GENREID", mserv_cmd_x_genreid,
    "Given a genre returns the corresponding id",
    "<genre name>" },
  { 1, level_guest, "GENREINFO", mserv_cmd_x_genreinfo,
    "Returns information about the given genre id",
    "<genre id>" },
  { 1, level_guest, "GENRETRACKS", mserv_cmd_x_genretracks,
    "Displays the tracks with given genre",
    "<genre id>" },
  { 0, level_guest, NULL, NULL, NULL, NULL }
};

t_cmds mserv_set_cmds[] = {
  { 1, level_priv, "AUTHOR", mserv_cmd_set_author,
    "Set the author of a track",
    "<album> <track> <author>" },
  { 1, level_priv, "NAME", mserv_cmd_set_name,
    "Set the name of a track",
    "<album> <track> <name>" },
  { 1, level_priv, "GENRE", mserv_cmd_set_genre,
    "Set the genre, or genres, of a track or entire album",
    "<album> [<track>] <genre>[,<genre>]*" },
  { 1, level_priv, "YEAR", mserv_cmd_set_year,
    "Set the year of a track",
    "<album> <track> <0|year>" },
  { 1, level_priv, "ALBUMAUTHOR", mserv_cmd_set_albumauthor,
    "Set the author of an album",
    "<album> <author>" },
  { 1, level_priv, "ALBUMNAME", mserv_cmd_set_albumname,
    "Set the name of an album",
    "<album> <name>" },
  { 0, level_guest, NULL, NULL, NULL, NULL }
};

/*** functions ***/

static void mserv_cmd_x(t_client *cl, const char *ru, const char *line)
{
  t_cmds *cmdsptr;
  int len;

  for (cmdsptr = mserv_x_cmds; cmdsptr->name; cmdsptr++) {
    if (!mserv_checklevel(cl, cmdsptr->userlevel))
      continue;
    len = strlen(cmdsptr->name);
    if (strnicmp(line, cmdsptr->name, len) == 0) {
      if (line[len] != '\0' && line[len] != ' ')
	continue;
      line+= len;
      while(*line == ' ')
	line++;
      if (cmdsptr->authed == 1 && cl->authed == 0)
	mserv_send(cl, "400 Not authenticated\r\n.\r\n", 0);
      else
	cmdsptr->function(cl, ru, line);
      return;
    }
  }
  mserv_response(cl, "BADCOM", NULL);
}

static void mserv_cmd_set(t_client *cl, const char *ru, const char *line)
{
  t_cmds *cmdsptr;
  int len;

  for (cmdsptr = mserv_set_cmds; cmdsptr->name; cmdsptr++) {
    if (!mserv_checklevel(cl, cmdsptr->userlevel))
      continue;
    len = strlen(cmdsptr->name);
    if (strnicmp(line, cmdsptr->name, len) == 0) {
      if (line[len] != '\0' && line[len] != ' ')
	continue;
      line+= len;
      while(*line == ' ')
	line++;
      if (cmdsptr->authed == 1 && cl->authed == 0)
	mserv_send(cl, "404 Not authenticated\r\n.\r\n", 0);
      else
	cmdsptr->function(cl, ru, line);
      return;
    }
  }
  mserv_response(cl, "BADCOM", NULL);
}

static void mserv_cmd_moo(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  (void)line;
  mserv_response(cl, "MOO", NULL);
  if (cl->mode == mode_human) {
    mserv_send(cl, "[]   __  __          _\r\n", 0);
    mserv_send(cl, "[]  |  \\/  |___  ___| |\r\n", 0);
    mserv_send(cl, "[]  | |\\/| / _ \\/ _ \\_|\r\n", 0);
    mserv_send(cl, "[]  |_|  |_\\___/\\___(_)\r\n", 0);
  }
}

static void mserv_cmd_quit(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  (void)line;
  mserv_response(cl, "QUIT", NULL);
  mserv_close(cl);
}

static void mserv_cmd_help(t_client *cl, const char *ru, const char *line)
{
  t_cmds *cmdsptr;
  int i = 0;
  char buffer[1024];

  (void)ru;
  if ((*line == 'x' || *line == 'X') &&
      (*(line+1) == '\0' || *(line+1) == ' ')) {
    line++;
    while (*line == ' ')
      line++;
    cmdsptr = mserv_x_cmds;
  } else if (!strnicmp(line, "set", 3) && (*(line+3) == '\0' ||
					   *(line+3) == ' ')) {
    line+= 3;
    while (*line == ' ')
      line++;
    cmdsptr = mserv_set_cmds;
  } else {
    cmdsptr = mserv_cmds;
  }

  if (*line) {
    for (; cmdsptr->name; cmdsptr++) {
      if (cmdsptr->authed && !cl->authed)
	continue;
      if (!mserv_checklevel(cl, cmdsptr->userlevel))
	continue;
      if (strnicmp(line, cmdsptr->name, strlen(cmdsptr->name)) == 0) {
	if (line[strlen(cmdsptr->name)] != '\0')
	  continue;
	if (cl->mode == mode_human) {
	  snprintf(buffer, 1024, "[] %s\r\n[] Syntax: %s%s %s\r\n",
		   cmdsptr->help, (cmdsptr == mserv_x_cmds ? "X " : 
				   (cmdsptr == mserv_set_cmds ? "SET" : "")),
		   cmdsptr->name, cmdsptr->syntax);
	  mserv_send(cl, buffer, 0);
	} else {
	  mserv_responsent(cl, "HELP", "%s", cmdsptr->name);
	  snprintf(buffer, 1024, "%s\r\nSyntax: %s%s %s\r\n.\r\n",
		   cmdsptr->help, (cmdsptr == mserv_x_cmds ? "X " : 
				   (cmdsptr == mserv_set_cmds ? "SET" : "")),
		   cmdsptr->name, cmdsptr->syntax);
	  mserv_send(cl, buffer, 0);
	}
	return;
      }
    }
    mserv_response(cl, "NOHELP", "%s", line);
    return;
  }
  if (cmdsptr == mserv_x_cmds)
    mserv_responsent(cl, "HELPCX", "%s", VERSION);
  else
    mserv_responsent(cl, "HELPC", "%s", VERSION);
  if (cl->mode == mode_human) {
    mserv_send(cl, "[] Commands:\r\n[]     ", 0);
    for (; cmdsptr->name; cmdsptr++) {
      if (cmdsptr->authed && !cl->authed)
	continue;
      if (!mserv_checklevel(cl, cmdsptr->userlevel))
	continue;
      if (i > 4) {
	i = 0;
	mserv_send(cl, "\r\n[]     ", 0);
      }
      mserv_send(cl, cmdsptr->name, 0);
      if (strlen(cmdsptr->name) < 14) {
	mserv_send(cl, "                ", 14-strlen(cmdsptr->name));
      } else {
	mserv_send(cl, " ", 1);
      }
      i++;
    }
    mserv_send(cl, "\r\n", 0);
    if (cmdsptr == mserv_cmds) {
      mserv_send(cl, "[] To report bugs in the implementation please "
		 "send email to\r\n", 0);
      mserv_send(cl, "[]    james@squish.net\r\n", 0);
      mserv_send(cl, "[] Thanks for using this program.\r\n", 0);
    }
  } else {
    for (cmdsptr = mserv_cmds; cmdsptr->name; cmdsptr++) {
      if (cmdsptr->authed && !cl->authed)
	continue;
      if (!mserv_checklevel(cl, cmdsptr->userlevel))
	continue;
      mserv_send(cl, cmdsptr->name, 0);
      mserv_send(cl, "\r\n", 0);
    }
    mserv_send(cl, ".\r\n", 0);
  }
}

static void mserv_cmd_user(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  if (!*line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  if (cl->authed) {
    mserv_response(cl, "AAUTHED", NULL);
    return;
  }
  strncpy(cl->user, line, USERNAMELEN);
  cl->user[USERNAMELEN] = '\0';
  mserv_response(cl, "USEROK", NULL);
}

static void mserv_cmd_pass(t_client *cl, const char *ru, const char *line)
{
  char password[16];
  char *s;
  t_acl *acl;

  (void)ru;
  if (!*line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  if (!cl->user[0]) {
    mserv_response(cl, "NOUSER", NULL);
    return;
  }
  if (cl->authed) {
    mserv_response(cl, "AAUTHED", NULL);
    return;
  }
  if (!(s = strchr(line, ' '))) {
    strncpy(password, line, 15);
    password[15] = '\0';
    cl->mode = mode_human;
  } else {
    if ((s - line) > 15) {
      strncpy(password, line, 15);
      password[15] = '\0';
    } else {
      strncpy(password, line, s-line);
      password[s-line] = '\0';
    }
    s++;
    if (!stricmp("HUMAN", s)) {
      cl->mode = mode_human;
    } else if (!stricmp("COMPUTER", s)) {
      cl->mode = mode_computer;
    } else if (!stricmp("RTCOMPUTER", s)) {
      cl->mode = mode_rtcomputer;
    } else {
      mserv_response(cl, "BADMODE", NULL);
      return;
    }
  }
  if (acl_checkpassword(cl->user, password, &acl) == -1) {
    mserv_response(cl, "ACLFAIL", NULL);
    return;
  }
  if (acl->nexttime > time(NULL)) {
    mserv_response(cl, "GOAWAY", NULL);
    return;
  }
  if (cl->mode != mode_computer)
    mserv_broadcast("CONNECT", "%s", cl->user);
  cl->authed = 1;
  cl->userlevel = acl->userlevel;
  mserv_response(cl, "ACLSUCC", "%s", cl->user);
  if (cl->userlevel != level_guest && cl->mode != mode_computer)
    mserv_recalcratings();
  return;
}

static void mserv_cmd_status(t_client *cl, const char *ru, const char *line)
{
  char token[16];
  char *a;
  int i;
  time_t ago;

  (void)ru;
  (void)line;
  if (mserv_playing.track) {
    a = mserv_paused ? "STATPAU" : "STATPLA";
    ago = time(NULL)-mserv_playingstart;
    mserv_response(cl, a, "%s\t%d\t%d\t%d\t%d\t%s\t%s\t%d\t%d:%02d\t%s\t%.2f",
		   mserv_getfilter(), mserv_filter_ok, mserv_filter_notok,
		   mserv_playing.track->n_album,
		   mserv_playing.track->n_track,
		   mserv_playing.track->author,
		   mserv_playing.track->name, mserv_playingstart,
		   ago/60, ago % 60,
		   mserv_random ? "ON" : "OFF", mserv_factor);
    if (cl->mode == mode_human) {
      for (i = 1; i <= 7; i++) {
	sprintf(token, "STAT%d", i);
	mserv_response(cl, token, "%s\t%d\t%d\t%d\t%d\t%s\t%s\t%d\t%d:%02d"
		       "\t%s\t%.2f",
		       mserv_getfilter(), mserv_filter_ok, mserv_filter_notok,
		       mserv_playing.track->n_album,
		       mserv_playing.track->n_track,
		       mserv_playing.track->author,
		       mserv_playing.track->name, mserv_playingstart,
		       ago/60, ago % 60,
		       mserv_random ? "ON" : "OFF", mserv_factor);
      }
    }
  } else {
    mserv_response(cl, "STATN", "%s\t%d\t%d\t%s\t%.2f",
		   mserv_getfilter(), mserv_filter_ok, mserv_filter_notok,
		       mserv_random ? "ON" : "OFF", mserv_factor);
    if (cl->mode == mode_human) {
      for (i = 1; i <= 4; i++) {
	sprintf(token, "STATN%d", i);
	mserv_response(cl, token, "%s\t%d\t%d\t%s\t%.2f",
		       mserv_getfilter(), mserv_filter_ok, mserv_filter_notok,
		       mserv_random ? "ON" : "OFF", mserv_factor);
      }
    }
  }
}

static void mserv_cmd_userinfo(t_client *cl, const char *ru, const char *line)
{
  t_acl *acl;
  t_track *track;
  t_rating *rate1, *rate2;
  unsigned int total = 0, heard = 0, unheard = 0;
  unsigned int awful = 0, bad = 0, neutral = 0, good = 0, superb = 0;
  char token[16];
  unsigned int diffs = 0, indexnum = 0;
  double index;
  int i;

  if (!*line)
    line = cl->user;
  for (acl = mserv_acl; acl; acl = acl->next) {
    if (!stricmp(acl->user, line)) {
      break;
    }
  }
  if (!acl) {
    mserv_response(cl, "NOUSER", NULL);
    return;
  }
  for (track = mserv_tracks; track; track = track->next) {
    total++;
    rate1 = mserv_getrate(acl->user, track);
    if (rate1) {
      switch(rate1->rating) {
      case 0: heard++; break;
      case 1: awful++; break;
      case 2: bad++; break;
      case 3: neutral++; break;
      case 4: good++; break;
      case 5: superb++; break;
      default: break;
      }
    } else {
      unheard++;
    }
    rate2 = mserv_getrate(ru, track);
    if (rate1 && rate2 && rate1->rating && rate2->rating) {
      diffs+= abs(rate1->rating - rate2->rating);
      indexnum++;
    }
  }
  if (indexnum)
    index = 1.0 - diffs/(double)(4*indexnum);
  else
    index = 0;
  mserv_response(cl, "INFU", "%s\t%s\t"
		 "%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t"
		 "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%.1f",
		 acl->user, ru, 100*superb/(double)total,
		 100*good/(double)total, 100*neutral/(double)total,
		 100*bad/(double)total, 100*awful/(double)total,
		 100*heard/(double)total, 100*unheard/(double)total,
		 superb, good, neutral, bad, awful, heard, unheard,
		 100*index);
  if (cl->mode == mode_human) {
    for (i = 1; i <= 8; i++) {
      sprintf(token, "INFU%d", i);
      mserv_response(cl, token, "%s\t%s\t"
		     "%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t"
		     "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%.1f",
		     acl->user, ru, 100*superb/(double)total,
		     100*good/(double)total, 100*neutral/(double)total,
		     100*bad/(double)total, 100*awful/(double)total,
		     100*heard/(double)total, 100*unheard/(double)total,
		     superb, good, neutral, bad, awful, heard, unheard,
		     100*index);
    }
  }
}

static void mserv_cmd_who(t_client *cl, const char *ru, const char *line)
{
  t_client *clu;
  char tmp[256];
  int total = 0;
  int connected = 0;

  (void)ru;
  (void)line;
  mserv_responsent(cl, "WHO", NULL);

  if (cl->mode == mode_human) {
    mserv_send(cl, "[]   Name                         Flags Idle Site\r\n", 0);
    for (clu = mserv_clients; clu; clu = clu->next) {
      sprintf(tmp, "[]   %-28s %1.1s     %4s %s\r\n",
	      (clu->authed ? (const char *)clu->user : "[connecting]"),
	      mserv_levelstr(clu->userlevel),
	      mserv_idletxt(time(NULL) - clu->lastread),
	      inet_ntoa(clu->sin.sin_addr));
      mserv_send(cl, tmp, 0);
      if (clu->authed)
	connected++;
      total++;
    }
    sprintf(tmp, "[] %d connected, %d total\r\n", connected, total);
    mserv_send(cl, tmp, 0);
  } else {
    for (clu = mserv_clients; clu; clu = clu->next) {
      sprintf(tmp, "%s\t%s\t%d\t%s\r\n",
	      (clu->authed ? (const char *)clu->user : ""),
	      mserv_levelstr(clu->userlevel),
	      (unsigned int)(time(NULL) - clu->lastread),
	      inet_ntoa(clu->sin.sin_addr));
      mserv_send(cl, tmp, 0);
    }
    mserv_send(cl, ".\r\n", 0);
  }
}

static void mserv_cmd_password(t_client *cl, const char *ru, const char *line)
{
  char linespl[LINEBUFLEN];
  t_acl *acl;
  char *str[3];

  (void)ru;
  strcpy(linespl, line);

  if (mserv_split(str, 2, linespl, " ") != 2) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  if (acl_checkpassword(cl->user, str[0], &acl) == -1) {
    mserv_response(cl, "ACLFAIL", NULL);
    return;
  }
  strncpy(acl->pass, acl_crypt(str[1]), PASSWORDLEN);
  acl->pass[PASSWORDLEN] = '\0';
  mserv_response(cl, "PASSCHG", NULL);
  acl_save();
}

static void mserv_cmd_level(t_client *cl, const char *ru, const char *line)
{
  char linespl[LINEBUFLEN];
  char *str[3];
  t_acl *acl;
  t_userlevel *ul;
  
  (void)ru;
  strcpy(linespl, line);

  if (mserv_split(str, 2, linespl, " ") != 2) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  for (acl = mserv_acl; acl; acl = acl->next) {
    if (!stricmp(acl->user, str[0])) {
      break;
    }
  }
  if (!acl) {
    mserv_response(cl, "NOUSER", NULL);
    return;
  }
  if ((ul = mserv_strtolevel(str[1])) == NULL) {
    mserv_response(cl, "BADLEV", NULL);
    return;
  }
  acl->userlevel = *ul;
  acl_save();
  mserv_response(cl, "USERCHG", NULL);
}

static void mserv_cmd_create(t_client *cl, const char *ru, const char *line)
{
  /* usernames must never start _ or have a : in them */
  char linespl[LINEBUFLEN];
  char *str[4];
  char *p;
  char c;
  t_acl *acl, *acli;
  t_userlevel *ul;

  (void)ru;
  strcpy(linespl, line);
  
  if (mserv_split(str, 3, linespl, " ") != 3) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  for (p = str[0]; (c = *p++); ) {
    if (!isalnum((signed int)c)) {
      mserv_response(cl, "BADUSER", NULL);
      return;
    }
  }
  if ((ul = mserv_strtolevel(str[2])) == NULL) {
    mserv_response(cl, "BADLEV", NULL);
    return;
  }
  for (acl = mserv_acl; acl; acl = acl->next) {
    if (!stricmp(acl->user, str[0])) {
      break;
    }
  }
  if (acl) {
    mserv_response(cl, "USERCHG", NULL);
  } else {
    if ((acl = malloc(sizeof(t_acl))) == NULL) {
      mserv_log("Out of memory adding to ACL");
      mserv_response(cl, "MEMORYR", NULL);
      return;
    }
    acl->next = NULL;
    strncpy(acl->user, str[0], USERNAMELEN);
    acl->user[USERNAMELEN] = '\0';
    mserv_response(cl, "USERADD", NULL);
    for (acli = mserv_acl; acli && acli->next; acli = acli->next);
    if (!acli)
      mserv_acl = acl;
    else
      acli->next = acl;
  }
  strncpy(acl->pass, acl_crypt(str[1]), PASSWORDLEN);
  acl->pass[PASSWORDLEN] = '\0';
  acl->userlevel = *ul;
  acl_save();
}

static void mserv_cmd_remove(t_client *cl, const char *ru, const char *line)
{
  t_acl *acl, *aclp;
  t_track *track;
  t_rating *rate, *rp;

  (void)ru;
  for (aclp = NULL, acl = mserv_acl; acl; aclp = acl, acl = acl->next) {
    if (!stricmp(acl->user, line)) {
      break;
    }
  }
  if (!acl) {
    mserv_response(cl, "NOUSER", NULL);
    return;
  }
  if (aclp)
    aclp->next = acl->next;
  else
    mserv_acl = acl->next;
  for (track = mserv_tracks; track; track = track->next) {
    for (rp = NULL, rate = track->ratings; rate;
	 rp = rate, rate = rate->next) {
      if (!stricmp(rate->user, acl->user)) {
	break;
      }
    }
    if (!rate)
      continue;
    if (rp)
      rp->next = rate->next;
    else
      track->ratings = rate->next;
    free(rate);
    track->modified = 1;
  }
  free(acl);
  acl_save();
  mserv_savechanges();
  mserv_response(cl, "REMOVED", NULL);
}

static void mserv_cmd_albums(t_client *cl, const char *ru, const char *line)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  t_album *album;

  (void)ru;
  (void)line;
  mserv_responsent(cl, "ALBUMS", NULL);
  if (cl->mode == mode_human) {
    for (album = mserv_albums; album; album = album->next) {
      sprintf(buffer, "[] %3d %-20.20s %-51.51s\r\n", album->id, album->author,
	      album->name);
      mserv_send(cl, buffer, 0);
    }
  } else {
    for (album = mserv_albums; album; album = album->next) {
      sprintf(buffer, "%d\t%s\t%s\r\n", album->id, album->author, album->name);
      mserv_send(cl, buffer, 0);
    }
    mserv_send(cl, ".\r\n", 0);
  }
}

static void mserv_cmd_tracks(t_client *cl, const char *ru, const char *line)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  t_album *album;
  unsigned int id;
  int i;
  char *end;
  char bit[32];
  t_rating *rate;

  if (!*line) {
    if (mserv_playing.track == NULL) {
      mserv_response(cl, "NOTHING", NULL);
      return;
    }
    id = mserv_playing.track->n_album;
  } else {
    id = strtol(line, &end, 10);
    if (!*line || *end) {
      mserv_response(cl, "NAN", NULL);
      return;
    }
  }
  if ((album = mserv_getalbum(id)) == NULL) {
    mserv_response(cl, "NOALBUM", NULL);
    return;
  }
  mserv_responsent(cl, "TRACKS", "%d\t%s\t%s", album->id, album->author,
		   album->name);
  for (i = 0; i < TRACKSPERALBUM; i++) {
    if (album->tracks[i]) {
      rate = mserv_getrate(ru, album->tracks[i]);
      sprintf(bit, "%d/%d", album->tracks[i]->n_album,
	      album->tracks[i]->n_track);
      if (cl->mode == mode_human) {
	sprintf(buffer, "[] %6.6s %-1.1s %-20.20s %-40.40s %2ld:%02ld\r\n",
		bit, rate && rate->rating ? mserv_ratestr(rate) : "-",
		album->tracks[i]->author, album->tracks[i]->name,
		(album->tracks[i]->duration / 100) / 60,
		(album->tracks[i]->duration / 100) % 60);
	mserv_send(cl, buffer, 0);
      } else {
	sprintf(buffer, "%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
		album->tracks[i]->n_album, album->tracks[i]->n_track,
		album->tracks[i]->author, album->tracks[i]->name,
		mserv_ratestr(rate),
		(album->tracks[i]->duration / 100) / 60,
		(album->tracks[i]->duration / 100) % 60);
	mserv_send(cl, buffer, 0);
      }
    }
  }
  if (cl->mode != mode_human)
    mserv_send(cl, ".\r\n", 0);
}

static void mserv_cmd_ratings(t_client *cl, const char *ru, const char *line)
{
  char linespl[1024];
  char buffer[AUTHORLEN+NAMELEN+64];
  t_rating *rate;
  t_track *track;
  unsigned int n_track, n_album;
  char *str[3];
  char *end;
  struct tm curtm;
  char date_format[64];
  char date[64];

  (void)ru;
  strcpy(linespl, line);

  if (!*line) {
    if ((track = mserv_playing.track) == NULL) {
      mserv_response(cl, "NOTHING", NULL);
      return;
    }
  } else {
    if (mserv_split(str, 2, linespl, " ") != 2) {
      mserv_response(cl, "BADPARM", NULL);
      return;
    }
    n_album = strtol(str[0], &end, 10);
    if (!*str[0] || *end) {
      mserv_response(cl, "NAN", NULL);
      return;
    }
    n_track = strtol(str[1], &end, 10);
    if (!*str[1] || *end) {
      mserv_response(cl, "NAN", NULL);
      return;
    }
    if ((track = mserv_gettrack(n_album, n_track)) == NULL) {
      mserv_response(cl, "NOTRACK", NULL);
      return;
    }
  }
  mserv_responsent(cl, "RATINGS", "%d\t%d\t%s\t%s", track->n_album,
		   track->n_track, track->author, track->name);
  for (rate = track->ratings; rate; rate = rate->next) {
    if (cl->mode == mode_human) {
      if (rate->rating == 0)
	continue;
      curtm = *localtime(&rate->when);
      /* Wednesday 1st January 1998 */
      if ((unsigned int)snprintf(date_format, 64, "%d%s %%B %%Y",
				 curtm.tm_mday,
				 mserv_stndrdth(curtm.tm_mday)) >= 64 ||
	  strftime(date, 64, date_format, &curtm) == 0) {
	break;
      }
      sprintf(buffer, "[]   %-15.15s %10.10s - %s\r\n", rate->user,
	      mserv_ratestr(rate), date);
      mserv_send(cl, buffer, 0);
    } else {
      sprintf(buffer, "%s\t%s\t%lu\r\n", rate->user,
	      mserv_ratestr(rate), (unsigned long int)rate->when);
      mserv_send(cl, buffer, 0);
    }
  }
  if (cl->mode != mode_human)
    mserv_send(cl, ".\r\n", 0);
}

static void mserv_cmd_unqueue(t_client *cl, const char *ru, const char *line)
{
  char linespl[LINEBUFLEN];
  char *str[3];
  unsigned int n_album, n_track;
  char *end;
  t_track *track;
  t_queue *p, *q;

  (void)ru;
  strcpy(linespl, line);

  if (mserv_split(str, 2, linespl, " ") != 2) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_album = strtol(str[0], &end, 10);
  if (!*str[0] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  n_track = strtol(str[1], &end, 10);
  if (!*str[1] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  if ((track = mserv_gettrack(n_album, n_track)) == NULL) {
    mserv_response(cl, "NOTRACK", NULL);
    return;
  }
  /* no, I refuse to use pointers to pointers */
  for (p = NULL, q = mserv_queue; q; p = q, q = q->next) {
    if (q->supinfo.track == track)
      break;
  }
  if (q) {
    mserv_broadcast("UNQ", "%s\t%d\t%d\t%s\t%s", cl->user,
		    q->supinfo.track->n_album, q->supinfo.track->n_track,
		    q->supinfo.track->author, q->supinfo.track->name);
    if (cl->mode != mode_human)
      mserv_response(cl, "UNQR", "%d\t%d\t%s\t%s",
		     q->supinfo.track->n_album, q->supinfo.track->n_track,
		     q->supinfo.track->author, q->supinfo.track->name);
    if (p)
      p->next = q->next;
    else
      mserv_queue = q->next;
    free(q);
    return;
  }
  mserv_response(cl, "NOUNQ", NULL);
}

static void mserv_cmd_queue(t_client *cl, const char *ru, const char *line)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  char bit[32];
  char linespl[LINEBUFLEN];
  char *str[4];
  char *end;
  t_album *album;
  t_queue *q;
  unsigned int n_album, n_track;
  int p;
  unsigned int i, j, k, numtracks;
  t_rating *rate;
  int random, mode;
  unsigned char tracktally[TRACKSPERALBUM];

  if (!*line) {
    if (!mserv_queue) {
      mserv_response(cl, "NOQUEUE", NULL);
      return;
    }
    mserv_responsent(cl, "SHOW", NULL);
    for (q = mserv_queue; q; q = q->next) {
      rate = mserv_getrate(ru, q->supinfo.track);
      if (cl->mode == mode_human) {
	sprintf(bit, "%d/%d", q->supinfo.track->n_album,
		q->supinfo.track->n_track);
	sprintf(buffer, "[] %-10.10s %6.6s %-1.1s %-20.20s "
		"%-30.30s%2ld:%02ld\r\n",
		q->supinfo.user, bit,
		rate && rate->rating ? mserv_ratestr(rate) : "-",
		q->supinfo.track->author, q->supinfo.track->name,
		(q->supinfo.track->duration / 100) / 60,
		(q->supinfo.track->duration / 100) % 60);
	mserv_send(cl, buffer, 0);
      } else {
	sprintf(buffer, "%s\t%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
		q->supinfo.user, q->supinfo.track->n_album,
		q->supinfo.track->n_track, q->supinfo.track->author,
		q->supinfo.track->name,	mserv_ratestr(rate),
		(q->supinfo.track->duration / 100) / 60,
		(q->supinfo.track->duration / 100) % 60);
	mserv_send(cl, buffer, 0);
      }
    }
    if (cl->mode != mode_human)
      mserv_send(cl, ".\r\n", 0);
    return;
  }
  if (cl->userlevel == level_guest) {
    mserv_response(cl, "ACLFAIL", NULL);
    return;
  }
  if (mserv_shutdown) {
    mserv_response(cl, "SHUTNQ", NULL);
    return;
  }
  strcpy(linespl, line);
  if ((p = mserv_split(str, 2, linespl, " ")) < 1) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_album = strtol(str[0], &end, 10);
  if (*end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  if ((album = mserv_getalbum(n_album)) == NULL) {
    mserv_response(cl, "NOALBUM", NULL);
    return;
  }
  mode = 0; /* add track */
  random = 0;
  if (p == 2) {
    if (!stricmp(str[1], "RANDOM")) {
      random = 1;
      mode = 1; /* add album */
    }
  } else if (p == 1) {
    mode = 1; /* add album */
  }
  if (mode == 1) {
    /* add album */
    mserv_broadcast("QADD", "%s", cl->user);
    if (cl->mode != mode_human)
      mserv_responsent(cl, "QADDR", NULL);
    if (random) {
      numtracks = 0;
      for (i = 0; i < TRACKSPERALBUM; i++) {
	if (album->tracks[i]) {
	  numtracks++;
	  tracktally[i] = 0;
	} else {
	  tracktally[i] = 1;
	}
      }
      for (i = 0; i < numtracks; i++) {
	mserv_log("LOOP: %d of %d", i, numtracks);
	j = ((double)(numtracks-i))*rand()/(RAND_MAX+1.0);
	mserv_log("  -->%d", j);
	for (k = 0; k < TRACKSPERALBUM; k++) {
	  if (!tracktally[k]) {
	    if (!j--)
	      break;
	  }
	}
	if (k >= TRACKSPERALBUM || tracktally[k]) {
	  /* should never happen */
	  mserv_log("Internal error in randomising album (%d)", k);
	  exit(1);
	}
	tracktally[k] = 1;
	mserv_cmd_queue_sub(cl, album, k+1, 0);
      }	
    } else {     
      for (i = 0; i < TRACKSPERALBUM; i++) {
	if (album->tracks[i])
	  mserv_cmd_queue_sub(cl, album, i+1, 0);
      }
    }
    if (cl->mode != mode_human)
      mserv_send(cl, ".\r\n", 0);
    return;
  }
  /* add track */
  n_track = strtol(str[1], &end, 10);
  if (*end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  if (n_track < 1 || !album->tracks[n_track-1]) {
    mserv_response(cl, "NOTRACK", NULL);
    return;
  }
  if (mserv_cmd_queue_sub(cl, album, n_track, 1) == -1) {
    mserv_response(cl, "QNONE", NULL);
    return;
  }
  if (cl->mode != mode_human)
    mserv_send(cl, ".\r\n", 0);
}

static int mserv_cmd_queue_sub(t_client *cl, t_album *album, int n_track,
			       int header)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  char bit[32];
  t_rating *rate;
  t_client *client;
  int i = n_track-1;
  static t_lang *lang = NULL;

  if (!lang && (lang = mserv_gettoken("QADDTR")) == NULL) {
    mserv_log("Failed to find language token QADDTR");
    exit(1);
  }
  if (!album->tracks[i] || mserv_addqueue(cl, album->tracks[i]) == -1)
    return -1;
  if (header) {
    mserv_broadcast("QADD", "%s", cl->user);
    if (cl->mode != mode_human)
      mserv_responsent(cl, "QADDR", NULL);
  }
  if (cl->mode != mode_human) {
    rate = mserv_getrate(cl->user, album->tracks[i]);
    sprintf(buffer, "%s\t%d\t%d\t%s\t%s\t%s\r\n", cl->user,
	    album->tracks[i]->n_album, album->tracks[i]->n_track,
	    album->tracks[i]->author, album->tracks[i]->name,
	    mserv_ratestr(rate));
    mserv_send(cl, buffer, 0);
  }
  sprintf(bit, "%d/%d", album->tracks[i]->n_album,
	  album->tracks[i]->n_track);
  for (client = mserv_clients; client; client = client->next) {
    rate = mserv_getrate(client->user, album->tracks[i]);
    if (client->mode == mode_human) {
      sprintf(buffer, "[] %-10.10s %6.6s %-1.1s %-20.20s "
	      "%-30.30s%2ld:%02ld\r\n", cl->user, bit,
	      rate && rate->rating ? mserv_ratestr(rate) : "-",
	      album->tracks[i]->author, album->tracks[i]->name,
	      (album->tracks[i]->duration / 100) / 60,
	      (album->tracks[i]->duration / 100) % 60);
      mserv_send(client, buffer, 0);
    } else if (client->mode == mode_rtcomputer) {
      sprintf(buffer, "=%d\t%s\t%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
	      lang->code, cl->user, album->tracks[i]->n_album,
	      album->tracks[i]->n_track, album->tracks[i]->author,
	      album->tracks[i]->name, mserv_ratestr(rate),
	      (album->tracks[i]->duration / 100) / 60,
	      (album->tracks[i]->duration / 100) % 60);
      mserv_send(client, buffer, 0);
    }
  }
  return 0;
}

static void mserv_cmd_play(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  (void)line;
  if (mserv_playing.track && mserv_paused) {
    mserv_resumeplay();
    mserv_broadcast("RESUME", "%s\t%d\t%d\t%s\t%s", cl->user,
		    mserv_playing.track->n_album,
		    mserv_playing.track->n_track, mserv_playing.track->author,
		    mserv_playing.track->name);
    if (cl->mode != mode_human) {
      /* humans will already have seen broadcast */
      mserv_response(cl, "STARTED", "%d\t%d\t%s\t%s",
		     mserv_playing.track->n_album,
		     mserv_playing.track->n_track,
		     mserv_playing.track->author,
		     mserv_playing.track->name);
    }
    return;
  }

  if (mserv_playing.track) {
    mserv_response(cl, "ALRPLAY", "%d\t%d\t%s\t%s",
		   mserv_playing.track->n_album,
		   mserv_playing.track->n_track, mserv_playing.track->author,
		   mserv_playing.track->name);
    return;
  }
  if (mserv_playnext()) {
    mserv_response(cl, "NOMORE", NULL);
  } else {
    if (cl->mode != mode_human) {
      /* humans will already have seen broadcast */
      mserv_response(cl, "STARTED", "%d\t%d\t%s\t%s",
		     mserv_playing.track->n_album,
		     mserv_playing.track->n_track, mserv_playing.track->author,
		     mserv_playing.track->name);
    }
  }
}

static void mserv_cmd_stop(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  (void)line;
  if (mserv_playing.track) {
    mserv_broadcast("STOPPED", "%s\t%d\t%d\t%s\t%s", cl->user,
		    mserv_playing.track->n_album, mserv_playing.track->n_track,
		    mserv_playing.track->author, mserv_playing.track->name);
    if (cl->mode != mode_human) {
      /* humans will already have seen broadcast */
      mserv_response(cl, "STOP", "%d\t%d\t%s\t%s",
		     mserv_playing.track->n_album,
		     mserv_playing.track->n_track,
		     mserv_playing.track->author, mserv_playing.track->name);
    }
    mserv_abortplay();
  } else {
    mserv_response(cl, "NOTHING", NULL);
  }
}

static void mserv_cmd_pause(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  (void)line;
  if (mserv_paused) {
    mserv_response(cl, "APAUSED", NULL);
    return;
  }
  if (mserv_playing.track || mserv_playnextat) {
    mserv_pauseplay(cl);
    if (cl->mode != mode_human) {
      /* humans will already have seen broadcast */
      mserv_response(cl, "PAUSED", NULL);
    }
  } else {
    mserv_response(cl, "NOTHING", NULL);
  }
}

static void mserv_cmd_next(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  (void)line;
  if (mserv_playing.track)
    mserv_broadcast("SKIP", "%s", cl->user);
  if (mserv_playnext()) {
    mserv_broadcast("FINISH", NULL);
    if (cl->mode != mode_human)
      mserv_response(cl, "NOMORE", NULL);
  } else {
    if (cl->mode != mode_human) {
      /* humans will already have seen broadcast */
      mserv_response(cl, "NEXT", "%d\t%d\t%s\t%s",
		     mserv_playing.track->n_album,
		     mserv_playing.track->n_track, mserv_playing.track->author,
		     mserv_playing.track->name);
    }
  }
}

static void mserv_cmd_clear(t_client *cl, const char *ru, const char *line)
{
  t_queue *q, *n;

  (void)ru;
  (void)line;
  if (!mserv_queue) {
    mserv_response(cl, "ACLEAR", NULL);
    return;
  }
  for (q = mserv_queue, q ? n = q->next : NULL;
       q;
       q = n, n = n ? n->next : NULL) {
    free(q);
  }
  mserv_queue = NULL;
  mserv_broadcast("CLEAR", "%s", cl->user);
  if (cl->mode != mode_human)
    mserv_response(cl, "CLEARR", NULL);
}

static void mserv_cmd_repeat(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  (void)line;
  if (!mserv_playing.track) {
    mserv_response(cl, "NOTHING", NULL);
    return;
  }
  if (mserv_shutdown) {
    mserv_response(cl, "SHUTNQ", NULL);
    return;
  }
  if (!mserv_addqueue(cl, mserv_playing.track)) {
    mserv_broadcast("REPEAT", "%s\t%d\t%d\t%s\t%s", cl->user,
		    mserv_playing.track->n_album, mserv_playing.track->n_track,
		    mserv_playing.track->author, mserv_playing.track->name);
    if (cl->mode != mode_human)
      mserv_response(cl, "REPEAT", "%s\t%d\t%d\t%s\t%s", cl->user,
		     mserv_playing.track->n_album,
		     mserv_playing.track->n_track,
		     mserv_playing.track->author, mserv_playing.track->name);
  } else {
    mserv_response(cl, "QNONE", NULL);
  }
}

static void mserv_cmd_random(t_client *cl, const char *ru, const char *line)
{
  char linespl[LINEBUFLEN];
  char *str[3];

  (void)ru;
  if (!*line) {
    mserv_response(cl, "RANCUR", "%s", mserv_random ? "on" : "off");
    return;
  }
  if (cl->userlevel == level_guest) {
    mserv_response(cl, "ACLFAIL", NULL);
    return;
  }
  strcpy(linespl, line);
  if (mserv_split(str, 1, linespl, " ") == 0) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  if (!stricmp(str[0], "on")) {
    if (mserv_random == 1) {
      mserv_response(cl, "ARANDOM", NULL);
      return;
    }
    mserv_random = 1;
    mserv_broadcast("RANDOM1", "%s", cl->user);
    if (cl->mode != mode_human)
      mserv_response(cl, "RANDOM", NULL);
  } else if (!stricmp(line, "off")) {
    if (!mserv_random) {
      mserv_response(cl, "ARANDOM", NULL);
      return;
    }
    mserv_random = 0;
    mserv_broadcast("RANDOM0", "%s", cl->user);
    if (cl->mode != mode_human)
      mserv_response(cl, "RANDOM", NULL);
  } else
    mserv_response(cl, "BADPARM", NULL);
}

static void mserv_cmd_filter(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  if (!*line) {
    mserv_response(cl, "CURFILT", "%s\t%s\t%d\t%d", "", mserv_getfilter(),
		   mserv_filter_ok, mserv_filter_notok);
    return;
  }
  if (cl->userlevel == level_guest) {
    mserv_response(cl, "ACLFAIL", NULL);
    return;
  }
  if (!stricmp(line, "off")) {
    mserv_setfilter("");
    mserv_broadcast("NFILTER", "%s", cl->user);
    mserv_recalcratings();
  } else {
    if (mserv_setfilter(line) == -1) {
      mserv_response(cl, "BADFILT", NULL);
      return;
    }
    mserv_recalcratings();
    mserv_broadcast("FILTER", "%s\t%s\t%d\t%d", cl->user, mserv_getfilter(),
		    mserv_filter_ok, mserv_filter_notok);
  }
  if (cl->mode != mode_human)
    mserv_response(cl, "FILTR", "%d\t%d", mserv_filter_ok,
		   mserv_filter_notok);
}

static void mserv_cmd_factor(t_client *cl, const char *ru, const char *line)
{
  double f = atof(line);

  (void)ru;
  if (!*line) {
    mserv_response(cl, "FACTCUR", "%.2f", mserv_factor);
    return;
  }
  if (cl->userlevel == level_guest) {
    mserv_response(cl, "ACLFAIL", NULL);
    return;
  }
  if (f < 0 || f > 0.99) {
    mserv_response(cl, "BADFACT", NULL);
    return;
  }
  mserv_factor = (double)((int)((f+0.005)*100))/100;
  mserv_broadcast("FACTOR", "%.2f\t%s", f, cl->user);
  if (cl->mode != mode_human)
    mserv_response(cl, "FACTSET", "%.2f", f);
}

static void mserv_cmd_top(t_client *cl, const char *ru, const char *line)
{
  t_track *track;
  t_rating *rate;
  char buffer[AUTHORLEN+NAMELEN+64];
  char bit[32];
  int c;
  double prob, cprob, pprob = 0;
  int ntracks = 0;
  double t;
  char *end;
  int top_start = 1;
  int top_end = 20;

  for (track = mserv_tracks; track; track = track->next) {
    if (track->filterok)
      ntracks++;
  }
  if (*line) {
    top_end = strtol(line, &end, 10);
    if (*end) {
      mserv_response(cl, "NAN", NULL);
      return;
    }
    if (top_end < 0) {
      top_end = -top_end;
      top_start = ntracks-top_end;
    }
  }
  if (mserv_factor < 0.5)
    t = 1/(((0.5-mserv_factor)*RFACT)+1);
  else
    t = 1.0-((0.5-mserv_factor)*RFACT);
  mserv_responsent(cl, "TOP", NULL);
  for (c = 1, track = mserv_tracks; track;
       track = track->next) {
    if (!track->filterok)
      continue;
    if (c > top_end)
      break;
    if (c < top_start)
      continue;
    /* get probability of playing this song or any better than it */
    if (c == ntracks) {
      cprob = 1;
    } else {
      cprob = 1-exp(t*log(1-((float)c/ntracks)));
    }
    prob = cprob;
    if (c > 1) {
      /* get probability of just this song being played */
      prob-= pprob;
    }
    pprob = cprob;
    if (prob < 0) /* rounding errors can result in -0.00000...0001, tsk */
      prob = 0;
    rate = mserv_getrate(ru, track);
    if (cl->mode == mode_human) {
      sprintf(bit, "%d/%d", track->n_album, track->n_track);
      sprintf(buffer, "[] %5.2f%%     %6.6s %-1.1s %-20.20s "
	      "%-30.30s%2ld:%02ld\r\n",
	      prob > 0.9999 ? 99.99 : 100*prob, bit,
	      rate && rate->rating ? mserv_ratestr(rate) : "-",
	      track->author, track->name,
	      (track->duration / 100) / 60,
	      (track->duration / 100) % 60);
      mserv_send(cl, buffer, 0);
    } else {
      sprintf(buffer, "%2.2f\t%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n", 100*prob,
	      track->n_album, track->n_track, track->author, track->name,
	      mserv_ratestr(rate),
	      (track->duration / 100) / 60,
	      (track->duration / 100) % 60);
      mserv_send(cl, buffer, 0);
    }
    c++;
  }
  if (cl->mode != mode_human)
    mserv_send(cl, ".\r\n", 0);
}

static void mserv_cmd_volume(t_client *cl, const char *ru, const char *line)
{
#ifdef SOUNDCARD
  int val;

  (void)ru;
  if (*line && cl->userlevel == level_guest) {
    mserv_response(cl, "ACLFAIL", NULL);
  } else if ((val = mserv_setmixer(cl, SOUND_MIXER_VOLUME, line)) != -1) {
    if (!*line) {
      mserv_response(cl, "VOLCUR", "%d", val & 0xFF);
    } else {
      mserv_broadcast("VOLSET", "%d\t%s", val & 0xFF, cl->user);
      if (cl->mode != mode_human)
	mserv_response(cl, "VOLREP", "%d", val & 0xFF);
    }
  }
#else
  (void)ru;
  (void)line;
  mserv_response(cl, "NOSCARD", NULL);
#endif
}

static void mserv_cmd_bass(t_client *cl, const char *ru, const char *line)
{
#ifdef SOUNDCARD
  int val;

  (void)ru;
  if (*line && cl->userlevel == level_guest) {
    mserv_response(cl, "ACLFAIL", NULL);
  } else if ((val = mserv_setmixer(cl, SOUND_MIXER_BASS, line)) != -1) {
    if (!*line) {
      mserv_response(cl, "BASSCUR", "%d", val & 0xFF);
    } else {
      mserv_broadcast("BASSSET", "%d\t%s", val & 0xFF, cl->user);
      if (cl->mode != mode_human)
	mserv_response(cl, "BASSREP", "%d", val & 0xFF);
    }
  }
#else
  (void)ru;
  (void)line;
  mserv_response(cl, "NOSCARD", NULL);
#endif
}

static void mserv_cmd_treble(t_client *cl, const char *ru, const char *line)
{
#ifdef SOUNDCARD
  int val;

  (void)ru;
  if (*line && cl->userlevel == level_guest) {
    mserv_response(cl, "ACLFAIL", NULL);
  } else if ((val = mserv_setmixer(cl, SOUND_MIXER_TREBLE, line)) != -1) {
    if (!*line) {
      mserv_response(cl, "TREBCUR", "%d", val & 0xFF);
    } else {
      mserv_broadcast("TREBSET", "%d\t%s", val & 0xFF, cl->user);
      if (cl->mode != mode_human)
	mserv_response(cl, "TREBREP", "%d", val & 0xFF);
    }
  }
#else
  (void)ru;
  (void)line;
  mserv_response(cl, "NOSCARD", NULL);
#endif
}

static void mserv_cmd_say(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  mserv_broadcast("SAY", "%s\t%s", cl->user, line);
  if (cl->mode != mode_human)
    mserv_response(cl, "SAY", "%s\t%s", cl->user, line);
}

static void mserv_cmd_emote(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  mserv_broadcast("EMOTE", "%s\t%s", cl->user, line);
  if (cl->mode != mode_human)
    mserv_response(cl, "EMOTE", "%s\t%s", cl->user, line);
}

static void mserv_cmd_set_author(t_client *cl, const char *ru,
				 const char *line)
{
  char linespl[LINEBUFLEN];
  char *str[4];
  unsigned int n_album, n_track;
  char *end;
  t_track *track;
  
  /* <album> <track> <author> */

  (void)ru;
  strcpy(linespl, line);

  if (mserv_split(str, 3, linespl, " ") != 3) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_album = strtol(str[0], &end, 10);
  if (!*str[0] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  n_track = strtol(str[1], &end, 10);
  if (!*str[1] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  if ((track = mserv_gettrack(n_album, n_track)) == NULL) {
    mserv_response(cl, "NOTRACK", NULL);
    return;
  }
  if (mserv_checkauthor(str[2]) == -1) {
    mserv_response(cl, "BADAUTH", NULL);
    return;
  }
  /* altertrack invalidates track pointer */
  if ((track = mserv_altertrack(track, str[2], NULL, NULL, NULL)) == NULL) {
    if (cl->mode != mode_human)
      mserv_response(cl, "MEMORYR", NULL);
    mserv_broadcast("MEMORY", NULL);
  }
  mserv_broadcast("AUTH", "%s\t%d\t%d\t%s\t%s\t%s", cl->user, track->n_album,
		  track->n_track, track->author, track->name, str[2]);
  if (cl->mode != mode_human)
    mserv_response(cl, "AUTHR", "%s\t%d\t%d\t%s\t%s\t%s", cl->user,
		   track->n_album, track->n_track, track->author,
		   track->name, str[2]);
}

static void mserv_cmd_set_name(t_client *cl, const char *ru, const char *line)
{
  char linespl[LINEBUFLEN];
  char *str[4];
  unsigned int n_album, n_track;
  char *end;
  t_track *track;

  /* <album> <track> <name> */

  (void)ru;
  strcpy(linespl, line);
  if (mserv_split(str, 3, linespl, " ") != 3) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_album = strtol(str[0], &end, 10);
  if (!*str[0] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  n_track = strtol(str[1], &end, 10);
  if (!*str[1] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  if ((track = mserv_gettrack(n_album, n_track)) == NULL) {
    mserv_response(cl, "NOTRACK", NULL);
    return;
  }
  if (mserv_checkname(str[2]) == -1) {
    mserv_response(cl, "BADNAME", NULL);
    return;
  }
  /* altertrack invalidates track pointer */
  if ((track = mserv_altertrack(track, NULL, str[2], NULL, NULL)) == NULL) {
    if (cl->mode != mode_human)
      mserv_response(cl, "MEMORYR", NULL);
    mserv_broadcast("MEMORY", NULL);
  }
  mserv_broadcast("NAME", "%s\t%d\t%d\t%s\t%s\t%s", cl->user, track->n_album,
		  track->n_track, track->author, track->name, str[2]);
  if (cl->mode != mode_human)
    mserv_response(cl, "NAMER", "%s\t%d\t%d\t%s\t%s\t%s", cl->user,
		   track->n_album, track->n_track, track->author,
		   track->name, str[2]);
}

static void mserv_cmd_set_genre(t_client *cl, const char *ru,
				const char *line)
{
  char linespl[LINEBUFLEN];
  char *str[4];
  unsigned int n_album, n_track;
  char *end;
  t_track *track;
  t_album *album;
  int n, i;

  /* <album> [<track>] <genre>[,genre]* */

  (void)ru;
  strcpy(linespl, line);
  n = mserv_split(str, 3, linespl, " ");
  if (n < 2 || n > 3) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_album = strtol(str[0], &end, 10);
  if (!*str[0] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  if (mserv_checkgenre(str[n-1]) == -1) {
    mserv_response(cl, "GENRERR", NULL);
    return;
  }
  if (n == 3) {
    /* set genre of track */
    n_track = strtol(str[1], &end, 10);
    if (!*str[1] || *end) {
      mserv_response(cl, "NAN", NULL);
      return;
    }
    if ((track = mserv_gettrack(n_album, n_track)) == NULL) {
      mserv_response(cl, "NOTRACK", NULL);
      return;
    }
    /* altertrack invalidates track pointer */
    if ((track = mserv_altertrack(track, NULL, NULL, str[n-1],
				  NULL)) == NULL) {
      if (cl->mode != mode_human)
	mserv_response(cl, "MEMORYR", NULL);
      mserv_broadcast("MEMORY", NULL);
      return;
    }
    mserv_broadcast("GENRE", "%s\t%d\t%d\t%s\t%s\t%s", cl->user,
		    track->n_album, track->n_track,
		    track->author, track->name, str[n-1]);
    if (cl->mode != mode_human)
      mserv_response(cl, "GENRER", "%s\t%d\t%d\t%s\t%s\t%s", cl->user,
		     track->n_album, track->n_track, track->author,
		     track->name, str[n-1]);
  } else {
    /* set genre of album */
    if ((album = mserv_getalbum(n_album)) == NULL) {
      mserv_response(cl, "NOALBUM", NULL);
      return;
    }
    for (i = 0; i < TRACKSPERALBUM; i++) {
      if (album->tracks[i]) {
	/* altertrack invalidates track pointer */
	if (mserv_altertrack(album->tracks[i], NULL, NULL, str[n-1],
			     NULL) == NULL) {
	  if (cl->mode != mode_human)
	    mserv_response(cl, "MEMORYR", NULL);
	  mserv_broadcast("MEMORY", NULL);
	  return;
	}
      }
    }
    mserv_broadcast("GENREAL", "%s\t%d\t%s\t%s\t%s", cl->user,
		    n_album, album->author, album->name, str[2]);
    if (cl->mode != mode_human)
      mserv_response(cl, "GENRER", NULL);
  }
}

static void mserv_cmd_set_year(t_client *cl, const char *ru, const char *line)
{
  char linespl[LINEBUFLEN];
  char *str[4];
  unsigned int n_album, n_track, year;
  char *end;
  t_track *track;

  /* <album> <track> <year> */

  (void)ru;
  strcpy(linespl, line);
  if (mserv_split(str, 3, linespl, " ") != 3) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_album = strtol(str[0], &end, 10);
  if (!*str[0] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  n_track = strtol(str[1], &end, 10);
  if (!*str[1] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  year = strtol(str[2], &end, 10);
  if (!*str[2] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  if ((track = mserv_gettrack(n_album, n_track)) == NULL) {
    mserv_response(cl, "NOTRACK", NULL);
    return;
  }
  track = mserv_checkdisk_track(track);
  track->year = year;
  track->modified = 1;
  mserv_broadcast("YEAR", "%s\t%d\t%d\t%s\t%s\t%d", cl->user, track->n_album,
		  track->n_track, track->author, track->name, year);
  if (cl->mode != mode_human)
    mserv_response(cl, "YEARR", "%s\t%d\t%d\t%s\t%s\t%d", cl->user,
		   track->n_album, track->n_track, track->author,
		   track->name, year);
  mserv_savechanges();
}

static void mserv_cmd_set_albumauthor(t_client *cl, const char *ru, const char *line)
{
  char linespl[LINEBUFLEN];
  char *str[3];
  unsigned int n_album;
  char *end;
  t_album *album;

  /* <album> <author> */

  (void)ru;
  strcpy(linespl, line);

  if (mserv_split(str, 2, linespl, " ") != 2) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_album = strtol(str[0], &end, 10);
  if (!*str[0] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  if ((album = mserv_getalbum(n_album)) == NULL) {
    mserv_response(cl, "NOTRACK", NULL);
    return;
  }
  if (mserv_checkauthor(str[1]) == -1) {
    mserv_response(cl, "BADAUTH", NULL);
    return;
  }
  /* alteralbum invalidates track pointer */
  if ((album = mserv_alteralbum(album, str[1], NULL)) == NULL) {
    if (cl->mode != mode_human)
      mserv_response(cl, "MEMORYR", NULL);
    mserv_broadcast("MEMORY", NULL);
  }
  mserv_broadcast("ALBAUTH", "%s\t%d\t%s\t%s\t%s", cl->user, n_album,
		  album->author, album->name, str[1]);
  if (cl->mode != mode_human)
    mserv_response(cl, "AUTHR", "%s\t%d\t%s\t%s\t%s", cl->user, n_album,
		   album->author, album->name, str[1]);
}

static void mserv_cmd_set_albumname(t_client *cl, const char *ru,
				    const char *line)
{
  char linespl[LINEBUFLEN];
  char *str[3];
  unsigned int n_album;
  char *end;
  t_album *album;

  /* <album> <name> */

  (void)ru;
  strcpy(linespl, line);

  if (mserv_split(str, 2, linespl, " ") != 2) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_album = strtol(str[0], &end, 10);
  if (!*str[0] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  if ((album = mserv_getalbum(n_album)) == NULL) {
    mserv_response(cl, "NOTRACK", NULL);
    return;
  }
  if (mserv_checkname(str[1]) == -1) {
    mserv_response(cl, "BADAUTH", NULL);
    return;
  }
  /* alteralbum invalidates track pointer */
  if ((album = mserv_alteralbum(album, NULL, str[1])) == NULL) {
    if (cl->mode != mode_human)
      mserv_response(cl, "MEMORYR", NULL);
    mserv_broadcast("MEMORY", NULL);
  }
  mserv_broadcast("ALBNAME", "%s\t%d\t%s\t%s\t%s", cl->user, n_album,
		  album->author, album->name, str[1]);
  if (cl->mode != mode_human)
    mserv_response(cl, "NAMER", "%s\t%d\t%s\t%s\t%s", cl->user, n_album,
		   album->author, album->name, str[1]);
}

static void mserv_cmd_history(t_client *cl, const char *ru, const char *line)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  char bit[32];
  int i;
  t_rating *rate;

  (void)line;
  mserv_responsent(cl, "HISTORY", NULL);
  for (i = 0; mserv_history[i] && i < HISTORYLEN; i++) {
    rate = mserv_getrate(ru, mserv_history[i]->track);
    if (cl->mode == mode_human) {
      sprintf(bit, "%d/%d", mserv_history[i]->track->n_album,
	      mserv_history[i]->track->n_track);
      sprintf(buffer, "[] %-10.10s %6.6s %-1.1s %-20.20s %-35.35s\r\n",
	      mserv_history[i]->user, bit,
	      rate && rate->rating ? mserv_ratestr(rate) : "-",
	      mserv_history[i]->track->author, mserv_history[i]->track->name);
      mserv_send(cl, buffer, 0);
    } else {
      sprintf(buffer, "%s\t%d\t%d\t%s\t%s\t%s\r\n",
	      mserv_history[i]->user, mserv_history[i]->track->n_album,
	      mserv_history[i]->track->n_track,
	      mserv_history[i]->track->author, mserv_history[i]->track->name,
	      mserv_ratestr(rate));
      mserv_send(cl, buffer, 0);
    }
  }
  if (cl->mode != mode_human)
    mserv_send(cl, ".\r\n", 0);
}

static void mserv_cmd_rate(t_client *cl, const char *ru, const char *line)
{
  char linespl[LINEBUFLEN];
  char buffer[AUTHORLEN+NAMELEN+64];
  char bit[32];
  char *str[4];
  unsigned int n_album, n_track;
  int val;
  char *end;
  t_track *track, *track2;
  int n, i;
  t_album *album;
  t_rating *rate, *rate2;
  int ratetoo;
  int total;

  /* [<album> [<track>]] <rating> */

  (void)ru;
  strcpy(linespl, line);
  n = mserv_split(str, 3, linespl, " ");
  if (n < 1) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  if ((val = mserv_strtorate(str[n-1])) == -1) {
    mserv_response(cl, "BADRATE", NULL);
    return;
  }
  if (n == 1) {
    if (!mserv_playing.track) {
      mserv_response(cl, "NOTHING", NULL);
      return;
    }
    n_album = mserv_playing.track->n_album;
    n_track = mserv_playing.track->n_track;
  } else {
    n_album = strtol(str[0], &end, 10);
    if (!*str[0] || *end) {
      mserv_response(cl, "NAN", NULL);
      return;
    }
    if (n == 2) {
      n_track = 1<<16;
    } else {
      n_track = strtol(str[1], &end, 10);
      if (!*str[1] || *end) {
        mserv_response(cl, "NAN", NULL);
        return;
      }
    }
  }
  if (n_track != 1<<16) {
    if ((track = mserv_gettrack(n_album, n_track)) == NULL) {
      mserv_response(cl, "NOTRACK", NULL);
      return;
    }
    if ((rate = mserv_ratetrack(cl, &track, val)) == NULL) {
      mserv_broadcast("MEMORY", NULL);
      if (cl->mode != mode_human)
	mserv_response(cl, "MEMORYR", NULL);
      return;
    }
    if (mserv_playing.track && n_album == mserv_playing.track->n_album &&
	n_track == mserv_playing.track->n_track) {
      mserv_broadcast("RATECUR", "%s\t%d\t%d\t%s\t%s\t%s", cl->user,
		      mserv_playing.track->n_album,
		      mserv_playing.track->n_track, track->author,
		      track->name, mserv_ratestr(rate));
      if (cl->mode != mode_human)
	mserv_response(cl, "RATED", "%s\t%d\t%d\t%s\t%s\t%s", cl->user,
		       mserv_playing.track->n_album,
		       mserv_playing.track->n_track, track->author,
		       track->name, mserv_ratestr(rate));
    } else {
      mserv_broadcast("RATE", "%s\t%d\t%d\t%s\t%s\t%s", cl->user, n_album,
		      n_track, track->author, track->name,
		      mserv_ratestr(rate));
      if (cl->mode != mode_human)
	mserv_response(cl, "RATED", "%s\t%d\t%d\t%s\t%s\t%s", cl->user,
		       n_album, n_track, track->author, track->name,
		       mserv_ratestr(rate));
    }
    if (cl->mode == mode_human) {
      ratetoo = 0;
      for (album = mserv_albums; album; album = album->next) {
	for (i = 0; i < TRACKSPERALBUM; i++) {
	  track2 = album->tracks[i];
	  if (track2 && track2 != track &&
	      !stricmp(track2->author, track->author) &&
	      !stricmp(track2->name, track->name)) {
	    if (track->duration && track2->duration &&
		(track->duration > track2->duration ?
		 (float)track2->duration/track->duration :
		 (float)track->duration/track2->duration) < 0.9)
	      /* tracks are not within 10% tollerance of durations */
	      continue;
	    rate2 = mserv_getrate(cl->user, track2);
	    if (rate2 && rate->rating == rate2->rating)
	      continue;
	    if (!ratetoo) {
	      mserv_response(cl, "RATETOO", NULL);
	      ratetoo = 1;
	    }
	    sprintf(bit, "%d/%d", track2->n_album, track2->n_track);
	    sprintf(buffer, "[]            %6.6s %-1.1s %-20.20s "
		    "%-30.30s%ld:%02ld\r\n",
		    bit, rate2 && rate2->rating ? mserv_ratestr(rate2) : "-",
		    track2->author, track2->name,
		    (track2->duration / 100) / 60,
		    (track2->duration / 100) % 60);
	    mserv_send(cl, buffer, 0);
	  }
	}
      }
    }
  } else {
    if ((album = mserv_getalbum(n_album)) == NULL) {
      mserv_response(cl, "NOALBUM", NULL);
      return;
    }
    rate = NULL;
    total = 0;
    for (i = 0; i < TRACKSPERALBUM; i++) {
      if (album->tracks[i]) {
	if ((rate = mserv_getrate(cl->user, album->tracks[i])) == NULL ||
	    rate->rating == 0) { /* 0 means heard, not rated */
	  if ((rate = mserv_ratetrack(cl, &album->tracks[i], val)) == NULL) {
	    mserv_broadcast("MEMORY", NULL);
	    if (cl->mode != mode_human)
	      mserv_response(cl, "MEMORYR", NULL);
	    goto exit;
	  } else {
	    total++;
	  }
	}
      }
    }
    if (rate) {
      mserv_broadcast("RATEDAL", "%s\t%d\t%s\t%s\t%s\t%d", cl->user, n_album,
		      album->author, album->name, mserv_ratestr(rate),
		      total);
      if (cl->mode != mode_human)
	mserv_response(cl, "RATED", NULL);
    } else {
      mserv_response(cl, "NOTRKS", NULL);
    }
  }
 exit:
  mserv_recalcratings();
  mserv_savechanges();
}

static void mserv_cmd_check(t_client *cl, const char *ru, const char *line)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  char bit[32];
  t_track *track1, *track2;
  t_author *author;
  t_rating *rate1, *rate2;
  int f = 0;
  int i;

  if (*line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  for (author = mserv_authors; author; author = author->next) {
    for (i = 0; i < TRACKSPERALBUM-1; i++) {
      if ((track1 = author->tracks[i]) && (track2 = author->tracks[i+1])) {
	if (!stricmp(track1->author, track2->author) &&
	    !stricmp(track1->name, track2->name)) {
	  if (!f) {
	    f = 1;
	    mserv_responsent(cl, "CHECKR", NULL);
	  }
	  rate1 = mserv_getrate(ru, track1);
	  rate2 = mserv_getrate(ru, track2);
	  if ((rate1 == NULL && rate2 == NULL) ||
	      (rate1 && rate2 && rate1->rating == rate2->rating) ||
	      (rate1 && !rate2 && rate1->rating == 0) ||
	      (!rate1 && rate2 && rate2->rating == 0))
	    continue;
	  if (cl->mode == mode_human) {
	    sprintf(bit, "%d/%d", track1->n_album, track1->n_track);
	    sprintf(buffer, "[]            %6.6s %-1.1s %-20.20s "
		    "%-30.30s%2ld:%02ld\r\n",
		    bit, rate1 && rate1->rating ? mserv_ratestr(rate1) : "-",
		    track1->author, track1->name,
		    (track1->duration / 100) / 60,
		    (track1->duration / 100) % 60);
	    mserv_send(cl, buffer, 0);
	    sprintf(bit, "%d/%d", track2->n_album, track2->n_track);
	    sprintf(buffer, "[]            %6.6s %-1.1s %-20.20s "
		    "%-30.30s%2ld:%02ld\r\n",
		    bit, rate2 && rate2->rating ? mserv_ratestr(rate2) : "-",
		    track2->author, track2->name,
		    (track2->duration / 100) / 60,
		    (track2->duration / 100) % 60);
	    mserv_send(cl, buffer, 0);
	  } else {
	    sprintf(buffer, "%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
		    track1->n_album, track1->n_track,
		    track1->author, track1->name,
		    mserv_ratestr(rate1),
		    (track1->duration / 100) / 60,
		    (track1->duration / 100) % 60);
	    mserv_send(cl, buffer, 0);
	    sprintf(buffer, "%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
		    track2->n_album, track2->n_track,
		    track2->author, track2->name,
		    mserv_ratestr(rate2),
		    (track2->duration / 100) / 60,
		    (track2->duration / 100) % 60);
	    mserv_send(cl, buffer, 0);
	  }
	}
      }
    }
  }
  if (f) {
    if (cl->mode != mode_human)
      mserv_send(cl, ".\r\n", 0);
  } else {
    mserv_response(cl, "CHECKRN", NULL);
  }
}

static void mserv_cmd_search(t_client *cl, const char *ru, const char *line)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  char bit[32];
  t_track *track;
  t_author *author;
  t_rating *rate;
  int f = 0;
  int i;

  if (!*line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  for (author = mserv_authors; author; author = author->next) {
    for (i = 0; i < TRACKSPERALBUM; i++) {
      if ((track = author->tracks[i])) {
	if ((stristr(track->author, line)) ||
	    (stristr(track->name, line))) {
	  if (!f) {
	    f = 1;
	    mserv_responsent(cl, "SEARCHA", NULL);
	  }
	  rate = mserv_getrate(ru, track);
	  if (cl->mode == mode_human) {
	    sprintf(bit, "%d/%d", track->n_album, track->n_track);
	    sprintf(buffer, "[]            %6.6s %-1.1s %-20.20s "
		    "%-30.30s%2ld:%02ld\r\n", bit,
		    rate && rate->rating ? mserv_ratestr(rate) : "-",
		    track->author, track->name,
		    (track->duration / 100) / 60,
		    (track->duration / 100) % 60);
	    mserv_send(cl, buffer, 0);
	  } else {
	    sprintf(buffer, "%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
		    track->n_album, track->n_track,
		    track->author, track->name, mserv_ratestr(rate),
		    (track->duration / 100) / 60,
		    (track->duration / 100) % 60);
	    mserv_send(cl, buffer, 0);
	  }
	}
      }
    }
  }
  if (f) {
    if (cl->mode != mode_human)
      mserv_send(cl, ".\r\n", 0);
  } else {
    mserv_response(cl, "SEARCHB", NULL);
  }
}

static void mserv_cmd_searchf(t_client *cl, const char *ru, const char *line)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  char bit[32];
  t_track *track;
  t_author *author;
  t_rating *rate;
  int f = 0;
  int i;

  if (!*line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  if (mserv_tracks == NULL || filter_check(line, mserv_tracks) == -1) {
    mserv_response(cl, "BADFILT", NULL);
    return;
  }
  for (author = mserv_authors; author; author = author->next) {
    for (i = 0; i < TRACKSPERALBUM; i++) {
      if ((track = author->tracks[i])) {
	if (filter_check(line, track) == 1) {
	  if (!f) {
	    f = 1;
	    mserv_responsent(cl, "SEARCFA", NULL);
	  }
	  rate = mserv_getrate(ru, track);
	  if (cl->mode == mode_human) {
	    sprintf(bit, "%d/%d", track->n_album, track->n_track);
	    sprintf(buffer, "[]            %6.6s %-1.1s %-20.20s "
		    "%-30.30s%2ld:%02ld\r\n", bit,
		    rate && rate->rating ? mserv_ratestr(rate) : "-",
		    track->author, track->name,
		    (track->duration / 100) / 60,
		    (track->duration / 100) % 60);
	    mserv_send(cl, buffer, 0);
	  } else {
	    sprintf(buffer, "%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
		    track->n_album, track->n_track,
		    track->author, track->name, mserv_ratestr(rate),
		    (track->duration / 100) / 60,
		    (track->duration / 100) % 60);
	    mserv_send(cl, buffer, 0);
	  }
	}
      }
    }
  }
  if (f) {
    if (cl->mode != mode_human)
      mserv_send(cl, ".\r\n", 0);
  } else {
    mserv_response(cl, "SEARCFB", NULL);
  }
}

#ifdef IDEA
static void mserv_cmd_idea(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  if (!*line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  if (mserv_idea(line) == -1)
    mserv_response(cl, "IDEAF", NULL);
  else
    mserv_response(cl, "IDEAD", NULL);
}
#endif

static void mserv_cmd_info(t_client *cl, const char *ru, const char *line)
{
  char linespl[LINEBUFLEN];
  t_rating *rate;
  t_album *album;
  t_track *track;
  unsigned int n_album;
  unsigned int n_track;
  unsigned int i;
  char *str[3];
  char *end;
  char ago[64];
  int diff;
  char token[16];
  char year[32];
  unsigned int ttime;
  
  if (!*line) {
    if ((track = mserv_playing.track) == NULL) {
      mserv_response(cl, "NOTHING", NULL);
      return;
    }
  } else {
    strcpy(linespl, line);
    if (mserv_split(str, 2, linespl, " ") < 1) {
      mserv_response(cl, "BADPARM", NULL);
      return;
    }
    n_album = strtol(str[0], &end, 10);
    if (!*str[0] || *end) {
      mserv_response(cl, "NAN", NULL);
      return;
    }
    if (!str[1]) {
      /* album information */
      if ((album = mserv_getalbum(n_album)) == NULL) {
        mserv_response(cl, "NOALBUM", NULL);
        return;
      }
      ttime = 0;
      for (i = 0; i < TRACKSPERALBUM; i++) {
	if (album->tracks[i])
	  ttime+= album->tracks[i]->duration;
      }
      mserv_response(cl, "INFA", "%d\t%s\t%s\t%d:%02d.%d",
		     n_album, album->author, album->name,
		     (ttime/100)/60, (ttime/100) % 60,
		     (ttime/10) % 10);
      if (cl->mode == mode_human) {
	for (i = 1; i <= 3; i++) {
	  sprintf(token, "INFA%d", i);
	  mserv_response(cl, token, "%d\t%s\t%s\t%d:%02d.%d",
			 n_album, album->author, album->name,
			 (ttime/100)/60, (ttime/100) % 60,
			 (ttime/10) % 10);
	}
      }
      return;
    }
    n_track = strtol(str[1], &end, 10);
    if (!*str[1] || *end) {
      mserv_response(cl, "NAN", NULL);
      return;
    }
    if ((track = mserv_gettrack(n_album, n_track)) == NULL) {
      mserv_response(cl, "NOTRACK", NULL);
      return;
    }
  }
  if ((album = mserv_getalbum(track->n_album)) == NULL) {
    mserv_response(cl, "NOALBUM", NULL);
    return;
  }
  strcpy(ago, "never");
  if (track->lastplay != 0) {
    diff = time(NULL) - track->lastplay;
    if (diff < 60*2)
      sprintf(ago, "%d seconds ago", diff);
    else if (diff < 60*60*2)
      sprintf(ago, "%d minutes ago", diff/60);
    else if (diff < 60*60*48)
      sprintf(ago, "%d hours ago", diff/60/60);
    else
      sprintf(ago, "%d days ago", diff/60/60/24);
  }
  rate = mserv_getrate(ru, track);
  snprintf(year, sizeof(year), "%d", track->year);
  mserv_response(cl, "INFT",
		 "%d\t%d\t%s\t%s\t%s\t%s\t%s\t%d\t%s\t%.1f\t%.1f\t"
		 "%s\t%s\t%s\t%d:%02d.%d\t%s",
		 track->n_album, track->n_track, album->author, album->name,
		 track->author, track->name, track->year ? year : "unknown",
		 track->lastplay, ago, 100*track->prating, 100*track->rating,
		 mserv_ratestr(rate), track->genres,
		 track->filterok ? "included" : "excluded",
		 (track->duration/100)/60, (track->duration/100) % 60,
		 (track->duration/10) % 10, track->miscinfo);
  if (cl->mode == mode_human) {
    for (i = 1; i <= 11; i++) {
      sprintf(token, "INFT%d", i);
      mserv_response(cl, token,
		     "%d\t%d\t%s\t%s\t%s\t%s\t%s\t%d\t%s\t%.1f\t%.1f\t"
		     "%s\t%s\t%s\t%d:%02d.%d\t%s",
		     track->n_album, track->n_track, album->author,
		     album->name, track->author, track->name,
		     track->year ? year : "unknown", track->lastplay,
		     ago, 100*track->prating, 100*track->rating,
		     mserv_ratestr(rate), track->genres,
		     track->filterok ? "included" : "excluded",
		     (track->duration/100)/60, (track->duration/100) % 60,
		     (track->duration/10) % 10, track->miscinfo);
    }
  }
}

static void mserv_cmd_x_authors(t_client *cl, const char *ru,
				const char *line)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  t_author *author;
  int total, rated, i;
  t_rating *rate;

  (void)line;
  mserv_responsent(cl, "AUTHORS", NULL);
  for (author = mserv_authors; author; author = author->next) {
    total = 0;
    rated = 0;
    for (i = 0; i < TRACKSPERALBUM; i++) {
      if (author->tracks[i]) {
	total++;
	rate = mserv_getrate(ru, author->tracks[i]);
	if (rate && rate->rating) /* rate->rating being 0 means 'heard' */
	  rated++;
      }
    }
    if (cl->mode == mode_human) {
      sprintf(buffer, "[] %3d %-62.62s %4d %4d\r\n", author->id,
	      author->name, rated, total);
    } else {
      sprintf(buffer, "%d\t%s\t%d\t%d\r\n", author->id, author->name,
	      total, rated);
    }
    mserv_send(cl, buffer, 0);
  }
  if (cl->mode != mode_human)
    mserv_send(cl, ".\r\n", 0);
}

static void mserv_cmd_x_authorid(t_client *cl, const char *ru,
				 const char *line)
{
  t_author *author;

  (void)ru;
  for (author = mserv_authors; author; author = author->next) {
    if (!stricmp(line, author->name)) {
      mserv_response(cl, "AUTHID", "%d\t%s", author->id, author->name);
      return;
    }
  }
  mserv_response(cl, "NOAUTH", NULL);
}

static void mserv_cmd_x_authorinfo(t_client *cl, const char *ru,
				   const char *line)
{
  unsigned int n_author;
  char *end;
  int total, rated;
  t_author *author;
  t_rating *rate;
  int i;

  if (!*line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_author = strtol(line, &end, 10);
  if (*end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  for (author = mserv_authors; author; author = author->next) {
    if (author->id == n_author) {
      total = 0;
      rated = 0;
      for (i = 0; i < TRACKSPERALBUM; i++) {
	if (author->tracks[i]) {
	  total++;
	  rate = mserv_getrate(ru, author->tracks[i]);
	  if (rate && rate->rating) /* rate->rating being 0 means 'heard' */
	    rated++;
	}
      }
      mserv_response(cl, "AUTHINF", "%d\t%s\t%d\t%d", author->id, author->name,
		     total, rated);
      return;
    }
  }
  mserv_response(cl, "NOAUTH", NULL);
}

static void mserv_cmd_x_authortracks(t_client *cl, const char *ru,
				     const char *line)
{
  unsigned int n_author;
  char *end;
  t_author *author;
  int i;
  char bit[32];
  char buffer[AUTHORLEN+NAMELEN+64];
  t_rating *rate;

  if (!*line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_author = strtol(line, &end, 10);
  if (*end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  for (author = mserv_authors; author; author = author->next) {
    if (author->id == n_author) {
      mserv_responsent(cl, "AUTHTRK", "%d\t%s", author->id, author->name);
      for (i = 0; i < TRACKSPERALBUM; i++) {
	if (author->tracks[i]) {
	  rate = mserv_getrate(ru, author->tracks[i]);
	  sprintf(bit, "%d/%d", author->tracks[i]->n_album,
		  author->tracks[i]->n_track);
	  if (cl->mode == mode_human) {
	    sprintf(buffer, "[] %6.6s %-1.1s %-20.20s %-45.45s\r\n", bit,
		    rate && rate->rating ? mserv_ratestr(rate) : "-",
		    author->tracks[i]->author, author->tracks[i]->name);
	    mserv_send(cl, buffer, 0);
	  } else {
	    sprintf(buffer, "%d\t%d\t%d\t%s\t%s\t%s\r\n", author->id,
		    author->tracks[i]->n_album, author->tracks[i]->n_track,
		    author->tracks[i]->author, author->tracks[i]->name,
		    mserv_ratestr(rate));
	    mserv_send(cl, buffer, 0);
	  }
	}
      }
      if (cl->mode != mode_human)
	mserv_send(cl, ".\r\n", 0);
      return;
    }
  }
  mserv_response(cl, "NOAUTH", NULL);
}

static void mserv_cmd_date(t_client *cl, const char *ru, const char *line)
{
  time_t curtime = time(NULL);
  struct tm curtm = *localtime(&curtime);
  char date[64];
  char date_format[64];
  char time[16];
  char ct[32];

  (void)ru;
  (void)line;
  strcpy(ct, ctime(&curtime));
  ct[24] = '\0';
  /* Wednesday 1st January 1998 */
  if ((unsigned int)snprintf(date_format, 64, "%%A %d%s %%B %%Y",
			     curtm.tm_mday,
			     mserv_stndrdth(curtm.tm_mday)) >= 64) {
    mserv_response(cl, "IMPLERR", NULL);
    return;
  }
  if (strftime(date, 64, date_format, &curtm) == 0 ||
      strftime(time, 16, "%H:%M", &curtm) == 0) {
    mserv_response(cl, "IMPLERR", NULL);
    return;
  }
  mserv_response(cl, "DATE", "%d\t%d\t%s\t%s\t%s", curtime,
		 mktime(&curtm), ct, date, time);
}

static void mserv_cmd_kick(t_client *cl, const char *ru, const char *line)
{
  char linespl[LINEBUFLEN];
  char *str[3];
  t_acl *acl;
  t_client *client;
  char *end;

  (void)ru;
  strcpy(linespl, line);

  if (mserv_split(str, 2, linespl, " ") < 1) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  for (acl = mserv_acl; acl; acl = acl->next) {
    if (!stricmp(acl->user, str[0]))
      break;
  }
  if (!acl) {
    mserv_response(cl, "NOUSER", NULL);
    return;
  }
  if (str[1]) {
    acl->nexttime = time(NULL) + 60 * strtol(str[1], &end, 10);
    if (!*line || *end) {
      mserv_response(cl, "NAN", NULL);
      return;
    }
  } else {
    acl->nexttime = time(NULL) + 60;
  }
  mserv_log("Kick of %s by %s", str[0], cl->user);
  for (client = mserv_clients; client; client = client->next) {
    if (client->authed && !stricmp(client->user, str[0]))
      break;
  }
  if (!client) {
    mserv_response(cl, "USERCHG", NULL);
    return;
  }
  if (cl->mode != mode_human) /* humans will see 'disconnected' message */
    mserv_response(cl, "USERCHG", NULL);
  if (client->mode == mode_human)
    mserv_response(client, "KICKED", NULL);
  mserv_close(client);
}

static void mserv_cmd_reset(t_client *cl, const char *ru, const char *line)
{
  static int timer = 0;

  (void)ru;
  (void)line;
  mserv_abortplay();
  acl_save();
  if (mserv_savechanges() && time(NULL) > timer) {
    timer = time(NULL) + 60;
    mserv_response(cl, "RESETER", NULL);
    return;
  }
  mserv_log("Reset initiated by %s", cl->user);
  mserv_reset();
  mserv_broadcast("RESET", "%s", cl->user);
  if (cl->mode != mode_human)
    mserv_response(cl, "RESETR", NULL);
}

static void mserv_cmd_sync(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  (void)line;
  mserv_log("Syncing disks/memory (%s)...", cl->user);
  mserv_savechanges();
  mserv_ensuredisk();
  mserv_log("Sync completed.", cl->user);
  mserv_response(cl, "SYNCED", NULL);
}

static void mserv_cmd_shutdown(t_client *cl, const char *ru, const char *line)
{
  (void)ru;
  (void)line;
  if (mserv_shutdown) {
    mserv_response(cl, "SHUTALR", NULL);
    return;
  }
  mserv_log("Shutdown initiated by %s", cl->user);
  mserv_shutdown = 1;
  if (mserv_playing.track) {
    mserv_broadcast("SHUTEND", cl->user);
  } else {
    mserv_broadcast("SHUTNOW", cl->user);
  }
  if (cl->mode != mode_human)
    mserv_response(cl, "SHUTYES", NULL);

  if (!mserv_playing.track) {
    mserv_flush();
    mserv_closedown(0);
  }
}

static void mserv_cmd_gap(t_client *cl, const char *ru, const char *line)
{
  int delay;
  char *end;

  (void)ru;
  if (!*line) {
    mserv_response(cl, "GAPCUR", "%d", mserv_setgap(-1));
    return;
  }    
  delay = strtol(line, &end, 10);
  if (!*line || *end || delay < 0 || delay > 3600) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  mserv_broadcast("DELAY", "%s\t%d", cl->user, mserv_setgap(delay));
  if (cl->mode != mode_human)
    mserv_response(cl, "DELAYR", NULL);
}

static void mserv_cmd_x_genres(t_client *cl, const char *ru, const char *line)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  t_genre *genre;
  int rated;
  unsigned int ui;
  t_rating *rate;

  (void)line;
  mserv_responsent(cl, "GENRES", NULL);
  for (genre = mserv_genres; genre; genre = genre->next) {
    rated = 0;
    for (ui = 0; ui < genre->total; ui++) {
      rate = mserv_getrate(ru, genre->tracks[ui]);
      if (rate && rate->rating) /* rate->rating being 0 means 'heard' */
	rated++;
    }
    if (cl->mode == mode_human) {
      sprintf(buffer, "[] %3d %-62.62s %4d %4d\r\n", genre->id,
	      genre->name, rated, genre->total);
    } else {
      sprintf(buffer, "%d\t%s\t%d\t%d\r\n", genre->id, genre->name,
	      genre->total, rated);
    }
    mserv_send(cl, buffer, 0);
  }
  if (cl->mode != mode_human)
    mserv_send(cl, ".\r\n", 0);
}

static void mserv_cmd_x_genreid(t_client *cl, const char *ru,
				const char *line)
{
  t_genre *genre;

  (void)ru;
  for (genre = mserv_genres; genre; genre = genre->next) {
    if (!stricmp(line, genre->name)) {
      mserv_response(cl, "GENID", "%d\t%s", genre->id, genre->name);
      return;
    }
  }
  mserv_response(cl, "NOGEN", NULL);
}

static void mserv_cmd_x_genreinfo(t_client *cl, const char *ru,
				  const char *line)
{
  unsigned int n_genre;
  char *end;
  t_genre *genre;
  int rated;
  unsigned int ui;
  t_rating *rate;

  if (!*line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_genre = strtol(line, &end, 10);
  if (*end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  for (genre = mserv_genres; genre; genre = genre->next) {
    if (genre->id == n_genre) {
      rated = 0;
      for (ui = 0; ui < genre->total; ui++) {
	rate = mserv_getrate(ru, genre->tracks[ui]);
	if (rate && rate->rating) /* rate->rating being 0 means 'heard' */
	  rated++;
      }
      mserv_response(cl, "GENINF", "%d\t%s\t%d\t%d", genre->id, genre->name,
		     genre->total, rated);
      return;
    }
  }
  mserv_response(cl, "NOGEN", NULL);
}

static void mserv_cmd_x_genretracks(t_client *cl, const char *ru,
				    const char *line)
{
  unsigned int n_genre;
  char *end;
  t_genre *genre;
  unsigned int ui;
  char bit[32];
  char buffer[AUTHORLEN+NAMELEN+64];
  t_rating *rate;

  if (!*line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_genre = strtol(line, &end, 10);
  if (*end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  for (genre = mserv_genres; genre; genre = genre->next) {
    if (genre->id == n_genre) {
      mserv_responsent(cl, "GENTRK", "%d\t%s\t%d", genre->id, genre->name,
		       genre->total);
      for (ui = 0; ui < genre->total; ui++) {
	rate = mserv_getrate(ru, genre->tracks[ui]);
	sprintf(bit, "%d/%d", genre->tracks[ui]->n_album,
		genre->tracks[ui]->n_track);
	if (cl->mode == mode_human) {
	  sprintf(buffer, "[] %6.6s %-1.1s %-20.20s %-45.45s\r\n", bit,
		  rate && rate->rating ? mserv_ratestr(rate) : "-",
		  genre->tracks[ui]->author, genre->tracks[ui]->name);
	  mserv_send(cl, buffer, 0);
	} else {
	  sprintf(buffer, "%d\t%d\t%d\t%s\t%s\t%s\r\n", genre->id,
		  genre->tracks[ui]->n_album, genre->tracks[ui]->n_track,
		  genre->tracks[ui]->author, genre->tracks[ui]->name,
		  mserv_ratestr(rate));
	  mserv_send(cl, buffer, 0);
	}
      }
      if (cl->mode != mode_human)
	mserv_send(cl, ".\r\n", 0);
      return;
    }
  }
  mserv_response(cl, "NOGEN", NULL);
}

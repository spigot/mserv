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

/*
  cmd - command table and associated commands.  see also:
    cmd_x        X commands
    cmd_set      SET commands
    cmd_channel  CHANNEL commands
*/

#define _GNU_SOURCE 1
#define _BSD_SOURCE 1
#define __EXTENSIONS__ 1

#include <sys/types.h> /* inet_ntoa on freebsd */
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>

#include "mserv.h"
#include "misc.h"
#include "mserv-soundcard.h"
#include "acl.h"
#include "filter.h"
#include "cmd.h"
#include "cmd_x.h"
#include "cmd_set.h"
#include "cmd_channel.h"

/*** file-scope (static) function declarations ***/

static void cmd_moo(t_client *cl, t_cmdparams *cp);
static void cmd_quit(t_client *cl, t_cmdparams *cp);
static void cmd_help(t_client *cl, t_cmdparams *cp);
static void cmd_user(t_client *cl, t_cmdparams *cp);
static void cmd_pass(t_client *cl, t_cmdparams *cp);
static void cmd_status(t_client *cl, t_cmdparams *cp);
static void cmd_userinfo(t_client *cl, t_cmdparams *cp);
static void cmd_who(t_client *cl, t_cmdparams *cp);
static void cmd_say(t_client *cl, t_cmdparams *cp);
static void cmd_emote(t_client *cl, t_cmdparams *cp);
static void cmd_create(t_client *cl, t_cmdparams *cp);
static void cmd_remove(t_client *cl, t_cmdparams *cp);
static void cmd_level(t_client *cl, t_cmdparams *cp);
static void cmd_password(t_client *cl, t_cmdparams *cp);
static void cmd_albums(t_client *cl, t_cmdparams *cp);
static void cmd_tracks(t_client *cl, t_cmdparams *cp);
static void cmd_ratings(t_client *cl, t_cmdparams *cp);
static void cmd_queue(t_client *cl, t_cmdparams *cp);
static void cmd_unqueue(t_client *cl, t_cmdparams *cp);
static void cmd_play(t_client *cl, t_cmdparams *cp);
static void cmd_stop(t_client *cl, t_cmdparams *cp);
static void cmd_next(t_client *cl, t_cmdparams *cp);
static void cmd_clear(t_client *cl, t_cmdparams *cp);
static void cmd_repeat(t_client *cl, t_cmdparams *cp);
static void cmd_random(t_client *cl, t_cmdparams *cp);
static void cmd_filter(t_client *cl, t_cmdparams *cp);
static void cmd_factor(t_client *cl, t_cmdparams *cp);
static void cmd_top(t_client *cl, t_cmdparams *cp);
static void cmd_pause(t_client *cl, t_cmdparams *cp);
static void cmd_volume(t_client *cl, t_cmdparams *cp);
static void cmd_bass(t_client *cl, t_cmdparams *cp);
static void cmd_treble(t_client *cl, t_cmdparams *cp);
static void cmd_history(t_client *cl, t_cmdparams *cp);
static void cmd_rate(t_client *cl, t_cmdparams *cp);
static void cmd_check(t_client *cl, t_cmdparams *cp);
static void cmd_search(t_client *cl, t_cmdparams *cp);
static void cmd_searchf(t_client *cl, t_cmdparams *cp);
static void cmd_asearch(t_client *cl, t_cmdparams *cp);
static void cmd_idea(t_client *cl, t_cmdparams *cp);
static void cmd_info(t_client *cl, t_cmdparams *cp);
static void cmd_date(t_client *cl, t_cmdparams *cp);
static void cmd_kick(t_client *cl, t_cmdparams *cp);
static void cmd_reset(t_client *cl, t_cmdparams *cp);
static void cmd_sync(t_client *cl, t_cmdparams *cp);
static void cmd_shutdown(t_client *cl, t_cmdparams *cp);
static void cmd_gap(t_client *cl, t_cmdparams *cp);

static int cmd_queue_sub(t_client *cl, t_album *album, int n_track,
			       int header);

/*** file-scope (static) globals ***/

/*** externed variables ***/

t_cmds cmd_cmds[] = {
  /* authed_flag, level, name, sub-command table, command, help, syntax */
  { 0, level_guest, "QUIT", NULL, cmd_quit,
    "Quits you from this server",
    "" },
  { 0, level_guest, "HELP", NULL, cmd_help,
    "Displays help",
    "[<command> [<sub-command>]]" },
  { 0, level_guest, "USER", NULL, cmd_user,
    "Informs server of your username",
    "<username>" },
  { 0, level_guest, "PASS", NULL, cmd_pass,
    "Informs server of your password",
    "<password> [<mode: HUMAN, COMPUTER, RTCOMPUTER>]" },
  { 0, level_guest, "DATE", NULL, cmd_date,
    "Display the time and date",
    "" },
  { 1, level_guest, "STATUS", NULL, cmd_status,
    "Displays the current status of the server",
    "" },
  { 1, level_guest, "USERINFO", NULL, cmd_userinfo,
    "Displays information about a particular user",
    "<username>" },
  { 1, level_user, "WHO", NULL, cmd_who,
    "Displays who is currently connected",
    "" },
  { 1, level_user, "SAY", NULL, cmd_say,
    "Say something to the rest of the server",
    "<text>" },
  { 1, level_user, "EMOTE", NULL, cmd_emote,
    "Emote something to the rest of the server",
    "<text>" },
  { 1, level_guest, "ALBUMS", NULL, cmd_albums,
    "List all albums",
    "" },
  { 1, level_guest, "TRACKS", NULL, cmd_tracks,
    "List all tracks belonging to an album",
    "[<album>]" },
  { 1, level_guest, "RATINGS", NULL, cmd_ratings,
    "List ratings for a given track or current track",
    "[<album> <track>]" },
  { 1, level_guest, "QUEUE", NULL, cmd_queue,
    "Show queue contents, add an album (optionally random order) or a track",
    "[<album> [<track>|RANDOM]]" },
  { 1, level_user, "UNQUEUE", NULL, cmd_unqueue,
    "Remove track from the queue",
    "<album> <track>" },
  { 1, level_user, "PLAY", NULL, cmd_play,
    "Start playing queued tracks",
    "" },
  { 1, level_user, "STOP", NULL, cmd_stop,
    "Stop playing current song",
    "" },
  { 1, level_user, "NEXT", NULL, cmd_next,
    "Stop playing current song and play next in queue",
    "" },
  { 1, level_user, "PAUSE", NULL, cmd_pause,
    "Pause current song, use PLAY to resume",
    "" },
  { 1, level_user, "CLEAR", NULL, cmd_clear,
    "Clear the queue",
    "" },
  { 1, level_user, "REPEAT", NULL, cmd_repeat,
    "Repeat the current track by putting it in the queue",
    "" },
  { 1, level_guest, "RANDOM", NULL, cmd_random,
    "Enable, disable or show current random song selection mode",
    "[off|on]" },
  { 1, level_user, "FILTER", NULL, cmd_filter,
    "Limit random play to tracks matching filter",
    "off|<filter>" },
  { 1, level_guest, "FACTOR", NULL, cmd_factor,
    "Set random factor (default 0.6)",
    "<0=play worst, 0.5=play any, 0.99=play best, auto>" },
  { 1, level_guest, "TOP", NULL, cmd_top,
    "Show most likely to be played tracks, default 20 lines",
    "[<lines>]" },
  { 1, level_guest, "VOLUME", NULL, cmd_volume,
    "Display or set the volume (+/- for relative)",
    "[[+|-][<0-100>]]" },
  { 1, level_guest, "BASS", NULL, cmd_bass,
    "Display or set the bass level (+/- for relative)",
    "[[+|-][<0-100>]]" },
  { 1, level_guest, "TREBLE", NULL, cmd_treble,
    "Display or set the treble level (+/- for relative)",
    "[[+|-][<0-100>]]" },
  { 1, level_guest, "HISTORY", NULL, cmd_history,
    "View list of last tracks played (default shows last 20)",
    "[<entries> [<from entry>]]" },
  { 1, level_user, "RATE", NULL, cmd_rate,
    "Rate the current track, a specified track or all tracks in an album",
    "[<album> [<track>]] <AWFUL|BAD|NEUTRAL|GOOD|SUPERB>" },
  { 1, level_user, "CHECK", NULL, cmd_check,
    "Check ratings of same-named tracks and inform of inconsitencies",
    "" },
  { 1, level_guest, "SEARCH", NULL, cmd_search,
    "Search for a track based on string",
    "<string>" },
  { 1, level_guest, "SEARCHF", NULL, cmd_searchf,
    "Search for a track based on filter",
    "<filter>" },
  { 1, level_guest, "ASEARCH", NULL, cmd_asearch,
    "Search for a album based on string",
    "<string>" },
  { 1, level_guest, "INFO", NULL, cmd_info,
    "Display information on the current track, a specified track or an album",
    "[<album> [<track>]]" },
  { 1, level_user, "PASSWORD", NULL, cmd_password,
    "Change your password",
    "<old password> <new password>" },
#ifdef IDEA
  { 1, level_user, "IDEA", NULL, cmd_idea,
    "Store your idea in a file",
    "<string>" },
#endif
  { 1, level_master, "MOO", NULL, cmd_moo,
    "Moo!",
    "" },
  { 1, level_master, "CREATE", NULL, cmd_create,
    "Create a user, or change details if it exists",
    "<username> <password> <GUEST|USER|PRIV|MASTER>" },
  { 1, level_master, "REMOVE", NULL, cmd_remove,
    "Removes a user from the system - WILL PURGE ALL RATINGS!",
    "<username>" },
  { 1, level_master, "LEVEL", NULL, cmd_level,
    "Set a user's user level",
    "<username> <GUEST|USER|PRIV|MASTER>" },
  { 1, level_priv, "KICK", NULL, cmd_kick,
    "Kick a user off the server and prohibit from re-connecting for a bit",
    "<user> [0|<minutes>]" },
  { 1, level_priv, "RESET", NULL, cmd_reset,
    "Clear everything and reload tracks",
    "" },
  { 1, level_priv, "SYNC", NULL, cmd_sync,
    "Ensure track information on disk and in memory are synchronised",
    "" },
  { 1, level_priv, "GAP", NULL, cmd_gap,
    "Set or display the gap between each track, in seconds",
    "[<delay>]" },
  { 1, level_master, "SHUTDOWN", NULL, cmd_shutdown,
    "Shutdown server when server isn't playing a song",
    "" },

  { 1, level_priv, "SET", cmd_set_cmds, NULL,
    "Commands for setting track and album information",
    "<params>" },
  { 1, level_priv, "CHANNEL", cmd_channel_cmds, NULL,
    "Commands relating to channels",
    "<params>" },
  { 1, level_guest, "X", cmd_x_cmds, NULL,
    "Extra commands for advanced users or computers",
    "<params>" },

  { 0, level_guest, NULL, NULL, NULL, NULL, NULL }
};

/*** functions ***/

static void cmd_moo(t_client *cl, t_cmdparams *cp)
{
  (void)cp;

  mserv_response(cl, "MOO", NULL);
  if (cl->mode == mode_human) {
    mserv_send(cl, "[]   __  __          _\r\n", 0);
    mserv_send(cl, "[]  |  \\/  |___  ___| |\r\n", 0);
    mserv_send(cl, "[]  | |\\/| / _ \\/ _ \\_|\r\n", 0);
    mserv_send(cl, "[]  |_|  |_\\___/\\___(_)\r\n", 0);
  }
}

static void cmd_quit(t_client *cl, t_cmdparams *cp)
{
  (void)cp;

  mserv_response(cl, "QUIT", NULL);
  mserv_close(cl);
}

static void cmd_help(t_client *cl, t_cmdparams *cp)
{
  t_cmds *cmdsptr, *c;
  int i = 0;
  char buffer[1024];
  const char *line = cp->line;
  char command_prefix[256]; /* to hold, e.g. CHANNEL OUTPUT */

  command_prefix[0] = '\0';
  cmdsptr = cmd_cmds;
again:
  while (*line == ' ')
    line++;
  if (*line) {
    for (; cmdsptr->name; cmdsptr++) {
      if (cmdsptr->authed && !cl->authed)
	continue;
      if (!mserv_checklevel(cl, cmdsptr->userlevel))
	continue;
      if (strnicmp(line, cmdsptr->name, strlen(cmdsptr->name)) == 0) {
        if (line[strlen(cmdsptr->name)] != '\0' &&
            line[strlen(cmdsptr->name)] != ' ')
          continue;
        if (cmdsptr->sub_cmds != NULL) {
          /* sub-command help required */
          strcat(command_prefix, " ");
          strcat(command_prefix, cmdsptr->name);
          line+= strlen(cmdsptr->name);
          cmdsptr = cmdsptr->sub_cmds;
          goto again;
        }
	if (cl->mode == mode_human) {
	  snprintf(buffer, sizeof(buffer), "[] %s\r\n[] Syntax:%s %s %s\r\n",
                   cmdsptr->help, command_prefix, cmdsptr->name,
                   cmdsptr->syntax);
	  mserv_send(cl, buffer, 0);
	} else {
	  mserv_responsent(cl, "HELP", "%s", cmdsptr->name);
	  snprintf(buffer, sizeof(buffer), "%s\r\nSyntax:%s %s %s\r\n.\r\n",
                   cmdsptr->help, command_prefix, cmdsptr->name,
                   cmdsptr->syntax);
	  mserv_send(cl, buffer, 0);
	}
	return;
      }
    }
    mserv_response(cl, "NOHELP", "%s", cp->line);
    return;
  }
  if (command_prefix[0])
    mserv_responsent(cl, "HELPCX", "%s", command_prefix + 1); /* skip space */
  else
    mserv_responsent(cl, "HELPC", "%s", VERSION);
  if (cl->mode == mode_human) {
    mserv_send(cl, "[] Commands:\r\n[]     ", 0);
    for (c = cmdsptr; c->name; c++) {
      if (c->authed && !cl->authed)
	continue;
      if (!mserv_checklevel(cl, c->userlevel))
	continue;
      if (i > 4) {
	i = 0;
	mserv_send(cl, "\r\n[]     ", 0);
      }
      mserv_send(cl, c->name, 0);
      if (strlen(c->name) < 14) {
	mserv_send(cl, "                ", 14-strlen(c->name));
      } else {
	mserv_send(cl, " ", 1);
      }
      i++;
    }
    mserv_send(cl, "\r\n", 0);
    if (cmdsptr == cmd_cmds) {
      mserv_send(cl, "[] To report bugs in the implementation please "
		 "send email to james@squish.net\r\n", 0);
      mserv_send(cl, "[] Thanks for using this program.\r\n", 0);
    }
  } else {
    for (c = cmdsptr; c->name; c++) {
      if (c->authed && !cl->authed)
	continue;
      if (!mserv_checklevel(cl, c->userlevel))
	continue;
      mserv_send(cl, c->name, 0);
      mserv_send(cl, "\r\n", 0);
    }
    mserv_send(cl, ".\r\n", 0);
  }
}

static void cmd_user(t_client *cl, t_cmdparams *cp)
{
  if (!*cp->line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  if (cl->authed) {
    mserv_response(cl, "AAUTHED", NULL);
    return;
  }
  strncpy(cl->user, cp->line, USERNAMELEN);
  cl->user[USERNAMELEN] = '\0';
  mserv_response(cl, "USEROK", NULL);
}

static void cmd_pass(t_client *cl, t_cmdparams *cp)
{
  char password[16];
  char *s;
  t_acl *acl;

  if (!*cp->line) {
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
  if (!(s = strchr(cp->line, ' '))) {
    strncpy(password, cp->line, 15);
    password[15] = '\0';
    cl->mode = mode_human;
  } else {
    if ((s - cp->line) > 15) {
      strncpy(password, cp->line, 15);
      password[15] = '\0';
    } else {
      strncpy(password, cp->line, (s - cp->line));
      password[s - cp->line] = '\0';
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
  mserv_log("%s: %s successfully logged in", inet_ntoa(cl->sin.sin_addr),
            cl->user);
  cl->authed = 1;
  cl->userlevel = acl->userlevel;
  cl->loggedinatsong = mserv_n_songs_started;
  cl->last_unrated = time(NULL);
  mserv_response(cl, "ACLSUCC", "%s", cl->user);
  if (cl->userlevel != level_guest && cl->mode != mode_computer)
    mserv_recalcratings();

  if (mserv_autofactor) {
    if (cl->mode != mode_computer && cl->userlevel != level_guest) {
      mserv_factor = 0.99;
      mserv_log("Autofactor: %s logged in, factor set to %.2f",
		cl->user,
		mserv_factor);
    }
  }
  
  return;
}

static void cmd_status(t_client *cl, t_cmdparams *cp)
{
  char token[16];
  char *a;
  int i;
  t_channel *channel = channel_find(cl->channel);
  t_trkinfo *playing = channel_getplaying(channel);
  long playTimeMs = channel_getplaying_msecs(channel);
  long playTimeSeconds = playTimeMs / 1000;
  time_t now = time(NULL);
  
  (void)cp;
  if (playing && playing->track) {
    a = channel_paused(channel) ? "STATPAU" : "STATPLA";
    mserv_response(cl, a, "%s\t%d\t%d\t%d\t%d\t%s\t%s\t%d\t%d:%02d\t%s"
                   "\t%.2f",
		   mserv_getfilter(), mserv_filter_ok, mserv_filter_notok,
		   playing->track->album->id,
		   playing->track->n_track,
		   playing->track->author,
		   playing->track->name,
		   now - playTimeSeconds,
		   playTimeSeconds / 60, playTimeSeconds % 60,
		   mserv_random ? "ON" : "OFF", mserv_factor);
    if (cl->mode == mode_human) {
      for (i = 1; i <= 7; i++) {
	sprintf(token, "STAT%d", i);
	mserv_response(cl, token, "%s\t%d\t%d\t%d\t%d\t%s\t%s\t%d\t%d:%02d.%d"
		       "\t%s\t%.2f",
		       mserv_getfilter(), mserv_filter_ok, mserv_filter_notok,
		       playing->track->album->id, playing->track->n_track,
                       playing->track->author, playing->track->name, 
		       now - playTimeSeconds,
                       playTimeSeconds / 60, playTimeSeconds % 60,
		       (playTimeMs % 1000) / 100,
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

static void cmd_userinfo(t_client *cl, t_cmdparams *cp)
{
  t_acl *acl;
  t_track *track;
  t_rating *rate1, *rate2;
  unsigned int total = 0, heard = 0, unheard = 0;
  unsigned int awful = 0, bad = 0, neutral = 0, good = 0, superb = 0;
  char token[16];
  unsigned int diffs = 0, indexnum = 0;
  double index;
  double satisfaction;
  int i;
  t_client *rcl;
  const char *line = cp->line;

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
    rate2 = mserv_getrate(cp->ru, track);
    if (rate1 && rate2 && rate1->rating && rate2->rating) {
      diffs+= abs(rate1->rating - rate2->rating);
      indexnum++;
    }
  }
  if (indexnum)
    index = 1.0 - diffs/(double)(4*indexnum);
  else
    index = 0;

  rcl = mserv_user2client(line);
  if (rcl == NULL ||
      !mserv_getsatisfaction(rcl, &satisfaction))
  {
    /* No satisfaction value available for this user, use the
     * goal value as a guess for what this user will think. */
    satisfaction = mserv_getsatisfactiongoal();
  }
  
  mserv_response(cl, "INFU", "%s\t%s\t"
		 "%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t"
		 "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%.1f\t%.1f",
		 acl->user, cp->ru, 100*superb/(double)total,
		 100*good/(double)total, 100*neutral/(double)total,
		 100*bad/(double)total, 100*awful/(double)total,
		 100*heard/(double)total, 100*unheard/(double)total,
		 superb, good, neutral, bad, awful, heard, unheard,
		 100*index, 100*satisfaction);
  if (cl->mode == mode_human) {
    for (i = 1; i <= 9; i++) {
      sprintf(token, "INFU%d", i);
      mserv_response(cl, token, "%s\t%s\t"
		     "%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t"
		     "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%.1f\t%.1f",
		     acl->user, cp->ru, 100*superb/(double)total,
		     100*good/(double)total, 100*neutral/(double)total,
		     100*bad/(double)total, 100*awful/(double)total,
		     100*heard/(double)total, 100*unheard/(double)total,
		     superb, good, neutral, bad, awful, heard, unheard,
		     100*index, 100*satisfaction);
    }
  }
}

static void cmd_who(t_client *cl, t_cmdparams *cp)
{
  t_client *clu;
  char tmp[256];
  int total = 0;
  int connected = 0;

  (void)cp;
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

static void cmd_password(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  t_acl *acl;
  char *str[3];

  strcpy(linespl, cp->line);

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

static void cmd_level(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char *str[3];
  t_acl *acl;
  t_userlevel *ul;
  
  strcpy(linespl, cp->line);

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

static void cmd_create(t_client *cl, t_cmdparams *cp)
{
  /* usernames must never start _ or have a : in them */
  char linespl[LINEBUFLEN];
  char *str[4];
  char *p;
  char c;
  t_acl *acl, *acli;
  t_userlevel *ul;

  strcpy(linespl, cp->line);
  
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

static void cmd_remove(t_client *cl, t_cmdparams *cp)
{
  t_acl *acl, *aclp;
  t_track *track;
  t_rating *rate, *rp;

  for (aclp = NULL, acl = mserv_acl; acl; aclp = acl, acl = acl->next) {
    if (!stricmp(acl->user, cp->line)) {
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

static void cmd_albums(t_client *cl, t_cmdparams *cp)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  t_album *album;

  (void)cp;
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

static void cmd_tracks(t_client *cl, t_cmdparams *cp)
{
  char buffer[USERNAMELEN+AUTHORLEN+NAMELEN+64];
  t_album *album;
  unsigned int id;
  unsigned int i;
  char *end;
  t_rating *rate;
  t_channel *channel = channel_find(cl->channel);
  t_trkinfo *playing = channel_getplaying(channel);

  if (!*cp->line) {
    if (playing == NULL || playing->track == NULL) {
      mserv_response(cl, "NOTHING", NULL);
      return;
    }
    id = playing->track->album->id;
  } else {
    id = strtol(cp->line, &end, 10);
    if (!*cp->line || *end) {
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
  for (i = 0; i < album->ntracks; i++) {
    if (album->tracks[i]) {
      rate = mserv_getrate(cp->ru, album->tracks[i]);
      if (cl->mode == mode_human) {
        mserv_send_trackinfo(cl, album->tracks[i],  rate, 0, NULL);
      } else {
	sprintf(buffer, "%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
		album->tracks[i]->album->id, album->tracks[i]->n_track,
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

static void cmd_ratings(t_client *cl, t_cmdparams *cp)
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
  t_channel *channel = channel_find(cl->channel);
  t_trkinfo *playing = channel_getplaying(channel);

  strcpy(linespl, cp->line);

  if (!*cp->line) {
    if (playing == NULL || (track = playing->track) == NULL) {
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
  mserv_responsent(cl, "RATINGS", "%d\t%d\t%s\t%s", track->album->id,
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

static void cmd_unqueue(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char *str[3];
  unsigned int n_album, n_track;
  char *end;
  t_track *track;
  t_queue *p, *q;

  strcpy(linespl, cp->line);

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
    if (q->trkinfo.track == track)
      break;
  }
  if (q) {
    mserv_broadcast("UNQ", "%s\t%d\t%d\t%s\t%s", cl->user,
		    q->trkinfo.track->album->id, q->trkinfo.track->n_track,
		    q->trkinfo.track->author, q->trkinfo.track->name);
    if (cl->mode != mode_human)
      mserv_response(cl, "UNQR", "%d\t%d\t%s\t%s",
		     q->trkinfo.track->album->id, q->trkinfo.track->n_track,
		     q->trkinfo.track->author, q->trkinfo.track->name);
    if (p)
      p->next = q->next;
    else
      mserv_queue = q->next;
    free(q);
    return;
  }
  mserv_response(cl, "NOUNQ", NULL);
}

static void cmd_queue(t_client *cl, t_cmdparams *cp)
{
  char buffer[USERNAMELEN + AUTHORLEN + NAMELEN + 64];
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
  unsigned char *tracktally;

  if (!*cp->line) {
    if (!mserv_queue) {
      mserv_response(cl, "NOQUEUE", NULL);
      return;
    }
    mserv_responsent(cl, "SHOW", NULL);
    for (q = mserv_queue; q; q = q->next) {
      rate = mserv_getrate(cp->ru, q->trkinfo.track);
      if (cl->mode == mode_human) {
        mserv_send_trackinfo(cl, q->trkinfo.track, rate, 0, q->trkinfo.user);
      } else {
	sprintf(buffer, "%s\t%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
		q->trkinfo.user, q->trkinfo.track->album->id,
		q->trkinfo.track->n_track, q->trkinfo.track->author,
		q->trkinfo.track->name,	mserv_ratestr(rate),
		(q->trkinfo.track->duration / 100) / 60,
		(q->trkinfo.track->duration / 100) % 60);
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
  strcpy(linespl, cp->line);
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
      if ((tracktally = malloc(album->ntracks)) == NULL) {
        mserv_log("Out of memory creating tally for album randomisation");
      } else {
        /* this code seems to think tracks could be missing in an album?! */
        for (i = 0; i < album->ntracks; i++) {
          if (album->tracks[i]) {
            numtracks++;
            tracktally[i] = 0;
          } else {
            tracktally[i] = 1;
          }
        }
        for (i = 0; i < numtracks; i++) {
          j = ((double)(numtracks-i))*rand()/(RAND_MAX+1.0);
          for (k = 0; k < album->ntracks; k++) {
            if (!tracktally[k]) {
              if (!j--)
                break;
            }
          }
          if (k >= album->ntracks || tracktally[k]) {
            /* assertion - should never happen */
            mserv_log("Internal error in randomising album (%d)", k);
            break;
          }
          tracktally[k] = 1;
          cmd_queue_sub(cl, album, k+1, 0);
        }
        free(tracktally);
      }
    } else {     
      for (i = 0; i < album->ntracks; i++) {
	if (album->tracks[i])
	  cmd_queue_sub(cl, album, i+1, 0);
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
  if (n_track < 1 || n_track > album->ntracks || !album->tracks[n_track-1]) {
    mserv_response(cl, "NOTRACK", NULL);
    return;
  }
  if (cmd_queue_sub(cl, album, n_track, 1) == -1) {
    mserv_response(cl, "QNONE", NULL);
    return;
  }
  if (cl->mode != mode_human)
    mserv_send(cl, ".\r\n", 0);
}

static int cmd_queue_sub(t_client *cl, t_album *album, int n_track,
			       int header)
{
  char buffer[USERNAMELEN + AUTHORLEN + NAMELEN + 64];
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
	    album->tracks[i]->album->id, album->tracks[i]->n_track,
	    album->tracks[i]->author, album->tracks[i]->name,
	    mserv_ratestr(rate));
    mserv_send(cl, buffer, 0);
  }
  for (client = mserv_clients; client; client = client->next) {
    rate = mserv_getrate(client->user, album->tracks[i]);
    if (client->mode == mode_human) {
      mserv_send_trackinfo(client, album->tracks[i], rate, 0, cl->user);
    } else if (client->mode == mode_rtcomputer) {
      sprintf(buffer, "=%d\t%s\t%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
	      lang->code, cl->user, album->tracks[i]->album->id,
	      album->tracks[i]->n_track, album->tracks[i]->author,
	      album->tracks[i]->name, mserv_ratestr(rate),
	      (album->tracks[i]->duration / 100) / 60,
	      (album->tracks[i]->duration / 100) % 60);
      mserv_send(client, buffer, 0);
    }
  }
  return 0;
}

static void cmd_play(t_client *cl, t_cmdparams *cp)
{
  char error[256];
  t_channel *channel = channel_find(cl->channel);
  t_trkinfo *playing = channel_getplaying(channel);

  (void)cp;
  if (channel_paused(channel)) {
    if (channel_unpause(mserv_channel, error, sizeof(error)) != MSERV_SUCCESS) {
      mserv_log("Failed to resume %s: %s", mserv_channel->name, error);
      mserv_response(cl, "SERROR", "Failed to resume: %s", error);
      return;
    }
    mserv_broadcast("RESUME", "%s\t%d\t%d\t%s\t%s", cl ? cl->user : "unknown",
                    playing->track->album->id,
                    playing->track->n_track,
                    playing->track->author, playing->track->name);
    if (cl->mode != mode_human) {
      /* humans will already have seen broadcast */
      if (playing) {
	t_track *track = playing->track;
	mserv_response(cl, "STARTED", "%d\t%d\t%s\t%s",
                       track->album->id, track->n_track,
                       track->author, track->name);
      } else {
        /* must be resuming silence */
        mserv_response(cl, "STARTED", "%d\t%d\t%s\t%s", 0, 0, "", "silence");
      }
    }
    return;
  }
  if (!channel_stopped(channel)) {
    if (playing) {
      t_track *track = playing->track;
      mserv_response(cl, "ALRPLAY", "%d\t%d\t%s\t%s",
                     track->album->id, track->n_track,
                     track->author, track->name);
    } else {
      /* must be in silence */
      mserv_response(cl, "ALRPLAY", "%d\t%d\t%s\t%s", 0, 0, "", "silence");
    }
    return;
  }
  if (mserv_player_playnext()) {
    mserv_response(cl, "NOMORE", NULL);
    return;
  }
  if (channel_start(mserv_channel, error, sizeof(error)) != MSERV_SUCCESS) {
    mserv_log("Failed to commence play on channel %s: %s", mserv_channel->name,
              error);
    mserv_response(cl, "CHANF", "%s", error);
  } else {
    if (cl->mode != mode_human) {
      /* humans will already have seen broadcast */
      t_track *track =
	(playing && playing->track) ? playing->track : mserv_player_playing.track;
      mserv_response(cl, "STARTED", "%d\t%d\t%s\t%s",
                     track->album->id, track->n_track,
                     track->author, track->name);
    }
  }
}

static void cmd_stop(t_client *cl, t_cmdparams *cp)
{
  t_channel *channel = channel_find(cl->channel);
  t_trkinfo *playing = channel_getplaying(channel);

  (void)cp;
  if (playing != NULL && !channel_stopped(channel)) {
    mserv_broadcast("STOPPED", "%s\t%d\t%d\t%s\t%s", cl->user,
		    playing->track->album->id, playing->track->n_track,
		    playing->track->author, playing->track->name);
    if (cl->mode != mode_human) {
      /* humans will already have seen broadcast */
      mserv_response(cl, "STOP", "%d\t%d\t%s\t%s",
		     playing->track->album->id, playing->track->n_track,
		     playing->track->author, playing->track->name);
    }
    mserv_abortplay();
  } else {
    mserv_response(cl, "NOTHING", NULL);
  }
}

static void cmd_pause(t_client *cl, t_cmdparams *cp)
{
  t_channel *channel = channel_find(cl->channel);
  t_trkinfo *playing = channel_getplaying(channel);
  char error[256];

  (void)cp;
  if (channel_paused(channel)) {
    mserv_response(cl, "APAUSED", NULL);
    return;
  }
  if (playing && playing->track) {
    if (channel_pause(mserv_channel, error, sizeof(error)) != MSERV_SUCCESS) {
      mserv_log("Failed to pause %s: %s", mserv_channel->name, error);
      mserv_response(cl, "SERROR", "Failed to pause: %s", error);
      return;
    }
    mserv_broadcast("PAUSE", "%s\t%d\t%d\t%s\t%s", cl ? cl->user : "unknown",
                    playing->track->album->id,
                    playing->track->n_track,
                    playing->track->author, playing->track->name);
    if (cl->mode != mode_human) {
      /* humans will already have seen broadcast */
      mserv_response(cl, "PAUSED", NULL);
    }
  } else {
    mserv_response(cl, "NOTHING", NULL);
  }
}

static void cmd_next(t_client *cl, t_cmdparams *cp)
{
  t_channel *channel = channel_find(cl->channel);
  t_trkinfo *playing = channel_getplaying(channel);
  char error[256];

  (void)cp;
  if (playing == NULL || playing->track == NULL) {
    mserv_response(cl, "NOTHING", NULL);
    return;
  }
  if (playing && playing->track)
    mserv_broadcast("SKIP", "%s", cl->user);
  if (playing && playing->track == mserv_player_playing.track) {
    /* we haven't moved to the next track yet */
    mserv_abortplay();
    if (mserv_player_playnext()) {
      mserv_broadcast("FINISH", NULL);
      if (cl->mode != mode_human)
        mserv_response(cl, "NOMORE", NULL);
      return;
    }
  }
  if (channel_start(channel, error, sizeof(error)) != MSERV_SUCCESS)
    mserv_log("Failed to commence play on channel %s: %s", channel->name,
              error);
  if (playing && cl->mode != mode_human) {
    /* humans will already have seen broadcast */
    t_track *track =
      (playing && playing->track) ? playing->track : mserv_player_playing.track;
    mserv_response(cl, "NEXT", "%d\t%d\t%s\t%s",
                   track->album->id, /* TODO: mserv_player_playing */
                   track->n_track, track->author,
                   track->name);
  }
}

static void cmd_clear(t_client *cl, t_cmdparams *cp)
{
  t_queue *q, *n;

  (void)cp;
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

static void cmd_repeat(t_client *cl, t_cmdparams *cp)
{
  t_channel *channel = channel_find(cl->channel);
  t_trkinfo *playing = channel_getplaying(channel);

  (void)cp;
  if (!playing || !playing->track) {
    mserv_response(cl, "NOTHING", NULL);
    return;
  }
  if (mserv_shutdown) {
    mserv_response(cl, "SHUTNQ", NULL);
    return;
  }
  if (!mserv_addqueue(cl, playing->track)) {
    mserv_broadcast("REPEAT", "%s\t%d\t%d\t%s\t%s", cl->user,
		    playing->track->album->id, playing->track->n_track,
		    playing->track->author, playing->track->name);
    if (cl->mode != mode_human)
      mserv_response(cl, "REPEAT", "%s\t%d\t%d\t%s\t%s", cl->user,
		     playing->track->album->id, playing->track->n_track,
		     playing->track->author, playing->track->name);
  } else {
    mserv_response(cl, "QNONE", NULL);
  }
}

static void cmd_random(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char *str[3];

  if (!*cp->line) {
    mserv_response(cl, "RANCUR", "%s", mserv_random ? "on" : "off");
    return;
  }
  if (cl->userlevel == level_guest) {
    mserv_response(cl, "ACLFAIL", NULL);
    return;
  }
  strcpy(linespl, cp->line);
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
  } else if (!stricmp(cp->line, "off")) {
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

static void cmd_filter(t_client *cl, t_cmdparams *cp)
{
  if (!*cp->line) {
    mserv_response(cl, "CURFILT", "%s\t%s\t%d\t%d", "", mserv_getfilter(),
		   mserv_filter_ok, mserv_filter_notok);
    return;
  }
  if (cl->userlevel == level_guest) {
    mserv_response(cl, "ACLFAIL", NULL);
    return;
  }
  if (!stricmp(cp->line, "off")) {
    mserv_setfilter("");
    mserv_broadcast("NFILTER", "%s", cl->user);
    mserv_recalcratings();
  } else {
    if (mserv_setfilter(cp->line) == -1) {
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

static void cmd_factor(t_client *cl, t_cmdparams *cp)
{
  double f = atof(cp->line);
  const char *line = cp->line;
  
  /* Skip leading space in the command line */
  while (*line == ' ')
    line++;
  
  if (!*line) {
    mserv_response(cl, "FACTCUR", "%.2f\t%s",
		   mserv_factor,
		   mserv_autofactor ? "enabled" : "disabled");
    return;
  }
  
  if (cl->userlevel == level_guest) {
    mserv_response(cl, "ACLFAIL", NULL);
    return;
  }
  
  if (stricmp(line, "auto") == 0) {
    mserv_autofactor = 1;
  } else if (f >=0 && f <= 0.99) {
    /* We got a good numeric value */
    mserv_autofactor = 0;
    mserv_factor = (double)((int)((f+0.005)*100))/100;
  } else {
    mserv_response(cl, "BADFACT", NULL);
    return;
  }
  
  mserv_broadcast("FACTOR", "%.2f\t%s\t%s", mserv_factor, cl->user,
		  mserv_autofactor ? "enabled" : "disabled");
  if (cl->mode != mode_human) {
    mserv_response(cl, "FACTSET", "%.2f\t%s", mserv_factor,
		   mserv_autofactor ? "enabled" : "disabled");
  }
}

static void cmd_top(t_client *cl, t_cmdparams *cp)
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
  if (*cp->line) {
    top_end = strtol(cp->line, &end, 10);
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
    rate = mserv_getrate(cp->ru, track);
    if (cl->mode == mode_human) {
      snprintf(bit, sizeof(bit), "%5.2f%%", prob > 0.9999 ? 99.99 : 100*prob);
      mserv_send_trackinfo(cl, track,  rate, 0, bit);
    } else {
      sprintf(buffer, "%2.2f\t%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n", 100*prob,
	      track->album->id, track->n_track, track->author, track->name,
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

static void cmd_volume(t_client *cl, t_cmdparams *cp)
{
  int val;

  if (*cp->line && cl->userlevel == level_guest) {
    mserv_response(cl, "ACLFAIL", NULL);
  } else if ((val = mserv_channelvolume(cl, cp->line)) != -1) {
    if (!*cp->line) {
      mserv_response(cl, "VOLCUR", "%d", val);
    } else {
      mserv_broadcast("VOLSET", "%d\t%s", val, cl->user);
      if (cl->mode != mode_human)
	mserv_response(cl, "VOLREP", "%d", val);
    }
  }
}

static void cmd_bass(t_client *cl, t_cmdparams *cp)
{
  (void)cl;
  (void)cp;
  
  mserv_response(cl, "NOTIMPL", NULL);
}

static void cmd_treble(t_client *cl, t_cmdparams *cp)
{
  (void)cl;
  (void)cp;
  
  mserv_response(cl, "NOTIMPL", NULL);
}

static void cmd_say(t_client *cl, t_cmdparams *cp)
{
  mserv_broadcast("SAY", "%s\t%s", cl->user, cp->line);
  if (cl->mode != mode_human)
    mserv_response(cl, "SAY", "%s\t%s", cl->user, cp->line);
}

static void cmd_emote(t_client *cl, t_cmdparams *cp)
{
  mserv_broadcast("EMOTE", "%s\t%s", cl->user, cp->line);
  if (cl->mode != mode_human)
    mserv_response(cl, "EMOTE", "%s\t%s", cl->user, cp->line);
}

static void cmd_history(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char buffer[USERNAMELEN + AUTHORLEN + NAMELEN + 64];
  char *str[3];
  int i;
  t_rating *rate;
  int start = 0;
  int entries = 20;
  char *end;

  /* [<entries> [<from entry>]] */

  if (*cp->line) {
    strcpy(linespl, cp->line);
    if (mserv_split(str, 2, linespl, " ") < 1) {
      mserv_response(cl, "BADPARM", NULL);
      return;
    }
    entries = strtol(str[0], &end, 10);
    if (!*str[0] || *end || entries <= 0) {
      mserv_response(cl, "NAN", NULL);
      return;
    }
    if (str[1]) {
      start = strtol(str[1], &end, 10);
      if (!*str[1] || *end || start <= 0) {
        mserv_response(cl, "NAN", NULL);
        return;
      }
      start--; /* offset from 0, not 1 */
    }
  }
  if (mserv_history[0] == NULL) {
    /* no entries at all! */
    mserv_response(cl, "NOHIST", NULL);
    return;
  }
  if (mserv_history[start] == NULL) {
    /* no entries for given range */
    mserv_response(cl, "NOHISTR", NULL);
    return;
  }
  mserv_responsent(cl, "HISTORY", NULL);
  for (i = start;
       mserv_history[i] && i < HISTORYLEN && i < (start + entries);
       i++) {
    rate = mserv_getrate(cp->ru, mserv_history[i]->track);
    if (cl->mode == mode_human) {
      mserv_send_trackinfo(cl, mserv_history[i]->track, rate, 0,
                           mserv_history[i]->user);
    } else {
      sprintf(buffer, "%s\t%d\t%d\t%s\t%s\t%s\r\n",
	      mserv_history[i]->user, mserv_history[i]->track->album->id,
	      mserv_history[i]->track->n_track,
	      mserv_history[i]->track->author, mserv_history[i]->track->name,
	      mserv_ratestr(rate));
      mserv_send(cl, buffer, 0);
    }
  }
  if (cl->mode != mode_human)
    mserv_send(cl, ".\r\n", 0);
}

static void cmd_rate(t_client *cl, t_cmdparams *cp)
{
  t_channel *channel = channel_find(cl->channel);
  t_trkinfo *playing = channel_getplaying(channel);
  char linespl[LINEBUFLEN];
  char *str[4];
  unsigned int n_album, n_track;
  int val;
  char *end;
  t_track *track, *track2;
  int n;
  unsigned int ui;
  t_album *album;
  t_rating *rate, *rate2;
  int ratetoo;
  int total;

  /* [<album> [<track>]] <rating> */

  strcpy(linespl, cp->line);
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
    if (!playing || !playing->track) {
      mserv_response(cl, "NOTHING", NULL);
      return;
    }
    n_album = playing->track->album->id;
    n_track = playing->track->n_track;
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
    if (playing && playing->track && n_album == playing->track->album->id &&
	n_track == playing->track->n_track) {
      mserv_broadcast("RATECUR", "%s\t%d\t%d\t%s\t%s\t%s", cl->user,
		      playing->track->album->id,
		      playing->track->n_track, track->author,
		      track->name, mserv_ratestr(rate));
      if (cl->mode != mode_human)
	mserv_response(cl, "RATED", "%s\t%d\t%d\t%s\t%s\t%s", cl->user,
		       playing->track->album->id,
		       playing->track->n_track, track->author,
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
	for (ui = 0; ui < album->ntracks; ui++) {
	  track2 = album->tracks[ui];
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
            mserv_send_trackinfo(cl, track2, rate2, 0, "");
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
    for (ui = 0; ui < album->ntracks; ui++) {
      if (album->tracks[ui]) {
	if ((rate2 = mserv_getrate(cl->user, album->tracks[ui])) == NULL ||
	    rate2->rating == 0) { /* 0 means heard, not rated */
	  if ((rate = mserv_ratetrack(cl, &album->tracks[ui], val)) == NULL) {
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

static void cmd_check(t_client *cl, t_cmdparams *cp)
{
  char buffer[AUTHORLEN + NAMELEN + 64];
  t_track *track1, *track2;
  t_author *author;
  t_rating *rate1, *rate2;
  int f = 0;
  unsigned int ui;

  if (*cp->line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  for (author = mserv_authors; author; author = author->next) {
    for (ui = 0; ui < author->ntracks; ui++) {
      if ((track1 = author->tracks[ui]) && (track2 = author->tracks[ui+1])) {
	if (!stricmp(track1->author, track2->author) &&
	    !stricmp(track1->name, track2->name)) {
	  rate1 = mserv_getrate(cp->ru, track1);
	  rate2 = mserv_getrate(cp->ru, track2);
	  if ((rate1 == NULL && rate2 == NULL) ||
	      (rate1 && rate2 && rate1->rating == rate2->rating) ||
	      (rate1 && !rate2 && rate1->rating == 0) ||
	      (!rate1 && rate2 && rate2->rating == 0))
	    continue;
	  if (!f) {
	    f = 1;
	    mserv_responsent(cl, "CHECKR", NULL);
	  }
	  if (cl->mode == mode_human) {
            mserv_send_trackinfo(cl, track1, rate1, 0, "");
            mserv_send_trackinfo(cl, track2, rate2, 0, "");
	  } else {
	    sprintf(buffer, "%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
		    track1->album->id, track1->n_track,
		    track1->author, track1->name,
		    mserv_ratestr(rate1),
		    (track1->duration / 100) / 60,
		    (track1->duration / 100) % 60);
	    mserv_send(cl, buffer, 0);
	    sprintf(buffer, "%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
		    track2->album->id, track2->n_track,
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

static void cmd_search(t_client *cl, t_cmdparams *cp)
{
  char buffer[AUTHORLEN + NAMELEN + 64];
  t_track *track;
  t_author *author;
  t_rating *rate;
  int f = 0;
  unsigned int ui;

  if (!*cp->line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  for (author = mserv_authors; author; author = author->next) {
    for (ui = 0; ui < author->ntracks; ui++) {
      if ((track = author->tracks[ui])) {
	if ((stristr(track->author, cp->line)) ||
	    (stristr(track->name, cp->line))) {
	  if (!f) {
	    f = 1;
	    mserv_responsent(cl, "SEARCHA", NULL);
	  }
	  rate = mserv_getrate(cp->ru, track);
	  if (cl->mode == mode_human) {
            mserv_send_trackinfo(cl, track, rate, 0, "");
	  } else {
	    sprintf(buffer, "%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
		    track->album->id, track->n_track,
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

static void cmd_asearch(t_client *cl, t_cmdparams *cp)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  t_album *album;
  int f = 0;

  if (!*cp->line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  for (album = mserv_albums; album; album = album->next) {
    if ((stristr(album->author, cp->line)) ||
	(stristr(album->name, cp->line))) {
      if (!f) {
	f = 1;
	mserv_responsent(cl, "ASRCHA", NULL);
      }
      if (cl->mode == mode_human) {
	sprintf(buffer, "[] %3d %-20.20s %-51.51s\r\n",
		album->id, album->author, album->name);
	mserv_send(cl, buffer, 0);
      } else {
	sprintf(buffer, "%d\t%s\t%s\r\n", album->id,
		album->author, album->name);
	mserv_send(cl, buffer, 0);
      }
    }
  }
  if (f) {
    if (cl->mode != mode_human)
      mserv_send(cl, ".\r\n", 0);
  } else {
    mserv_response(cl, "ASRCHB", NULL);
  }
}

static void cmd_searchf(t_client *cl, t_cmdparams *cp)
{
  char buffer[AUTHORLEN + NAMELEN + 64];
  t_track *track;
  t_author *author;
  t_rating *rate;
  int f = 0;
  unsigned int ui;

  if (!*cp->line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  if (mserv_tracks == NULL || filter_check(cp->line, mserv_tracks) == -1) {
    mserv_response(cl, "BADFILT", NULL);
    return;
  }
  for (author = mserv_authors; author; author = author->next) {
    for (ui = 0; ui < author->ntracks; ui++) {
      if ((track = author->tracks[ui])) {
	if (filter_check(cp->line, track) == 1) {
	  if (!f) {
	    f = 1;
	    mserv_responsent(cl, "SEARCFA", NULL);
	  }
	  rate = mserv_getrate(cp->ru, track);
	  if (cl->mode == mode_human) {
            mserv_send_trackinfo(cl, track, rate, 0, "");
	  } else {
	    sprintf(buffer, "%d\t%d\t%s\t%s\t%s\t%ld:%02ld\r\n",
		    track->album->id, track->n_track,
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
static void cmd_idea(t_client *cl, t_cmdparams *cp)
{
  if (!*cp->line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  if (mserv_idea(cp->line) == -1)
    mserv_response(cl, "IDEAF", NULL);
  else
    mserv_response(cl, "IDEAD", NULL);
}
#endif

static void cmd_info(t_client *cl, t_cmdparams *cp)
{
  t_channel *channel = channel_find(cl->channel);
  t_trkinfo *playing = channel_getplaying(channel);
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
  
  if (!*cp->line) {
    if (playing == NULL || (track = playing->track) == NULL) {
      mserv_response(cl, "NOTHING", NULL);
      return;
    }
  } else {
    strcpy(linespl, cp->line);
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
      for (i = 0; i < album->ntracks; i++) {
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
  if ((album = mserv_getalbum(track->album->id)) == NULL) {
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
  rate = mserv_getrate(cp->ru, track);
  snprintf(year, sizeof(year), "%d", track->year);
  mserv_response(cl, "INFT",
		 "%d\t%d\t%s\t%s\t%s\t%s\t%s\t%d\t%s\t%.1f\t%.1f\t"
		 "%s\t%s\t%s\t%d:%02d.%d\t%s\t%d",
		 track->album->id, track->n_track, album->author, album->name,
		 track->author, track->name, track->year ? year : "unknown",
		 track->lastplay, ago, 100*track->prating, 100*track->rating,
		 mserv_ratestr(rate), track->genres,
		 track->filterok ? "included" : "excluded",
		 (track->duration/100)/60, (track->duration/100) % 60,
		 (track->duration/10) % 10, track->miscinfo, track->volume);
  if (cl->mode == mode_human) {
    for (i = 1; i <= 12; i++) {
      sprintf(token, "INFT%d", i);
      mserv_response(cl, token,
		     "%d\t%d\t%s\t%s\t%s\t%s\t%s\t%d\t%s\t%.1f\t%.1f\t"
		     "%s\t%s\t%s\t%d:%02d.%d\t%s\t%d",
		     track->album->id, track->n_track, album->author,
		     album->name, track->author, track->name,
		     track->year ? year : "unknown", track->lastplay,
		     ago, 100*track->prating, 100*track->rating,
		     mserv_ratestr(rate), track->genres,
		     track->filterok ? "included" : "excluded",
		     (track->duration/100)/60, (track->duration/100) % 60,
                     (track->duration/10) % 10, track->miscinfo,
                     track->volume);
    }
  }
}

static void cmd_date(t_client *cl, t_cmdparams *cp)
{
  time_t curtime = time(NULL);
  struct tm curtm = *localtime(&curtime);
  char date[64];
  char date_format[64];
  char time[16];
  char ct[32];

  (void)cp;
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

static void cmd_kick(t_client *kicker, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char *str[3];
  t_acl *acl;
  t_client *victim;
  char *end;
  int have_kicked = 0;
  
  strcpy(linespl, cp->line);
  
  if (mserv_split(str, 2, linespl, " ") < 1) {
    mserv_response(kicker, "BADPARM", NULL);
    return;
  }
  
  /* Find the user to kick */
  for (acl = mserv_acl; acl; acl = acl->next) {
    if (!stricmp(acl->user, str[0]))
      break;
  }
  if (!acl) {
    mserv_response(kicker, "NOUSER", NULL);
    return;
  }
  
  /* Prevent the victim from logging back in for some time */
  if (str[1]) {
    acl->nexttime = time(NULL) + 60 * strtol(str[1], &end, 10);
    if (!*cp->line || *end) {
      mserv_response(kicker, "NAN", NULL);
      return;
    }
  } else {
    acl->nexttime = time(NULL) + 60;
  }
  mserv_log("Kick of %s by %s", str[0], kicker->user);

  /* Kick the user from all his / hers active connections */
  for (victim = mserv_clients; victim; victim = victim->next) {
    if (victim->authed && stricmp(victim->user, str[0]) == 0) {
      if (kicker->mode != mode_human) /* humans will see 'disconnected' message */
	mserv_response(kicker, "USERCHG", NULL);
      mserv_informnt(victim, "KICKED", NULL);
      have_kicked = 1;
      mserv_close(victim);
    }
  }
  
  if (!have_kicked) {
    mserv_response(kicker, "USERCHG", NULL);
    return;
  }
}

static void cmd_reset(t_client *cl, t_cmdparams *cp)
{
  static int timer = 0;

  (void)cp;
  mserv_abortplay();
  acl_save();
  if (mserv_savechanges() && time(NULL) > timer) {
    timer = time(NULL) + 60;
    mserv_response(cl, "RESETER", NULL);
    return;
  }
  mserv_broadcast("RESETGO", "%s", cl->user);
  mserv_flush();
  mserv_log("Reset initiated by %s", cl->user);
  mserv_reset();
  mserv_broadcast("RESET", "%s", cl->user);
  if (cl->mode != mode_human)
    mserv_response(cl, "RESETR", NULL);
}

static void cmd_sync(t_client *cl, t_cmdparams *cp)
{
  (void)cp;

  mserv_log("Syncing disks/memory (%s)...", cl->user);
  mserv_savechanges();
  mserv_ensuredisk();
  mserv_log("Sync completed.", cl->user);
  mserv_response(cl, "SYNCED", NULL);
}

static void cmd_shutdown(t_client *cl, t_cmdparams *cp)
{
  t_channel *channel = channel_find(cl->channel);
  t_trkinfo *playing = channel_getplaying(channel);

  (void)cp;
  if (mserv_shutdown) {
    mserv_response(cl, "SHUTALR", NULL);
    return;
  }
  mserv_log("Shutdown initiated by %s", cl->user);
  mserv_shutdown = 1; /* 1 = shutdown requested */
  if (playing && playing->track) {
    mserv_broadcast("SHUTEND", cl->user);
  } else {
    mserv_broadcast("SHUTNOW", cl->user);
  }
  if (cl->mode != mode_human)
    mserv_response(cl, "SHUTYES", NULL);

  if (!playing || !playing->track) {
    mserv_flush();
    mserv_closedown(0);
  }
}

static void cmd_gap(t_client *cl, t_cmdparams *cp)
{
  double delay;
  char *end;

  if (!*cp->line) {
    mserv_response(cl, "GAPCUR", "%.1f", mserv_getgap());
    return;
  }    
  delay = strtod(cp->line, &end);
  if (!*cp->line || *end || delay < 0 || delay > 3600) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  mserv_setgap(delay);
  mserv_broadcast("DELAY", "%s\t%.1f", cl->user, delay);
  if (cl->mode != mode_human)
    mserv_response(cl, "DELAYR", NULL);
}

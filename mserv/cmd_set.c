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
#include <string.h>
#include <stdio.h>

#include "mserv.h"
#include "misc.h"
#include "cmd_set.h"

/*** file-scope (static) function declarations ***/

static void cmd_set_author(t_client *cl, t_cmdparams *cp);
static void cmd_set_name(t_client *cl, t_cmdparams *cp);
static void cmd_set_genre(t_client *cl, t_cmdparams *cp);
static void cmd_set_year(t_client *cl, t_cmdparams *cp);
static void cmd_set_volume(t_client *cl, t_cmdparams *cp);
static void cmd_set_albumauthor(t_client *cl, t_cmdparams *cp);
static void cmd_set_albumname(t_client *cl, t_cmdparams *cp);

/*** file-scope (static) globals ***/

/*** externed variables ***/

t_cmds cmd_set_cmds[] = {
  { 1, level_priv, "AUTHOR", NULL, cmd_set_author,
    "Set the author of a track",
    "<album> <track> <author>" },
  { 1, level_priv, "NAME", NULL, cmd_set_name,
    "Set the name of a track",
    "<album> <track> <name>" },
  { 1, level_priv, "GENRE", NULL, cmd_set_genre,
    "Set the genre, or genres, of a track or entire album",
    "<album> [<track>] <genre>[,<genre>]*" },
  { 1, level_priv, "YEAR", NULL, cmd_set_year,
    "Set the year of a track",
    "<album> <track> <0|year>" },
  { 1, level_priv, "VOLUME", NULL, cmd_set_volume,
    "Set the volume of a track (in percentage - 100 is normal)",
    "<album> <track> <0-1000>" },
  { 1, level_priv, "ALBUMAUTHOR", NULL, cmd_set_albumauthor,
    "Set the author of an album",
    "<album> <author>" },
  { 1, level_priv, "ALBUMNAME", NULL, cmd_set_albumname,
    "Set the name of an album",
    "<album> <name>" },
  { 0, level_guest, NULL, NULL, NULL, NULL, NULL }
};

/*** functions ***/

static void cmd_set_author(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char *str[4];
  unsigned int n_album, n_track;
  char *end;
  t_track *track;
  
  /* <album> <track> <author> */

  strcpy(linespl, cp->line);

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
  mserv_broadcast("AUTH", "%s\t%d\t%d\t%s\t%s\t%s", cl->user, track->album->id,
		  track->n_track, track->author, track->name, str[2]);
  if (cl->mode != mode_human)
    mserv_response(cl, "AUTHR", "%s\t%d\t%d\t%s\t%s\t%s", cl->user,
		   track->album->id, track->n_track, track->author,
		   track->name, str[2]);
}

static void cmd_set_name(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char *str[4];
  unsigned int n_album, n_track;
  char *end;
  t_track *track;

  /* <album> <track> <name> */

  strcpy(linespl, cp->line);
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
  mserv_broadcast("NAME", "%s\t%d\t%d\t%s\t%s\t%s", cl->user, track->album->id,
		  track->n_track, track->author, track->name, str[2]);
  if (cl->mode != mode_human)
    mserv_response(cl, "NAMER", "%s\t%d\t%d\t%s\t%s\t%s", cl->user,
		   track->album->id, track->n_track, track->author,
		   track->name, str[2]);
}

static void cmd_set_genre(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char *str[4];
  unsigned int n_album, n_track;
  char *end;
  t_track *track;
  t_album *album;
  int n;
  unsigned int ui;

  /* <album> [<track>] <genre>[,genre]* */

  strcpy(linespl, cp->line);
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
		    track->album->id, track->n_track,
		    track->author, track->name, str[n-1]);
    if (cl->mode != mode_human)
      mserv_response(cl, "GENRER", "%s\t%d\t%d\t%s\t%s\t%s", cl->user,
		     track->album->id, track->n_track, track->author,
		     track->name, str[n-1]);
  } else {
    /* set genre of album */
    if ((album = mserv_getalbum(n_album)) == NULL) {
      mserv_response(cl, "NOALBUM", NULL);
      return;
    }
    for (ui = 0; ui < album->ntracks; ui++) {
      if (album->tracks[ui]) {
	/* altertrack invalidates track pointer */
	if (mserv_altertrack(album->tracks[ui], NULL, NULL, str[n-1],
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

static void cmd_set_year(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char *str[4];
  unsigned int n_album, n_track, year;
  char *end;
  t_track *track;

  /* <album> <track> <year> */

  strcpy(linespl, cp->line);
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
  if (year < 100 || year > 10000) {
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
  mserv_broadcast("YEAR", "%s\t%d\t%d\t%s\t%s\t%d", cl->user, track->album->id,
		  track->n_track, track->author, track->name, year);
  if (cl->mode != mode_human)
    mserv_response(cl, "YEARR", "%s\t%d\t%d\t%s\t%s\t%d", cl->user,
		   track->album->id, track->n_track, track->author,
		   track->name, year);
  mserv_savechanges();
}

static void cmd_set_volume(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char *str[4];
  unsigned int n_album, n_track;
  int volume;
  char *end;
  t_track *track;

  /* <volume> */

  strcpy(linespl, cp->line);
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
  volume = strtol(str[2], &end, 10);
  if (!*str[2] || *end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  if (volume < 0 || volume > 1000) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  if ((track = mserv_gettrack(n_album, n_track)) == NULL) {
    mserv_response(cl, "NOTRACK", NULL);
    return;
  }
  track = mserv_checkdisk_track(track);
  track->volume = volume;
  track->modified = 1;
  mserv_broadcast("VOLUME", "%s\t%d\t%d\t%s\t%s\t%d", cl->user, track->album->id,
		  track->n_track, track->author, track->name, volume);
  if (cl->mode != mode_human)
    mserv_response(cl, "VOLUMER", "%s\t%d\t%d\t%s\t%s\t%d", cl->user,
		   track->album->id, track->n_track, track->author,
		   track->name, volume);
  mserv_savechanges();
}

static void cmd_set_albumauthor(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char *str[3];
  unsigned int n_album;
  char *end;
  t_album *album;

  /* <album> <author> */

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

static void cmd_set_albumname(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char *str[3];
  unsigned int n_album;
  char *end;
  t_album *album;

  /* <album> <name> */

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

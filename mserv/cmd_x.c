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

#include "mserv.h"
#include "cmd.h"

/*** file-scope (static) function declarations ***/

void mserv_cmd_x(t_client *cl, t_cmdparams *cp);
static void mserv_cmd_x_authors(t_client *cl, t_cmdparams *cp);
static void mserv_cmd_x_authorid(t_client *cl, t_cmdparams *cp);
static void mserv_cmd_x_authorinfo(t_client *cl, t_cmdparams *cp);
static void mserv_cmd_x_authortracks(t_client *cl, t_cmdparams *cp);
static void mserv_cmd_x_genres(t_client *cl, t_cmdparams *cp);
static void mserv_cmd_x_genreid(t_client *cl, t_cmdparams *cp);
static void mserv_cmd_x_genreinfo(t_client *cl, t_cmdparams *cp);
static void mserv_cmd_x_genretracks(t_client *cl, t_cmdparams *cp);

/*** file-scope (static) globals ***/

/*** externed variables ***/

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

/*** functions ***/

void mserv_cmd_x(t_client *cl, t_cmdparams *cp)
{
  t_cmds *cmdsptr;
  int len;

  for (cmdsptr = mserv_x_cmds; cmdsptr->name; cmdsptr++) {
    if (!mserv_checklevel(cl, cmdsptr->userlevel))
      continue;
    len = strlen(cmdsptr->name);
    if (strnicmp(cp->line, cmdsptr->name, len) == 0) {
      if (cp->line[len] != '\0' && cp->line[len] != ' ')
	continue;
      cp->line+= len;
      while (*cp->line == ' ')
	cp->line++;
      if (cmdsptr->authed == 1 && cl->authed == 0)
	mserv_send(cl, "400 Not authenticated\r\n.\r\n", 0);
      else
	cmdsptr->function(cl, cp);
      return;
    }
  }
  mserv_response(cl, "BADCOM", NULL);
}

static void mserv_cmd_x_authors(t_client *cl, t_cmdparams *cp)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  t_author *author;
  int total, rated;
  unsigned int ui;
  t_rating *rate;

  mserv_responsent(cl, "AUTHORS", NULL);
  for (author = mserv_authors; author; author = author->next) {
    total = 0;
    rated = 0;
    for (ui = 0; ui < author->ntracks; ui++) {
      if (author->tracks[ui]) {
	total++;
	rate = mserv_getrate(cp->ru, author->tracks[ui]);
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

static void mserv_cmd_x_authorid(t_client *cl, t_cmdparams *cp)
{
  t_author *author;

  for (author = mserv_authors; author; author = author->next) {
    if (!stricmp(cp->line, author->name)) {
      mserv_response(cl, "AUTHID", "%d\t%s", author->id, author->name);
      return;
    }
  }
  mserv_response(cl, "NOAUTH", NULL);
}

static void mserv_cmd_x_authorinfo(t_client *cl, t_cmdparams *cp)
{
  unsigned int n_author;
  char *end;
  int total, rated;
  t_author *author;
  t_rating *rate;
  unsigned int ui;

  if (!*cp->line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_author = strtol(cp->line, &end, 10);
  if (*end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  for (author = mserv_authors; author; author = author->next) {
    if (author->id == n_author) {
      total = 0;
      rated = 0;
      for (ui = 0; ui < author->ntracks; ui++) {
	if (author->tracks[ui]) {
	  total++;
	  rate = mserv_getrate(cp->ru, author->tracks[ui]);
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

static void mserv_cmd_x_authortracks(t_client *cl, t_cmdparams *cp)
{
  unsigned int n_author;
  char *end;
  t_author *author;
  unsigned int ui;
  char bit[32];
  char buffer[AUTHORLEN+NAMELEN+64];
  t_rating *rate;

  if (!*cp->line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_author = strtol(cp->line, &end, 10);
  if (*end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  for (author = mserv_authors; author; author = author->next) {
    if (author->id == n_author) {
      mserv_responsent(cl, "AUTHTRK", "%d\t%s", author->id, author->name);
      for (ui = 0; ui < author->ntracks; ui++) {
	if (author->tracks[ui]) {
	  rate = mserv_getrate(cp->ru, author->tracks[ui]);
	  sprintf(bit, "%d/%d", author->tracks[ui]->n_album,
		  author->tracks[ui]->n_track);
	  if (cl->mode == mode_human) {
	    sprintf(buffer, "[] %7.7s %-1.1s %-20.20s %-44.44s\r\n", bit,
		    rate && rate->rating ? mserv_ratestr(rate) : "-",
		    author->tracks[ui]->author, author->tracks[ui]->name);
	    mserv_send(cl, buffer, 0);
	  } else {
	    sprintf(buffer, "%d\t%d\t%d\t%s\t%s\t%s\r\n", author->id,
		    author->tracks[ui]->n_album, author->tracks[ui]->n_track,
		    author->tracks[ui]->author, author->tracks[ui]->name,
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

static void mserv_cmd_x_genres(t_client *cl, t_cmdparams *cp)
{
  char buffer[AUTHORLEN+NAMELEN+64];
  t_genre *genre;
  int rated;
  unsigned int ui;
  t_rating *rate;

  mserv_responsent(cl, "GENRES", NULL);
  for (genre = mserv_genres; genre; genre = genre->next) {
    rated = 0;
    for (ui = 0; ui < genre->ntracks; ui++) {
      rate = mserv_getrate(cp->ru, genre->tracks[ui]);
      if (rate && rate->rating) /* rate->rating being 0 means 'heard' */
	rated++;
    }
    if (cl->mode == mode_human) {
      sprintf(buffer, "[] %3d %-62.62s %4d %4d\r\n", genre->id,
	      genre->name, rated, genre->ntracks);
    } else {
      sprintf(buffer, "%d\t%s\t%d\t%d\r\n", genre->id, genre->name,
	      genre->ntracks, rated);
    }
    mserv_send(cl, buffer, 0);
  }
  if (cl->mode != mode_human)
    mserv_send(cl, ".\r\n", 0);
}

static void mserv_cmd_x_genreid(t_client *cl, t_cmdparams *cp)
{
  t_genre *genre;

  for (genre = mserv_genres; genre; genre = genre->next) {
    if (!stricmp(cp->line, genre->name)) {
      mserv_response(cl, "GENID", "%d\t%s", genre->id, genre->name);
      return;
    }
  }
  mserv_response(cl, "NOGEN", NULL);
}

static void mserv_cmd_x_genreinfo(t_client *cl, t_cmdparams *cp)
{
  unsigned int n_genre;
  char *end;
  t_genre *genre;
  int rated;
  unsigned int ui;
  t_rating *rate;

  if (!*cp->line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_genre = strtol(cp->line, &end, 10);
  if (*end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  for (genre = mserv_genres; genre; genre = genre->next) {
    if (genre->id == n_genre) {
      rated = 0;
      for (ui = 0; ui < genre->ntracks; ui++) {
	rate = mserv_getrate(cp->ru, genre->tracks[ui]);
	if (rate && rate->rating) /* rate->rating being 0 means 'heard' */
	  rated++;
      }
      mserv_response(cl, "GENINF", "%d\t%s\t%d\t%d", genre->id, genre->name,
		     genre->ntracks, rated);
      return;
    }
  }
  mserv_response(cl, "NOGEN", NULL);
}

static void mserv_cmd_x_genretracks(t_client *cl, t_cmdparams *cp)
{
  unsigned int n_genre;
  char *end;
  t_genre *genre;
  unsigned int ui;
  char bit[32];
  char buffer[AUTHORLEN+NAMELEN+64];
  t_rating *rate;

  if (!*cp->line) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  n_genre = strtol(cp->line, &end, 10);
  if (*end) {
    mserv_response(cl, "NAN", NULL);
    return;
  }
  for (genre = mserv_genres; genre; genre = genre->next) {
    if (genre->id == n_genre) {
      mserv_responsent(cl, "GENTRK", "%d\t%s\t%d", genre->id, genre->name,
		       genre->ntracks);
      for (ui = 0; ui < genre->ntracks; ui++) {
	rate = mserv_getrate(cp->ru, genre->tracks[ui]);
	sprintf(bit, "%d/%d", genre->tracks[ui]->n_album,
		genre->tracks[ui]->n_track);
	if (cl->mode == mode_human) {
	  sprintf(buffer, "[] %7.7s %-1.1s %-20.20s %-44.44s\r\n", bit,
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

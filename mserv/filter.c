/*
All of the documentation and software included in the Mserv releases is
copyrighted by James Ponder <james@squish.net>.

Copyright 1999-2003 James Ponder.  All rights reserved.

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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "mserv.h"
#include "misc.h"
#include "filter.h"

#define PARSERDEBUG 0

/*** externed variables ***/

/* none */

/*** file-scope (static) globals ***/

static t_stack stack[MAXNSTACK];
static unsigned int stack_n = 0;

/*** functions ***/

static int filter_parseradd(const t_stack item)
{
  int i;

#ifdef PARSERDEBUG
  mserv_log("filter_parseradd: %d", item);
#endif
  if (stack_n >= MAXNSTACK)
    goto error;
  stack[stack_n++] = item;
  for (;;) {
#ifdef PARSERDEBUG
    for (i = 0; i < (int)stack_n; i++) {
      switch(stack[i]) {
      case stack_false:
	mserv_log("%d: false", i);
	break;
      case stack_true:
	mserv_log("%d: true", i);
	break;
      case stack_and:
	mserv_log("%d: and", i);
	break;
      case stack_or:
	mserv_log("%d: or", i);
	break;
      case stack_not:
	mserv_log("%d: not", i);
	break;
      case stack_open:
	mserv_log("%d: open", i);
	break;
      case stack_close:
	mserv_log("%d: close", i);
	break;
      }
    }
#endif
    switch(stack[stack_n-1]) {
    case stack_and:
    case stack_or:
    case stack_not:
    case stack_open:
      /* leave as is, and stop iteration */
      goto done;
    case stack_close:
      if (stack_n > 2 && ((stack[stack_n-2] == stack_false ||
			   stack[stack_n-2] == stack_true) &&
			  stack[stack_n-3] == stack_open)) {
	stack[stack_n-3] = stack[stack_n-2];
	stack_n-= 2;
      } else {
	goto error;
      }
      break;
    case stack_false:
    case stack_true:
      if (stack_n > 1) {
	switch(stack[stack_n-2]) {
	case stack_and:
	case stack_or:
	  if (stack_n > 2 && (stack[stack_n-3] == stack_false ||
			      stack[stack_n-3] == stack_true)) {
	    i = stack[stack_n-3] == stack_true ? 1 : 0;
	    if (stack[stack_n-2] == stack_and)
	      i = i && (stack[stack_n-1] == stack_true ? 1 : 0);
	    else
	      i = i || (stack[stack_n-1] == stack_true ? 1 : 0);
	    stack[stack_n-3] = i ? stack_true : stack_false;
	    stack_n-= 2;
	  } else {
	    goto error;
	  }
	  break;
	case stack_open:
	  /* leave this and stop iteration */
	  goto done;
	case stack_not:
	  stack[stack_n-2] = (stack[stack_n-1] ==
			      stack_true ? stack_false : stack_true);
	  stack_n--;
	  break;
	default:
	  goto error;
	}
	break;
      }
      /* just a lowly true/false */
      goto done;
    default:
      goto error;
    }
  }
 done:
#ifdef PARSERDEBUG
    mserv_log("filter_parseradd: returning success");
#endif
  return 0;
error:
#ifdef PARSERDEBUG
    mserv_log("filter_parseradd: returning error");
#endif
  return -1;
}

int filter_check(const char *filter, t_track *track)
{
  char *genres_str[MAXNGENRE+1];
  char genres_spl[GENRESLEN+1];
  int genres_n;
  t_client *cl;
  t_rating *rate;
  int ok, val;
  unsigned int val1, val2;
  char token[TOKENLEN+1];
  unsigned int token_n = 0;
  char c;
  int i;
  unsigned int ui;
  char *end, *p;
  char user[USERNAMELEN+1];
  unsigned int type, eq;

  stack_n = 0;

#ifdef PARSERDEBUG
  mserv_log("filter_check: checking %d/%d", track->n_album, track->n_track);
#endif

  if (!*filter)
    /* no filter */
    return 1;
  for (;;) {
    c = *filter++;
#ifdef PARSERDEBUG
    mserv_log("char %c", c);
#endif
    if (c == ' ')
      continue;
    if (c == '(') {
      if (token_n)
	goto error; /* in middle of token */
      if (filter_parseradd(stack_open) == -1)
	goto error;
    } else if (isalnum((int)c) || c == '-' || c == '=' || c == '<' ||
	       c == '>') {
      if (token_n > TOKENLEN)
	goto error;
      token[token_n++] = c;
    } else {
      if (token_n) {
	/* convert token to value */
	token[token_n] = '\0';
#ifdef PARSERDEBUG
	mserv_log("token %s", token);
#endif
	ok = 0;
	if ((p = strchr(token, '=')) != NULL &&
	    (val = mserv_strtorate(p+1)) != -1) {
	  /* user=rating - matches if user has rated it this rating */
	  if (p-token > USERNAMELEN)
	    goto error;
	  strncpy(user, token, p-token);
	  user[p-token] = '\0';
	  rate = mserv_getrate(user, track);
	  if (rate && rate->rating == val)
	    ok = 1;
	} else if ((p = strchr(token, '=')) != NULL &&
		   !stricmp(p+1, "heard")) {
	  /* user=heard - matches if user has heard this song */
	  if (p-token > USERNAMELEN)
	    goto error;
	  strncpy(user, token, p-token);
	  user[p-token] = '\0';
	  rate = mserv_getrate(user, track);
	  if (rate)
	    /* there can only be a rate if it's been rated or
	       heard (which is a rating of 0) */
	    ok = 1;
	} else if ((p = strchr(token, '=')) != NULL &&
		   !stricmp(p+1, "rated")) {
	  /* user=heard - matches if user has rated this song */
	  if (p-token > USERNAMELEN)
	    goto error;
	  strncpy(user, token, p-token);
	  user[p-token] = '\0';
	  rate = mserv_getrate(user, track);
	  if (rate && rate->rating > 0)
            /* rating of 0 means heard, anything greater means rated */
	    ok = 1;
	} else if (!strnicmp(token, "genre=", 6)) {
	  p = token+6;
	  if (!track->genres[0]) {
	    /* no genre in track, no match */
	  } else {
	    strncpy(genres_spl, track->genres, GENRESLEN);
	    genres_spl[GENRESLEN] = '\0'; /* shouldn't ever overflow */
	    if ((genres_n = mserv_split(genres_str, MAXNGENRE, genres_spl,
					",")) >= MAXNGENRE) {
	      /* too many genres should never happen */
	      mserv_log("Track %s has too many genres!", track->filename);
	    }
	    for (i = 0; i < genres_n; i++) {
	      if (!stricmp(p, genres_str[i])) {
		ok = 1;
		break;
	      }
	    }
	  }
	} else if (!strnicmp(token, "search=", 7)) {
	  p = token + 7;
	  if ((stristr(track->author, p)) ||
	      (stristr(track->name, p)))
	    ok = 1;
	} else if (strlen(token) != (ui = strcspn(token, "><="))) {
	  /* parameter <= >= < > = == number */
	  if (ui == 0)
	    goto error;
	  switch(token[ui]) {
	  case '<': type = 0; break;
	  case '>': type = 1; break;
	  case '=': type = 2; break;
	  default:
	    goto error;
	  }
	  p = token+ui+1;
	  if (*p == '=') {
	    /* <= >= == */
	    eq = 1;
	    p++;
	  } else {
	    eq = 0;
	  }
	  val2 = strtol(p, &end, 10);
	  if (!*p || *end)
	    goto error;
	  if (ui == 4 && !strnicmp(token, "year", 4)) {
	    if (track->year == 0) {
	      ok = 0;
	      goto decided;
	    }
	    val1 = track->year;
	  } else if (ui == 8 && !strnicmp(token, "duration", 8)) {
	    if (track->duration == 0) {
	      ok = 0;
	      goto decided;
	    }
	    val1 = track->duration / 100;
	  } else if (ui == 8 && !strnicmp(token, "lastplay", 8)) {
	    if (track->lastplay == 0) {
	      ok = 0;
	      goto decided;
	    }
	    val1 = (unsigned int)(time(NULL) - track->lastplay) / 3600;
	  } else if (ui == 5 && !strnicmp(token, "album", 5)) {
	    val1 = track->n_album;
	  } else if (ui == 5 && !strnicmp(token, "track", 5)) {
	    val1 = track->n_track;
	  } else {
	    /* unknown parameter */
	    goto error;
	  }
	  if (ok || (eq && val1 == val2)) {
	    ok = 1;
	  } else {
	    switch(type) {
	    case 0: /* lt */ if (val1 < val2)  ok = 1; break;
	    case 1: /* gt */ if (val1 > val2)  ok = 1; break;
	    case 2: /* eq */ if (val1 == val2) ok = 1; break;
	    }
	  }
	} else if ((val = mserv_strtorate(token)) != -1) {
	  /* track matches if someone has rated it this */
	  for (cl = mserv_clients; cl; cl = cl->next) {
	    if (!cl->authed || cl->mode == mode_computer)
	      continue;
	    rate = mserv_getrate(cl->user, track);
	    if (rate && rate->rating == val) {
	      ok = 1;
	      break;
	    }
	  }
	} else if (!stricmp(token, "played")) {
	  /* track matches if it has been played before */
	  if (track->lastplay)
	    ok = 1;
	} else if (!stricmp(token, "heard")) {
	  /* track matches if someone has heard it */
	  for (cl = mserv_clients; cl; cl = cl->next) {
	    if (!cl->authed || cl->mode == mode_computer)
	      continue;
	    rate = mserv_getrate(cl->user, track);
	    if (rate) {
	      /* there can only be a rate (in *rate) if it's been rated or
		 heard (which is a rating of 0) */
	      ok = 1;
	      break;
	    }
	  }
	} else if (!stricmp(token, "rated")) {
	  /* track matches if someone has rated it */
	  for (cl = mserv_clients; cl; cl = cl->next) {
	    if (!cl->authed || cl->mode == mode_computer)
	      continue;
	    rate = mserv_getrate(cl->user, track);
	    if (rate && rate->rating > 0) {
              /* rating of 0 means heard, anything greater means rated */
	      ok = 1;
	      break;
	    }
	  }
	} else {
	  goto error;
	}
      decided:
#ifdef PARSERDEBUG
        mserv_log("decided %d", (ok ? stack_true : stack_false));
#endif
	if (filter_parseradd(ok ? stack_true : stack_false) == -1)
	  goto error;
	token_n = 0;
      }
#ifdef PARSERDEBUG
      mserv_log("filter_check: end char is 0x%02x", c);
#endif
      switch(c) {
      case '&':
	if (filter_parseradd(stack_and) == -1)
	  goto error;
	break;
      case '|':
	if (filter_parseradd(stack_or) == -1)
	  goto error;
	break;
      case '!':
	if (filter_parseradd(stack_not) == -1)
	  goto error;
	break;
      case ')':
	if (filter_parseradd(stack_close) == -1)
	  goto error;
	break;
      case '\0':
	goto done;
      default:
	goto error;
      }
    }
  }
 done:
  if (stack_n != 1)
    goto error;
#ifdef PARSERDEBUG
  mserv_log("filter_check: ok! %d", (stack[0] == stack_true ? 1 : 0));
#endif
  return stack[0] == stack_true ? 1 : 0;
 error:
#ifdef PARSERDEBUG
  mserv_log("filter_check: Parser error - backtrace:");
  for (ui = 0; ui < stack_n; ui++) {
    switch(stack[ui]) {
    case stack_false:
      mserv_log("%d: false", ui);
      break;
    case stack_true:
      mserv_log("%d: true", ui);
      break;
    case stack_and:
      mserv_log("%d: and", ui);
      break;
    case stack_or:
      mserv_log("%d: or", ui);
      break;
    case stack_not:
      mserv_log("%d: not", ui);
      break;
    case stack_open:
      mserv_log("%d: open", ui);
      break;
    case stack_close:
      mserv_log("%d: close", ui);
      break;
    }
  }
  mserv_log("end bt - returning failure");
#endif
  return -1;
}

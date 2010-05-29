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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "mserv.h"
#include "misc.h"
#include "filter.h"
#include "acl.h"

#undef PARSERDEBUG

typedef enum token_state_t {
  token_before_equal,
  token_after_equal,
  token_in_value
} t_token_state;

/*** externed variables ***/

/* none */

/*** file-scope (static) globals ***/

static t_stack stack[MAXNSTACK];
static unsigned int stack_n = 0;


typedef struct token_table {
        t_field code;
        const char *token;
} TOKEN_TABLE;


static TOKEN_TABLE commands [] = {
        { ff_author,	"author" },
        { ff_albumname,	"albumname" },
        { ff_genre,	"genre" },
        { ff_title,	"title" },
        { ff_search,	"search" },
        { ff_year,	"year" },
        { ff_duration,	"duration" },
        { ff_lastplay,	"lastplay" },
        { ff_album,	"album" },
        { ff_track,	"track" },
        { ff_played,	"played" },
        { ff_heard,	"heard" },
        { ff_rated,	"rated" },
        { ff_invalid,	NULL }
};

t_field get_token (const char *command, int len, TOKEN_TABLE *table) {
        while (table -> token &&
			(strncasecmp (table -> token, command, len) != 0 ||
			table->token [len] != '\0')) {
                table++;
        }
        return (table -> code);
}


/*** functions ***/

static void free_cmd(t_cmd *cmd)
{
	if (cmd) {
		if (cmd->value)
			free (cmd->value);
		free_cmd(cmd->l_eval);
		free_cmd(cmd->r_eval);
		free (cmd);
	};
}


void free_filter(t_filter *filter)
{
	free (filter->filter_cmd);
	free_cmd (filter->parsetree);
	free (filter);
}


static t_cmd *parse_and (char **command);

static t_cmd *parse_parenthesis (char **command) {
	t_cmd *parse = 0;
	if (**command == '(') {
		(*command)++;
		parse = parse_and (command);
		if (parse && **command == ')') {
			(*command)++;
		} else {
			free_cmd (parse);
			parse = 0;
		}
	}
	return (parse);
}

static t_cmd *eval_comparison (char **command) {
	t_cmd *parse = 0;
	t_field field;
	const char *before, *after;
	char *error;
	char user [USERNAMELEN+1];
	t_acl *acl = NULL;

	before = *command;
	while (isalnum (**command)) {
		(*command)++;
	}
	field = get_token (before, *command - before, commands);
	if (field == ff_invalid && (*command - before) <= USERNAMELEN ) {
		/* user=<heard|rating> - check for username */
		strncpy(user, before, *command - before);
		user[*command - before] = '\0';
		for (acl = mserv_acl; acl; acl = acl->next) {
			if (!stricmp(acl->user, user)) {
				field = ff_userrating;
				break;
			}
		}
		/* rating */
		if (field == ff_invalid && (mserv_strtorate (user) >=1)) {
			field = ff_rating;
		}
	}
	if (field != ff_invalid && (parse = calloc (sizeof (t_cmd), 1))) {
		while (isspace (**command)) {
			(*command)++;
		}
		before = *command;

		/* Determine comparison */
		if (**command == '<') {
			(*command)++;
			if (**command == '=') {
				parse->action = fa_lessequal;
				(*command)++;
			} else {
				parse->action = fa_lessthan;
			}
		} else if (**command == '>') {
			(*command)++;
			if (**command == '=') {
				parse->action = fa_greaterequal;
				(*command)++;
			} else {
				parse->action = fa_greaterthan;
			}
		} else if (**command == '=') {
			(*command)++;
			if (**command == '~') {
				parse->action = fa_match;
				(*command)++;
			} else {
				if (**command == '=')
				(*command)++;
				parse->action = fa_equal;
			}
		} else {
			parse->action = fa_noop;
		}
		while (isspace (**command)) {
			(*command)++;
		}
		if (parse->action != fa_noop) {
			before = *command;
			if (**command == '"' || **command == '\'') {
				(*command)++;
				while (**command && **command != *before) {
					(*command)++;
				}
				before++;
				after=(*command)++;
				while (isspace (**command)) {
					(*command)++;
				}
			} else {
				while (**command && !ispunct (**command)) {
					(*command)++;
				}
				after = *command;
				/* strip off trailing whitespace from pattern */
				while (before < after && isspace (*(after-1))) {
					after--;
				}
			}
			if (before < after &&
			    (parse->value = malloc (after - before + 1))) {
				strncpy (parse->value, before, after - before);
				*(parse->value + (after - before)) = '\0';
			}
		}
			
		parse->field = field;
		switch (field) {
		    case (ff_author):
		    case (ff_albumname):
		    case (ff_title):
			/* value = string cases */
			if (parse->action == fa_noop || !parse->value) {
				parse->field = ff_invalid;
			} else {
				parse->match_length = strlen (parse->value);
				if (*(parse->value +
				      parse->match_length - 1) == '*') {
					parse->match_length--;
				} else {
					parse->match_length++;
				}
			}
			break;

		    case (ff_search):
		    case (ff_genre):
			if ((parse->action != fa_equal &&
			     parse->action != fa_match) || !parse->value) {
				parse->field = ff_invalid;
			} else if (field == ff_search) {
				/* for backward compatibility, search */
				/* uses = but actually does matching */
				parse->action = fa_match;
				parse->match_length = strlen
				(parse->value);
			}
			break;
		    case (ff_year):
		    case (ff_duration):
		    case (ff_lastplay):
		    case (ff_album):
		    case (ff_track):
			/* value = number cases */
			if (parse->action == fa_noop
			 || parse->action == fa_match || !parse->value) {
				parse->field = ff_invalid;
			} else {
				parse->value_numeric = strtol (parse->value,
							 &error, 10);
				if (error && *error) {
					parse->field = ff_invalid;
				}
			}
			break;

		    case (ff_rating):
			if (parse->action != fa_noop)
				parse->field = ff_invalid;
			parse->action = fa_equal;
			parse->value_numeric = mserv_strtorate (user);
			break;
		    case (ff_played):
		    case (ff_heard):
		    case (ff_rated):
			/* Booleans */
			if (parse->action != fa_noop)
				parse->field = ff_invalid;
			parse->action =
				parse->field == ff_rated ? fa_greaterthan
							 : fa_greaterequal;
			break;
		    case (ff_userrating):
			strcpy (parse->user, user);
			if (parse->action != fa_equal) {
				parse->field = ff_invalid;
			} else if (strcasecmp (parse->value, "heard") == 0) {
				/* nothing */
			} else if (strcasecmp (parse->value, "rated") == 0) {
				parse->action = fa_greaterthan;
			} else if ((parse->value_numeric = mserv_strtorate (
					parse->value)) >= 0) {
				/* nothing */
			} else {
				parse->field = ff_invalid;
			}
			break;
		}
		if (parse->field == ff_invalid) {
			free_cmd (parse);
			parse = 0;
		}
	}
	return (parse);
}


static t_cmd *parse_comparison (char **command) {
	t_cmd *parse;
	int invert = 0;

	while (isspace (**command))
		(*command)++;
	if (**command == '!') {
		(*command)++;
		invert=1;
	}
	while (isspace (**command))
		(*command)++;
	if (isalpha (**command)) {
		parse = eval_comparison (command);
	} else {
		parse = parse_parenthesis (command);
	}
	while (isspace (**command))
		(*command)++;
	if (parse)
		parse->invert = invert;
	return (parse);
}

static t_cmd *parse_or (char **command) {
	t_cmd *parse;
	t_cmd *newparse;

	if (!(parse = parse_comparison (command)))
		return (0);
	if (**command == '|') {
		(*command)++;
		newparse = calloc (sizeof (t_cmd), 1);
		newparse -> action = fa_or;
		newparse -> l_eval = parse;
		if (!(newparse -> r_eval = parse_or (command))) {
			free_cmd (newparse);
			parse = 0;
		} else {
			parse = newparse;
		};
	}
	return (parse);
}

static t_cmd *parse_and (char **command) {
	t_cmd *parse;
	t_cmd *newparse;

	if (!(parse = parse_or (command)))
		return (0);
	if (**command == '&') {
		(*command)++;
		newparse = calloc (sizeof (t_cmd), 1);
		newparse -> action = fa_and;
		newparse -> l_eval = parse;
		if (!(newparse -> r_eval = parse_and (command))) {
			free_cmd (newparse);
			parse = 0;
		} else {
			parse = newparse;
		};
	}
	return (parse);
}


t_filter *build_filter(const char *filter)
{
	t_filter *newfilter;
	char *parse;
	if (newfilter = calloc (sizeof (t_filter), 1)) {
		if (newfilter -> filter_cmd = strdup (filter)) {
			parse = newfilter -> filter_cmd;
			if ((newfilter -> parsetree = parse_and (&parse))
			     && !*parse) {
				return (newfilter);
			}
		}
		free_filter (newfilter);
	}
	return (0);
}


static int filter_evaluate(const t_cmd *filter, t_track *track)
{
	int result = 0;
	int diff = 0;
	int tempnum;
	const char *compare = 0;
	const char *temp;
	t_client *cl;
	t_rating *rating;

	switch (filter->action) {
	    case(fa_and):
		result = filter_evaluate (filter->l_eval, track) &&
			 filter_evaluate (filter->r_eval, track);
		break;
	    case(fa_or):
		result = filter_evaluate (filter->l_eval, track) ||
			 filter_evaluate (filter->r_eval, track);
		break;
	    default:
	    	switch (filter->field) {
		    case(ff_author):
			compare = track -> author;
			break;
		    case(ff_albumname):
			compare = track->album->name;
			break;
		    case(ff_title):
			compare = track->name;
			break;
		    case(ff_genre):
			compare = track->genres;
			if (filter->action == fa_match)
				break;
			do {
				temp = compare + strcspn (compare, "/,+");
				if (*temp && (temp - compare == 
					      filter->match_length)) {
					diff = strncasecmp (compare,
						filter->value, temp - compare);
					compare = temp + 1;
				} else {
					diff = strcasecmp (compare,
						filter->value);
					compare = 0;
				}
			} while (diff && compare);
			compare = NULL;
			break;
		    case(ff_search):
			result = stristr(track->author, filter->value) ||
				 stristr(track->name, filter->value);
			break;
		    case (ff_year):
			if ((diff = track->year) == 0) {
				result = 0;
				goto decided; /* my very first goto in C code */
			}
			break;
		    case (ff_duration):
			diff = track->duration / 100;
			break;
		    case (ff_played):
		    case (ff_lastplay):
			/* time since last played in hours */
			if (track->lastplay == 0) {
				result = 0;
				goto decided;
			}
			diff = (time(NULL) - track->lastplay) / 3600;
			break;
		    case (ff_album):
			diff = track->album->id;
			break;
		    case (ff_track):
			diff = track->n_track;
			break;

		    case (ff_rating):
			for (cl = mserv_clients; cl; cl = cl->next) {
			    if (!cl->authed || cl->mode == mode_computer)
				continue;
			    rating = mserv_getrate(cl->user, track);
			    if (rating &&
				(rating->rating == filter->value_numeric)) {
				diff = rating->rating;
				break;
			    }
			}
			break;

		    case (ff_heard):
		    case (ff_rated):
			diff = -1;
			for (cl = mserv_clients; cl; cl = cl->next) {
			    if (!cl->authed || cl->mode == mode_computer)
				continue;
			    rating = mserv_getrate(cl->user, track);
			    if (rating && rating->rating > diff)
				diff = rating->rating;
			}
			break;

		    case (ff_userrating):
			rating = mserv_getrate (filter->user, track);
			if (rating)
				diff = rating->rating;
			else
				diff = -1;
			break;
		}

		if (compare) {
			if (filter->action == fa_match) {
				result = (stristr (compare,filter->value) != 0);
			} else {
				if (strncasecmp (compare, "the ", 4) == 0 &&
				    strncasecmp (filter->value, "the ", 4) != 0)
				{
					diff = strncasecmp (compare + 4,
							  filter->value,
							  filter->match_length);
				} else {
					diff = strncasecmp (compare,
							  filter->value,
							  filter->match_length);
				}
			}
		}
		switch (filter->action) {
		    case (fa_lessthan):
			result = (diff < filter->value_numeric);
			break;
		    case (fa_lessequal):
			result = (diff <= filter->value_numeric);
			break;
		    case (fa_greaterthan):
			result = (diff > filter->value_numeric);
			break;
		    case (fa_greaterequal):
			result = (diff >= filter->value_numeric);
			break;
		    case (fa_equal):
			result = (diff == filter->value_numeric);
			break;
		}
	}
	decided:
	if (filter->invert)
		result = !result;
	return (result);
}


int filter_check (const t_filter *filter, t_track *track)
{
	return (filter ? filter_evaluate (filter->parsetree, track) : 1);
}



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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mserv.h"
#include "misc.h"
#include "conf.h"
#include "opt.h"

const char *opt_path_acl = NULL;
const char *opt_path_webacl = NULL;
const char *opt_path_logfile = NULL;
const char *opt_path_tracks = NULL;
const char *opt_path_trackinfo = NULL;
const char *opt_path_playout = NULL;
const char *opt_path_idea = NULL;
const char *opt_path_mixer = NULL;
const char *opt_path_language = NULL;
unsigned int opt_port = 4444;
unsigned int opt_gap = 1;
unsigned int opt_play = 0;
unsigned int opt_random = 0;
double opt_factor = 0.6;
const char *opt_player = NULL;
const char *opt_filter = NULL;

typedef struct {
  const char *option;
  const char *type;
  void *pval;
  const char *def;
} t_opts;

t_opts opt_opts[] = {
  { "path_acl",       "path",    &opt_path_acl,       "acl" },
  { "path_webacl",    "path",    &opt_path_webacl,    "webacl" },
  { "path_logfile",   "path",    &opt_path_logfile,   "log" },
  { "path_tracks",    "path",    &opt_path_tracks,    "tracks" },
  { "path_trackinfo", "path",    &opt_path_trackinfo, "trackinfo" },
  { "path_playout",   "path",    &opt_path_playout,   "player.out" },
  { "path_idea",      "path",    &opt_path_idea,      "idea" },
  { "path_mixer",     "path",    &opt_path_mixer,     "/dev/mixer" },
  { "path_language",  "path",    &opt_path_language, SHAREDIR "/english.lang"},
  { "port",           "int",     &opt_port,           "4444" },
  { "gap",            "int",     &opt_gap,            "1" },
  { "play",           "switch",  &opt_play,           "off" },
  { "random",         "switch",  &opt_random,         "off" },
  { "factor",         "double",  &opt_factor,         "0.60" },
  { "filter",         "string",  &opt_filter,         "" },
  { NULL, NULL, NULL, NULL }
};

int opt_read(const char *root)
{
  t_opts *p;
  const char *val;
  char *m, *end;
  unsigned int rl = strlen(root);

  /* remove superfluous slashes from end of root */
  while (rl > 1 && root[rl-1] == '/')
    rl--;
  for (p = opt_opts; p->option; p++) {
    if ((val = conf_getvalue(p->option)) == NULL)
      val = p->def;
    if (!stricmp(p->type, "path")) {
      if (*val != '/') {
	/* value is relative to mserv root */
	if ((m = malloc(rl+2+strlen(val))) == NULL) {
	  fprintf(stderr, "%s: out of memory building path\n", progname);
	  return -1;
	}
	sprintf(m, "%s/%s", root, val);
      } else {
	/* value is absolute path */
	if ((m = malloc(strlen(val))) == NULL) {
	  fprintf(stderr, "%s: out of memory building path\n", progname);
	  return -1;
	}
	strcpy(m, val);
      }
      *(const char **)p->pval = m;
      if (mserv_verbose)
	printf("opt path %s=%s\n", p->option, m);
    } else if (!stricmp(p->type, "string")) {
      *(const char **)p->pval = val;
      if (mserv_verbose)
	printf("opt string %s=%s\n", p->option, val);
    } else if (!stricmp(p->type, "int")) {
      *(unsigned int *)p->pval = strtol(val, &end, 10);
      if (!*val || *end) {
	fprintf(stderr, "%s: invalid integer for %s\n", progname, p->option);
	return -1;
      }
      if (mserv_verbose)
	printf("opt int %s=%u\n", p->option, *(unsigned int *)p->pval);
    } else if (!stricmp(p->type, "double")) {
      *(double *)p->pval = strtod(val, &end);
      if (!*val || *end) {
	fprintf(stderr, "%s: invalid value for %s\n", progname, p->option);
	return -1;
      }
      if (mserv_verbose)
	printf("opt double %s=%f\n", p->option, *(double *)p->pval);
    } else if (!stricmp(p->type, "switch")) {
      if (!stricmp(val, "on")) {
	*(unsigned int *)p->pval = 1;
      } else if (!stricmp(val, "off")) {
	*(unsigned int *)p->pval = 0;
      } else {
	fprintf(stderr, "%s: invalid value for %s\n", progname, p->option);
	return -1;
      }
      if (mserv_verbose)
	printf("opt switch %s=%u\n", p->option, *(unsigned int *)p->pval);
    } else {
      fprintf(stderr, "%s: internal error, invalid conf type %s\n", progname,
	      p->type);
      return -1;
    }
  }
  /* special case - player variable is an indirected to another variable */
  if ((val = conf_getvalue("player")) == NULL) {
    if (mserv_verbose)
      printf("No player specified, defaulting to /usr/local/bin/mpg123\n");
    opt_player = "/usr/local/bin/mpg123";
  } else {
    if ((opt_player = conf_getvalue(val)) == NULL) {
      fprintf(stderr, "%s: player setting '%s' not found\n", progname,
	      val);
      return -1;
    }
  }
  if (mserv_verbose)
    printf("opt special player=%s\n", opt_player);
  return 0;
}

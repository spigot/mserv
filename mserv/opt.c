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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mserv.h"
#include "misc.h"
#include "conf.h"
#include "opt.h"

const char *opt_default_icecast_output = NULL;
const char *opt_default_icecast_bitrate = NULL;
const char *opt_path_distconf = NULL;
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
double opt_gap = 1;
unsigned int opt_play = 0;
unsigned int opt_random = 0;
double opt_factor = 0.6;
double opt_rate_unheard = 0.55;
double opt_rate_unrated = 0.50;
const char *opt_filter = NULL;

typedef struct {
  const char *option;
  const char *type;
  void *pval;
  const char *def;
} t_opts;

t_opts opt_opts[] = {
  { "default_icecast_output", "string", &opt_default_icecast_output,
    "http://source:password@localhost:8000/mserv.ogg" },
  { "default_icecast_bitrate", "string", &opt_default_icecast_bitrate,
    "48000" },
  { "path_distconf",  "path",    &opt_path_distconf,  "config.dist" },
  { "path_acl",       "path",    &opt_path_acl,       "acl" },
  { "path_webacl",    "path",    &opt_path_webacl,    "webacl" },
  { "path_logfile",   "path",    &opt_path_logfile,   "log" },
  { "path_tracks",    "path",    &opt_path_tracks,    "tracks" },
  { "path_trackinfo", "path",    &opt_path_trackinfo, "trackinfo" },
  { "path_playout",   "path",    &opt_path_playout,   "player.out" },
  { "path_idea",      "path",    &opt_path_idea,      "idea" },
  { "path_mixer",     "path",    &opt_path_mixer,     "/dev/mixer" },
  { "path_language",  "path",    &opt_path_language,  DATADIR "/english.lang"},
  { "port",           "int",     &opt_port,           "4444" },
  { "gap",            "double",  &opt_gap,            "1" },
  { "play",           "switch",  &opt_play,           "off" },
  { "random",         "switch",  &opt_random,         "off" },
  { "factor",         "double",  &opt_factor,         "0.60" },
  { "rate_unheard",   "double",  &opt_rate_unheard,   "0.55" },
  { "rate_unrated",   "double",  &opt_rate_unrated,   "0.50" },
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
  return 0;
}

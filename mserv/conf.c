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
#include <errno.h>
#include "mserv.h"
#include "misc.h"
#include "conf.h"

t_conf *mserv_conf = NULL;

int conf_load(const char *file)
{
  char buffer[CONFLINELEN];
  FILE *fd;
  char *a, *p, *q;
  int line = 0;
  t_conf *conf, *confi;

  if ((fd = fopen(file, "r")) == NULL) {
    fprintf(stderr, "%s: unable to open conf %s for reading: %s\n",
	    progname, file, strerror(errno));
    return -1;
  }
  while(fgets(buffer, CONFLINELEN, fd)) {
    line++;
    if (buffer[strlen(buffer)-1] != '\n') {
      fprintf(stderr, "%s: line %d too long in conf file, ignoring.\n",
	      progname, line);
      while((a = fgets(buffer, CONFLINELEN, fd))) {
	if (buffer[strlen(buffer)-1] == '\n')
	  continue;
      }
      if (!a)
	goto finished;
      continue;
    }
    buffer[strlen(buffer)-1] = '\0';
    /* remove whitespace off start and end */
    p = buffer + strlen(buffer) - 1;
    while (p > buffer && *p == ' ')
      *p-- = '\0';
    p = buffer;
    while(*p == ' ')
      p++;
    /* check for comment */
    if (!*p || *p == '#' || *p == '|')
      continue;
    if ((conf = malloc(sizeof(t_conf))) == NULL) {
      fprintf(stderr, "%s: Out of memory adding to conf\n", progname);
      goto error;
    }
    q = p;
    strsep(&q, "=");
    if (q == NULL) {
      fprintf(stderr, "%s: line %d not understood in conf file\n", progname,
	      line);
      goto error;
    }
    if ((conf->key = malloc(strlen(p)+1)) == NULL ||
	(conf->value = malloc(strlen(q)+1)) == NULL) {
      fprintf(stderr, "%s: Out of memory building conf\n", progname);
      goto error;
    }
    strcpy(conf->key, p);
    strcpy(conf->value, q);
    if (mserv_verbose)
      printf("conf key='%s' value='%s'\n", conf->key, conf->value);
    conf->next = NULL;
    for (confi = mserv_conf; confi && confi->next; confi = confi->next);
    if (!confi)
      mserv_conf = conf;
    else
      confi->next = conf;
  }
 finished:
   if (ferror(fd) || fclose(fd)) {
     fprintf(stderr, "%s: error whilst reading conf: %s\n", progname,
	     strerror(errno));
    return -1;
  }
  return 0;
 error:
  fclose(fd);
  return -1;
}

const char *conf_getvalue(const char *key)
{
  t_conf *c;

  for (c = mserv_conf; c; c = c->next) {
    if (!stricmp(key, c->key))
      return c->value;
  }
  return NULL;
}

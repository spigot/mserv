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

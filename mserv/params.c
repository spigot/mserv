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
   params - routines to parse a string of the form a=b,c=d and provide easy 
   access functions to retrieve values given a key.  Copes with leading and
   trailing spaces.
*/

#define _GNU_SOURCE 1
#define _BSD_SOURCE 1
#define __EXTENSIONS__ 1

#include <stdlib.h>
#include <string.h>

#include "mserv.h"
#include "params.h"

#define SKIP_LEADING_SPACES(string) \
do { \
  while (*string == ' ') \
    string++; \
} while(0)

#define STRIP_TRAILING_SPACES(string) \
do { \
  int len = strlen(string); \
  while (len > 0 && string[len - 1] == ' ') { \
    len--; \
    string[len] = '\0'; \
  } \
} while(0)
      
int params_parse(t_params **params, const char *str, char *error,
                 int errsize)
{
  t_params *newparams;
  t_params_list **ppl_next;
  t_params_list *pl;
  char **strsep_pointer;
  char *key, *val;

  if ((newparams = (t_params *)malloc(sizeof(t_params))) == NULL) {
    snprintf(error, errsize, "out of memory");
    return MSERV_FAILURE;
  }
  newparams->memory = NULL;
  newparams->list = NULL;
  ppl_next = &(newparams->list);

  if ((newparams->memory = malloc(strlen(str) + 1)) == NULL) {
    snprintf(error, errsize, "out of memory");
    goto failed;
  }
  strcpy(newparams->memory, str);

  /* loop through parameters */
  strsep_pointer = &(newparams->memory);
  while ((key = strsep(strsep_pointer, ",")) != NULL) {
    if ((val = strchr(key, '=')) == NULL) {
      val = "1";
    } else {
      *val++ = '\0';
      SKIP_LEADING_SPACES(val);
      STRIP_TRAILING_SPACES(val);
    }
    SKIP_LEADING_SPACES(key);
    STRIP_TRAILING_SPACES(key);
    if (*key == '\0') {
      snprintf(error, errsize, "empty parameter");
      goto failed;
    }
    if ((pl = malloc(sizeof(t_params_list))) == NULL) {
      snprintf(error, errsize, "out of memory");
      goto failed;
    }
    pl->key = key;
    pl->val = val;
    pl->next = NULL;
    *ppl_next = pl;
    ppl_next = &(pl->next);
  }
  *params = newparams;
  return MSERV_SUCCESS;
failed:
  params_free(newparams);
  return MSERV_FAILURE;
}

int params_log(t_params *params, char *prefix)
{
  t_params_list *pl;

  for (pl = params->list; pl; pl = pl->next)
    mserv_log("%s:%s=%s", prefix, pl->key, pl->val);
  return MSERV_SUCCESS;
}

int params_get(t_params *params, char *key, char **val)
{
  t_params_list *pl;

  for (pl = params->list; pl; pl = pl->next) {
    if (stricmp(pl->key, key) == 0) {
      *val = pl->val;
      return MSERV_SUCCESS;
    }
  }
  return MSERV_FAILURE;
}

void params_free(t_params *params)
{
  t_params_list *pl, *pl_next;

  for (pl = params->list; pl; pl = pl_next) {
    pl_next = pl->next;
    free(pl);
  }
  if (params->memory)
    free(params->memory);
  free(params);
}

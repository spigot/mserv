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

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "misc.h"

/*** file-scope globals ***/

/*** externed variables ***/

/*** functions ***/

int stricmp(const char *str1, const char *str2)
{
  while(tolower(*str1) == tolower(*str2++)) {
    if (*str1++ == '\0')
      return 0;
  }
  return (tolower(*str1) - tolower(*--str2));
}

int strnicmp(const char *str1, const char *str2, int n)
{
  while (n-- > 0) {
    if (tolower(*str1) != tolower(*str2++))
      return (tolower(*str1) - tolower(*--str2));
    if (*str1++ == '\0')
      break;
  }
  return(0);
}

const char *stristr(const char *s, const char *find)
{
  char c, sc;
  int len;

  if ((c = *find++)) {
    len = strlen(find);
    do {
      do {
        if ((sc = *s++) == 0)
          return NULL;
      } while (tolower(sc) != tolower(c));
    } while (strnicmp(s, find, len) != 0);
    s--;
  }
  return s;
}

#ifndef HAVE_STRSEP

char *strsep(char **str, const char *stuff)
{
  const char *p;
  char *s, *tok;
  unsigned char c, sc;

  if (!(s = *str))
    return NULL;
  tok = s;
  do {
    p = stuff;
    c = *s++;
    do {
      if ((sc = *p++) == c) {
        if (!c)
          s = NULL;
        else
          s[-1] = 0;
        *str = s;
        return tok;
      }
    } while (sc);
  } while (1);
}

#endif

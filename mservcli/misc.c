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

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

#ifndef _MSERVCLI_H
#define _MSERVCLI_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct mservcli_data {
  unsigned int maxparams;
  unsigned int params;
  char **param;
};

struct mservcli_id {
  int sock;
  FILE *in, *out;
  int rtflag;
  int bufmine;
  char *buffer;
  unsigned int buflen;
  void (*rt_handler)(void *, int, struct mservcli_data *);
  void *rt_private;
  struct mservcli_data *rt_data;
};

struct mservcli_id *mservcli_connect(const struct sockaddr_in *servaddr,
				     char *buffer, unsigned int buflen,
				     const char *user, const char *pass,
				     int rtflag);
int mservcli_free(struct mservcli_id *id);
int mservcli_rthandler(struct mservcli_id *id,
                       void (*rth)(void *, int, struct mservcli_data *),
                       void *private,
                       struct mservcli_data *data);
int mservcli_getresult(struct mservcli_id *id);
char *mservcli_getresultstr(struct mservcli_id *id);
int mservcli_getdata(struct mservcli_id *id, struct mservcli_data *data);
int mservcli_send(struct mservcli_id *id, const char *output);
int mservcli_discarddata(struct mservcli_id *id);
int mservcli_poll(struct mservcli_id *id);

int mservcli_stricmp(const char *str1, const char *str2);
int mservcli_strnicmp(const char *str1, const char *str2, int n);
const char *mservcli_stristr(const char *s, const char *find);
char *mservcli_strsep(char **str, const char *stuff);

#endif /* _MSERVCLIH */

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

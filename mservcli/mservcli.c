#define _GNU_SOURCE
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#include "mservcli.h"

#define MAXLINE 1024

static int mservcli_processrt(struct mservcli_id *id);

struct mservcli_id *mservcli_connect(const struct sockaddr_in *servaddr,
                                     char *buffer, unsigned int buflen,
                                     const char *user, const char *pass,
                                     int rtflag)
{
  struct protoent *ip = getprotobyname("IP");
  struct mservcli_id *id;
  char outbuf[MAXLINE];
  int code;
  
  if (!ip) {
    errno = ENODEV;
    return NULL;
  }
  if ((id = malloc(sizeof(struct mservcli_id))) == NULL) {
    errno = ENOMEM;
    return NULL;
  }
  memset(id, 0, sizeof(id));
  id->rtflag = rtflag ? 0 : 1;
  if (((id->sock = socket(AF_INET, SOCK_STREAM, ip->p_proto)) == -1) ||
      (connect(id->sock, (const struct sockaddr *)servaddr,
               sizeof(*servaddr)) == -1)) {
    free(id);
    return NULL;
  }
  if ((id->in = fdopen(id->sock, "r")) == NULL ||
      (id->out = fdopen(id->sock, "w")) == NULL) {
    goto error;
  }
  setvbuf(id->out, NULL, _IOLBF, 0);
  if (buffer && buflen) {
    id->buffer = buffer;
    id->buflen = buflen;
  } else {
    id->bufmine = 1;
    id->buflen = MAXLINE;
    if ((id->buffer = malloc(id->buflen)) == NULL) {
      errno = ENOMEM;
      goto error;
    }
  }
  if ((code = mservcli_getresult(id)) == -1)
    goto error;
  if (code != 200) {
    errno = EBUSY;
    goto error;
  }
  if (mservcli_discarddata(id) == -1)
    goto error;
  snprintf(outbuf, sizeof(outbuf), "USER %s\r\n", user);
  if (mservcli_send(id, outbuf) == -1)
    goto error;
  if ((code = mservcli_getresult(id)) == -1)
    goto error;
  if (code != 201) {
    errno = EACCES;
    goto error;
  }
  if (mservcli_discarddata(id) == -1)
    goto error;
  snprintf(outbuf, sizeof(outbuf), "PASS %s %s\r\n", pass,
           rtflag ? "RTCOMPUTER" : "COMPUTER");
  if (mservcli_send(id, outbuf) == -1)
    goto error;
  if ((code = mservcli_getresult(id)) == -1)
    goto error;
  if (code != 202) {
    errno = EACCES;
    goto error;
  }
  if (mservcli_discarddata(id) == -1)
    goto error;
  return id;
error:
  code = errno;
  if (id->bufmine && id->buffer)
    free(id->buffer);
  close(id->sock);
  free(id);
  errno = code;
  return NULL;
}

int mservcli_free(struct mservcli_id *id)
{
  if (close(id->sock) == -1)
    return -1;
  if (id->bufmine && id->buffer)
    free(id->buffer);
  free(id);
  return 0;
}

int mservcli_rthandler(struct mservcli_id *id,
                       void (*rth)(void *, int, struct mservcli_data *),
                       void *private,
                       struct mservcli_data *data)
{
  id->rt_handler = rth;
  id->rt_private = private;
  id->rt_data = data;
  return 0;
}

int mservcli_getresult(struct mservcli_id *id)
{
  int code;
  char *end;
  int i;
  int rt;

  do {
    rt = 0;
    if (fgets(id->buffer, id->buflen, id->in) == NULL) {
      errno = EPIPE;
      return -1;
    }
    if (!*id->buffer || id->buffer[(i = strlen(id->buffer))-1] != '\n') {
      errno = EPIPE;
      return -1;
    }
    id->buffer[--i] = '\0';
    if (*id->buffer && id->buffer[i-1] == '\r')
      id->buffer[--i] = '\0';
    if (*id->buffer == '=') {
      if (mservcli_processrt(id) == -1)
        return -1;
      rt = 1;
    }
  } while (rt);
  code = strtol(id->buffer, &end, 10);
  if (!*id->buffer || *end++ != ' ') {
    errno = ERANGE;
    return -1;
  }
  while (*end == ' ')
    end++;
  memmove(id->buffer, end, id->buflen-(end-id->buffer));
  return code;
}

char *mservcli_getresultstr(struct mservcli_id *id)
{
  return id->buffer;
}

int mservcli_getdata(struct mservcli_id *id, struct mservcli_data *data)
{
  char *p = id->buffer;
  unsigned int i;
  int rt;

  do {
    rt = 0;
    if (fgets(id->buffer, id->buflen, id->in) == NULL) {
      errno = EPIPE;
      return -1;
    }
    if (!*id->buffer || id->buffer[(i = strlen(id->buffer))-1] != '\n') {
      errno = EPIPE;
      return -1;
    }
    id->buffer[--i] = '\0';
    if (*id->buffer && id->buffer[i-1] == '\r')
      id->buffer[--i] = '\0';
    if (*id->buffer == '=') {
      if (mservcli_processrt(id) == -1)
        return -1;
      rt = 1;
    }
  } while (rt);
  if (*p == '.' && *(p+1) == '\0') {
    return 1;
  }
  i = 0;
  if (*p) {
    data->param[i++] = p;
    while(i < data->maxparams-1 && (p = strchr(p, '\t'))) {
      *p++ = '\0';
      data->param[i++] = p;
    }
    if (i >= data->maxparams-1) {
      errno = EMLINK;
      return -1;
    }
  }
  data->params = i;
  for (; i < data->maxparams; i++) {
    data->param[i] = NULL;
  }
  return 0;
}

static int mservcli_processrt(struct mservcli_id *id)
{
  char *p = id->buffer + 1;
  unsigned int i = 0;
  int code;
  char *end;

  code = strtol(p, &end, 10);
  if (!*p || (*end != '\t' && *end != '\0'))
    return ERANGE;
  if (*end) {
    p = end+1;
    id->rt_data->param[i++] = p;
    while(i < id->rt_data->maxparams-1 && (p = strchr(p, '\t'))) {
      *p++ = '\0';
      id->rt_data->param[i++] = p;
    }
    if (i >= id->rt_data->maxparams-1) {
      errno = EMLINK;
      return -1;
    }
  }
  id->rt_data->params = i;
  for (; i < id->rt_data->maxparams; i++) {
    id->rt_data->param[i] = NULL;
  }
  id->rt_handler(id->rt_private, code, id->rt_data);
  return 0;
}

int mservcli_send(struct mservcli_id *id, const char *output)
{
  if (fputs(output, id->out) == EOF) {
    errno = EINVAL;
  }
  if (fflush(id->out) == EOF)
    return -1;
  return 0;
}

int mservcli_discarddata(struct mservcli_id *id)
{
  int rt = 0;
  int i;

  do {
    do {
      if (fgets(id->buffer, id->buflen, id->in) == NULL) {
        errno = EPIPE;
        return -1;
      }
      if (!*id->buffer || id->buffer[(i = strlen(id->buffer))-1] != '\n') {
        errno = EPIPE;
        return -1;
      }
      id->buffer[--i] = '\0';
      if (*id->buffer && id->buffer[i-1] == '\r')
        id->buffer[--i] = '\0';
      if (*id->buffer == '=') {
        if (mservcli_processrt(id) == -1)
          return -1;
        rt = 1;
      }
    } while (rt);
  } while (*id->buffer != '.' && *(id->buffer+1) != '\0');
  return 0;
}

int mservcli_poll(struct mservcli_id *id)
{
  int c = fgetc(id->in);
  int i;

  if (c == EOF) {
    errno = EPIPE;
    return -1;
  }
  if (c == '=') {
    id->buffer[0] = '=';
    if (fgets(id->buffer+1, id->buflen, id->in) == NULL) {
      errno = EPIPE;
      return -1;
    }
    if (!*id->buffer || id->buffer[(i = strlen(id->buffer))-1] != '\n') {
      errno = EPIPE;
      return -1;
    }
    id->buffer[--i] = '\0';
    if (*id->buffer && id->buffer[i-1] == '\r')
      id->buffer[--i] = '\0';
    if (mservcli_processrt(id) == -1)
      return -1;
    return 0;
  }
  if (ungetc(c, id->in) == EOF)
    return -1;
  return 0;
}

int mservcli_stricmp(const char *str1, const char *str2)
{
  while(tolower(*str1) == tolower(*str2++)) {
    if (*str1++ == '\0')
      return 0;
  }
  return (tolower(*str1) - tolower(*--str2));
}

int mservcli_strnicmp(const char *str1, const char *str2, int n)
{
  while (n-- > 0) {
    if (tolower(*str1) != tolower(*str2++))
      return (tolower(*str1) - tolower(*--str2));
    if (*str1++ == '\0')
      break;
  }
  return(0);
}

const char *mservcli_stristr(const char *s, const char *find)
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
    } while (mservcli_strnicmp(s, find, len) != 0);
    s--;
  }
  return s;
}

char *mservcli_strsep(char **str, const char *stuff)
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

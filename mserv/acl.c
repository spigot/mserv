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
#include "opt.h"
#include "acl.h"

int acl_load(void)
{
  char buffer[ACLLINELEN];
  FILE *fd;
  char *a;
  int line = 0;
  t_acl *acl, *acli;
  char *str[4];
  t_userlevel *ul;

  if ((fd = fopen(opt_path_acl, "r")) == NULL) {
    perror("fopen");
    mserv_log("Unable to fopen ACL for reading");
    return -1;
  }
  while(fgets(buffer, ACLLINELEN, fd)) {
    line++;
    if (buffer[strlen(buffer)-1] != '\n') {
      mserv_log("Line %d too long in ACL, ignoring.\n", line);
      while((a = fgets(buffer, ACLLINELEN, fd))) {
	if (buffer[strlen(buffer)-1] == '\n')
	  continue;
      }
      if (!a)
	goto finished;
      continue;
    }
    buffer[strlen(buffer)-1] = '\0';
    if ((acl = malloc(sizeof(t_acl))) == NULL) {
      mserv_log("Out of memory adding to ACL");
      goto error;
    }
    if (mserv_split(str, 4, buffer, ":") < 2) {
      mserv_log("Invalid ACL line");
      goto error;
    }
    strncpy(acl->user, str[0], USERNAMELEN);
    acl->user[USERNAMELEN] = '\0';
    strncpy(acl->pass, str[1], PASSWORDLEN);
    acl->pass[PASSWORDLEN] = '\0';
    if (str[2] && (ul = mserv_strtolevel(str[2])) != NULL) {
      acl->userlevel = *ul;
    } else {
      acl->userlevel = level_guest;
    }
    acl->nexttime = time(NULL);
    acl->next = NULL;
    for (acli = mserv_acl; acli && acli->next; acli = acli->next);
    if (!acli)
      mserv_acl = acl;
    else
      acli->next = acl;
  }
 finished:
   if (ferror(fd) || fclose(fd)) {
    perror("fgets");
    mserv_log("Error whilst reading conf: %s", strerror(errno));
    return -1;
  }
  return 0;
 error:
  fclose(fd);
  return -1;
}

void acl_save(void)
{
  FILE *fdacl, *fdweb;
  t_acl *acl;

  if ((fdacl = fopen(opt_path_acl, "w")) == NULL) {
    perror("fopen");
    mserv_log("Unable to fopen ACL for writing");
    return;
  }
  if ((fdweb = fopen(opt_path_webacl, "w")) == NULL) {
    perror("fopen");
    mserv_log("Unable to fopen web ACL for writing");
    return;
  }
  for (acl = mserv_acl; acl; acl = acl->next) {
    fprintf(fdacl, "%s:%s:%s\n", acl->user, acl->pass,
	    mserv_levelstr(acl->userlevel));
    fprintf(fdweb, "%s:%s\n", acl->user, acl->pass);
  }
  if (fclose(fdacl) == EOF) {
    perror("fclose");
    mserv_log("Unable to fclose ACL whilst writing");
  }
  if (fclose(fdweb) == EOF) {
    perror("fclose");
    mserv_log("Unable to fclose web ACL whilst writing");
  }
  return;
}

int acl_checkpassword(const char *user, const char *password,
		      t_acl **acl)
{
  t_acl *a;
  char salt[4];

  for (a = mserv_acl; a; a = a->next) {
    if (!stricmp(a->user, user)) {
      break;
    }
  }  
  if (!a)
    return -1;
  sprintf(salt, "%c%c", a->pass[0], a->pass[1]);
  if (!strcmp(crypt(password, salt), a->pass)) {
    if (acl)
      *acl = a;
    return 0;
  }
  return -1;
}

char *acl_crypt(const char *password)
{
  static const char saltkeys[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./]";
  char salt[3];
  static char pblk[PASSWORDLEN+1];

  *salt = saltkeys[(int)(64.0*rand()/(RAND_MAX+1.0))];
  *(salt+1) = saltkeys[(int)(64.0*rand()/(RAND_MAX+1.0))];
  *(salt+2) = '\0';
  strncpy(pblk, crypt(password, salt), PASSWORDLEN);
  pblk[PASSWORDLEN] = '\0';
  return pblk;
}

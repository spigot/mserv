/* written by James Ponder <james@squish.net> */

#define _GNU_SOURCE
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <mservcli.h>

int main(int argc, char *argv[])
{
  int i;
  unsigned int ui;
  char *user = "guest";
  char pass[32];
  char host[256];
  int port = 4444;
  struct sockaddr_in sin;
  struct hostent *hent;
  struct mservcli_id *id;
  int code;
  struct mservcli_data data;
  char *param[64];
  char *p;

  data.param = param;
  data.maxparams = 64;
  strcpy(pass, "guest");
  strcpy(host, "127.0.0.1");

  while ((i = getopt(argc, argv, "u:p:h:")) != -1) {
    switch(i) {
    case 'u':
      user = optarg;
      break;
    case 'p':
      strncpy(pass, optarg, sizeof(pass));
      pass[sizeof(pass)-1] = '\0';
      for (p = optarg; *p; p++)
	*p = 'x';
      break;
    case 'h':
      if ((p = strchr(optarg, ':')) != NULL) {
	port = atoi(p+1);
	i = ((unsigned int)(p-optarg) > sizeof(host)-1 ? sizeof(host)-1 :
	     p-optarg);
	strncpy(host, optarg, i);
	host[i] = '\0';
      } else {
	strncpy(host, optarg, sizeof(host)-1);
	host[sizeof(host)-1] = '\0';
      }
      break;
    default:
      fputs("mservcmd: parameters not understood\n", stderr);
      exit(1);
    }
  }
  argc-= optind;
  argv+= optind;

  if (argc != 1) {
    fputs("Syntax: mservcmd [-u <user>] [-p <pass>] [-h <host>] <cmd>\n"
	  "          See man page for further information\n",
	  stderr);
    exit(1);
  }
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  if ((hent = gethostbyname(host)) == NULL) {
    fprintf(stderr, "mservcmd: unknown host\n", host);
    exit(2);
  }
  sin.sin_addr = *(struct in_addr *)hent->h_addr_list[0];
  if ((id = mservcli_connect(&sin, NULL, 0, user, pass, 0)) == NULL) {
    if (errno == EACCES) {
      fprintf(stderr, "mservcmd: server said access denied\n");
      exit(4);
    }
    fprintf(stderr, "mservcmd: unable to connect to server: %s\n",
	    strerror(errno));
    exit(3);
  }
  if (mservcli_send(id, argv[0]) || mservcli_send(id, "\r\n")) {
    fprintf(stderr, "mservcmd: unable to send command: %s\n", strerror(errno));
    exit(5);
  }
  if ((code = mservcli_getresult(id)) == -1) {
    fprintf(stderr, "mservcmd: unable to get result to command: %s\n",
	    strerror(errno));
    exit(5);
  }
  printf("%d\t%s\n", code, mservcli_getresultstr(id));
  do {
    if ((code = mservcli_getdata(id, &data)) == -1) {
      fprintf(stderr, "mservcmd: unable to get data: %s\n",
	      strerror(errno));
      exit(5);
    }
    if (code)
      break;
    for (ui = 0; ui < data.params; ui++) {
      printf("%s%s", (ui ? "\t" : ""), data.param[ui]);
    }
    putchar('\n');
  } while (1);
  mservcli_free(id);
  return 0;
}

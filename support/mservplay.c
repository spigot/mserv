/* idea and written by Joseph Heenan <jogu@ping.demon.co.uk> */
/* modifications by James Ponder <james@squish.net> */

#define _GNU_SOURCE
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#include "config.h"
#include "defines.h"
#include "misc.h"

#ifndef PATH_MPG123
#define PATH_MPG123 "/usr/local/bin/mpg123"
#endif

int main(int argc, char *argv[])
{
  const pid_t pid = getpid();
  char *p;
  int priority;

  if (argc <= 3) {
    fprintf(stderr, "Usage: %s <priority> <\"mpg123\"> <parameters>\n",
	    argv[0]);
    exit(1);
  }
  priority = strtol(argv[1], &p, 10);
  if (*argv[1] == 0 || *p || priority < -20 || priority >= 20) {
    fprintf(stderr, "%s: Invalid priority\n", argv[0]);
    exit(1);
  }
  if (setpriority(PRIO_PROCESS, pid, priority) == -1) {
    fprintf(stderr, "%s: setpriority: %s\n", argv[0], strerror(errno));
    exit(1);
  }
  setgid(getgid());
  if (setuid(getuid()) != 0) {
    fprintf(stderr, "%s: setuid: %s\n", argv[0], strerror(errno));
    exit(1);
  }
  if (stricmp(argv[2], "mpg123")) {
    fprintf(stderr, "%s: Unrecognised player type, must be 'mpg123'",
	    argv[0]);
    exit(1);
  }
  execvp(PATH_MPG123, argv+2);
  fprintf(stderr, "%s: execvp: %s\n", argv[0], strerror(errno));
  exit(1);
}

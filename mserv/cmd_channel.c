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
  cmd_channel - CHANNEL commands
*/

#define _GNU_SOURCE 1
#define _BSD_SOURCE 1
#define __EXTENSIONS__ 1

#include <string.h>

#include "mserv.h"
#include "misc.h"
#include "channel.h"
#include "module.h"
#include "cmd_channel.h"

/*** file-scope (static) function declarations ***/

static void cmd_channel_output_add(t_client *cl, t_cmdparams *cp);
static void cmd_channel_output_remove(t_client *cl, t_cmdparams *cp);

/*** file-scope (static) globals ***/

/*** externed variables ***/

static t_cmds cmd_channel_output_cmds[];

t_cmds cmd_channel_cmds[] = {
  { 1, level_priv, "OUTPUT", cmd_channel_output_cmds, NULL,
    "Modify output streams of a channel",
    "<params>" },
  { 0, level_guest, NULL, NULL, NULL, NULL, NULL }
};

static t_cmds cmd_channel_output_cmds[] = {
  { 1, level_priv, "ADD", NULL, cmd_channel_output_add,
    "Add a new output stream to the channel",
    "<channel> <module> <location> [<parameters>]" },
  { 1, level_priv, "REMOVE", NULL, cmd_channel_output_remove,
    "Remove an output stream from a channel",
    "<channel> <module> <location>" },
  { 0, level_guest, NULL, NULL, NULL, NULL, NULL }
};

/*** functions ***/

static void cmd_channel_output_add(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char error[256];
  char *str[5];
  t_channel *channel;

  strcpy(linespl, cp->line);

  if (mserv_split(str, 4, linespl, " ") < 3) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  if ((channel = channel_find(str[0])) == NULL) {
    mserv_response(cl, "BADCHAN", NULL);
    return;
  }
  if (module_find(str[1]) == NULL) {
    mserv_response(cl, "BADMOD", NULL);
    return;
  }
  if (channel_addoutput(channel, str[1], str[2], str[3] ? str[3] : "",
                        error, sizeof(error)) != MSERV_SUCCESS) {
    mserv_response(cl, "CHANF", "%s", error);
    return;
  }
  mserv_response(cl, "CHANMOD", NULL);
}

static void cmd_channel_output_remove(t_client *cl, t_cmdparams *cp)
{
  char linespl[LINEBUFLEN];
  char error[256];
  char *str[4];
  t_channel *channel;

  strcpy(linespl, cp->line);

  if (mserv_split(str, 3, linespl, " ") != 3) {
    mserv_response(cl, "BADPARM", NULL);
    return;
  }
  if ((channel = channel_find(str[0])) == NULL) {
    mserv_response(cl, "BADCHAN", NULL);
    return;
  }
  if (module_find(str[1]) == NULL) {
    mserv_response(cl, "BADMOD", NULL);
    return;
  }
  if (channel_removeoutput(channel, str[1], str[2],
                        error, sizeof(error)) != MSERV_SUCCESS) {
    mserv_response(cl, "CHANF", "%s", error);
    return;
  }
  mserv_response(cl, "CHANMOD", NULL);
}

  /*
#if ENGINE == icecast
  if (channel_addoutput(mserv_channel, "icecast", opt_default_icecast_output,
                        opt_default_icecast_bitrate,
                        error, sizeof(error)) != MSERV_SUCCESS) {
    mserv_log("Failed to add initial output stream to default channel: %s",
              error);
    mserv_closedown(1);
  }
#elif ENGINE == local
  if (channel_addoutput(mserv_channel, "local", opt_default_local_output,
                        NULL, error, sizeof(error)) != MSERV_SUCCESS) {
    mserv_log("Failed to add initial output stream to default channel: %s",
              error);
    mserv_closedown(1);
  }
#else
#error unknown engine
#endif
  */


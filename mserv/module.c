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

#include <stdlib.h>
#include <ltdl.h>

#include "mserv.h"

static t_modinfo *module_list;

#define MODULE_FUNC(name) #name, MSERV_STRUCTOFFSET(t_modinfo, name)
struct _t_module_funcs {
  char *name;
  int offset;
} module_funcs[] = {
  MODULE_FUNC(init),
  MODULE_FUNC(final),
  MODULE_FUNC(output_create),
  MODULE_FUNC(output_destroy),
  MODULE_FUNC(output_poll),
  MODULE_FUNC(output_sync),
  MODULE_FUNC(output_volume),
  NULL, 0
};

int module_init(char *error, int errsize)
{
  LTDL_SET_PRELOADED_SYMBOLS();
  if (lt_dlinit()) {
    snprintf(error, errsize, "failed to initialise libltdl: %s", lt_dlerror());
    return MSERV_FAILURE;
  }
}

int module_final(char *error, int errsize)
{
  lt_dlexit();
  return MSERV_SUCCESS;
}

int module_load(char *name, char *error, int errsize)
{
  lt_dlhandle dlh = NULL;
  t_modinfo *mi = NULL, *mi_end;
  char fname[1024];
  void *sym;
  t_module *module;
  int flags = 0;
  int i;

  if (strlen(name) + 64 > sizeof(fname)) {
    snprintf(error, errsize, "module name too long");
    goto failed;
  }
  if (snprintf(fname, sizeof(fname), "%s/%s", LIBDIR, name) >= sizeof(fname)) {
    snprintf(error, errsize, "module path too long");
    goto failed;
  }
  if ((dlh = lt_dlopenext(fname)) == NULL) {
    snprintf(error, errsize, "failed to open (via lt_dlopenext) '%s': %s",
             fname, lt_dlerror());
    goto failed;
  }

  snprintf(fname, sizeof(fname), "%s_module", name);
  if ((sym = lt_dlsym(dlh, fname)) == NULL) {
    snprintf(error, errsize, "failed to locate symbol '%s' - not an mserv "
             "module?", fname);
    goto failed;
  }

  module = (t_module *)sym;

  if (module->apiver != MSERV_APIVER) {
    snprintf(error, errsize, "module compiled with API version %d (we are "
             "using version %d)", module->apiver, MSERV_APIVER);
    goto failed;
  }

  if ((mi = malloc(sizeof(t_modinfo))) == NULL) {
    snprintf(error, errsize, "out of memory creating modlist structure");
    goto failed;
  }
  mi->dlh = dlh;
  mi->module = module;
  mi->next = NULL;

  if (module_list) {
    /* find end of module list and append our new entry */
    for (mi_end = module_list; mi_end->next; mi_end = mi_end->next) ;
    mi_end->next = mi;
  } else {
    /* start the list */
    module_list = mi;
  }

  mserv_log("Module '%s' (%s) loaded.\n", name, module->version);

  /* look for functions */

  for (i = 0; module_funcs[i].name; i++) {
    snprintf(fname, sizeof(fname), "%s_%s", name, module_funcs[i].name);
    sym = lt_dlsym(dlh, fname);
    *(void **)(mi + module_funcs[i].offset) = (void *)(sym ? sym : NULL);
    if (sym && mserv_debug)
      mserv_log("found %s", fname);
  }

  /* call initialiser if present */

  if (mi->init && mi->init(error, errsize) != MSERV_SUCCESS)
    goto failed;

  mserv_log("Module '%s' (%s) loaded.\n", name, module->version);

  return MSERV_SUCCESS;
failed:
  if (dlh)
    lt_dlclose(dlh);
  if (mi)
    free(mi);
  return MSERV_FAILURE;
}

t_modinfo *module_find(const char *modname)
{
  t_modinfo *mi;

  for (mi = module_list; mi; mi = mi->next) {
    if (stricmp(mi->module->name, modname) == 0)
      return mi;
  }
  return NULL;
}

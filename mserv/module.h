#ifndef _MSERV_MODULE_H
#define _MSERV_MODULE_H

extern int module_init(char *error, int errsize);
extern int module_final(char *error, int errsize);
extern int module_load(char *name, char *error, int errsize);
extern t_modinfo *module_find(const char *modname);

#endif

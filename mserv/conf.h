extern int conf_load(const char *file);
const char *conf_getvalue(const char *key);

typedef struct _t_conf {
  struct _t_conf *next;
  char *key;
  char *value;
} t_conf;

extern t_conf *mserv_conf;

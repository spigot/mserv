extern int conf_load(const char *file);
const char *conf_getvalue(const char *key);
const char *conf_getvalue_n(const char *key, unsigned int n);

typedef struct _t_conf {
  struct _t_conf *next;
  char *key;
  char *value;
} t_conf;

extern t_conf *mserv_conf;

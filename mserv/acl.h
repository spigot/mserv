extern int acl_load(void);
extern void acl_save(void);
extern int acl_checkpassword(const char *user, const char *password,
			     t_acl **acl);
extern char *acl_crypt(const char *password);

int stricmp(const char *str1, const char *str2);
int strnicmp(const char *str1, const char *str2, int n);
const char *stristr(const char *s, const char *find);

#ifndef HAVE_STRSEP
char *strsep(char **str, const char *stuff);
#endif

extern const char *opt_path_distconf;
extern const char *opt_path_acl;
extern const char *opt_path_webacl;
extern const char *opt_path_logfile;
extern const char *opt_path_tracks;
extern const char *opt_path_trackinfo;
extern const char *opt_path_playout;
extern const char *opt_path_idea;
extern const char *opt_path_language;
extern const char *opt_path_libdir;
extern unsigned int opt_port;
extern double opt_gap;
extern unsigned int opt_random;
extern unsigned int opt_play;
extern double opt_factor;
extern double opt_rate_unheard;
extern double opt_rate_unrated;
extern const char *opt_filter;

int opt_read(const char *root);

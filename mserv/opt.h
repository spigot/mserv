extern const char *opt_path_distconf;
extern const char *opt_path_acl;
extern const char *opt_path_webacl;
extern const char *opt_path_logfile;
extern const char *opt_path_tracks;
extern const char *opt_path_trackinfo;
extern const char *opt_path_playout;
extern const char *opt_path_startout;
extern const char *opt_path_idea;
extern const char *opt_path_language;
extern const char *opt_path_libdir;
extern const char *opt_factor;
extern unsigned int opt_port;
extern double opt_gap;
extern unsigned int opt_random;
extern unsigned int opt_play;
extern double opt_rate_unheard;
extern double opt_rate_unrated;
extern const char *opt_filter;
extern unsigned int opt_experimental_fairness;
extern unsigned int opt_queue_clear_human;
extern unsigned int opt_queue_clear_computer;
extern unsigned int opt_queue_clear_rtcomputer;
extern unsigned int opt_human_alert_unheard;
extern unsigned int opt_human_alert_unrated;
extern unsigned int opt_human_alert_firstplay;
extern unsigned int opt_human_alert_nogenre;
extern unsigned int opt_human_display_screen;
extern unsigned int opt_human_display_info;
extern unsigned int opt_human_display_track;
extern unsigned int opt_human_display_author;
extern unsigned int opt_human_display_align;
extern unsigned int opt_human_display_bold;

int opt_read(const char *root);

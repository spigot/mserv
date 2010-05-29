/* int filter_check(const char *filter, t_track *track)
   filter_check takes a filter string and a track and returns whether or not
   the filter matches the track.  The return value is -1 for error, 0 for
   false or 1 for true.  The filter can be NULL which means no filter, and
   all tracks will return true.  Errors occur for failures to parse the filter
   which generally means that the syntax is wrong */

typedef enum _t_action {
	fa_noop,
	fa_and,
	fa_or,
	fa_lessthan,
	fa_lessequal,
	fa_equal,
	fa_greaterthan,
	fa_greaterequal,
	fa_match
} t_action;

typedef enum _t_field {
	ff_invalid = 0,
	ff_author,
	ff_albumname,
	ff_genre,
	ff_title,
	ff_search,
	ff_year,
	ff_duration,
	ff_lastplay,
	ff_album,
	ff_track,
	ff_rating,
	ff_played,
	ff_heard,
	ff_rated,
	ff_userrating
} t_field;

typedef struct _t_cmd {
	t_field field;
	t_action action;
	char *value;
	int value_numeric;
	int match_length;
	struct _t_cmd *l_eval;
	struct _t_cmd *r_eval;
	int invert;
	char user [USERNAMELEN + 1];
} t_cmd;

typedef struct _t_filter {
	char *filter_cmd;
	t_cmd *parsetree;
} t_filter;

extern t_filter *build_filter(const char *filter);
extern void free_filter(t_filter *filter);
extern int filter_check(const t_filter *filter, t_track *track);


/* int filter_check(const char *filter, t_track *track)
   filter_check takes a filter string and a track and returns whether or not
   the filter matches the track.  The return value is -1 for error, 0 for
   false or 1 for true.  The filter can be NULL which means no filter, and
   all tracks will return true.  Errors occur for failures to parse the filter
   which generally means that the syntax is wrong */

extern int filter_check(const char *filter, t_track *track);

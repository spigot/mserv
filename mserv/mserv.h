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

#include "global.h"
#include "channel.h"

void mserv_log(const char *text, ...);
void mserv_response(t_client *cl, const char *token, const char *fmt, ...);
void mserv_responsent(t_client *cl, const char *token, const char *fmt, ...);
void mserv_informnt(t_client *cl, const char *token, const char *fmt, ...);
void mserv_broadcast(const char *token, const char *fmt, ...);
void mserv_send(t_client *cl, const char *data, unsigned int len);
t_lang *mserv_gettoken(const char *token);
int mserv_split(char *str[], int nelements, char *line, const char *sep);
t_track *mserv_gettrack(unsigned int n_album, unsigned int n_track);
t_album *mserv_getalbum(unsigned int n_album);
t_rating *mserv_getrate(const char *user, t_track *track);
int mserv_getsatisfaction(const t_client *cl, double *satisfaction);
int mserv_addqueue(t_client *cl, t_track *track);
int mserv_player_playnext(void);
void mserv_abortplay(void);
void mserv_recalcratings(void);
t_track *mserv_altertrack(t_track *track, const char *author,
                          const char *name, const char *genres,
                          const char *miscinfo);
t_album *mserv_alteralbum(t_album *album, const char *author,
			  const char *name);
int mserv_savechanges(void);
t_rating *mserv_ratetrack(t_client *cl, t_track **track, unsigned int val);
const char *mserv_ratestr(t_rating *rate);
t_userlevel *mserv_strtolevel(const char *level);
const char *mserv_levelstr(t_userlevel userlevel);
const char *mserv_stndrdth(int day);
int mserv_setmixer(t_client *cl, int what, const char *line);
int mserv_channelvolume(t_client *cl, const char *line);
int mserv_flush(void);
void mserv_closedown(int exitcode);
char *mserv_idletxt(time_t idletime);
int mserv_acl_checkpassword(const char *user, const char *password,
                            t_acl **acl);
char *mserv_crypt(const char *password);
void mserv_saveacl(void);
int mserv_idea(const char *text);
void mserv_reset(void);
void mserv_close(t_client *cl);
void mserv_setgap(double gap);
double mserv_getgap(void);
int mserv_setfilter(const char *filter);
int mserv_checkgenre(const char *genres);
int mserv_checkauthor(const char *author);
int mserv_checkname(const char *name);
char *mserv_getfilter(void);
int mserv_strtorate(const char *str);
t_track *mserv_checkdisk_track(t_track *track);
t_album *mserv_checkdisk_album(t_album *album);
void mserv_ensuredisk(void);
int mserv_checklevel(t_client *cl, t_userlevel level);
t_client *mserv_user2client(const char *user);
void mserv_setplaying(t_channel *c, t_trkinfo *wasplaying,
                      t_trkinfo *nowplaying);
t_trkinfo *mserv_getplaying(void);
void mserv_addtohistory(t_trkinfo *sup);
const char *mserv_clientmodetext(t_client *cl);
void mserv_send_trackinfo(t_client *cl, t_track *track, t_rating *rate,
                          unsigned int bold, const char *info);

extern char *progname;
extern int mserv_verbose;
extern int mserv_debug;
extern t_client *mserv_clients;
extern t_track *mserv_tracks;
extern t_album *mserv_albums;
extern t_queue *mserv_queue;
extern t_trkinfo *mserv_history[];
extern t_trkinfo mserv_player_playing;
extern t_acl *mserv_acl;
extern int mserv_shutdown;
extern int mserv_random;
extern double mserv_factor;
extern t_author *mserv_authors;
extern t_genre *mserv_genres;
extern unsigned int mserv_filter_ok;
extern unsigned int mserv_filter_notok;

extern t_channel *mserv_channel;

extern int mserv_n_songs_started;
extern char *mserv_path_acl;
extern char *mserv_path_webacl;
extern char *mserv_path_conf;

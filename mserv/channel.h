#ifndef _MSERV_channel_H
#define _MSERV_channel_H

int channel_init(char *error, int errsize);
int channel_final(char *error, int errsize);
int channel_create(t_channel **channel, const char *name,
                   char *error, int errsize);
int channel_addoutput(t_channel *c, const char *modname, const char *uri,
                     const char *params, char *error, int errsize);
int channel_removeoutput(t_channel *c, const char *modname,
                        const char *uri, char *error, int errsize);
int channel_volume(t_channel *c, int *volume, char *error, int errsize);
int channel_close(t_channel *c, char *error, int errsize);
int channel_addinput(t_channel *c, int fd, t_supinfo *track_supinfo,
                     unsigned int samplerate, unsigned int channels, 
                     double delay_start, double delay_end,
                     char *error, int errsize);
int channel_inputfinished(t_channel *c);
int channel_sync(t_channel *c, char *error, int errsize);
void channel_align(t_channel *c);
void channel_replacetrack(t_channel *c, t_track *track, t_track *newtrack);
int channel_stop(t_channel *c, char *error, int errsize);
int channel_start(t_channel *c, char *error, int errsize);
int channel_pause(t_channel *c, char *error, int errsize);
int channel_unpause(t_channel *c, char *error, int errsize);
int channel_stopped(t_channel *c);
int channel_paused(t_channel *c);

#endif

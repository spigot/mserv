#include <shout/shout.h>
#include <vorbis/vorbisenc.h>

#define MSERV_SAMPLERATE 44100
#define MSERV_SAMPLEBLOCKSIZE MSERV_SAMPLERATE*2*2 /* stereo 16 bit PCM data */

typedef struct {
  /* struct timeval lasttime; */ /* interval timer */
  int input;               /* file descriptor of input */
  shout_t *shout;          /* shout output object */
  int volume;              /* volume level */
  char url[128];           /* http://user:pass@hostname:port/thing */
  vorbis_info vi;          /* vorbis object */
  vorbis_comment vc;       /* vorbis comment object */
  vorbis_dsp_state vd;     /* PCM decoder object */
  vorbis_block vb;         /* PCM decoder local working space */
  ogg_stream_state os;     /* Take physical pages, weld into logical stream */
  ogg_page og;             /* one Ogg bitstream page containing Vorbis pkts */
  ogg_packet op;           /* one raw packet of data for decode */
  int buffer_ready_bytes;  /* bytes we have so far */
  int buffer_ready_size;   /* size of buffer_ready buffer */
  char *buffer_ready;      /* previous buffer that's ready to send or NULL */
  int buffer_bytes;        /* bytes we have so far */
  char buffer[MSERV_SAMPLEBLOCKSIZE];
} t_output;

int output_init(void);
t_output *output_create(const char *destination, char **error);
int output_getvolume(t_output *o);
int output_setvolume(t_output *o, int volume);
void output_setinput(t_output *o, int input);
int output_closeinput(t_output *o);
void output_close(t_output *o);
void output_final(void);
void output_sync(t_output *o);
int output_delay(t_output *o);

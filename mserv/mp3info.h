
#define MP3ID3_TITLELEN NAMELEN
#define MP3ID3_ARTISTLEN AUTHORLEN
#define MP3ID3_ALBUMLEN 30
#define MP3ID3_YEARLEN 4
#define MP3ID3_COMMENTLEN 30

typedef struct {
  int present:1;
  char title[MP3ID3_TITLELEN+1];
  char artist[MP3ID3_ARTISTLEN+1];
  char album[MP3ID3_ALBUMLEN+1];
  char year[MP3ID3_YEARLEN+1];
  char comment[MP3ID3_COMMENTLEN+1];
  char genre[31];
} t_id3tag;

int mserv_mp3info_readlen(const char *fname, int *bitrate_ret,
			  t_id3tag *id3tag);

/* written by Joseph Heenan <joseph@ping.demon.co.uk> */

#define _GNU_SOURCE 1
#define _BSD_SOURCE 1
#define __EXTENSIONS__ 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "mserv.h"
#include "mp3info.h"

/* mp3 frame header structure */

#define h_emphasis(val)       (val&3)
#define h_original(val)       ((val>>2)&1)
#define h_copyright(val)      ((val>>3)&1)
#define h_mode_extension(val) ((val>>4)&3)
#define h_mode(val)           ((val>>6)&3)
#define h_private_bit(val)    ((val>>8)&1)
#define h_padding_bit(val)    ((val>>9)&1)
#define h_sampling_freq(val)  ((val>>10)&1)
#define h_bitrate_index(val)  ((val>>12)&15)
#define h_protection_bit(val) ((val>>16)&1)
#define h_layer(val)          ((val>>17)&3)
#define h_id(val)             ((val>>19)&1)
#define h_thing(val)          ((val>>20)&0xfff)

#define ID3V2HEADERLEN		10

/* mp3 bit rate and sampling frequency tables */

const int bitrate_table[2][3][16] =
  { { {  0, 32, 48, 56, 64, 80, 96,112,128,144,160,176,192,224,256,},
      {  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160,},
      {  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160,} },
    { {  0, 32, 64, 96,128,160,192,224,256,288,320,352,384,416,448,},
      {  0, 32, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,384,},
      {  0, 32, 40, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,} } };

const int sampling_freq_table[2][3] =
    { { 22050, 24000, 16000,},
      { 44100, 48000, 32000,} };

/* structure of id3 tag in mp3 file */

typedef struct id3tag_disc_str
{
  char title[MP3ID3_TITLELEN];
  char artist[MP3ID3_ARTISTLEN];
  char album[MP3ID3_ALBUMLEN];
  char year[MP3ID3_YEARLEN];
  char comment[MP3ID3_COMMENTLEN];
  unsigned char genre;
} id3tag_disc;

/* id3 v2 frame tag data */

typedef struct id3v2_frame_str
{
   char frameid[4];
   uint32_t size;
   uint16_t flags;
   char data[1024];
   size_t datalen;
} id3v2_frame;
/* id3 tags genre */

const char *genres_table[] = {
        "Blues",
        "Classic Rock",
        "Country",
        "Dance",
        "Disco",
        "Funk",
        "Grunge",
        "Hip-Hop",
        "Jazz",
        "Metal",
        "New Age",
        "Oldies",
        "Other",
        "Pop",
        "R&B",
        "Rap",
        "Reggae",
        "Rock",
        "Techno",
        "Industrial",
        "Alternative",
        "Ska",
        "Death Metal",
        "Pranks",
        "Soundtrack",
        "Euro-Techno",
        "Ambient",
        "Trip-Hop",
        "Vocal",
        "Jazz+Funk",
        "Fusion",
        "Trance",
        "Classical",
        "Instrumental",
        "Acid",
        "House",
        "Game",
        "Sound Clip",
        "Gospel",
        "Noise",
        "AlternRock",
        "Bass",
        "Soul",
        "Punk",
        "Space",
        "Meditative",
        "Instrumental Pop",
        "Instrumental Rock",
        "Ethnic",
        "Gothic",
        "Darkwave",
        "Techno-Industrial",
        "Electronic",
        "Pop-Folk",
        "Eurodance",
        "Dream",
        "Southern Rock",
        "Comedy",
        "Cult",
        "Gangsta",
        "Top 40",
        "Christian Rap",
        "Pop/Funk",
        "Jungle",
        "Native American",
        "Cabaret",
        "New Wave",
        "Psychadelic",
        "Rave",
        "Showtunes",
        "Trailer",
        "Lo-Fi",
        "Tribal",
        "Acid Punk",
        "Acid Jazz",
        "Polka",
        "Retro",
        "Musical",
        "Rock & Roll",
        "Hard Rock",
        "Folk",
        "Folk-Rock",
        "National Folk",
        "Swing",
        "Fast Fusion",
        "Bebob",
        "Latin",
        "Revival",
        "Celtic",
        "Bluegrass",
        "Avantgarde",
        "Gothic Rock",
        "Progressive Rock",
        "Psychedelic Rock",
        "Symphonic Rock",
        "Slow Rock",
        "Big Band",
        "Chorus",
        "Easy Listening",
        "Acoustic",
        "Humour",
        "Speech",
        "Chanson",
        "Opera",
        "Chamber Music",
        "Sonata",
        "Symphony",
        "Booty Bass",
        "Primus",
        "Porn Groove",
        "Satire",
        "Slow Jam",
        "Club",
        "Tango",
        "Samba",
        "Folklore",
        "Ballad",
        "Power Ballad",
        "Rhythmic Soul",
        "Freestyle",
        "Duet",
        "Punk Rock",
        "Drum Solo",
        "Acapella",
        "Euro-House",
        "Dance Hall"
};

/* returns 1 if the header is a valid mp3 one, 0 otherwise */

static int is_mp3(unsigned long int flags)
{
  if ((h_thing(flags) & 0xffe) != 0xffe)
    return 0; /* identifier wrong */

  if (h_layer(flags) == 0)
    return 0; /* invalid layer */

  if (h_bitrate_index(flags) == 0xf)
    return 0; /* invalid bitrate index */

  if (h_sampling_freq(flags) == 3)
    return 0; /* invalid sampling freq index */

  return 1;
}

/* returns 0 if id3v2 frame found, -1 otherwise */

static int read_id3v2_frame(FILE *f, id3v2_frame *frame)
{
  if (fread(frame->frameid, 1, 4, f) != 4 ||
      fread(&frame->size, 1, 4, f) != 4 || fread(&frame->flags, 1, 2, f) != 2)
    return -1;
  frame->size = ntohl(frame->size);
  frame->flags = ntohs(frame->flags);
  frame->datalen = (frame->size >= sizeof(frame->data) - 1)
			?(sizeof(frame->data) - 1) :frame->size;
  if (fread(frame->data, 1, frame->datalen, f) != frame->datalen)
    return -1;
  if (frame->frameid[0] == 'T' && memcmp(frame->frameid + 1, "XXX", 3)) {
    frame->data[frame->datalen] = 0;
    if (frame->data[0] == 0) /* Only handle non unicode */
      strcpy(frame->data, frame->data + 1);
    else
      frame->data[0] = 0;
  }
  return 0;
}

/* returns 0 if no id3v2 header, otherwise returns tag length, or -1
   and errno set */

static int mp3_id3v2head(FILE *f)
{
  char tag[3];
  char size[4];
  if (fseek(f, 0, SEEK_SET) == -1 || fread(tag, 1, 3, f) != 3)
    return -1;

  if (strncmp(tag, "ID3", 3))
    return 0; /* no header */
  if (fseek(f, 2 + 1, SEEK_CUR) == -1 || fread(size, 1, 4, f) != 4)
    return -1;

  return ID3V2HEADERLEN + size[3] + (size[2] << 7) + (size[1] << 14) +
							    (size[0] << 21);
}

/* determines length and bitrate of an mp3. returns -1 on failure with errno
   set, otherwise returns length in hundredths seconds */

int mserv_mp3info_readlen(const char *fname, int *bitrate_ret,
			  t_id3tag *id3tag)
{
  FILE              *f;
  int               bitrate, fs, length, headerlen;
  long int          filelen;
  float             mean_frame_size;
  char              tag[3];
  unsigned char     data[4];
  unsigned long int flags;
  int               errnok;
  char              *e;
  
  if (id3tag)
    id3tag->present = 0;

  if ((f = fopen(fname, "rb")) == NULL)
    return -1;

  if ((headerlen = mp3_id3v2head(f)) == -1)
    goto error;

  if (fseek(f, headerlen, SEEK_SET) == -1 || fread(&data, 4, 1, f) != 1)
    goto error;

  /* endian independent to 32-bit flags */
  flags = (data[0]<<24)+(data[1]<<16)+(data[2]<<8)+data[3];
  if (!is_mp3(flags)) {
    errno = EDOM;
    goto error;
  }

  bitrate = bitrate_table[h_id(flags)][3-h_layer(flags)]
    [h_bitrate_index(flags)];
  fs = sampling_freq_table[h_id(flags)][h_sampling_freq(flags)];

  if (h_id(flags) == 1) {
    mean_frame_size = 144000.0 * bitrate / fs;
  } else {
    mean_frame_size = 72000.0 * bitrate / fs;
  }

  if (fseek(f, -128, SEEK_END) == -1 ||
      (filelen = ftell(f)) == -1 ||
      fread(tag, 1, 3, f) != 3)
    goto error;

  if (strncmp(tag, "TAG", 3) == 0)
  {
    /* ID3 tag present */
    if (id3tag)
    {
      id3tag_disc tag_disc;

      if (fread(&tag_disc, 1, 125, f) != 125)
 	goto error;
      
      id3tag->present = 1;

      memcpy(id3tag->title, tag_disc.title, MP3ID3_TITLELEN);
      id3tag->title[MP3ID3_TITLELEN] = '\0';
      e = id3tag->title + strlen(id3tag->title);
      while (e > id3tag->title && *(e-1) == ' ')
	*--e = '\0';
      
      memcpy(id3tag->artist, tag_disc.artist, MP3ID3_ARTISTLEN);
      id3tag->artist[MP3ID3_ARTISTLEN] = '\0';
      e = id3tag->artist + strlen(id3tag->artist);
      while (e > id3tag->artist && *(e-1) == ' ')
	*--e = '\0';

      memcpy(id3tag->album, tag_disc.album, MP3ID3_ALBUMLEN);
      id3tag->album[MP3ID3_ALBUMLEN] = '\0';
      e = id3tag->album + strlen(id3tag->album);
      while (e > id3tag->album && *(e-1) == ' ')
	*--e = '\0';

      memcpy(id3tag->year, tag_disc.year, MP3ID3_YEARLEN);
      id3tag->year[MP3ID3_YEARLEN] = '\0';
      e = id3tag->year + strlen(id3tag->year);
      while (e > id3tag->year && *(e-1) == ' ')
	*--e = '\0';
      
      memcpy(id3tag->comment, tag_disc.comment, MP3ID3_COMMENTLEN);
      id3tag->comment[MP3ID3_COMMENTLEN] = '\0';
      e = id3tag->comment + strlen(id3tag->comment);
      while (e > id3tag->comment && *(e-1) == ' ')
	*--e = '\0';

      if (tag_disc.genre < (sizeof(genres_table) / sizeof(char *))) {
        strncpy(id3tag->genre, genres_table[tag_disc.genre],
		sizeof(id3tag->genre)-1);
	id3tag->genre[sizeof(id3tag->genre)-1] = '\0';
      } else {
	*id3tag->genre = '\0';
      }
    }
  } else {
    /* No ID3 tag present; last 128 bytes is music. */
    filelen += 128;
  }

  *bitrate_ret = bitrate;
  length = ((filelen - headerlen) / mean_frame_size ) *
           ((115200/2) * (1+h_id(flags)) ) / fs; /* in 1/100 seconds */

  /* If mp3v2 header present, read its contents */
  if (headerlen && fseek(f, ID3V2HEADERLEN, SEEK_SET) != -1) {
    id3v2_frame frame;
    while (read_id3v2_frame(f, &frame) != -1) {
      if (!memcmp(frame.frameid, "TALB", 4)) {
	memcpy(id3tag->album, frame.data, sizeof(id3tag->album) - 1);
	id3tag->album[sizeof(id3tag->album) - 1] = 0;
      }
      else if (!memcmp(frame.frameid, "TPE1", 4)) {
	memcpy(id3tag->artist, frame.data, sizeof(id3tag->artist) - 1);
	id3tag->artist[sizeof(id3tag->artist) - 1] = 0;
      }
      else if (!memcmp(frame.frameid, "TIT2", 4)) {
	memcpy(id3tag->title, frame.data, sizeof(id3tag->title) - 1);
	id3tag->title[sizeof(id3tag->title) - 1] = 0;
      }
      if (ftell(f) >= headerlen)
	break;
    }
  }

  fclose(f); /* ignore error */
  return length;

 error:
  errnok = errno;
  fclose(f); /* ignore error */
  errno = errnok;
  return -1;
}

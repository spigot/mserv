/* written by Joseph Heenan <joseph@ping.demon.co.uk> */

#define _GNU_SOURCE 1
#define _BSD_SOURCE 1
#define __EXTENSIONS__ 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
#define h_id(val)             ((val>>19)&3)
#define h_thing(val)          ((val>>21)&0x7ff)

#define ID3V2HEADERLEN		10

#define H_MODE_MONO	(3)
#define H_MODE_DUAL	(2)
#define H_MODE_JOINT	(1)
#define H_MODE_STEREO	(0)

#define H_ID_MPEG1	(3)
#define H_ID_MPEG2	(2)
#define H_ID_INVALID	(1)
#define H_ID_MPEG2_5	(0)

/* mp3 bit rate and sampling frequency tables */

const int bitrate_table[2][3][16] =
  { { {  0, 32, 48, 56, 64, 80, 96,112,128,144,160,176,192,224,256,},
      {  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160,},
      {  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160,} },
    { {  0, 32, 64, 96,128,160,192,224,256,288,320,352,384,416,448,},
      {  0, 32, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,384,},
      {  0, 32, 40, 48, 56, 167, 80, 96,112,128,160,192,224,256,320,} } };

const int sampling_freq_table[4][3] =
    {
      { 11025, 12000, 8000  },
      { 0,     0,     0     },
      { 22050, 24000, 16000 },
      { 44100, 48000, 32000 }
    };

/* structure of id3 tag in mp3 file */

// RVW: MP3ID3_TITLE and others are made longer. Now the do
//     not correspond to the lengths on disc any more.
//     so use specific set for the id3v1 lengths
#define MP3ID3V1_DISC_TITLELEN 30
#define MP3ID3V1_DISC_ARTISTLEN 30
#define MP3ID3V1_DISC_ALBUMLEN 30
#define MP3ID3V1_DISC_YEARLEN  4
#define MP3ID3V1_DISC_COMMENTLEN 30

typedef struct id3tag_disc_str
{
  char title[MP3ID3_TITLELEN];
  char artist[MP3ID3_ARTISTLEN];
  char album[MP3ID3_ALBUMLEN];
  char year[MP3ID3_YEARLEN];
  char comment[MP3ID3_COMMENTLEN];
  char title[MP3ID3V1_DISC_TITLELEN];
  char artist[MP3ID3V1_DISC_ARTISTLEN];
  char album[MP3ID3V1_DISC_ALBUMLEN];
  char year[MP3ID3V1_DISC_YEARLEN];
  char comment[MP3ID3V1_DISC_COMMENTLEN];
  unsigned char genre;
} id3tag_disc;

/* id3 frame tag data */
typedef struct id3_frame_str
{
   char frameid[4];
   int size;
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
  if (h_thing(flags) != 0x7ff)
    return 0; /* identifier wrong */

  if (h_id (flags) == H_ID_INVALID)
    return (0); /* Invalid MPEG version */

  if (h_layer(flags) == 0)
    return 0; /* invalid layer */

  if (h_bitrate_index(flags) == 0xf)
    return 0; /* invalid bitrate index */

  if (h_sampling_freq(flags) == 3)
    return 0; /* invalid sampling freq index */

  return 1;
}




/* returns 0 if id3v2.3 or v2.4 frame found, -1 otherwise */
static int read_id3v2_frame(FILE *f, id3v2_frame *frame, int id3type)
{
  unsigned char size[4];

  switch (id3type & 0xff00) {
    case (0x0200):
      if (fread(frame->frameid, 1, 3, f) != 3 ||
          fread(size, 1, 3, f) != 3)
        return -1;
      frame->size = size[2] + (size [1] << 7) + (size [0] << 14);
      break;
    case (0x0300):
      if (fread(frame->frameid, 1, 4, f) != 4 ||
          fread(&frame->size, 1, 4, f) != 4 ||
	  fread(&frame->flags, 1, 2, f) != 2)
        return -1;
      frame->size = ntohl(frame->size);
      break;
    case (0x0400):
      if (fread(frame->frameid, 1, 4, f) != 4 ||
          fread(size, 1, 4, f) != 4 || fread(&frame->flags, 1, 2, f) != 2)
        return -1;
      frame->size = size[3] + (size [2] << 7) + (size [1] << 14) +
			      (size [0] << 21);
      break;
  }

  frame->flags = ntohs(frame->flags);
  frame->datalen = (frame->size >= sizeof(frame->data) - 1)
			        ? (sizeof(frame->data) - 1) : frame->size;
  if (fread(frame->data, 1, frame->datalen, f) != frame->datalen)
    return -1;
  frame->data[frame -> datalen] = '\0';
  if (frame->size > frame->datalen) {
    if (fseek(f, frame->size - frame->datalen, SEEK_CUR) == -1)
      return (-1);
  }
  if (frame->frameid[0] == 'T') {
    if (frame->data[0] == 0) {
      /* Non-unicode */
      strcpy(frame->data, frame->data + 1);
    } else {
      /* Deunicode the string */
      char *d = frame->data;
      char *s = frame->data + 1;
      while (*s || *(s+1)) {
	/* if it's first 128 bytes of unicode, keep it */
	/* Otherwise replace it with a '?' */
	*d++ = (*s ? '?' : *(s+1));
	s+=2;
      }
      *d = '\0';
    }
  }
  return 0;
}



/* returns 0 if no id3v2 header, otherwise returns tag length, or -1
   and errno set */
static int mp3_id3v2head(FILE *f, int *id3type)
{
  char tag[14];
  int extended_header = 0;
  *id3type = 0;
  if (fseek(f, 0, SEEK_SET) == -1 || 
      fread(tag, 1, sizeof(tag), f) != sizeof (tag))
    return -1;

  if (strncmp(tag, "ID3", 3))
    return 0; /* no header */

  /* Get the major & minor revisions of the ID3 tag type */
  *id3type = (tag [3] << 8) | tag [4];

  /* Check the flags bytes */
  if (tag [5] & 0x40) {
    /* There's an extended header */
    extended_header = tag[13] + (tag[12] << 7) + (tag[11] << 14) +
		      		(tag[10] << 21);
  }

  return ID3V2HEADERLEN + tag[9] + (tag[8] << 7) + (tag[7] << 14) +
				   (tag[6] << 21) + extended_header;
}

/* Search for and read ID3 V1 trailer.
   Return 0 if not found, non-0 if found */
int read_id3_v1_trailer (FILE *f, t_id3tag *id3tag)
{
  int filelen;
  id3tag_disc tag_disc;
  char *e;
  if (fseek(f, -128, SEEK_END) == -1 ||
      (filelen = ftell(f)) == -1 ||
      fread(&tag_disc, 1, sizeof (tag_disc), f) != sizeof (tag_disc))
    return (0);

  if (strncmp(tag_disc.tag, "TAG", 3) != 0)
    return (0);

  id3tag->present = 1;

  memcpy(id3tag->title, tag_disc.title, MP3ID3V1_DISC_TITLELEN);
  id3tag->title[MP3ID3V1_DISC_TITLELEN] = '\0';
  e = id3tag->title + strlen(id3tag->title);
  while (e > id3tag->title && *(e-1) == ' ')
    *--e = '\0';
  
  memcpy(id3tag->artist, tag_disc.artist, MP3ID3V1_DISC_ARTISTLEN);
  id3tag->artist[MP3ID3V1_DISC_ARTISTLEN] = '\0';
  e = id3tag->artist + strlen(id3tag->artist);
  while (e > id3tag->artist && *(e-1) == ' ')
    *--e = '\0';

  memcpy(id3tag->album, tag_disc.album, MP3ID3V1_DISC_ALBUMLEN);
  id3tag->album[MP3ID3V1_DISC_ALBUMLEN] = '\0';
  e = id3tag->album + strlen(id3tag->album);
  while (e > id3tag->album && *(e-1) == ' ')
    *--e = '\0';

  memcpy(id3tag->year, tag_disc.year, MP3ID3V1_DISC_YEARLEN);
  id3tag->year[MP3ID3V1_DISC_YEARLEN] = '\0';
  e = id3tag->year + strlen(id3tag->year);
  while (e > id3tag->year && *(e-1) == ' ')
    *--e = '\0';
  
  memcpy(id3tag->comment, tag_disc.comment, MP3ID3V1_DISC_COMMENTLEN);
  id3tag->comment[MP3ID3V1_DISC_COMMENTLEN] = '\0';
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

void read_id3_v22_header (FILE *f, int headerlen, t_id3tag *id3tag, int type)
{
  id3v2_frame frame;
  if (headerlen && fseek(f, ID3V2HEADERLEN, SEEK_SET) != -1) {

    while (read_id3v2_frame(f, &frame, type) != -1) {
      id3tag->present = 1;
      if (!memcmp(frame.frameid, "TAL", 3)) {
	memcpy(id3tag->album, frame.data, sizeof(id3tag->album) - 1);
	id3tag->album[sizeof(id3tag->album) - 1] = 0;
      }
      else if (!memcmp(frame.frameid, "TP1", 3)) {
	memcpy(id3tag->artist, frame.data, sizeof(id3tag->artist) - 1);
	id3tag->artist[sizeof(id3tag->artist) - 1] = 0;
      }
      else if (!memcmp(frame.frameid, "TT2", 3)) {
	memcpy(id3tag->title, frame.data, sizeof(id3tag->title) - 1);
	id3tag->title[sizeof(id3tag->title) - 1] = 0;
      }
      else if (!memcmp(frame.frameid, "TDY", 3)) {
	memcpy(id3tag->year, frame.data, sizeof(id3tag->year) - 1);
	id3tag->year[sizeof(id3tag->year) - 1] = 0;
      }
      else if (!memcmp(frame.frameid, "TCO", 3)) {
	memcpy(id3tag->genre, frame.data, sizeof(id3tag->genre) - 1);
	id3tag->genre[sizeof(id3tag->genre) - 1] = 0;
	if (id3tag->genre[0] == '(' && isdigit (id3tag->genre[1])) {
	  int number = atoi (&id3tag->genre[1]);
	  if (number < (sizeof(genres_table) / sizeof(char *))) {
	    strncpy(id3tag->genre, genres_table[number],
		    sizeof(id3tag->genre)-1);
	    id3tag->genre[sizeof(id3tag->genre)-1] = '\0';
	  } else {
	    *id3tag->genre = '\0';
	  }
	}
      }
      if (ftell(f) >= headerlen)
	break;
    }
  }
}


void read_id3_v23_24_header (FILE *f, int headerlen, t_id3tag *id3tag, int type)
{
  id3v2_frame frame;
  if (headerlen && fseek(f, ID3V2HEADERLEN, SEEK_SET) != -1) {

    while (read_id3v2_frame(f, &frame, type) != -1) {
      id3tag->present = 1;
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
      else if (!memcmp(frame.frameid, "TYER", 4) ||
	       !memcmp(frame.frameid, "TDRC", 4)) {
	/* For TDRC, year is first part of field so this works */
	memcpy(id3tag->year, frame.data, sizeof(id3tag->year) - 1);
	id3tag->year[sizeof(id3tag->year) - 1] = 0;
      }
      else if (!memcmp(frame.frameid, "TCON", 4)) {
	memcpy(id3tag->genre, frame.data, sizeof(id3tag->genre) - 1);
	id3tag->genre[sizeof(id3tag->genre) - 1] = 0;
	if (id3tag->genre[0] == '(' && isdigit (id3tag->genre[1])) {
	  int number = atoi (&id3tag->genre[1]);
	  if (number < (sizeof(genres_table) / sizeof(char *))) {
	    strncpy(id3tag->genre, genres_table[number],
		    sizeof(id3tag->genre)-1);
	    id3tag->genre[sizeof(id3tag->genre)-1] = '\0';
	  } else {
	    *id3tag->genre = '\0';
	  }
	}
      }
      if (ftell(f) >= headerlen)
	break;
    }
  }
}

/* determines length and bitrate of an mp3. returns -1 on failure with errno
   set, otherwise returns length in hundredths seconds */

int mserv_mp3info_readlen(const char *fname, int *bitrate_ret,
			  t_id3tag *id3tag)
{
  FILE              *f;
  int               bitrate, length, headerlen, frame_location, frame_size;
  int		    frame_size_sum = 0, frame_count = 0, sampling_rate, size;
  int               id3type;
  long int          filelen;
  char              tag[3];
  unsigned char     data[8192];
  unsigned long int flags, lastflags;
  int               errnok;
  unsigned char	    *mp3start, *xing;
  int		    variable_rate = 0, mangled = 0, last_bitrate;
  
  if (id3tag)
    bzero (id3tag, sizeof (*id3tag));

  if ((f = fopen(fname, "rb")) == NULL)
    return -1;

  if ((headerlen = mp3_id3v2head(f, &id3type)) == -1)
    goto error;

  frame_location = headerlen;
  while (fseek(f, frame_location, SEEK_SET) == 0 &&
	 (size = fread(&data, 1, frame_count == 0 ? sizeof(data) : 4, f)) >= 4)
    {

    /* endian independent to 32-bit flags */
    flags = (data[0]<<24)+(data[1]<<16)+(data[2]<<8)+data[3];
    if (!is_mp3(flags)) {
      /* If the first header isn't where we expect it,
	 try searching in a bit more for an MP3 frame sync */
      if (frame_count == 0) {
	mp3start = memchr (data + 1, 0xff, size - 1);
	if (mp3start) {
	  frame_location += mp3start - data;
	  continue;
	}
      }
      if (frame_count == 0)
	errno = EDOM;
      break;
    }

    bitrate = bitrate_table[h_id(flags) & 1][3-h_layer(flags)]
      [h_bitrate_index(flags)];
    sampling_rate = sampling_freq_table[h_id(flags)][h_sampling_freq(flags)];

    if (h_id(flags) == H_ID_MPEG1) {
      frame_size = 144000L * bitrate / sampling_rate + h_padding_bit(flags);
    } else {
      frame_size = 72000L * bitrate / sampling_rate + h_padding_bit(flags);
    }
    if (frame_size == 0) {
      errno = EDOM;
      break;
    }

    lastflags = flags;
    if (frame_count == 0 && size > 60) {
      /* Check for a variable rate header */
      if (h_id (flags) == H_ID_MPEG1 && h_mode (flags) != H_MODE_MONO)
	xing = data + 36;
      else if (h_id (flags) != H_ID_MPEG1 && h_mode (flags) == H_MODE_MONO)
	xing = data + 13;
      else
	xing = data + 21;

      if ((memcmp (xing, "Xing", 4) == 0 || memcmp (xing, "Info", 4) == 0) &&
	  (xing [7] & 0x03) == 0x03) {
	variable_rate = 1;
	frame_count = (xing [8] << 24)+ (xing [9]<<16)
		    + (xing [10] << 8)+ xing [11];
	frame_size_sum = (xing [12]<<24)+(xing [13]<<16)
		       + (xing [14]<< 8)+ xing [15];
	if (frame_count && frame_size_sum)
	  break;
	mserv_log ("%s: File has mangled VBR Xing header.", fname);
	mangled = 1;
	variable_rate = 1;
      }
      /* Check for the other format variable rate header */
      xing = data + 36;
      if (memcmp (xing, "VRBI", 4) == 0) {
	variable_rate = 1;
	/* I don't have any files with this encoding so this has */
	/* never been tested.  */
	frame_count = (xing [14] << 24)+ (xing [15]<<16)
		    + (xing [16] << 8) +  xing [17];
	frame_size_sum = (xing [10]<<24)+(xing [11]<<16)
		       + (xing [12]<< 8)+ xing [13];
	break;
      }
    }
    if (frame_count != 0 && last_bitrate != bitrate)
      variable_rate = 1;
    frame_count ++;
    if (frame_count > 50 && !variable_rate)
      break;

    if (frame_count == 51 && !mangled) {
      mserv_log ("%s: File is variable bitrate, but no VBR header.", fname);
    }
    frame_size_sum += frame_size;
    frame_location += frame_size;
    last_bitrate = bitrate;
  }

  if (frame_count == 0)
    goto error;

  if (!variable_rate) {
    if (fseek(f, -128, SEEK_END) == -1 || (filelen = ftell(f)) == -1)
      goto error;
    frame_size_sum = filelen - headerlen;
    frame_count = frame_size_sum / frame_size;
  }

  /* Calculate length in 1/100 seconds -
     length in time = frames * samples per frame / sampling rate */
  length = ((double) frame_count * (h_id(lastflags) == H_ID_MPEG1 ? 1152 : 576) * 100)
           / sampling_rate;
  /* rate in k = bytes * 8bits/byte / (length in milliseconds) */
  *bitrate_ret = (variable_rate ? ((frame_size_sum * 8) / (length * 10)) : bitrate);

  /* If passed an ID3 structure, read ID3 data. */
  /* For excitement and annoyance, the bastards keep changing the format. */
  if (id3tag) {
    if (headerlen) {
      switch (id3type & 0xff00) {
	case (0x200):
	  read_id3_v22_header (f, headerlen, id3tag, id3type);
	  break;
	case (0x300):
	case (0x400):
	  read_id3_v23_24_header (f, headerlen, id3tag, id3type);
	  break;
      }
    } else {
      /* Check for ID3 version 1 tag */
      read_id3_v1_trailer (f, id3tag);
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


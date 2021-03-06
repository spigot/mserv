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

#include "mserv.h"
#include "defines.h"

const char defconf_file[] = "# Mserv configuration file for " VERSION "\n\
\n\
#############################################################################\n\
#                                   PATHS                                   #\n\
#############################################################################\n\
# File locations, / at start is absolute, otherwise relative to mserv root\n\
#\n\
# path_acl=acl\n\
# path_webacl=webacl\n\
# path_logfile=log\n\
# path_tracks=tracks\n\
# path_trackinfo=trackinfo\n\
# path_playout=player.out\n\
# path_startout=startup.out\n\
# path_idea=idea\n\
# path_libdir=" PKGLIBDIR "\n\
# path_language=" DATADIR "/english.lang\n\
\n\
#############################################################################\n\
#                               OUTPUT STREAM                               #\n\
#############################################################################\n\
# Choose an output stream...\n\
#\n\
# Syntax: CHANNEL OUTPUT ADD <channel> <module> <location> <param>[,<param>]\n\
#\n\
# Example to output to local soundcard:\n\
# exec=channel output add default ossaudio /dev/dsp mixer=/dev/mixer\n\
#\n\
# Example to stream to an icecast server at samplerate 44100, 48kbit stereo:\n\
# exec=channel output add default icecast http://source:password@localhost:8000/mserv.ogg bitrate=48000\n\
#\n\
# Example to stream to an icecast server at samplerate 8000, 8kbit mono:\n\
# exec=channel output add default icecast http://source:password@localhost:8000/mserv.ogg resample=8000,bitrate=8000,downmix\n\
#\n\
# NOTE: Ogg Vorbis is picky what bitrate is allowed at a particular setting\n\
#       If you get \"failed to initialise vorbis engine\" this is probably\n\
#       the cause.  e.g. 48kb/s seems to be the lowest at 44100 stereo,\n\
#       32kb/s the lowest at 44100 mono.\n\
\n\
#############################################################################\n\
#                                  PLAYERS                                  #\n\
#############################################################################\n\
# Define player invokation methods\n\
#\n\
# prog_freeamp=freeamp -ui mpg123\n\
# prog_sox=sox %s -r 44100 -s -w -c 2 -t raw -\n\
# prog_mservplay=mservplay 0 mpg123 -s -b 1024\n\
prog_mpg123=mpg123 --stereo --rate 44100 -s -b 1024\n\
prog_ogg123=ogg123 --device=raw -f-\n\
prog_xmp=xmp --stdout --bits 16 --stereo --frequency 44100\n\
\n\
#############################################################################\n\
#                         RECOGNISED FILE EXTENSIONS                        #\n\
#############################################################################\n\
# Set streaming players for each file extension we want to support, unknown\n\
# extensions are ignored by mserv\n\
#\n\
# player_wav=prog_sox\n\
# player_au=prog_sox\n\
player_mp3=prog_mpg123\n\
player_ogg=prog_ogg123\n\
player_mod=prog_xmp\n\
\n\
#############################################################################\n\
#                                  OPTIONS                                  #\n\
#############################################################################\n\
#\n\
# Set default random mode, either on or off.  You must still tell mserv to\n\
# start playing (PLAY command or set play=on).\n\
# random=off\n\
#\n\
# Set whether or not play should start as soon as mserv has loaded.  You\n\
# need to have items queued or random on (RANDOM command or set random=on).\n\
# play=off\n\
#\n\
# Set default random factor, 0.5 is completely random, 0.6 is less random\n\
# and takes into account your ratings, 0.4 plays your worst tunes. 0.99 max.\n\
# factor=0.60\n\
#\n\
# Set default filter, leave blank for off.  Example: \"!genre=classical\" to\n\
# not play classical genre, or \"year>=1980&year<1990\" to only play\n\
# 80's songs.\n\
# filter=\n\
#\n\
# Set gap between songs, in seconds.  0 to start the next song as quick as\n\
# possible.\n\
# gap=1.0\n\
#\n\
# Default user rating for songs that haven't been heard yet (0 - 1)\n\
# 0=awful, 0.25=bad, 0.50=neutral, 0.75=good, 1=superb\n\
# rate_unheard=0.55\n\
#\n\
# Default user rating for songs that haven't been rated yet (0 - 1)\n\
# 0=awful, 0.25=bad, 0.50=neutral, 0.75=good, 1=superb\n\
# rate_unrated=0.50\n\
#\n\
#\n\
# Enable experimental fairness algorithm.  When enabled, users who have\n\
# had their music played a lot will become less important when selecting\n\
# the next song to play.  Users who have heard bad music lately will become\n\
# more important.\n\
# experimental_fairness=on\n\
#\n\
# Set whether or not each type of user has their queue cleared on disconnect\n\
# queue_clear_human=on\n\
# queue_clear_computer=off\n\
# queue_clear_rtcomputer=on\n\
#\n\
# Set whether or not particular alerts are displayed\n\
# human_alert_unheard=on\n\
# human_alert_unrated=on\n\
# human_alert_firstplay=on\n\
# human_alert_nogenre=on\n\
#\n\
# Set display options for HUMAN mode users\n\
# human_display_screen=79\n\
# human_display_info=10\n\
# human_display_track=7\n\
# human_display_author=20\n\
# human_display_align=on\n\
# human_display_bold=on\n\
";

unsigned int defconf_size = sizeof(defconf_file)-1;

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
# path_idea=idea\n\
# path_mixer=/dev/mixer\n\
# path_language=" DATADIR "/english.lang\n\
\n\
#############################################################################\n\
#                          OUTPUT ENGINE - ICECAST                          #\n\
#############################################################################\n\
# Icecast output engine parameters\n\
#\n\
# default_icecast_output=http://source:password@localhost:8000/mserv.ogg\n\
# default_icecast_bitrate=48000\n\
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
# start playing (PLAY).\n\
# random=off\n\
#\n\
# Set whether or not you play should start as soon as mserv has loaded.\n\
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
# opt_rate_unheard=0.55\n\
#\n\
# Default user rating for songs that haven't been rated yet (0 - 1)\n\
# 0=awful, 0.25=bad, 0.50=neutral, 0.75=good, 1=superb\n\
# opt_rate_unrated=0.50\n\
\n\
";

unsigned int defconf_size = sizeof(defconf_file)-1;

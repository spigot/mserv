#include "defines.h"

const char defconf_file[] = "# Mserv configuration file for 0.33 and later\n\
\n\
# File locations, / at start is absolute, otherwise relative to mserv root\n\
path_acl=acl\n\
path_webacl=webacl\n\
path_logfile=log\n\
path_tracks=tracks\n\
path_trackinfo=trackinfo\n\
path_playout=player.out\n\
path_idea=idea\n\
path_mixer=/dev/mixer\n\
path_language=" DATADIR "/english.lang\n\
\n\
# Define player invokation methods\n\
# mservplay is our special wrapper, the first parameter is a 'nice' level\n\
# play is part of sox\n\
prog_mpg123=/usr/local/bin/mpg123 -b 1024\n\
prog_freeamp=/usr/local/bin/freeamp -ui mpg123\n\
prog_mservplay=/usr/local/bin/mservplay 0 mpg123 -b 1024\n\
prog_play=/usr/local/bin/play\n\
\n\
# Set players for each file extension we want to support, unknown extensions\n\
# are ignored by mserv\n\
player_mp3=prog_mpg123\n\
player_wav=prog_play\n\
player_au=prog_play\n\
\n\
# Set default random mode, either on or off.  You must still tell mserv to\n\
# start playing (PLAY).\n\
random=off\n\
\n\
# Set whether or not you would like play to start as soon as mserv has loaded.\n\
play=off\n\
\n\
# Set default random factor, 0.5 is completely random, 0.6 is less random\n\
# and takes into account your ratings, 0.4 plays your worst tunes. 0.99 max.\n\
factor=0.60\n\
\n\
# Set default filter, leave blank for off.  Example: \"!classical\" to\n\
# not play classical genre, or \"year>=1980&year<1990\" to only play\n\
# 80's songs.\n\
filter=\n\
\n\
# Set gap between songs, in seconds.  0 to start the next song as quick as\n\
# possible.\n\
gap=1\n\
\n\
";

unsigned int defconf_size = sizeof(defconf_file)-1;

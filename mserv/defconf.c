#include "defines.h"

const char defconf_file[] = "# Mserv configuration file for 0.33 and later

# File locations, / at start is absolute, otherwise relative to mserv root
path_acl=acl
path_webacl=webacl
path_logfile=log
path_tracks=tracks
path_trackinfo=trackinfo
path_playout=player.out
path_idea=idea
path_mixer=/dev/mixer
path_language=" SHAREDIR "/english.lang

# Define player invokation methods
# mservplay is our special wrapper, the first parameter is a 'nice' level
# play is part of sox
prog_mpg123=/usr/local/bin/mpg123 -b 1024
prog_freeamp=/usr/local/bin/freeamp -ui mpg123
prog_mservplay=/usr/local/bin/mservplay 0 mpg123 -b 1024
prog_play=/usr/local/bin/play

# Set players for each file extension we want to support, unknown extensions
# are ignored by mserv
player_mp3=prog_mpg123
player_wav=prog_play
player_au=prog_play

# Set default random mode, either on or off.  You must still tell mserv to
# start playing (PLAY).
random=off

# Set whether or not you would like play to start as soon as mserv has loaded.
play=off

# Set default random factor, 0.5 is completely random, 0.6 is less random
# and takes into account your ratings, 0.4 plays your worst tunes. 0.99 max.
factor=0.60

# Set default filter, leave blank for off.  Example: \"!classical\" to
# not play classical genre, or \"year>=1980&year<1990\" to only play
# 80's songs.
filter=

# Set gap between songs, in seconds.  0 to start the next song as quick as
# possible.
gap=1

";

unsigned int defconf_size = sizeof(defconf_file)-1;

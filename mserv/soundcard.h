#if defined(HAVE_SYS_SOUNDCARD_H)
# include <sys/soundcard.h>
# define SOUNDCARD 1
#elif defined(HAVE_SOUNDCARD_H)
# include <soundcard.h>
# define SOUNDCARD 1
#else
# undef SOUNDCARD
#endif

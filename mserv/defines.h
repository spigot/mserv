/* maximum amount to queue in each output buffer */
#define OUTBUFLEN 32*1024

/* maximum number of output buffers per user */
/* check /usr/include/sys/uio.h before increasing this beyond 16 */
#define OUTVECTORS 16

/* maximum amount to load into input buffer in one go */
#define INBUFLEN 256

/* maximum input length */
#define LINEBUFLEN 512

/* number of connections to hold on the accept queue */
#define BACKLOG 5

/* maximum username length */
#define USERNAMELEN 16

/* maximum password length */
#define PASSWORDLEN 16

/* maximum acl line length */
#define ACLLINELEN 1024

/* maximum conf line length */
#define CONFLINELEN 1024

/* maximum track or album author length */
#define AUTHORLEN 128

/* maximum track name or album name length */
#define NAMELEN 128

/* maximum track genres length */
#define GENRESLEN 128

/* maximum misc format-specific length */
#define MISCINFOLEN 128

/* maximum language line length */
#define LANGLINELEN 1024

/* number of entries to keep in history */
#define HISTORYLEN 20

/* maximum number of passed parameters to setuid player */
#define PLAYER_MAXPARAMS 10

/* maximum filename supported */
#define MAXFNAME 512

/* debug on/off */
/* #define DEBUG(a) (a) */
#define DEBUG(a) 

/* IDEA on/off */
#define IDEA 1

/* debugging of parser on/off */
#undef DEBUGPARSER

/* factor for deciding curve for factor command */
#define RFACT 256

/* maximum number of genres in tracks */
#define MAXNGENRE 16

/* maximum items on stack */
#define MAXNSTACK 128

/* maximum token length in mode string */
#define TOKENLEN 32

/* maximum filter string length */
#define FILTERLEN 512

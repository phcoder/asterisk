#include <stddef.h>
#include <sys/time.h>
#include "asterisk/localtime.h"

struct timeval unpackdate(unsigned char *i);
int unpacksms(unsigned char dcs, unsigned char *i, unsigned char *udh, int *udhl, unsigned short *ud, int *udl, char udhi);
unsigned char unpackaddress(char *o, unsigned char *i, unsigned int maxlen);
int packsms(unsigned char dcs, unsigned char *base, unsigned int udhl, unsigned char *udh, int udl, unsigned short *ud);
long utf8decode(unsigned char **pp);
void utf16_to_utf8(unsigned short *in, size_t inlen, char *dest, int maxlen);
unsigned char packaddress(unsigned char *o, const char *i);
int packsms7(unsigned char *o, int udhl, unsigned char *udh, int udl, unsigned short *ud);
int packsms8(unsigned char *o, int udhl, unsigned char *udh, int udl, unsigned short *ud);
int packsms16(unsigned char *o, int udhl, unsigned char *udh, int udl, unsigned short *ud);
void packdate(unsigned char *o, time_t w);

/* different types of encoding */
#define is7bit(dcs)  ( ((dcs) & 0xC0) ? (!((dcs) & 4) ) : (((dcs) & 0xc) == 0) )
#define is8bit(dcs)  ( ((dcs) & 0xC0) ? ( ((dcs) & 4) ) : (((dcs) & 0xc) == 4) )
#define is16bit(dcs) ( ((dcs) & 0xC0) ? 0               : (((dcs) & 0xc) == 8) )

#define SMSLEN      160          /*!< max SMS length */
#define SMSLEN_8    140          /*!< max SMS length for 8-bit char */
extern const unsigned short defaultalphabet[128];
extern const unsigned short escapes[128];

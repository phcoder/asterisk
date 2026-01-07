/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2004 - 2005, Adrian Kennard, rights assigned to Digium
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief SMS application - ETSI ES 201 912 protocol 1 implementation
 *
 * \par Development notes
 * \note The ETSI standards are available free of charge from ETSI at
 *	http://pda.etsi.org/pda/queryform.asp
 * 	Among the relevant documents here we have:
 *
 *	ES 201 912	SMS for PSTN/ISDN
 *	TS 123 040	Technical realization of SMS
 *
 *
 * \ingroup applications
 *
 * \author Adrian Kennard (for the original protocol 1 code)
 * \author Filippo Grassilli (Hyppo) - protocol 2 support
 *		   Not fully tested, under development
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"
#include "smslib.h"

#include "asterisk/pbx.h"

#include <stddef.h>
#include <ctype.h>

/* SMS 7 bit character mapping to UCS-2 */
const unsigned short defaultalphabet[] = {
	0x0040, 0x00A3, 0x0024, 0x00A5, 0x00E8, 0x00E9, 0x00F9, 0x00EC,
	0x00F2, 0x00E7, 0x000A, 0x00D8, 0x00F8, 0x000D, 0x00C5, 0x00E5,
	0x0394, 0x005F, 0x03A6, 0x0393, 0x039B, 0x03A9, 0x03A0, 0x03A8,
	0x03A3, 0x0398, 0x039E, 0x00A0, 0x00C6, 0x00E6, 0x00DF, 0x00C9,
	' ', '!', '"', '#', 164, '%', '&', 39, '(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
	161, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 196, 214, 209, 220, 167,
	191, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 228, 246, 241, 252, 224,
};

const unsigned short escapes[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x000C, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0x005E, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0x007B, 0x007D, 0, 0, 0, 0, 0, 0x005C,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x005B, 0x007E, 0x005D, 0,
	0x007C, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0x20AC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/*! \brief unpack a date and return */
struct timeval unpackdate(unsigned char *i)
{
	struct ast_tm t;

	t.tm_year = 100 + (i[0] & 0xF) * 10 + (i[0] >> 4);
	t.tm_mon = (i[1] & 0xF) * 10 + (i[1] >> 4) - 1;
	t.tm_mday = (i[2] & 0xF) * 10 + (i[2] >> 4);
	t.tm_hour = (i[3] & 0xF) * 10 + (i[3] >> 4);
	t.tm_min = (i[4] & 0xF) * 10 + (i[4] >> 4);
	t.tm_sec = (i[5] & 0xF) * 10 + (i[5] >> 4);
	t.tm_isdst = 0;
	if (i[6] & 0x08) {
		t.tm_min += 15 * ((i[6] & 0x7) * 10 + (i[6] >> 4));
	} else {
		t.tm_min -= 15 * ((i[6] & 0x7) * 10 + (i[6] >> 4));
	}

	return ast_mktime(&t, NULL);
}

static unsigned int
decode_7bit(unsigned short *ud, unsigned char *i, unsigned int l, unsigned char b)
{
  	unsigned short *o = ud;
	unsigned int p = 0;
  	while (l--) {
		unsigned char v;
		if (b < 2) {
			v = ((i[p] >> b) & 0x7F);       /* everything in one byte */
		} else {
			v = ((((i[p] >> b) + (i[p + 1] << (8 - b)))) & 0x7F);
		}
		b += 7;
		if (b >= 8) {
			b -= 8;
			p++;
		}
		/* 0x00A0 is the encoding of ESC (27) in defaultalphabet */
		if (o > ud && o[-1] == 0x00A0 && escapes[v]) {
			o[-1] = escapes[v];
		} else {
			*o++ = defaultalphabet[v];
		}
	}
	return o - ud;
}

/*! \brief unpacks bytes (7 bit encoding) at i, len l septets,
	and places in udh and ud setting udhl and udl. udh not used
	if udhi not set */
static void unpacksms7(unsigned char *i, unsigned char l, unsigned char *udh, int *udhl, unsigned short *ud, int *udl, char udhi)
{
	unsigned char b = 0, p = 0;
	*udhl = 0;
	if (udhi && l) {                        /* header */
		int h = i[p];
		*udhl = h;
		if (h) {
			b = 1;
			p++;
			l--;
			while (h-- && l) {
				*udh++ = i[p++];
				b += 8;
				while (b >= 7) {
					b -= 7;
					l--;
					if (!l) {
						break;
					}
				}
			}
			/* adjust for fill, septets */
			if (b) {
				b = 7 - b;
				l--;
			}
		}
	}
	*udl = decode_7bit(ud, i + p, l, b);
}

/*! \brief unpacks bytes (8 bit encoding) at i, len l septets,
 *  and places in udh and ud setting udhl and udl. udh not used
 *  if udhi not set.
 */
static void unpacksms8(unsigned char *i, unsigned char l, unsigned char *udh, int *udhl, unsigned short *ud, int *udl, char udhi)
{
	unsigned short *o = ud;
	*udhl = 0;
	if (udhi) {
		int n = *i;
		*udhl = n;
		if (n) {
			i++;
			l--;
			while (l && n) {
				l--;
				n--;
				*udh++ = *i++;
			}
		}
	}
	while (l--) {
		*o++ = *i++;                        /* not to UTF-8 as explicitly 8 bit coding in DCS */
	}
	*udl = (o - ud);
}

/*! \brief unpacks bytes (16 bit encoding) at i, len l septets,
	 and places in udh and ud setting udhl and udl.
	udh not used if udhi not set */
static void unpacksms16(unsigned char *i, unsigned char l, unsigned char *udh, int *udhl, unsigned short *ud, int *udl, char udhi)
{
	unsigned short *o = ud;
	*udhl = 0;
	if (udhi) {
		int n = *i;
		*udhl = n;
		if (n) {
			i++;
			l--;
			while (l && n) {
				l--;
				n--;
				*udh++ = *i++;
			}
		}
	}
	while (l--) {
		int v = *i++;
		if (l && l--) {
			v = (v << 8) + *i++;
		}
		*o++ = v;
	}
	*udl = (o - ud);
}

/*! \brief general unpack - starts with length byte (octet or septet) and returns number of bytes used, inc length */
int unpacksms(unsigned char dcs, unsigned char *i, unsigned char *udh, int *udhl, unsigned short *ud, int *udl, char udhi)
{
	int l = *i++;
	if (is7bit(dcs)) {
		unpacksms7(i, l, udh, udhl, ud, udl, udhi);
		l = (l * 7 + 7) / 8;                /* adjust length to return */
	} else if (is8bit(dcs)) {
		unpacksms8(i, l, udh, udhl, ud, udl, udhi);
	} else {
		l += l % 2;
		unpacksms16(i, l, udh, udhl, ud, udl, udhi);
	}
	return l + 1;
}

void
utf16_to_utf8(unsigned short *in, size_t inlen, char *dest, int maxlen)
{
	uint32_t code_high = 0;

	if (!maxlen)
		return;
	maxlen--;

	while (inlen-- && maxlen > 0)
	{
		uint32_t code = *in++;

		if (code_high)
		{
			if (code >= 0xDC00 && code <= 0xDFFF)
			{
				/* Surrogate pair.  */
				code = ((code_high - 0xD800) << 10) + (code - 0xDC00) + 0x10000;

				if (maxlen-- > 0) *dest++ = (code >> 18) | 0xF0;
				if (maxlen-- > 0) *dest++ = ((code >> 12) & 0x3F) | 0x80;
				if (maxlen-- > 0) *dest++ = ((code >> 6) & 0x3F) | 0x80;
				if (maxlen-- > 0) *dest++ = (code & 0x3F) | 0x80;
			}
			else
			{
				/* Error...  */
				if (maxlen-- > 0) *dest++ = '?';
				/* *src may be valid. Don't eat it.  */
				in--;
				inlen++;
			}

			code_high = 0;
		}
		else
		{
			if (code <= 0x007F)
			{
				if (maxlen-- > 0) *dest++ = code;
			}
			else if (code <= 0x07FF)
			{
				if (maxlen-- > 0) *dest++ = (code >> 6) | 0xC0;
				if (maxlen-- > 0) *dest++ = (code & 0x3F) | 0x80;
			}
			else if (code >= 0xD800 && code <= 0xDBFF)
			{
				code_high = code;
				continue;
			}
			else if (code >= 0xDC00 && code <= 0xDFFF)
			{
				/* Error... */
				if (maxlen-- > 0) *dest++ = '?';
			}
			else
			{
				if (maxlen-- > 0) *dest++ = (code >> 12) | 0xE0;
				if (maxlen-- > 0) *dest++ = ((code >> 6) & 0x3F) | 0x80;
				if (maxlen-- > 0) *dest++ = (code & 0x3F) | 0x80;
			}
		}
	}

	*dest = '\0';
}

/*! \brief unpack an address from i, return byte length, unpack to o */
unsigned char unpackaddress(char *o, unsigned char *i, unsigned int maxlen)
{
	unsigned char l = i[0], p;
	if ((i[1] & 0xf0) == 0xd0) {
		unsigned short s[300];
		unsigned int sl = decode_7bit(s, i + 2, (l * 4) / 7, 0);
		utf16_to_utf8(s, sl, o, maxlen);
		return (l + 5) / 2;
	}
	if (i[1] == 0x91) {
		*o++ = '+';
	}
	for (p = 0; p < l && p < maxlen - 1; p++) {
		if (p & 1) {
			*o++ = (i[2 + p / 2] >> 4) + '0';
		} else {
			*o++ = (i[2 + p / 2] & 0xF) + '0';
		}
	}
	*o = 0;
	return (l + 5) / 2;
}

/*! \brief store an address at o, and return number of bytes used */
unsigned char packaddress(unsigned char *o, const char *i)
{
	unsigned char p = 2;
	o[0] = 0;                               /* number of bytes */
	if (*i == '+') {                        /* record as bit 0 in byte 1 */
		i++;
		o[1] = 0x91;
	} else {
		o[1] = 0x81;
	}
	for ( ; *i ; i++) {
		if (!isdigit(*i)) {                 /* ignore non-digits */
			continue;
		}
		if (o[0] & 1) {
			o[p++] |= ((*i & 0xF) << 4);
		} else {
			o[p] = (*i & 0xF);
		}
		o[0]++;
	}
	if (o[0] & 1) {
		o[p++] |= 0xF0;                     /* pad */
	}
	return p;
}

/*! \brief takes a binary header (udhl bytes at udh) and UCS-2 message (udl characters at ud) and packs in to o using SMS 7 bit character codes */
/* The return value is the number of septets packed in to o, which is internally limited to SMSLEN */
/* o can be null, in which case this is used to validate or count only */
/* if the input contains invalid characters then the return value is -1 */
int packsms7(unsigned char *o, int udhl, unsigned char *udh, int udl, unsigned short *ud)
{
	unsigned char p = 0;                    /* output pointer (bytes) */
	unsigned char b = 0;                    /* bit position */
	unsigned char n = 0;                    /* output character count */
	unsigned char dummy[SMSLEN];

	if (o == NULL) {                        /* output to a dummy buffer if o not set */
		o = dummy;
	}

	if (udhl) {                             /* header */
		o[p++] = udhl;
		b = 1;
		n = 1;
		while (udhl--) {
			o[p++] = *udh++;
			b += 8;
			while (b >= 7) {
				b -= 7;
				n++;
			}
			if (n >= SMSLEN)
				return n;
		}
		if (b) {
			b = 7 - b;
			if (++n >= SMSLEN)
				return n;
		}                                   /* filling to septet boundary */
	}
	o[p] = 0;
	/* message */
	while (udl--) {
		long u;
		unsigned char v;
		u = *ud++;
		/* XXX 0 is invalid ? */
		/* look up in defaultalphabet[]. If found, v is the 7-bit code */
		for (v = 0; v < 128 && defaultalphabet[v] != u; v++);
		if (v == 128 /* not found */ && u && n + 1 < SMSLEN) {
			/* if not found, look in the escapes table (we need 2 bytes) */
			for (v = 0; v < 128 && escapes[v] != u; v++);
			if (v < 128) {	/* escaped sequence, esc + v */
				/* store the low (8-b) bits in o[p], the remaining bits in o[p+1] */
				o[p] |= (27 << b);          /* the low bits go into o[p] */
				b += 7;
				if (b >= 8) {
					b -= 8;
					p++;
					o[p] = (27 >> (7 - b));
				}
				n++;
			}
		}
		if (v == 128)
			return -1;                      /* invalid character */
		/* store, same as above */
		o[p] |= (v << b);
		b += 7;
		if (b >= 8) {
			b -= 8;
			p++;
			o[p] = (v >> (7 - b));
		}
		if (++n >= SMSLEN)
			return n;
	}
	return n;
}

/*! \brief takes a binary header (udhl bytes at udh) and UCS-2 message (udl characters at ud)
 * and packs in to o using 8 bit character codes.
 * The return value is the number of bytes packed in to o, which is internally limited to 140.
 * o can be null, in which case this is used to validate or count only.
 * if the input contains invalid characters then the return value is -1
 */
int packsms8(unsigned char *o, int udhl, unsigned char *udh, int udl, unsigned short *ud)
{
	unsigned char p = 0;
	unsigned char dummy[SMSLEN_8];

	if (o == NULL)
		o = dummy;
	/* header - no encoding */
	if (udhl) {
		o[p++] = udhl;
		while (udhl--) {
			o[p++] = *udh++;
			if (p >= SMSLEN_8) {
				return p;
			}
		}
	}
	while (udl--) {
		long u;
		u = *ud++;
		if (u < 0 || u > 0xFF) {
			return -1;                      /* not valid */
		}
		o[p++] = u;
		if (p >= SMSLEN_8) {
			return p;
		}
	}
	return p;
}

/*! \brief takes a binary header (udhl bytes at udh) and UCS-2
	message (udl characters at ud) and packs in to o using 16 bit
	UCS-2 character codes
	The return value is the number of bytes packed in to o, which is
	internally limited to 140
	o can be null, in which case this is used to validate or count
	only if the input contains invalid characters then
	the return value is -1 */
int packsms16(unsigned char *o, int udhl, unsigned char *udh, int udl, unsigned short *ud)
{
	unsigned char p = 0;
	unsigned char dummy[SMSLEN_8];

	if (o == NULL) {
		o = dummy;
	}
	/* header - no encoding */
	if (udhl) {
		o[p++] = udhl;
		while (udhl--) {
			o[p++] = *udh++;
			if (p >= SMSLEN_8) {
				return p;
			}
		}
	}
	while (udl--) {
		long u;
		u = *ud++;
		o[p++] = (u >> 8);
		if (p >= SMSLEN_8) {
			return p - 1;                   /* could not fit last character */
		}
		o[p++] = u;
		if (p >= SMSLEN_8) {
			return p;
		}
	}
	return p;
}

/*! \brief general pack, with length and data,
	returns number of bytes of target used */
int packsms(unsigned char dcs, unsigned char *base, unsigned int udhl, unsigned char *udh, int udl, unsigned short *ud)
{
	unsigned char *p = base;
	if (udl == 0) {
		*p++ = 0;                           /* no user data */
	} else {

		int l = 0;
		if (is7bit(dcs)) {                  /* 7 bit */
			if ((l = packsms7(p + 1, udhl, udh, udl, ud)) < 0) {
				l = 0;
			}
			*p++ = l;
			p += (l * 7 + 7) / 8;
		} else if (is8bit(dcs)) {           /* 8 bit */
			if ((l = packsms8(p + 1, udhl, udh, udl, ud)) < 0) {
				l = 0;
			}
			*p++ = l;
			p += l;
		} else {                            /* UCS-2 */
			if ((l = packsms16(p + 1, udhl, udh, udl, ud)) < 0) {
				l = 0;
			}
			*p++ = l;
			p += l;
		}
	}
	return p - base;
}

/*! \brief Reads next UCS character from NUL terminated UTF-8 string and advance pointer */
/* for non valid UTF-8 sequences, returns character as is */
/* Does not advance pointer for null termination */
long utf8decode(unsigned char **pp)
{
	unsigned char *p = *pp;
	if (!*p) {
		return 0;                           /* null termination of string */
	}
	(*pp)++;
	if (*p < 0xC0) {
		return *p;                          /* ascii or continuation character */
	}
	if (*p < 0xE0) {
		if (*p < 0xC2 || (p[1] & 0xC0) != 0x80) {
			return *p;                      /* not valid UTF-8 */
		}
		(*pp)++;
		return ((*p & 0x1F) << 6) + (p[1] & 0x3F);
   	}
	if (*p < 0xF0) {
		if ((*p == 0xE0 && p[1] < 0xA0) || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) {
			return *p;                      /* not valid UTF-8 */
		}
		(*pp) += 2;
		return ((*p & 0x0F) << 12) + ((p[1] & 0x3F) << 6) + (p[2] & 0x3F);
	}
	if (*p < 0xF8) {
		if ((*p == 0xF0 && p[1] < 0x90) || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) {
			return *p;                      /* not valid UTF-8 */
		}
		(*pp) += 3;
		return ((*p & 0x07) << 18) + ((p[1] & 0x3F) << 12) + ((p[2] & 0x3F) << 6) + (p[3] & 0x3F);
	}
	if (*p < 0xFC) {
		if ((*p == 0xF8 && p[1] < 0x88) || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80
			|| (p[4] & 0xC0) != 0x80) {
			return *p;                      /* not valid UTF-8 */
		}
		(*pp) += 4;
		return ((*p & 0x03) << 24) + ((p[1] & 0x3F) << 18) + ((p[2] & 0x3F) << 12) + ((p[3] & 0x3F) << 6) + (p[4] & 0x3F);
	}
	if (*p < 0xFE) {
		if ((*p == 0xFC && p[1] < 0x84) || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80
			|| (p[4] & 0xC0) != 0x80 || (p[5] & 0xC0) != 0x80) {
			return *p;                      /* not valid UTF-8 */
		}
		(*pp) += 5;
		return ((*p & 0x01) << 30) + ((p[1] & 0x3F) << 24) + ((p[2] & 0x3F) << 18) + ((p[3] & 0x3F) << 12) + ((p[4] & 0x3F) << 6) + (p[5] & 0x3F);
	}
	return *p;                              /* not sensible */
}

/*! \brief pack a date and return */
void packdate(unsigned char *o, time_t w)
{
	struct ast_tm t;
	struct timeval topack = { w, 0 };
	int z;

	ast_localtime(&topack, &t, NULL);
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined( __NetBSD__ ) || defined(__APPLE__) || defined(__CYGWIN__)
	z = -t.tm_gmtoff / 60 / 15;
#else
	z = timezone / 60 / 15;
#endif
	*o++ = ((t.tm_year % 10) << 4) + (t.tm_year % 100) / 10;
	*o++ = (((t.tm_mon + 1) % 10) << 4) + (t.tm_mon + 1) / 10;
	*o++ = ((t.tm_mday % 10) << 4) + t.tm_mday / 10;
	*o++ = ((t.tm_hour % 10) << 4) + t.tm_hour / 10;
	*o++ = ((t.tm_min % 10) << 4) + t.tm_min / 10;
	*o++ = ((t.tm_sec % 10) << 4) + t.tm_sec / 10;
	if (z < 0) {
		*o++ = (((-z) % 10) << 4) + (-z) / 10 + 0x08;
	} else {
		*o++ = ((z % 10) << 4) + z / 10;
	}
}

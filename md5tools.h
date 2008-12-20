/*
 * md5tools.h: TVTV plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __MD5TOOLS_H
#define __MD5TOOLS_H

#include <stdint.h>
#include <string.h>


/* ---- MD5 ---- */
/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
    ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

// MD5

typedef unsigned long uint32;

struct MD5Context {
    uint32_t buf[4];
    uint32_t bits[2];
    unsigned char in[64];
};
			
typedef struct MD5Context MD5_CTX;
#define byteReverse(buf, len)	/* Nothing */


void MD5Init(MD5_CTX *ctx);
void MD5Update(MD5_CTX *ctx, unsigned char *buf, unsigned len);
void MD5Final(unsigned char digest[16], MD5_CTX *ctx);


#endif //__MD5TOOLS_H

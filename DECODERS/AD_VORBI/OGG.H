/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2001             *
 * by the XIPHOPHORUS Company http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: toplevel libogg include
 last mod: $Id: ogg.h,v 1.20 2003/02/10 00:00:00 PDSoft Exp $

 ********************************************************************/
#ifndef _OGG_H
#define _OGG_H

#include "os_types.h"

#define OGG_SYNC_BUFFER_SIZE 8192

#ifdef OGG_USE_ASM
#define OGGPACK_ASM 1
#endif

typedef struct {
	unsigned long bitpos;		// we assume that we never store more than 4Gbits
	unsigned long storedbits;	// detto
	unsigned char *buffer;		// begin of buffer
} oggpack_buffer;

extern unsigned long mask[33];

#if defined(OGGPACK_ASM) && defined(__WATCOMC__)

long oggpack_inline_look32(oggpack_buffer *, int);
#pragma aux oggpack_inline_look32=\
 "mov ebx,dword ptr [eax]"\
 "mov esi,ebx"\
 "add esi,edx"\
 "cmp esi,dword ptr 4[eax]"\
 "jbe lookok"\
  "mov eax,0xffffffff"\
  "jmp lookend"\
 "lookok:"\
 "mov ecx,ebx"\
 "shr ebx,3"\
 "add ebx,dword ptr 8[eax]"\
 "and ecx,7"\
 "mov eax,dword ptr [ebx]"\
 "shr eax,cl"\
 "lea esi,[edx+ecx]"\
 "cmp esi,32"\
 "jbe lookmask"\
  "test edx,edx"\
  "je  lookmask"\
   "mov esi,ecx"\
   "mov ecx,32"\
   "sub ecx,esi"\
   "movzx ebx,byte ptr 4[ebx]"\
   "shl ebx,cl"\
   "or eax,ebx"\
 "lookmask:"\
 "and eax,mask[edx*4]"\
 "lookend:"\
 parm[eax][edx] value[eax] modify[ebx ecx esi];

// no overflow check! (run oggpack_leftbits() before)
long oggpack_inline_look24noc(oggpack_buffer *, int);
#pragma aux oggpack_inline_look24noc=\
 "mov eax,dword ptr [edx]"\
 "mov ecx,eax"\
 "shr eax,3"\
 "add eax,dword ptr 8[edx]"\
 "and ecx,7"\
 "mov eax,dword ptr [eax]"\
 "shr eax,cl"\
 "and eax,mask[ebx*4]"\
 parm[edx][ebx] value[eax] modify[ecx];

//we get 25..32 bits (we don't know how many, but we don't care with it)
//we need 24 bits only
long oggpack_inline_look2432noc(oggpack_buffer *);
#pragma aux oggpack_inline_look2432noc=\
 "mov eax,dword ptr [edx]"\
 "mov ecx,eax"\
 "shr eax,3"\
 "add eax,dword ptr 8[edx]"\
 "and ecx,7"\
 "mov eax,dword ptr [eax]"\
 "shr eax,cl"\
 parm[edx] value[eax] modify[ecx];

void oggpack_inline_adv(oggpack_buffer *, int);
#pragma aux oggpack_inline_adv=\
 "add dword ptr [edx],eax"\
 parm [edx][eax]

long oggpack_inline_leftbits(oggpack_buffer *);
#pragma aux oggpack_inline_leftbits=\
 "mov eax,dword ptr 4[edx]"\
 "sub eax,dword ptr [edx]"\
 parm [edx] value [eax]

void oggpack_inline_setleft(oggpack_buffer *, int left);
#pragma aux oggpack_inline_setleft=\
 "mov ebx,dword ptr 4[edx]"\
 "sub ebx,eax"\
 "mov dword ptr [edx],ebx"\
 parm [edx][eax] modify[ebx]

#else

long oggpack_look32(oggpack_buffer *, int);
void oggpack_adv(oggpack_buffer *, int);
long oggpack_leftbits(oggpack_buffer *);
void oggpack_setleft(oggpack_buffer *, int left);

#define oggpack_inline_look32(b,n)    (oggpack_look32(b,n))
#define oggpack_inline_look24noc(b,n) (oggpack_look32(b,n))
#define oggpack_inline_look2432noc(b) (oggpack_look32(b,24))
#define oggpack_inline_adv(b,n)       (oggpack_adv(b,n))
#define oggpack_inline_leftbits(b)    (oggpack_leftbits(b))
#define oggpack_inline_setleft(b,n)   (oggpack_setleft(b,n))

#endif


/* ogg_page is used to encapsulate the data in one Ogg bitstream page *****/

typedef struct {
	unsigned char *header;
	long header_len;
	unsigned char *body;
	long body_len;
} ogg_page;

/* ogg_stream_state contains the current encode/decode state of a logical
   Ogg bitstream **********************************************************/

typedef struct {
	unsigned char *body_data;	/* bytes from packet bodies */
	long body_storage;			/* storage elements allocated */
	long body_fill;				/* elements stored; fill mark */
	long body_returned;			/* elements of fill returned */


	int *lacing_vals;			/* The values that will go to the segment table */
	ogg_int64_t *granule_vals;	/* granulepos values for headers. Not compact
								   this way, but it is simple coupled to the
								   lacing fifo */
	long lacing_storage;
	long lacing_fill;
	long lacing_packet;
	long lacing_returned;

	int e_o_s;					/* set when we have buffered the last packet in the
								   logical bitstream */
	int b_o_s;					/* set after we've written the initial page
								   of a logical bitstream */
	long serialno;
	long pageno;
	ogg_int64_t packetno;		/* sequence number for decode; the framing
								   knows where there's a hole in the data,
								   but we need coupling so that the codec
								   (which is in a seperate abstraction
								   layer) also knows about the gap */
	ogg_int64_t granulepos;

} ogg_stream_state;

/* ogg_packet is used to encapsulate the data and metadata belonging
   to a single raw Ogg/Vorbis packet *************************************/

typedef struct {
	unsigned char *packet;
	long bytes;
	long b_o_s;
	long e_o_s;

	ogg_int64_t granulepos;

	ogg_int64_t packetno;		/* sequence number for decode; the framing
								   knows where there's a hole in the data,
								   but we need coupling so that the codec
								   (which is in a seperate abstraction
								   layer) also knows about the gap */
} ogg_packet;

typedef struct {
	unsigned char *data;
	int storage;
	int fill;
	int returned;

	int headerbytes;
	int bodybytes;
} ogg_sync_state;

/* Ogg BITSTREAM PRIMITIVES: bitstream (decoding) ************************/

extern void oggpack_readinit(oggpack_buffer * b, unsigned char *buf, int bytes);
extern long oggpack_look32(oggpack_buffer * b, int bits);
extern void oggpack_adv(oggpack_buffer * b, int bits);
extern long oggpack_read24(oggpack_buffer * b, int bits);
extern long oggpack_read32(oggpack_buffer * b, int bits);
extern long oggpack_read1(oggpack_buffer * b);
extern long oggpack_leftbits(oggpack_buffer * b);

/* Ogg BITSTREAM PRIMITIVES: decoding **************************/

extern int ogg_sync_init(ogg_sync_state * oy);
extern int ogg_sync_clear(ogg_sync_state * oy);
extern int ogg_sync_reset(ogg_sync_state * oy);
extern int ogg_sync_destroy(ogg_sync_state * oy);

extern char *ogg_sync_buffer(ogg_sync_state * oy, long size);
extern int ogg_sync_wrote(ogg_sync_state * oy, long bytes);
extern int ogg_sync_pageout(ogg_sync_state * oy, ogg_page * og);
#define         ogg_sync_pageseek(oy,og) ogg_sync_pageout(oy,og)
extern int ogg_stream_pagein(ogg_stream_state * os, ogg_page * og);
extern int ogg_stream_packetout(ogg_stream_state * os, ogg_packet * op);

/* Ogg BITSTREAM PRIMITIVES: general ***************************/

extern int ogg_stream_init(ogg_stream_state * os, int serialno);
extern int ogg_stream_clear(ogg_stream_state * os);
extern int ogg_stream_reset(ogg_stream_state * os);
extern int ogg_stream_destroy(ogg_stream_state * os);
extern int ogg_stream_eos(ogg_stream_state * os);

extern void ogg_page_checksum_set(ogg_page * og);

extern int ogg_page_version(ogg_page * og);
extern int ogg_page_continued(ogg_page * og);
extern int ogg_page_bos(ogg_page * og);
extern int ogg_page_eos(ogg_page * og);
extern ogg_int64_t ogg_page_granulepos(ogg_page * og);
extern int ogg_page_serialno(ogg_page * og);
extern long ogg_page_pageno(ogg_page * og);
extern int ogg_page_packets(ogg_page * og);

extern void ogg_packet_clear(ogg_packet * op);


#endif							/* _OGG_H */

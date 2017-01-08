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

 function: basic codebook pack/unpack/code/decode operations
 last mod: $Id: codebook.c,v 1.35 2003/02/10 00:00:00 PDSoft Exp $

 ********************************************************************/

// a short note about Mpxplay:
//
// I use the codebook decoding of RC3 (with strong optimizations)
// because it's faster than the codebook of v1.0 (10-15% in total decoding time).
// I don't see any other (quality) difference between them...
//

#include "ogg.h"
#include "codebook.h"

#ifdef CODEBOOK_ASM				// in codebook.h

long vorbis_book_inline_decode(const decode_aux * t, oggpack_buffer * b);
//for maximum t->tab_maxlen+25 huffbits
//(I assume/hope that the huffbits are never more than 32 bits)

#pragma aux vorbis_book_inline_decode=\
 "mov edx,dword ptr 4[esi]"\
 "sub edx,dword ptr [esi]"\
 "cmp edx,dword ptr [edi]"\
 "jg vbd_use_lookup"\
  "test edx,edx"\
  "jg vbd_ok1"\
   "mov eax,-1"\
   "jmp vbd_end"\
  "vbd_ok1:"\
  "xor eax,eax"\
  "jmp vbd_huffdec_begin"\
 "vbd_use_lookup:"\
  "mov eax,dword ptr [esi]"\
  "mov ecx,eax"\
  "shr eax,3"\
  "add eax,dword ptr 8[esi]"\
  "and ecx,7"\
  "mov eax,dword ptr [eax]"\
  "shr eax,cl"\
  "mov ecx,dword ptr [edi]"\
  "and eax,mask[ecx*4]"\
  "mov ecx,dword ptr 8[edi]"\
  "movzx ebx,byte ptr [ecx+eax]"\
  "mov ecx,dword ptr 4[edi]"\
  "movsx eax,word ptr [ecx+eax*2]"\
  "add dword ptr [esi],ebx"\
  "test eax,eax"\
  "jle vbd_end_neg"\
  "sub edx,ebx"\
 "vbd_huffdec_begin:"\
 "mov ebx,dword ptr [esi]"\
 "mov ecx,ebx"\
 "shr ebx,3"\
 "add ebx,dword ptr 8[esi]"\
 "and ecx,7"\
 "mov ebx,dword ptr [ebx]"\
 "shr ebx,cl"\
 "mov ecx,dword ptr 12[edi]"\
 "mov edi,dword ptr 16[edi]"\
 "vbd_huffdec_back:"\
  "shr ebx,1"\
  "jnc vbd_ptr0"\
   "movsx eax,word ptr [edi+eax*2]"\
  "jmp vbd_ptr_end"\
  "vbd_ptr0:"\
   "movsx eax,word ptr [ecx+eax*2]"\
  "vbd_ptr_end:"\
  "dec edx"\
  "jz vbd_huffdec_end"\
  "test eax,eax"\
 "jg vbd_huffdec_back"\
 "vbd_huffdec_end:"\
 "mov ebx,dword ptr 4[esi]"\
 "sub ebx,edx"\
 "mov dword ptr [esi],ebx"\
 "vbd_end_neg:neg eax"\
 "vbd_end:"\
parm[edi][esi] value[eax] modify[eax ebx ecx edx edi esi];

long vorbis_book_decode(const decode_aux * t, oggpack_buffer * b)
{
	return vorbis_book_inline_decode(t, b);
}

// C version of the asm routine (don't use this)
/*long vorbis_book_decode(const decode_aux *t,oggpack_buffer *b)
{
 unsigned long bitstore;
 long ptr,leftbits;
 short *ptr0p,*ptr1p;

 leftbits=oggpack_inline_leftbits(b);
 if(leftbits<=t->tab_maxlen){
  if(leftbits<=0)
   return -1;
  ptr=0;
 }else{
  unsigned int lok=oggpack_inline_look24noc(b,t->tab_maxlen),adv;
  ptr=t->tab_ptr[lok];
  adv=t->tab_codelen[lok];
  oggpack_inline_adv(b,adv);
  if(ptr<=0)
   return(-ptr);
  leftbits-=adv;
 }

 bitstore=oggpack_inline_look2432noc(b);
 ptr0p=t->ptr0;
 ptr1p=t->ptr1;
 do{
  if(bitstore&1)
   ptr=ptr1p[ptr];
  else
   ptr=ptr0p[ptr];
  bitstore>>=1;
  if(!(--leftbits))
   break;
 }while(ptr>0);

 oggpack_inline_setleft(b,leftbits);

 return(-ptr);
}*/

#else

long vorbis_book_decode(const decode_aux * t, oggpack_buffer * b)
{
	ogg_uint32_t bitstore;
	ogg_int32_t ptr, leftbits;
	ogg_int16_t *ptr0p, *ptr1p;

	leftbits = oggpack_inline_leftbits(b);
	if(leftbits <= t->tab_maxlen) {
		if(leftbits <= 0)
			return -1;
		ptr = 0;
	} else
		ogg_uint32_t lok = oggpack_inline_look24noc(b, t->tab_maxlen), adv;
	ptr = t->tab_ptr[lok];
	adv = t->tab_codelen[lok];
	oggpack_inline_adv(b, adv);
	if(ptr <= 0)
		return (-ptr);
	leftbits -= adv;
}

 // I assume/hope that the huffbits are never more than t->tab_maxlen+32
bitstore = oggpack_inline_look32(b, (leftbits > 32) ? 32 : leftbits);
ptr0p = t->ptr0;
ptr1p = t->ptr1;
do {
	if(bitstore & 1)
		ptr = ptr1p[ptr];
	else
		ptr = ptr0p[ptr];
	bitstore >>= 1;
	if(!(--leftbits))
		break;
} while(ptr > 0);

oggpack_inline_setleft(b, leftbits);

return (-ptr);
}

#define vorbis_book_inline_decode(t,b) vorbis_book_decode(t,b)

#endif

//--------------------------------------------------------------------------
//res0
long vorbis_book_decodevs_add(codebook * book, ogg_double_t * a0, oggpack_buffer * b, int n)
{
	const ogg_float_t *bookvallist = book->valuelist;
	const decode_aux *bookdectree = book->decode_tree;
	const unsigned int bookdim = book->dim;
	const unsigned int step = n / bookdim;
	unsigned int i, j;

	i = step;
	do {
		ogg_float_t *t;
		ogg_double_t *a;
		long entry = vorbis_book_inline_decode(bookdectree, b);
		if(entry < 0)
			return (entry);
		t = (ogg_float_t *) (bookvallist + entry * bookdim);
		a = a0++;
		j = bookdim;
		do {
			a[0] += *t++;
			a += step;
		} while(--j);
	} while(--i);
	return (0);
}

long vorbis_book_decodevs_set(codebook * book, ogg_double_t * a0, oggpack_buffer * b, int n)
{
	const ogg_float_t *bookvallist = book->valuelist;
	const decode_aux *bookdectree = book->decode_tree;
	const unsigned int bookdim = book->dim;
	const unsigned int step = n / bookdim;
	unsigned int i, j;

	i = step;
	do {
		ogg_float_t *t;
		ogg_double_t *a;
		long entry = vorbis_book_inline_decode(bookdectree, b);
		if(entry < 0)
			return (entry);
		t = (ogg_float_t *) (bookvallist + entry * bookdim);
		a = a0++;
		j = bookdim;
		do {
			a[0] = *t++;
			a += step;
		} while(--j);
	} while(--i);
	return (0);
}

//-----------------------------------------------------------------------
//res1
long vorbis_book_decodev_add(codebook * book, ogg_double_t * a, oggpack_buffer * b, int n)
{
	const ogg_float_t *bookvallist = book->valuelist;
	const decode_aux *bookdectree = book->decode_tree;
	const unsigned int bookdim = book->dim;

	if(bookdim > 8) {
		do {
			ogg_float_t *t;
			long entry = vorbis_book_inline_decode(bookdectree, b), j;
			if(entry < 0)
				return (entry);
			t = (ogg_float_t *) (bookvallist + (entry * bookdim));
			j = bookdim;
			n -= j;
			do {
				*a++ += *t++;
			} while(--j);
		} while(n > 0);
	} else {
		do {
			ogg_float_t *t;
			long entry = vorbis_book_inline_decode(bookdectree, b);
			if(entry < 0)
				return (entry);
			t = (ogg_float_t *) (bookvallist + (entry * bookdim));
			switch (bookdim) {
			case 8:
				a[7] += t[7];
			case 7:
				a[6] += t[6];
			case 6:
				a[5] += t[5];
			case 5:
				a[4] += t[4];
			case 4:
				a[3] += t[3];
			case 3:
				a[2] += t[2];
			case 2:
				a[1] += t[1];
			case 1:
				a[0] += t[0];
			case 0:
				break;
			}
			a += bookdim;
			n -= bookdim;
		} while(n > 0);
	}
	return (0);
}

long vorbis_book_decodev_set(codebook * book, ogg_double_t * a, oggpack_buffer * b, int n)
{
	const ogg_float_t *bookvallist = book->valuelist;
	const decode_aux *bookdectree = book->decode_tree;
	const unsigned int bookdim = book->dim;

	if(bookdim > 8) {
		do {
			ogg_float_t *t;
			long entry = vorbis_book_inline_decode(bookdectree, b), j;
			if(entry < 0)
				return (entry);
			t = (ogg_float_t *) (bookvallist + (entry * bookdim));
			j = bookdim;
			n -= j;
			do {
				*a++ = *t++;
			} while(--j);
		} while(n > 0);
	} else {
		do {
			ogg_float_t *t;
			long entry = vorbis_book_inline_decode(bookdectree, b);
			if(entry < 0)
				return (entry);
			t = (ogg_float_t *) (bookvallist + (entry * bookdim));
			switch (bookdim) {
			case 8:
				a[7] = t[7];
			case 7:
				a[6] = t[6];
			case 6:
				a[5] = t[5];
			case 5:
				a[4] = t[4];
			case 4:
				a[3] = t[3];
			case 3:
				a[2] = t[2];
			case 2:
				a[1] = t[1];
			case 1:
				a[0] = t[0];
			case 0:
				break;
			}
			a += bookdim;
			n -= bookdim;
		} while(n > 0);
	}
	return (0);
}

//------------------------------------------------------------------------
//res2
long vorbis_book_decodevv_add(codebook * book, ogg_double_t ** a, long offset, int ch, oggpack_buffer * b, int n)
{
	const unsigned int bookdim = book->dim;
	const ogg_float_t *bookvallist = book->valuelist;
	const decode_aux *bookdectree = book->decode_tree;
	unsigned int i;

	if((ch == 2) && (bookdim <= 8) && !(bookdim & 1)) {
		ogg_double_t *al = a[0] + (offset >> 1), *ar = a[1] + (offset >> 1);
		const unsigned int bd2 = bookdim >> 1;
		do {
			ogg_float_t *t;
			long entry = vorbis_book_inline_decode(bookdectree, b);
			if(entry < 0)
				return (entry);
			t = (ogg_float_t *) (bookvallist + (entry * bookdim));
			switch (bookdim) {
			case 8:
				ar[3] += t[7];
			case 7:
				al[3] += t[6];
			case 6:
				ar[2] += t[5];
			case 5:
				al[2] += t[4];
			case 4:
				ar[1] += t[3];
			case 3:
				al[1] += t[2];
			case 2:
				ar[0] += t[1];
			case 1:
				al[0] += t[0];
			case 0:
				break;
			}
			al += bd2;
			ar += bd2;
			n -= bookdim;
		} while(n > 0);
	} else {
		unsigned int chptr = 0;
		for(i = offset / ch; i < (offset + n) / ch;) {
			long entry = vorbis_book_inline_decode(bookdectree, b);
			if(entry < 0)
				return (entry);
			{
				const ogg_float_t *t = bookvallist + entry * bookdim;
				unsigned int j = bookdim;
				do {
					a[chptr++][i] += *t++;
					if(chptr == ch) {
						chptr = 0;
						i++;
					}
				} while(--j);
			}
		}
	}
	return (0);
}

long vorbis_book_decodevv_set(codebook * book, ogg_double_t ** a, long offset, int ch, oggpack_buffer * b, int n)
{
	const unsigned int bookdim = book->dim;
	const ogg_float_t *bookvallist = book->valuelist;
	const decode_aux *bookdectree = book->decode_tree;
	unsigned int i;

	if((ch == 2) && (bookdim <= 8) && !(bookdim & 1)) {
		ogg_double_t *al = a[0] + (offset >> 1), *ar = a[1] + (offset >> 1);
		const unsigned int bdp2 = bookdim >> 1;
		do {
			ogg_float_t *t;
			long entry = vorbis_book_inline_decode(bookdectree, b);
			if(entry < 0)
				return (entry);
			t = (ogg_float_t *) (bookvallist + (entry * bookdim));
			switch (bookdim) {
			case 8:
				ar[3] = t[7];
			case 7:
				al[3] = t[6];
			case 6:
				ar[2] = t[5];
			case 5:
				al[2] = t[4];
			case 4:
				ar[1] = t[3];
			case 3:
				al[1] = t[2];
			case 2:
				ar[0] = t[1];
			case 1:
				al[0] = t[0];
			case 0:
				break;
			}
			al += bdp2;
			ar += bdp2;
			n -= bookdim;
		} while(n > 0);
	} else {
		unsigned int chptr = 0;
		for(i = offset / ch; i < (offset + n) / ch;) {
			long entry = vorbis_book_inline_decode(bookdectree, b);
			if(entry < 0)
				return (entry);
			{
				const ogg_float_t *t = bookvallist + entry * bookdim;
				unsigned int j = bookdim;
				do {
					a[chptr++][i] = *t++;
					if(chptr == ch) {
						chptr = 0;
						i++;
					}
				} while(--j);
			}
		}
	}
	return (0);
}

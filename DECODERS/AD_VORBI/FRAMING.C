/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: code raw [Vorbis] packets into framed OggSquish stream and
	   decode Ogg streams back into raw packets
 last mod: $Id: framing.c,v 1.22 2002/09/30 00:00:00 PDSoft Exp $

 note: The CRC code is directly derived from public domain code by
 Ross Williams (ross@guest.adelaide.edu.au).  See docs/framing.html
 for details.

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include "ogg.h"

#if 0
static ogg_uint32_t crc_lookup[256];

static void ogg_make_crc_table(void)
{
 static unsigned int table_ready;
 ogg_uint32_t *tablep;
 unsigned long index,is24;

 if(table_ready)
  return;

 tablep=&crc_lookup[0];

 index=256;
 is24=0;
 do{
  unsigned long r = is24;
  unsigned int i=8;

  do{
   if(r&0x80000000UL){
    r = (r<<1) ^ 0x04c11db7;
   }else
    r <<= 1;
  }while(--i);

  *tablep++ = r;

  is24+=(1<<24);

 }while(--index);
 table_ready=1;
}

#else

static ogg_uint32_t crc_lookup[256]={
  0x00000000,0x04c11db7,0x09823b6e,0x0d4326d9,
  0x130476dc,0x17c56b6b,0x1a864db2,0x1e475005,
  0x2608edb8,0x22c9f00f,0x2f8ad6d6,0x2b4bcb61,
  0x350c9b64,0x31cd86d3,0x3c8ea00a,0x384fbdbd,
  0x4c11db70,0x48d0c6c7,0x4593e01e,0x4152fda9,
  0x5f15adac,0x5bd4b01b,0x569796c2,0x52568b75,
  0x6a1936c8,0x6ed82b7f,0x639b0da6,0x675a1011,
  0x791d4014,0x7ddc5da3,0x709f7b7a,0x745e66cd,
  0x9823b6e0,0x9ce2ab57,0x91a18d8e,0x95609039,
  0x8b27c03c,0x8fe6dd8b,0x82a5fb52,0x8664e6e5,
  0xbe2b5b58,0xbaea46ef,0xb7a96036,0xb3687d81,
  0xad2f2d84,0xa9ee3033,0xa4ad16ea,0xa06c0b5d,
  0xd4326d90,0xd0f37027,0xddb056fe,0xd9714b49,
  0xc7361b4c,0xc3f706fb,0xceb42022,0xca753d95,
  0xf23a8028,0xf6fb9d9f,0xfbb8bb46,0xff79a6f1,
  0xe13ef6f4,0xe5ffeb43,0xe8bccd9a,0xec7dd02d,
  0x34867077,0x30476dc0,0x3d044b19,0x39c556ae,
  0x278206ab,0x23431b1c,0x2e003dc5,0x2ac12072,
  0x128e9dcf,0x164f8078,0x1b0ca6a1,0x1fcdbb16,
  0x018aeb13,0x054bf6a4,0x0808d07d,0x0cc9cdca,
  0x7897ab07,0x7c56b6b0,0x71159069,0x75d48dde,
  0x6b93dddb,0x6f52c06c,0x6211e6b5,0x66d0fb02,
  0x5e9f46bf,0x5a5e5b08,0x571d7dd1,0x53dc6066,
  0x4d9b3063,0x495a2dd4,0x44190b0d,0x40d816ba,
  0xaca5c697,0xa864db20,0xa527fdf9,0xa1e6e04e,
  0xbfa1b04b,0xbb60adfc,0xb6238b25,0xb2e29692,
  0x8aad2b2f,0x8e6c3698,0x832f1041,0x87ee0df6,
  0x99a95df3,0x9d684044,0x902b669d,0x94ea7b2a,
  0xe0b41de7,0xe4750050,0xe9362689,0xedf73b3e,
  0xf3b06b3b,0xf771768c,0xfa325055,0xfef34de2,
  0xc6bcf05f,0xc27dede8,0xcf3ecb31,0xcbffd686,
  0xd5b88683,0xd1799b34,0xdc3abded,0xd8fba05a,
  0x690ce0ee,0x6dcdfd59,0x608edb80,0x644fc637,
  0x7a089632,0x7ec98b85,0x738aad5c,0x774bb0eb,
  0x4f040d56,0x4bc510e1,0x46863638,0x42472b8f,
  0x5c007b8a,0x58c1663d,0x558240e4,0x51435d53,
  0x251d3b9e,0x21dc2629,0x2c9f00f0,0x285e1d47,
  0x36194d42,0x32d850f5,0x3f9b762c,0x3b5a6b9b,
  0x0315d626,0x07d4cb91,0x0a97ed48,0x0e56f0ff,
  0x1011a0fa,0x14d0bd4d,0x19939b94,0x1d528623,
  0xf12f560e,0xf5ee4bb9,0xf8ad6d60,0xfc6c70d7,
  0xe22b20d2,0xe6ea3d65,0xeba91bbc,0xef68060b,
  0xd727bbb6,0xd3e6a601,0xdea580d8,0xda649d6f,
  0xc423cd6a,0xc0e2d0dd,0xcda1f604,0xc960ebb3,
  0xbd3e8d7e,0xb9ff90c9,0xb4bcb610,0xb07daba7,
  0xae3afba2,0xaafbe615,0xa7b8c0cc,0xa379dd7b,
  0x9b3660c6,0x9ff77d71,0x92b45ba8,0x9675461f,
  0x8832161a,0x8cf30bad,0x81b02d74,0x857130c3,
  0x5d8a9099,0x594b8d2e,0x5408abf7,0x50c9b640,
  0x4e8ee645,0x4a4ffbf2,0x470cdd2b,0x43cdc09c,
  0x7b827d21,0x7f436096,0x7200464f,0x76c15bf8,
  0x68860bfd,0x6c47164a,0x61043093,0x65c52d24,
  0x119b4be9,0x155a565e,0x18197087,0x1cd86d30,
  0x029f3d35,0x065e2082,0x0b1d065b,0x0fdc1bec,
  0x3793a651,0x3352bbe6,0x3e119d3f,0x3ad08088,
  0x2497d08d,0x2056cd3a,0x2d15ebe3,0x29d4f654,
  0xc5a92679,0xc1683bce,0xcc2b1d17,0xc8ea00a0,
  0xd6ad50a5,0xd26c4d12,0xdf2f6bcb,0xdbee767c,
  0xe3a1cbc1,0xe760d676,0xea23f0af,0xeee2ed18,
  0xf0a5bd1d,0xf464a0aa,0xf9278673,0xfde69bc4,
  0x89b8fd09,0x8d79e0be,0x803ac667,0x84fbdbd0,
  0x9abc8bd5,0x9e7d9662,0x933eb0bb,0x97ffad0c,
  0xafb010b1,0xab710d06,0xa6322bdf,0xa2f33668,
  0xbcb4666d,0xb8757bda,0xb5365d03,0xb1f740b4};

#endif

int ogg_sync_init(ogg_sync_state *oy)
{
 if(oy)
  memset(oy,0,sizeof(*oy));
 //ogg_make_crc_table();
 return(0);
}

int ogg_sync_clear(ogg_sync_state *oy)
{
 if(oy){
  if(oy->data)
   _ogg_free(oy->data);
  memset(oy,0,sizeof(*oy));
 }
 return(0);
}

char *ogg_sync_buffer(ogg_sync_state *oy, long size)
{
 if(oy->returned){
  oy->fill-=oy->returned;
  if(oy->fill>0)
   _ogg_memmove(oy->data,oy->data+oy->returned,oy->fill);
  oy->returned=0;
 }

 if(size>oy->storage-oy->fill){
  long newsize=(size<<1)+oy->fill;

  if(oy->data)
   oy->data=_ogg_realloc(oy->data,newsize);
  else
   oy->data=_ogg_malloc(newsize);
  oy->storage=newsize;
 }

 return((char *)oy->data+oy->fill);
}

int ogg_sync_wrote(ogg_sync_state *oy, long bytes)
{
 if(oy->fill+bytes>oy->storage){
  oy->fill=oy->storage;
  return(-1);
 }
 oy->fill+=bytes;
 return(0);
}

unsigned int ogg_page_crc_check(unsigned char *pagedata,unsigned int pagelen)
{
 ogg_uint32_t crc_reg=0,*crc_table=&crc_lookup[0],checksum;
 unsigned char *pagedata_crc=pagedata+22;

 checksum=*((ogg_uint32_t *)pagedata_crc);
 *((ogg_uint32_t *)pagedata_crc)=0;

 do{
  crc_reg=(crc_reg<<8)^crc_table[(crc_reg>>24)^(ogg_uint32_t)(*pagedata++)];
 }while(--pagelen);

 if(crc_reg==checksum)
  return 1;

 *((ogg_uint32_t *)pagedata_crc)=checksum; // recover the original pagedata

 return 0;
}

#define HEADID_OggS  0x5367674f // OggS (SggO)

int ogg_sync_pageout(ogg_sync_state *oy,ogg_page *og)
{
 unsigned char *page=oy->data+oy->returned;
 long bytes=oy->fill-oy->returned;

 if(oy->headerbytes==0){
  int headerbytes,i;
  if(bytes<27)
   return (0);

  while(*((unsigned long *)(page))!=HEADID_OggS){
   page++;
   oy->returned++;
   if(--bytes<27)
    return (0);
  }

  headerbytes=page[26]+27;
  if(bytes<headerbytes)
   return (0);

  oy->headerbytes=headerbytes;

  for(i=0;i<page[26];i++)
   oy->bodybytes+=page[27+i];
 }

 if(oy->bodybytes+oy->headerbytes>bytes)
  return (0);

 bytes=oy->headerbytes+oy->bodybytes;

 if(!ogg_page_crc_check(page,bytes)){
  oy->returned++;                  // search a new header
  oy->headerbytes=oy->bodybytes=0;
  return (-1);
 }

 og->header=page;
 og->header_len=oy->headerbytes;
 og->body=page+oy->headerbytes;
 og->body_len=oy->bodybytes;

 oy->returned+=bytes;
 oy->headerbytes=0;
 oy->bodybytes=0;

 return (1);
}

int ogg_sync_reset(ogg_sync_state *oy)
{
 oy->fill=0;
 oy->returned=0;
 oy->headerbytes=0;
 oy->bodybytes=0;
 return(0);
}

//------------------------------------------------------------------------
int ogg_page_version(ogg_page *og)
{
 return((int)(og->header[4]));
}

int ogg_page_continued(ogg_page *og)
{
 return((int)(og->header[5]&0x01));
}

int ogg_page_bos(ogg_page *og)
{
 return((int)(og->header[5]&0x02));
}

int ogg_page_eos(ogg_page *og)
{
 return((int)(og->header[5]&0x04));
}

ogg_int64_t ogg_page_granulepos(ogg_page *og)
{
 return *((ogg_int64_t *)&og->header[6]);
}

int ogg_page_serialno(ogg_page *og)
{
 return *((int *)&og->header[14]);
}

long ogg_page_pageno(ogg_page *og)
{
 return *((long *)&og->header[18]);
}

//------------------------------------------------------------------------
int ogg_stream_init(ogg_stream_state *os,int serialno)
{
 if(os){
  memset(os,0,sizeof(*os));
  os->body_storage=16*1024;
  os->body_data=_ogg_malloc(os->body_storage*sizeof(*os->body_data));

  os->lacing_storage=1024;
  os->lacing_vals=_ogg_malloc(os->lacing_storage*sizeof(*os->lacing_vals));
  os->granule_vals=_ogg_malloc(os->lacing_storage*sizeof(*os->granule_vals));

  os->serialno=serialno;

  return(0);
 }
 return(-1);
}

int ogg_stream_clear(ogg_stream_state *os)
{
 if(os){
  if(os->body_data)_ogg_free(os->body_data);
  if(os->lacing_vals)_ogg_free(os->lacing_vals);
  if(os->granule_vals)_ogg_free(os->granule_vals);

  memset(os,0,sizeof(*os));
 }
 return(0);
}

static void _os_body_expand(ogg_stream_state *os,int needed)
{
 if(os->body_storage<=os->body_fill+needed){
  os->body_storage+=(needed+1024);
  os->body_data=_ogg_realloc(os->body_data,os->body_storage*sizeof(*os->body_data));
 }
}

static void _os_lacing_expand(ogg_stream_state *os,int needed)
{
 if(os->lacing_storage<=os->lacing_fill+needed){
  os->lacing_storage+=(needed+32);
  os->lacing_vals=_ogg_realloc(os->lacing_vals,os->lacing_storage*sizeof(*os->lacing_vals));
  os->granule_vals=_ogg_realloc(os->granule_vals,os->lacing_storage*sizeof(*os->granule_vals));
 }
}

int ogg_stream_pagein(ogg_stream_state *os, ogg_page *og)
{
 unsigned char *header=og->header;
 unsigned char *body=og->body;
 long           bodysize=og->body_len;
 int            segptr=0;

 int version=ogg_page_version(og);
 int continued=ogg_page_continued(og);
 int bos=ogg_page_bos(og);
 int eos=ogg_page_eos(og);
 ogg_int64_t granulepos=ogg_page_granulepos(og);
 int serialno=ogg_page_serialno(og);
 long pageno=ogg_page_pageno(og);
 int segments=header[26];

 {
  long lr=os->lacing_returned;
  long br=os->body_returned;

  if(br){
   os->body_fill-=br;
   if(os->body_fill)
    _ogg_memmove(os->body_data,os->body_data+br,os->body_fill);
   os->body_returned=0;
  }

  if(lr){
   if(os->lacing_fill-lr){
    _ogg_memmove(os->lacing_vals,os->lacing_vals+lr,(os->lacing_fill-lr)*sizeof(*os->lacing_vals));
    _ogg_memmove(os->granule_vals,os->granule_vals+lr,(os->lacing_fill-lr)*sizeof(*os->granule_vals));
   }
   os->lacing_fill-=lr;
   os->lacing_packet-=lr;
   os->lacing_returned=0;
  }
 }

 if(serialno!=os->serialno)return(-1);
 if(version>0)return(-1);

 _os_lacing_expand(os,segments+1);

 if(pageno!=os->pageno){
  int i;

  for(i=os->lacing_packet;i<os->lacing_fill;i++)
   os->body_fill-=os->lacing_vals[i]&0xff;
  os->lacing_fill=os->lacing_packet;

  if(os->pageno!=-1){
   os->lacing_vals[os->lacing_fill++]=0x400;
   os->lacing_packet++;
  }

  if(continued){
   bos=0;
   for(;segptr<segments;segptr++){
    int val=header[27+segptr];
    body+=val;
    bodysize-=val;
    if(val<255){
     segptr++;
     break;
    }
   }
  }
 }

 if(bodysize){
  _os_body_expand(os,bodysize);
  memcpy(os->body_data+os->body_fill,body,bodysize);
  os->body_fill+=bodysize;
 }

 {
  int saved=-1;
  while(segptr<segments){
   int val=header[27+segptr];
   os->lacing_vals[os->lacing_fill]=val;
   os->granule_vals[os->lacing_fill]=-1;

   if(bos){
    os->lacing_vals[os->lacing_fill]|=0x100;
    bos=0;
   }

   if(val<255)
    saved=os->lacing_fill;

   os->lacing_fill++;
   segptr++;

   if(val<255)
    os->lacing_packet=os->lacing_fill;
  }

  if(saved!=-1){
   os->granule_vals[saved]=granulepos;
  }
 }

 if(eos){
  os->e_o_s=1;
  if(os->lacing_fill>0)
   os->lacing_vals[os->lacing_fill-1]|=0x200;
 }

 os->pageno=pageno+1;

 return(0);
}

int ogg_stream_packetout(ogg_stream_state *os,ogg_packet *op)
{
 int ptr=os->lacing_returned;

 if(os->lacing_packet<=ptr)
  return(0);

 if(os->lacing_vals[ptr]&0x400){
  os->lacing_returned++;
  os->packetno++;
  return(-1);
 }

 {
  int size=os->lacing_vals[ptr]&0xff;
  int bytes=size;
  int eos=os->lacing_vals[ptr]&0x200;
  int bos=os->lacing_vals[ptr]&0x100;

  while(size==255){
   int val=os->lacing_vals[++ptr];
   size=val&0xff;
   if(val&0x200)
    eos=0x200;
   bytes+=size;
  }

  op->e_o_s=eos;
  op->b_o_s=bos;
  op->packet=os->body_data+os->body_returned;
  op->packetno=os->packetno;
  op->granulepos=os->granule_vals[ptr];
  op->bytes=bytes;

  os->body_returned+=bytes;
  os->lacing_returned=ptr+1;
  os->packetno++;
 }
 return(1);
}

int ogg_stream_reset(ogg_stream_state *os)
{
 os->body_fill=0;
 os->body_returned=0;

 os->lacing_fill=0;
 os->lacing_packet=0;
 os->lacing_returned=0;

 os->e_o_s=0;
 os->b_o_s=0;
 os->pageno=-1;
 os->packetno=0;
 os->granulepos=0;

 return(0);
}

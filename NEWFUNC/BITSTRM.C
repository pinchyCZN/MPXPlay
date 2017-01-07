//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: common demuxer/decoder bitstream reading
//Newfunc dir is not the best place for the decoder bitstream-handling,
//but on this way we can use it in the DLLs too (with the newfunc.lib)

#include "in_file.h"
#include "newfunc.h"

// open a new bitstream handler
struct mpxplay_bitstreambuf_s *mpxplay_bitstream_alloc(unsigned int required_bufsize)
{
 struct mpxplay_bitstreambuf_s *bs;
 bs=calloc(1,sizeof(struct mpxplay_bitstreambuf_s));
 if(!bs)
  return bs;
 required_bufsize*=2;
 bs->buffer=(unsigned char *)calloc(required_bufsize,1);
 if(!bs->buffer){
  free(bs);
  return NULL;
 }
 bs->bufsize=required_bufsize;
 return bs;
}

// close the bitstream
void mpxplay_bitstream_free(struct mpxplay_bitstreambuf_s *bs)
{
 if(bs){
  if(bs->buffer)
   free(bs->buffer);
  free(bs);
 }
}

// at static bs (and bs->buffer)
void mpxplay_bitstream_init(struct mpxplay_bitstreambuf_s *bs,char *data,unsigned int bytes)
{
 if(!bs)
  return;
 bs->bitpos=0;
 bs->storedbits=bytes*8;
 bs->buffer=data;
 bs->bufsize=bytes;
}

// delete used/readed bits
void mpxplay_bitstream_consolidate(struct mpxplay_bitstreambuf_s *bs)
{
 if(!bs)
  return;
 if(bs->bitpos>=bs->storedbits){
  bs->bitpos=bs->storedbits=0;
 }else{
  unsigned long bytepos=bs->bitpos/8;
  if(bytepos){
   unsigned long leftbytes=(bs->storedbits/8)-bytepos;
   pds_memcpy(bs->buffer,bs->buffer+bytepos,leftbytes);
   bs->storedbits-=bytepos*8;
   bs->bitpos-=bytepos*8;
  }
 }
}

// (re)fill the bitstream buffer from file (fbfs,fbds) with the required bytes (needbytes)
int mpxplay_bitstream_fill(struct mpxplay_bitstreambuf_s *bs,struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,unsigned int needbytes)
{
 unsigned long bytepos,storedbytes,leftbytes;

 if(!bs)
  return 0;

 if(needbytes>(bs->bufsize/2))
  return MPXPLAY_ERROR_MPXINBUF_READ_BUF;

 bytepos=bs->bitpos/8;
 storedbytes=bs->storedbits/8;
 if((bs->bufsize-storedbytes)<needbytes){ // empty-bufbytes < needbytes
  mpxplay_bitstream_consolidate(bs);
  bytepos=bs->bitpos/8;
  storedbytes=bs->storedbits/8;
 }

 leftbytes=storedbytes-bytepos;
 if(leftbytes<needbytes){
  unsigned long gotbytes=fbfs->fread(fbds,bs->buffer+storedbytes,needbytes-leftbytes);
  leftbytes+=gotbytes;
  bs->storedbits+=gotbytes*8;
 }

 return ((leftbytes>=needbytes)? MPXPLAY_ERROR_MPXINBUF_OK:MPXPLAY_ERROR_MPXINBUF_READ_LOW);
}

// fill the bitstream buffer with the sent srcbuf/newbytes data
unsigned int mpxplay_bitstream_putbytes(struct mpxplay_bitstreambuf_s *bs,unsigned char *srcbuf,unsigned int newbytes)
{
 unsigned long storedbytes,emptyspace;

 if(!bs || !newbytes)
  return 0;

 storedbytes=bs->storedbits/8;
 emptyspace=bs->bufsize-storedbytes;

 if(emptyspace<newbytes){
  mpxplay_bitstream_consolidate(bs);
  storedbytes=bs->storedbits/8;
  emptyspace=bs->bufsize-storedbytes;
  if(newbytes>emptyspace)
   newbytes=emptyspace;
 }
 if(newbytes){
  pds_memcpy(&bs->buffer[storedbytes],srcbuf,newbytes);
  bs->storedbits+=newbytes*8;
 }
 return newbytes;
}

// at seek
void mpxplay_bitstream_reset(struct mpxplay_bitstreambuf_s *bs)
{
 if(!bs)
  return;
 bs->storedbits=bs->bitpos=0;
}

//---------------------------------------------------------------------
//byte

unsigned char *mpxplay_bitstream_getbufpos(struct mpxplay_bitstreambuf_s *bs)
{
 if(!bs)
  return NULL;
 return (bs->buffer+(bs->bitpos/8));
}

int mpxplay_bitstream_lookbytes(struct mpxplay_bitstreambuf_s *bs,unsigned char *destbuf,unsigned int needbytes)
{
 unsigned long remainbytes;

 if(!bs)
  return MPXPLAY_ERROR_MPXINBUF_READ_BUF;
 if(!needbytes)
  return 0;

 if(bs->bitpos>=bs->storedbits)
  return MPXPLAY_ERROR_MPXINBUF_READ_BUF;
 remainbytes=(bs->storedbits-bs->bitpos)/8;
 if(remainbytes<needbytes)
  return MPXPLAY_ERROR_MPXINBUF_READ_BUF;
 // if byte aligned !!!
 pds_memcpy(destbuf,bs->buffer+(bs->bitpos/8),needbytes);
 return needbytes;
}

int mpxplay_bitstream_readbytes(struct mpxplay_bitstreambuf_s *bs,unsigned char *destbuf,unsigned int needbytes)
{
 int retcode;

 if(!bs)
  return MPXPLAY_ERROR_MPXINBUF_READ_BUF;

 retcode=mpxplay_bitstream_lookbytes(bs,destbuf,needbytes);
 if(retcode<=0)
  return retcode;
 bs->bitpos+=needbytes*8;
 return needbytes;
}

// seek in the buffer (relative only)
int mpxplay_bitstream_skipbytes(struct mpxplay_bitstreambuf_s *bs,int skipbytes)
{
 if(!bs)
  return MPXPLAY_ERROR_MPXINBUF_SEEK_BUF;
 if(skipbytes<0){
  skipbytes=-skipbytes;
  if(bs->bitpos<(skipbytes*8)){
   bs->bitpos=0;
   return MPXPLAY_ERROR_MPXINBUF_SEEK_BUF;
  }
  bs->bitpos-=skipbytes*8;
 }else{
  bs->bitpos+=skipbytes*8;
  if(bs->bitpos>=bs->storedbits)
   return MPXPLAY_ERROR_MPXINBUF_SEEK_BUF;
 }
 return MPXPLAY_ERROR_MPXINBUF_OK;
}

long mpxplay_bitstream_leftbytes(struct mpxplay_bitstreambuf_s *bs)
{
 if(bs->bitpos>=bs->storedbits)
  return 0;
 return ((bs->storedbits-bs->bitpos)/8);
}

//-----------------------------------------------------------------

mpxp_uint32_t mpxplay_bitstream_get_byte(struct mpxplay_bitstreambuf_s *bs)
{
 mpxp_uint32_t val;
 if(mpxplay_bitstream_leftbytes(bs)<1)
  return 0;
 val=PDS_GETB_8U(mpxplay_bitstream_getbufpos(bs));
 mpxplay_bitstream_skipbytes(bs,1);
 return val;
}

mpxp_uint32_t mpxplay_bitstream_get_le16(struct mpxplay_bitstreambuf_s *bs)
{
 mpxp_uint32_t val;
 if(mpxplay_bitstream_leftbytes(bs)<2)
  return 0;
 val=PDS_GETB_LE16(mpxplay_bitstream_getbufpos(bs));
 mpxplay_bitstream_skipbytes(bs,2);
 return val;
}

mpxp_uint32_t mpxplay_bitstream_get_le32(struct mpxplay_bitstreambuf_s *bs)
{
 mpxp_uint32_t val;
 if(mpxplay_bitstream_leftbytes(bs)<4)
  return 0;
 val=PDS_GETB_LE32(mpxplay_bitstream_getbufpos(bs));
 mpxplay_bitstream_skipbytes(bs,4);
 return val;
}

mpxp_uint64_t mpxplay_bitstream_get_le64(struct mpxplay_bitstreambuf_s *bs)
{
 mpxp_uint64_t val;
 if(mpxplay_bitstream_leftbytes(bs)<8)
  return 0;
 val=PDS_GETB_LE64(mpxplay_bitstream_getbufpos(bs));
 mpxplay_bitstream_skipbytes(bs,8);
 return val;
}

mpxp_uint32_t mpxplay_bitstream_get_be16(struct mpxplay_bitstreambuf_s *bs)
{
 mpxp_uint32_t val;
 if(mpxplay_bitstream_leftbytes(bs)<2)
  return 0;
 val=PDS_GETB_BE16(mpxplay_bitstream_getbufpos(bs));
 mpxplay_bitstream_skipbytes(bs,2);
 return val;
}

mpxp_uint32_t mpxplay_bitstream_get_be32(struct mpxplay_bitstreambuf_s *bs)
{
 mpxp_uint32_t val;
 if(mpxplay_bitstream_leftbytes(bs)<4)
  return 0;
 val=PDS_GETB_BE32(mpxplay_bitstream_getbufpos(bs));
 mpxplay_bitstream_skipbytes(bs,4);
 return val;
}

mpxp_uint64_t mpxplay_bitstream_get_be64(struct mpxplay_bitstreambuf_s *bs)
{
 mpxp_uint64_t val;
 unsigned char *bufpos;
 if(mpxplay_bitstream_leftbytes(bs)<8)
  return 0;
 bufpos=mpxplay_bitstream_getbufpos(bs);
 val =((mpxp_uint64_t)PDS_GETB_BE32(bufpos))<<32;
 val|=(mpxp_uint64_t)PDS_GETB_BE32(bufpos+4);
 mpxplay_bitstream_skipbytes(bs,8);
 return val;
}

//-----------------------------------------------------------------------
//bit

int mpxplay_bitstream_getbit1_be(struct mpxplay_bitstreambuf_s *bs)
{
 unsigned long bitindex;
 unsigned char *bufpos;

 bufpos=bs->buffer+(bs->bitpos>>3);
 bitindex=bs->bitpos&7;
 bs->bitpos++;

 if(bs->bitpos>bs->storedbits)
  return 0;//(-1);

 return ((bufpos[0]>>(7-bitindex))&1);
}

long mpxplay_bitstream_getbits_be24(struct mpxplay_bitstreambuf_s *bs,unsigned int bits)
{
 unsigned long ret,bitindex;
 unsigned char *bufpos;

 bufpos=bs->buffer+(bs->bitpos>>3);
 bitindex=bs->bitpos&7;
 bs->bitpos+=bits;

 if(bs->bitpos>bs->storedbits)
  return 0;//(-1);

 ret=*((unsigned long *)bufpos);
 ret=pds_bswap32(ret);
 ret<<=bitindex;
 ret>>=32-bits;

 return (ret);
}

mpxp_uint32_t mpxplay_bitstream_getbits_ube32(struct mpxplay_bitstreambuf_s *bs,unsigned int bits)
{
 mpxp_uint32_t retval=0;
 if(bits<=24){
  retval=(mpxp_uint32_t)mpxplay_bitstream_getbits_be24(bs,bits);
 }else{
  if(bits<=32){
   retval =(mpxp_uint32_t)mpxplay_bitstream_getbits_be24(bs,bits-24)<<24;
   retval|=(mpxp_uint32_t)mpxplay_bitstream_getbits_be24(bs,24);
  }
 }
 return retval;
}

mpxp_int64_t mpxplay_bitstream_getbits_be64(struct mpxplay_bitstreambuf_s *bs,unsigned int bits)
{
 mpxp_int64_t retval=0;

 if(bits<=24){
  retval=(mpxp_int64_t)mpxplay_bitstream_getbits_be24(bs,bits);
 }else{
  if(bits<=48){
   retval =(mpxp_int64_t)mpxplay_bitstream_getbits_be24(bs,bits-24)<<24;
   retval|=(mpxp_int64_t)mpxplay_bitstream_getbits_be24(bs,24);
  }else{
   if(bits<=64){
    retval =(mpxp_int64_t)mpxplay_bitstream_getbits_be24(bs,bits-48)<<48;
    retval|=(mpxp_int64_t)mpxplay_bitstream_getbits_be24(bs,24)<<24;
    retval|=(mpxp_int64_t)mpxplay_bitstream_getbits_be24(bs,24);
   }
  }
 }
 return retval;
}

/*mpxp_int64_t mpxplay_bitstream_getbits_le64(struct mpxplay_bitstreambuf_s *bs,unsigned int bits)
{
 mpxp_int64_t retval=0;

 if(bits<=24){
  retval=(mpxp_int64_t)mpxplay_bitstream_getbits_le24(bs,bits);
 }else{
  if(bits<=48){
   retval =(mpxp_int64_t)mpxplay_bitstream_getbits_le24(bs,24);
   retval|=(mpxp_int64_t)mpxplay_bitstream_getbits_le24(bs,bits-24)<<24;
  }else{
   if(bits<=64){
    retval =(mpxp_int64_t)mpxplay_bitstream_getbits_le24(bs,24);
    retval|=(mpxp_int64_t)mpxplay_bitstream_getbits_le24(bs,24)<<24;
    retval|=(mpxp_int64_t)mpxplay_bitstream_getbits_le24(bs,bits-48)<<48;
   }
  }
 }
 return retval;
}*/

int mpxplay_bitstream_skipbits(struct mpxplay_bitstreambuf_s *bs,int bits)
{
 bs->bitpos+=bits;
 if(bs->bitpos>bs->storedbits){
  if(bits<0)
   bs->bitpos=0;
  else
   bs->bitpos=bs->storedbits;
  return MPXPLAY_ERROR_MPXINBUF_SEEK_BUF;
 }
 return MPXPLAY_ERROR_MPXINBUF_OK;
}

long mpxplay_bitstream_leftbits(struct mpxplay_bitstreambuf_s *bs)
{
 return (bs->storedbits-bs->bitpos);
}

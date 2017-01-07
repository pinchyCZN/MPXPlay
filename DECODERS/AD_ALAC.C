//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2010 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: ALAC decoder

#include "in_file.h"

#ifdef MPXPLAY_LINK_DECODER_ALAC

#include "newfunc\newfunc.h"

typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed __int64      int64_t;
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned __int64    uint64_t;

typedef struct alac_file
{
 mpxplay_bitstreambuf_s bs;

 int samplesize;
 int numchannels;
 int bytespersample;

 int32_t *predicterror_buffer_a; // buffers
 int32_t *predicterror_buffer_b;
 int32_t *outputsamples_buffer_a;
 int32_t *outputsamples_buffer_b;
 int32_t *uncompressed_bytes_buffer_a;
 int32_t *uncompressed_bytes_buffer_b;

 uint32_t setinfo_max_samples_per_frame; /* 0x1000 = 4096 */    /* max samples per frame? */
 uint8_t setinfo_7a; /* 0x00 */
 uint8_t setinfo_sample_size; /* 0x10 */
 uint8_t setinfo_rice_historymult; /* 0x28 */
 uint8_t setinfo_rice_initialhistory; /* 0x0a */
 uint8_t setinfo_rice_kmodifier; /* 0x0e */
 uint8_t setinfo_7f; /* 0x02 */
 uint16_t setinfo_80; /* 0x00ff */
 uint32_t setinfo_82; /* 0x000020e7 */ /* max sample size?? */
 uint32_t setinfo_86; /* 0x00069fe4 */ /* bit rate (avarge)?? */
 uint32_t setinfo_8a_rate; /* 0x0000ac44 */
}alac_file;

static alac_file *alac_decoder_open(int samplesize, int numchannels);
static void alac_decoder_close(alac_file *alac);
static unsigned int alac_set_info(alac_file *alac, char *inputbuffer);
static unsigned int alac_allocate_buffers(alac_file *alac);
static void alac_decode_frame(alac_file *alac,unsigned char *inbuffer,unsigned int inbufsize,void *outbuffer,int *outputsize);

static void AD_ALAC_close(struct mpxplay_audio_decoder_info_s *adi);

typedef struct alac_decoder_data_s{
 alac_file *alacf;
 char *extradata;
}alac_decoder_data_s;

static int AD_ALAC_open(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct alac_decoder_data_s *alaci;
 alac_file *alacf;

 if(!adi->bits || !adi->filechannels || (adi->filechannels!=2)) // !!! something wrong with the 1 ch dec
  return MPXPLAY_ERROR_INFILE_CANTOPEN;

 alaci=(struct alac_decoder_data_s *)calloc(1,sizeof(struct alac_decoder_data_s));
 if(!alaci)
  return MPXPLAY_ERROR_INFILE_MEMORY;
 adi->private_data=alaci;

 alacf=alac_decoder_open(adi->bits,adi->filechannels);
 if(!alacf)
  goto err_out_open;
 alaci->alacf=alacf;
 alaci->extradata=spi->extradata;

 if(!alac_set_info(alacf,spi->extradata))
  goto err_out_open;
 if(!alac_allocate_buffers(alacf))
  goto err_out_open;

 adi->pcm_framelen=alacf->setinfo_max_samples_per_frame;

 funcbit_disable(spi->flags,MPXPLAY_SPI_FLAG_NEED_PARSING); // !!!

 return MPXPLAY_ERROR_INFILE_OK;

err_out_open:
 AD_ALAC_close(adi);
 return MPXPLAY_ERROR_INFILE_MEMORY;
}

static void AD_ALAC_close(struct mpxplay_audio_decoder_info_s *adi)
{
 struct alac_decoder_data_s *alaci=(struct alac_decoder_data_s *)adi->private_data;
 if(alaci){
  alac_decoder_close(alaci->alacf);
  free(alaci);
 }
}

static int AD_ALAC_decode(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi)
{
 struct alac_decoder_data_s *alaci=(struct alac_decoder_data_s *)adi->private_data;
 int outputbytes=adi->pcm_framelen*adi->bytespersample;

 if(!alaci || !spi->bs_leftbytes)
  return MPXPLAY_ERROR_INFILE_EOF;
 alac_decode_frame(alaci->alacf,spi->bitstreambuf,spi->bs_leftbytes,adi->pcm_bufptr,&outputbytes);
 spi->bs_usedbytes=spi->bs_leftbytes;
 adi->pcm_samplenum=outputbytes/adi->bytespersample;

 return MPXPLAY_ERROR_INFILE_OK;
}

static void AD_ALAC_clearbuff(struct mpxplay_audio_decoder_info_s *adi,unsigned int seektype)
{
 struct alac_decoder_data_s *alaci=(struct alac_decoder_data_s *)adi->private_data;
 if(alaci && (seektype&MPX_SEEKTYPE_BOF))
  alac_set_info(alaci->alacf,alaci->extradata);
}

struct mpxplay_audio_decoder_func_s AD_ALAC_funcs={
 0,
 NULL,
 NULL,
 NULL,
 &AD_ALAC_open,
 &AD_ALAC_close,
 NULL,
 NULL,
 &AD_ALAC_decode,
 &AD_ALAC_clearbuff,
 NULL,
 NULL,
 0,
 0,
 {{MPXPLAY_WAVEID_ALAC,"ALA"},{0,NULL}}
};

//------------------------------------------------------------------------
/*
 * ALAC (Apple Lossless Audio Codec) decoder
 * Copyright (c) 2005 David Hammerton
 * Modifications by PDSoft (Attila Padar) 2010
 * All rights reserved.
 *
 * This is the actual decoder.
 *
 * http://crazney.net/programs/itunes/alac.html
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#define HOST_BIGENDIAN 0

#define _Swap16(v) pds_bswap16(v)

struct {signed int x:24;} se_struct_24;
#define SignExtend24(val) (se_struct_24.x = val)

static alac_file *alac_decoder_open(int samplesize, int numchannels)
{
 alac_file *newfile = calloc(1,sizeof(alac_file));
 if(newfile){
  newfile->samplesize = samplesize;
  newfile->numchannels = numchannels;
  newfile->bytespersample = (samplesize / 8) * numchannels;
 }
 return newfile;
}

static void alac_decoder_close(alac_file *alac)
{
 if(alac){
  if(alac->predicterror_buffer_a) free(alac->predicterror_buffer_a);
  if(alac->predicterror_buffer_b) free(alac->predicterror_buffer_b);
  if(alac->outputsamples_buffer_a) free(alac->outputsamples_buffer_a);
  if(alac->outputsamples_buffer_b) free(alac->outputsamples_buffer_b);
  if(alac->uncompressed_bytes_buffer_a) free(alac->uncompressed_bytes_buffer_a);
  if(alac->uncompressed_bytes_buffer_b) free(alac->uncompressed_bytes_buffer_b);
  free(alac);
 }
}

static unsigned int alac_set_info(alac_file *alac, char *ptr)
{
 if(PDS_GETB_BE32(ptr)!=12) // 12=size+frma+alac
  return 0;
 ptr += 4; // size
 if(PDS_GETB_LE32(ptr)!=PDS_GET4C_LE32('f','r','m','a'))
  return 0;
 ptr += 4; // frma
 if(PDS_GETB_LE32(ptr)!=PDS_GET4C_LE32('a','l','a','c'))
  return 0;
 ptr += 4; // alac
 ptr += 4; // size
 if(PDS_GETB_LE32(ptr)!=PDS_GET4C_LE32('a','l','a','c'))
  return 0;
 ptr += 4; // alac
 ptr += 4; // 0 ?
 alac->setinfo_max_samples_per_frame= PDS_GETB_BE32(ptr); ptr+=4;
 alac->setinfo_7a                   = PDS_GETB_8U(ptr); ptr++;
 alac->setinfo_sample_size          = PDS_GETB_8U(ptr); ptr++;
 alac->setinfo_rice_historymult     = PDS_GETB_8U(ptr); ptr++;
 alac->setinfo_rice_initialhistory  = PDS_GETB_8U(ptr); ptr++;
 alac->setinfo_rice_kmodifier       = PDS_GETB_8U(ptr); ptr++;
 alac->setinfo_7f                   = PDS_GETB_8U(ptr); ptr++;
 alac->setinfo_80                   = PDS_GETB_BE16(ptr); ptr+=2;
 alac->setinfo_82                   = PDS_GETB_BE32(ptr); ptr+=4;
 alac->setinfo_86                   = PDS_GETB_BE32(ptr); ptr+=4;
 alac->setinfo_8a_rate              = PDS_GETB_BE32(ptr);
 return 1;
}

static unsigned int alac_allocate_buffers(alac_file *alac)
{
 alac->predicterror_buffer_a = malloc(alac->setinfo_max_samples_per_frame * 4);
 if(!alac->predicterror_buffer_a)
  goto err_out_allocbuf;
 alac->predicterror_buffer_b = malloc(alac->setinfo_max_samples_per_frame * 4);
 if(!alac->predicterror_buffer_b)
  goto err_out_allocbuf;
 alac->outputsamples_buffer_a = malloc(alac->setinfo_max_samples_per_frame * 4);
 if(!alac->outputsamples_buffer_a)
  goto err_out_allocbuf;
 alac->outputsamples_buffer_b = malloc(alac->setinfo_max_samples_per_frame * 4);
 if(!alac->outputsamples_buffer_b)
  goto err_out_allocbuf;
 alac->uncompressed_bytes_buffer_a = malloc(alac->setinfo_max_samples_per_frame * 4);
 if(!alac->uncompressed_bytes_buffer_a)
  goto err_out_allocbuf;
 alac->uncompressed_bytes_buffer_b = malloc(alac->setinfo_max_samples_per_frame * 4);
 if(!alac->uncompressed_bytes_buffer_b)
  goto err_out_allocbuf;
 return 1;
err_out_allocbuf:
 return 0;
}

#define readbits(alac,bits)    mpxplay_bitstream_getbits_ube32(&(alac->bs),bits)
#define readbit(alac)          (uint32_t)mpxplay_bitstream_getbit1_be(&(alac->bs))
#define unreadbits(alac,bits)  mpxplay_bitstream_skipbits(&(alac->bs),(-bits))

#if defined(__GNUC__) && (defined(_X86) || defined(__i386) || defined(i386))
static int count_leading_zeros(int input)
{
 int output = 0;
 if(!input) return 32;
 __asm("bsr %1, %0\n"
        : "=r" (output)
        : "r" (input));
 return (0x1f - output);
}
#elif defined(_MSC_VER) && defined(_M_IX86)
static int count_leading_zeros(int input)
{
 int output = 0;
 if (!input) return 32;
 __asm{
  mov eax, input;
  mov edx, 0x1f;
  bsr ecx, eax;
  sub edx, ecx;
  mov output, edx;
 }
 return output;
}
#elif defined(__WATCOMC__)
int asm_countleading_zeros(int);
static inline int count_leading_zeros(int input)
{
 #pragma aux asm_countleading_zeros=\
  "test eax,eax"\
  "jnz count"\
  "mov eax,32"\
  "jmp end"\
  "count:"\
  "bsr ecx, eax" \
  "mov eax, 0x1f"\
  "sub eax, ecx" \
  "end:"\
  parm[eax] value[eax] modify[eax ecx];
 return asm_countleading_zeros(input);
}
#else
static int count_leading_zeros(int input)
{
 int output = 0;
 int curbyte = 0;

 curbyte = input >> 24;
 if(curbyte) goto found;
 output += 8;

 curbyte = input >> 16;
 if(curbyte & 0xff) goto found;
 output += 8;

 curbyte = input >> 8;
 if(curbyte & 0xff) goto found;
 output += 8;

 curbyte = input;
 if(curbyte & 0xff) goto found;
 output += 8;

 return output;

found:
 if(!(curbyte & 0xf0))
  output += 4;
 else
  curbyte >>= 4;

 if(curbyte & 0x8)
  return output;
 if(curbyte & 0x4)
  return output + 1;
 if(curbyte & 0x2)
  return output + 2;
 if(curbyte & 0x1)
  return output + 3;

 // shouldn't get here:
 return output + 4;
}
#endif

#define RICE_THRESHOLD 8 // maximum number of bits for a rice prefix.

static int32_t entropy_decode_value(alac_file* alac,int readSampleSize,
                                    int k,int rice_kmodifier_mask)
{
 int32_t x = 0; // decoded value

 // read x, number of 1s before 0 represent the rice value.
 while(x <= RICE_THRESHOLD && readbit(alac))
  x++;

 if(x > RICE_THRESHOLD){
  // read the number from the bit stream (raw value)
  int32_t value = readbits(alac, readSampleSize);
  // mask value
  value &= (((uint32_t)0xffffffff) >> (32 - readSampleSize));
  x = value;
 }else{
  if(k != 1){
   int extraBits = readbits(alac, k);

   // x = x * (2^k - 1)
   x *= (((1 << k) - 1) & rice_kmodifier_mask);

   if(extraBits > 1)
    x += extraBits - 1;
   else
    unreadbits(alac, 1);
  }
 }
 return x;
}

static void entropy_rice_decode(alac_file* alac,int32_t* outputBuffer,
                                int outputSize,int readSampleSize,
				int rice_initialhistory,int rice_kmodifier,
				int rice_historymult,int rice_kmodifier_mask)
{
 int outputCount,history=rice_initialhistory,signModifier=0;

 for(outputCount = 0; outputCount < outputSize; outputCount++){
  int32_t decodedValue,finalValue,k;

  k = 31 - rice_kmodifier - count_leading_zeros((history >> 9) + 3);

  if(k < 0)
   k += rice_kmodifier;
  else
   k = rice_kmodifier;

  // note: don't use rice_kmodifier_mask here (set mask to 0xFFFFFFFF)
  decodedValue = entropy_decode_value(alac, readSampleSize, k, 0xFFFFFFFF);

  decodedValue += signModifier;
  finalValue = (decodedValue + 1) / 2; // inc by 1 and shift out sign bit
  if(decodedValue & 1) // the sign is stored in the low bit
   finalValue *= -1;

  outputBuffer[outputCount] = finalValue;

  signModifier = 0;

  // update history
  history += (decodedValue * rice_historymult) - ((history * rice_historymult) >> 9);

  if(decodedValue > 0xFFFF)
   history = 0xFFFF;

  // special case, for compressed blocks of 0
  if((history < 128) && (outputCount + 1 < outputSize)){
   int32_t blockSize;

   signModifier = 1;

   k = count_leading_zeros(history) + ((history + 16) / 64) - 24;

   // note: blockSize is always 16bit
   blockSize = entropy_decode_value(alac, 16, k, rice_kmodifier_mask);

   // got blockSize 0s
   if(blockSize > 0){
    pds_memset(&outputBuffer[outputCount + 1], 0, blockSize * sizeof(*outputBuffer));
    outputCount += blockSize;
   }

   if(blockSize > 0xFFFF)
    signModifier = 0;

   history = 0;
  }
 }
}

#define SIGN_EXTENDED32(val, bits) ((val << (32 - bits)) >> (32 - bits))

#define SIGN_ONLY(v) \
                     ((v < 0) ? (-1) : \
                                ((v > 0) ? (1) : \
                                           (0)))

static void predictor_decompress_fir_adapt(int32_t *error_buffer,
                                           int32_t *buffer_out,
                                           int output_size,
                                           int readsamplesize,
                                           int16_t *predictor_coef_table,
                                           int predictor_coef_num,
                                           int predictor_quantitization)
{
    int i;

    /* first sample always copies */
    *buffer_out = *error_buffer;

    if (!predictor_coef_num)
    {
        if (output_size <= 1) return;
        pds_memcpy(buffer_out+1, error_buffer+1, (output_size-1) * 4);
        return;
    }

    if (predictor_coef_num == 0x1f) /* 11111 - max value of predictor_coef_num */
    { /* second-best case scenario for fir decompression,
       * error describes a small difference from the previous sample only
       */
        if (output_size <= 1) return;
        for (i = 0; i < output_size - 1; i++)
        {
            int32_t prev_value;
            int32_t error_value;

            prev_value = buffer_out[i];
            error_value = error_buffer[i+1];
            buffer_out[i+1] = SIGN_EXTENDED32((prev_value + error_value), readsamplesize);
        }
        return;
    }

    /* read warm-up samples */
    if (predictor_coef_num > 0)
    {
        int i;
        for (i = 0; i < predictor_coef_num; i++)
        {
            int32_t val;

            val = buffer_out[i] + error_buffer[i+1];

            val = SIGN_EXTENDED32(val, readsamplesize);

            buffer_out[i+1] = val;
        }
    }

#if 0
    /* 4 and 8 are very common cases (the only ones i've seen). these
     * should be unrolled and optimised
     */
    if (predictor_coef_num == 4)
    {
        /* FIXME: optimised general case */
        return;
    }

    if (predictor_coef_table == 8)
    {
        /* FIXME: optimised general case */
        return;
    }
#endif


    /* general case */
    if (predictor_coef_num > 0)
    {
        for (i = predictor_coef_num + 1;
             i < output_size;
             i++)
        {
            int j;
            int sum = 0;
            int outval;
            int error_val = error_buffer[i];

            for (j = 0; j < predictor_coef_num; j++)
            {
                sum += (buffer_out[predictor_coef_num-j] - buffer_out[0]) *
                       predictor_coef_table[j];
            }

            outval = (1 << (predictor_quantitization-1)) + sum;
            outval = outval >> predictor_quantitization;
            outval = outval + buffer_out[0] + error_val;
            outval = SIGN_EXTENDED32(outval, readsamplesize);

            buffer_out[predictor_coef_num+1] = outval;

            if (error_val > 0)
            {
                int predictor_num = predictor_coef_num - 1;

                while (predictor_num >= 0 && error_val > 0)
                {
                    int val = buffer_out[0] - buffer_out[predictor_coef_num - predictor_num];
                    int sign = SIGN_ONLY(val);

                    predictor_coef_table[predictor_num] -= sign;

                    val *= sign; /* absolute value */

                    error_val -= ((val >> predictor_quantitization) *
                                  (predictor_coef_num - predictor_num));

                    predictor_num--;
                }
            }
            else if (error_val < 0)
            {
                int predictor_num = predictor_coef_num - 1;

                while (predictor_num >= 0 && error_val < 0)
                {
                    int val = buffer_out[0] - buffer_out[predictor_coef_num - predictor_num];
                    int sign = - SIGN_ONLY(val);

                    predictor_coef_table[predictor_num] -= sign;

                    val *= sign; /* neg value */

                    error_val -= ((val >> predictor_quantitization) *
                                  (predictor_coef_num - predictor_num));

                    predictor_num--;
                }
            }

            buffer_out++;
        }
    }
}

static void deinterlace_16(int32_t *buffer_a, int32_t *buffer_b,
                    int16_t *buffer_out,
                    int numchannels, int numsamples,
                    uint8_t interlacing_shift,
                    uint8_t interlacing_leftweight)
{
    int i;
    if (numsamples <= 0) return;

    /* weighted interlacing */
    if (interlacing_leftweight)
    {
        for (i = 0; i < numsamples; i++)
        {
            int32_t difference, midright;
            int16_t left;
            int16_t right;

            midright = buffer_a[i];
            difference = buffer_b[i];

            right = midright - ((difference * interlacing_leftweight) >> interlacing_shift);
            left = right + difference;

            /* output is always little endian */
#if (HOST_BIGENDIAN==1)
            left=_Swap16(left);
            right=_Swap16(right);
#endif

            buffer_out[i*numchannels] = left;
            buffer_out[i*numchannels + 1] = right;
        }

        return;
    }

    /* otherwise basic interlacing took place */
    for (i = 0; i < numsamples; i++)
    {
        int16_t left, right;

        left = buffer_a[i];
        right = buffer_b[i];

        /* output is always little endian */
#if (HOST_BIGENDIAN==1)
         left=_Swap16(left);
         right=_Swap16(right);
#endif

        buffer_out[i*numchannels] = left;
        buffer_out[i*numchannels + 1] = right;
    }
}

static void deinterlace_24(int32_t *buffer_a, int32_t *buffer_b,
					int uncompressed_bytes,
					int32_t *uncompressed_bytes_buffer_a, int32_t *uncompressed_bytes_buffer_b,
                    void *buffer_out,
                    int numchannels, int numsamples,
                    uint8_t interlacing_shift,
                    uint8_t interlacing_leftweight)
{
	int i;
    if (numsamples <= 0) return;
	
    /* weighted interlacing */
    if (interlacing_leftweight)
    {
        for (i = 0; i < numsamples; i++)
        {
            int32_t difference, midright;
            int32_t left;
            int32_t right;
			
            midright = buffer_a[i];
            difference = buffer_b[i];
			
            right = midright - ((difference * interlacing_leftweight) >> interlacing_shift);
            left = right + difference;
			
			if (uncompressed_bytes)
			{
				uint32_t mask = ~(0xFFFFFFFF << (uncompressed_bytes * 8));
				left <<= (uncompressed_bytes * 8);
				right <<= (uncompressed_bytes * 8);
				
				left |= uncompressed_bytes_buffer_a[i] & mask;
				right |= uncompressed_bytes_buffer_b[i] & mask;
			}
			
			((uint8_t*)buffer_out)[i * numchannels * 3] = (left) & 0xFF;
			((uint8_t*)buffer_out)[i * numchannels * 3 + 1] = (left >> 8) & 0xFF;
			((uint8_t*)buffer_out)[i * numchannels * 3 + 2] = (left >> 16) & 0xFF;
			
			((uint8_t*)buffer_out)[i * numchannels * 3 + 3] = (right) & 0xFF;
			((uint8_t*)buffer_out)[i * numchannels * 3 + 4] = (right >> 8) & 0xFF;
			((uint8_t*)buffer_out)[i * numchannels * 3 + 5] = (right >> 16) & 0xFF;
        }
		
        return;
    }
	
    /* otherwise basic interlacing took place */
    for (i = 0; i < numsamples; i++)
    {
        int32_t left, right;

        left = buffer_a[i];
        right = buffer_b[i];
		
		if (uncompressed_bytes)
		{
			uint32_t mask = ~(0xFFFFFFFF << (uncompressed_bytes * 8));
			left <<= (uncompressed_bytes * 8);
			right <<= (uncompressed_bytes * 8);
			
			left |= uncompressed_bytes_buffer_a[i] & mask;
			right |= uncompressed_bytes_buffer_b[i] & mask;
		}

		((uint8_t*)buffer_out)[i * numchannels * 3] = (left) & 0xFF;
		((uint8_t*)buffer_out)[i * numchannels * 3 + 1] = (left >> 8) & 0xFF;
		((uint8_t*)buffer_out)[i * numchannels * 3 + 2] = (left >> 16) & 0xFF;
		
		((uint8_t*)buffer_out)[i * numchannels * 3 + 3] = (right) & 0xFF;
		((uint8_t*)buffer_out)[i * numchannels * 3 + 4] = (right >> 8) & 0xFF;
		((uint8_t*)buffer_out)[i * numchannels * 3 + 5] = (right >> 16) & 0xFF;
		
    }
	
}

static void alac_decode_frame(alac_file *alac,
                  unsigned char *inbuffer,unsigned int inbufsize,
                  void *outbuffer, int *outputsize)
{
 int channels;
 int32_t outputsamples = alac->setinfo_max_samples_per_frame;

 *outputsize = outputsamples * alac->bytespersample;

 mpxplay_bitstream_init(&(alac->bs),inbuffer,inbufsize);

 channels = readbits(alac, 3);

 switch(channels){
    case 0: // 1 channel
    {
        int hassize;
        int isnotcompressed;
        int readsamplesize;

        int uncompressed_bytes;
        int ricemodifier;

        // 2^result = something to do with output waiting.
        // perhaps matters if we read > 1 frame in a pass?

        readbits(alac, 4);

        readbits(alac, 12); // unknown, skip 12 bits

        hassize = readbits(alac, 1); // the output sample size is stored soon

        uncompressed_bytes = readbits(alac, 2); // number of bytes in the (compressed) stream that are not compressed

        isnotcompressed = readbits(alac, 1); // whether the frame is compressed

        if (hassize)
        {
            // now read the number of samples, as a 32bit integer
            outputsamples = readbits(alac, 32);
            if(outputsamples>alac->setinfo_max_samples_per_frame){
             *outputsize=0;
             return;
            }
            *outputsize = outputsamples * alac->bytespersample;
        }

        readsamplesize = alac->setinfo_sample_size - (uncompressed_bytes * 8);

        if(!isnotcompressed)
        { // so it is compressed
            int16_t predictor_coef_table[32];
            int predictor_coef_num;
            int prediction_type;
            int prediction_quantitization;
            int i;

            // skip 16 bits, not sure what they are. seem to be used in two channel case
            readbits(alac, 8);
            readbits(alac, 8);

            prediction_type = readbits(alac, 4);
            prediction_quantitization = readbits(alac, 4);

            ricemodifier = readbits(alac, 3);
            predictor_coef_num = readbits(alac, 5);

            // read the predictor table
            for(i = 0; i < predictor_coef_num; i++)
             predictor_coef_table[i] = (int16_t)readbits(alac, 16);

            if(uncompressed_bytes)
	     for(i = 0; i < outputsamples; i++)
	      alac->uncompressed_bytes_buffer_a[i] = readbits(alac, uncompressed_bytes * 8);

            entropy_rice_decode(alac,alac->predicterror_buffer_a,outputsamples,
	     readsamplesize,alac->setinfo_rice_initialhistory,alac->setinfo_rice_kmodifier,
	     ricemodifier * alac->setinfo_rice_historymult / 4,
	     (1 << alac->setinfo_rice_kmodifier) - 1);

            if(prediction_type == 0){
             predictor_decompress_fir_adapt(alac->predicterror_buffer_a,
                                               alac->outputsamples_buffer_a,
                                               outputsamples,
                                               readsamplesize,
                                               predictor_coef_table,
                                               predictor_coef_num,
                                               prediction_quantitization);
            }else{
             //fprintf(stderr, "FIXME: unhandled predicition type: %i\n", prediction_type);
            }

        }else{ // not compressed, easy case
            if(alac->setinfo_sample_size <= 16){
             int i;
             for(i = 0; i < outputsamples; i++){
              int32_t audiobits = readbits(alac, alac->setinfo_sample_size);
              audiobits = SIGN_EXTENDED32(audiobits, alac->setinfo_sample_size);
              alac->outputsamples_buffer_a[i] = audiobits;
             }
            }else{
             int i;
             for(i = 0; i < outputsamples; i++){
              int32_t audiobits;

              audiobits = readbits(alac, 16);
              // special case of sign extension..
              // as we'll be ORing the low 16bits into this
              audiobits = audiobits << (alac->setinfo_sample_size - 16);
              audiobits |= readbits(alac, alac->setinfo_sample_size - 16);
	      audiobits = SignExtend24(audiobits);
              alac->outputsamples_buffer_a[i] = audiobits;
             }
            }
            uncompressed_bytes = 0; // always 0 for uncompressed
        }

        switch(alac->setinfo_sample_size)
        {
         case 16:
         {
          int i,ch;
          for(i = 0; i < outputsamples; i++){
           int16_t sample = alac->outputsamples_buffer_a[i];
#if (HOST_BIGENDIAN==1)
           sample=_Swap16(sample);
#endif
           for(ch=0;ch<alac->numchannels;ch++)
            ((int16_t*)outbuffer)[i * alac->numchannels + ch] = sample;
          }
          break;
         }
	 case 24:
	 {
	  int i,ch;
	  for(i = 0; i < outputsamples; i++){
	   int32_t sample = alac->outputsamples_buffer_a[i];

	   if(uncompressed_bytes){
	    uint32_t mask;
	    sample = sample << (uncompressed_bytes * 8);
	    mask = ~(0xFFFFFFFF << (uncompressed_bytes * 8));
	    sample |= alac->uncompressed_bytes_buffer_a[i] & mask;
	   }

           for(ch=0;ch<alac->numchannels;ch++){
	    ((uint8_t*)outbuffer)[i * alac->numchannels * 3 + 3*ch +0] = (sample) & 0xFF;
	    ((uint8_t*)outbuffer)[i * alac->numchannels * 3 + 3*ch +1] = (sample >> 8) & 0xFF;
	    ((uint8_t*)outbuffer)[i * alac->numchannels * 3 + 3*ch +2] = (sample >> 16) & 0xFF;
           }
	  }
	  break;
	 }
         case 20:
         case 32:
            //fprintf(stderr, "FIXME: unimplemented sample size %i\n", alac->setinfo_sample_size);
            break;
         default:
            break;
        }
        break;
    }
    case 1: // 2 channels
    {
        int hassize;
        int isnotcompressed;
        int readsamplesize;

        int uncompressed_bytes;

        uint8_t interlacing_shift;
        uint8_t interlacing_leftweight;

        /* 2^result = something to do with output waiting.
         * perhaps matters if we read > 1 frame in a pass?
         */
        readbits(alac, 4);

        readbits(alac, 12); /* unknown, skip 12 bits */

        hassize = readbits(alac, 1); /* the output sample size is stored soon */

        uncompressed_bytes = readbits(alac, 2); /* the number of bytes in the (compressed) stream that are not compressed */

        isnotcompressed = readbits(alac, 1); /* whether the frame is compressed */

        if (hassize)
        {
            // now read the number of samples, as a 32bit integer
            outputsamples = readbits(alac, 32);
            if(outputsamples>alac->setinfo_max_samples_per_frame){
             *outputsize=0;
             return;
            }
            *outputsize = outputsamples * alac->bytespersample;
        }

        readsamplesize = alac->setinfo_sample_size - (uncompressed_bytes * 8) + 1;

        if (!isnotcompressed)
        { /* compressed */
            int16_t predictor_coef_table_a[32];
            int predictor_coef_num_a;
            int prediction_type_a;
            int prediction_quantitization_a;
            int ricemodifier_a;

            int16_t predictor_coef_table_b[32];
            int predictor_coef_num_b;
            int prediction_type_b;
            int prediction_quantitization_b;
            int ricemodifier_b;

            int i;

            interlacing_shift = readbits(alac, 8);
            interlacing_leftweight = readbits(alac, 8);

            /******** channel 1 ***********/
            prediction_type_a = readbits(alac, 4);
            prediction_quantitization_a = readbits(alac, 4);

            ricemodifier_a = readbits(alac, 3);
            predictor_coef_num_a = readbits(alac, 5);

            /* read the predictor table */
            for(i = 0; i < predictor_coef_num_a; i++)
             predictor_coef_table_a[i] = (int16_t)readbits(alac, 16);

            /******** channel 2 *********/
            prediction_type_b = readbits(alac, 4);
            prediction_quantitization_b = readbits(alac, 4);

            ricemodifier_b = readbits(alac, 3);
            predictor_coef_num_b = readbits(alac, 5);

            /* read the predictor table */
            for(i = 0; i < predictor_coef_num_b; i++)
             predictor_coef_table_b[i] = (int16_t)readbits(alac, 16);

            /*********************/
            if(uncompressed_bytes){ /* see mono case */
	     int i;
	     for(i = 0; i < outputsamples; i++){
	      alac->uncompressed_bytes_buffer_a[i] = readbits(alac, uncompressed_bytes * 8);
	      alac->uncompressed_bytes_buffer_b[i] = readbits(alac, uncompressed_bytes * 8);
	     }
            }

            /* channel 1 */
            entropy_rice_decode(alac,alac->predicterror_buffer_a,outputsamples,
	                        readsamplesize,alac->setinfo_rice_initialhistory,
				alac->setinfo_rice_kmodifier,ricemodifier_a * alac->setinfo_rice_historymult / 4,
				(1 << alac->setinfo_rice_kmodifier) - 1);

            if(prediction_type_a == 0){ /* adaptive fir */
             predictor_decompress_fir_adapt(alac->predicterror_buffer_a,
                                            alac->outputsamples_buffer_a,
                                            outputsamples,
                                            readsamplesize,
                                            predictor_coef_table_a,
                                            predictor_coef_num_a,
                                            prediction_quantitization_a);
            }else{ /* see mono case */
             //fprintf(stderr, "FIXME: unhandled predicition type: %i\n", prediction_type_a);
            }

            /* channel 2 */
            entropy_rice_decode(alac,alac->predicterror_buffer_b,outputsamples,
	                        readsamplesize,alac->setinfo_rice_initialhistory,
				alac->setinfo_rice_kmodifier,ricemodifier_b * alac->setinfo_rice_historymult / 4,
				(1 << alac->setinfo_rice_kmodifier) - 1);

            if(prediction_type_b == 0){ /* adaptive fir */
             predictor_decompress_fir_adapt(alac->predicterror_buffer_b,
                                            alac->outputsamples_buffer_b,
                                            outputsamples,
                                            readsamplesize,
                                            predictor_coef_table_b,
                                            predictor_coef_num_b,
                                            prediction_quantitization_b);
            }else{
                //fprintf(stderr, "FIXME: unhandled predicition type: %i\n", prediction_type_b);
            }
        }else{ /* not compressed, easy case */
            if (alac->setinfo_sample_size <= 16)
            {
                int i;
                for (i = 0; i < outputsamples; i++)
                {
                    int32_t audiobits_a, audiobits_b;

                    audiobits_a = readbits(alac, alac->setinfo_sample_size);
                    audiobits_b = readbits(alac, alac->setinfo_sample_size);

                    audiobits_a = SIGN_EXTENDED32(audiobits_a, alac->setinfo_sample_size);
                    audiobits_b = SIGN_EXTENDED32(audiobits_b, alac->setinfo_sample_size);

                    alac->outputsamples_buffer_a[i] = audiobits_a;
                    alac->outputsamples_buffer_b[i] = audiobits_b;
                }
            }
            else
            {
                int i;
                for (i = 0; i < outputsamples; i++)
                {
                    int32_t audiobits_a, audiobits_b;

                    audiobits_a = readbits(alac, 16);
                    audiobits_a = audiobits_a << (alac->setinfo_sample_size - 16);
                    audiobits_a |= readbits(alac, alac->setinfo_sample_size - 16);
					audiobits_a = SignExtend24(audiobits_a);

                    audiobits_b = readbits(alac, 16);
                    audiobits_b = audiobits_b << (alac->setinfo_sample_size - 16);
                    audiobits_b |= readbits(alac, alac->setinfo_sample_size - 16);
					audiobits_b = SignExtend24(audiobits_b);

                    alac->outputsamples_buffer_a[i] = audiobits_a;
                    alac->outputsamples_buffer_b[i] = audiobits_b;
                }
            }
            uncompressed_bytes = 0; // always 0 for uncompressed
            interlacing_shift = 0;
            interlacing_leftweight = 0;
        }

        switch(alac->setinfo_sample_size){
         case 16:
         {
            deinterlace_16(alac->outputsamples_buffer_a,
                           alac->outputsamples_buffer_b,
                           (int16_t*)outbuffer,
                           alac->numchannels,
                           outputsamples,
                           interlacing_shift,
                           interlacing_leftweight);
            break;
         }
	 case 24:
	 {
	    deinterlace_24(alac->outputsamples_buffer_a,
                           alac->outputsamples_buffer_b,
			   uncompressed_bytes,
			   alac->uncompressed_bytes_buffer_a,
			   alac->uncompressed_bytes_buffer_b,
                           (int16_t*)outbuffer,
                           alac->numchannels,
                           outputsamples,
                           interlacing_shift,
                           interlacing_leftweight);
			break;
	 }
         case 20:
         case 32:
            //fprintf(stderr, "FIXME: unimplemented sample size %i\n", alac->setinfo_sample_size);
            break;
         default:
            break;
        }

        break;
    }
    default:*outputsize=0;break;

 }
}

#endif // MPXPLAY_LINK_DECODER_ALAC

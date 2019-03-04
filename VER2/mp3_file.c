#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "minimp3_ex.h"
#include "cmos.h"
#define FALSE 0
#define TRUE 1
typedef unsigned int DWORD;

typedef struct{
	FILE *f;
	unsigned int flen;
}FILE_INFO;

typedef struct{
	char *data;
	int data_size;
	int position;
}DATA_BUF;

int fill_buffer(FILE_INFO finfo,unsigned char *buf,int buf_size,int *buf_level,int forward)
{
	int result=0;
	if(*buf_level>buf_size)
		*buf_level=buf_size;
	if(forward>*buf_level){
		int offset;
		offset=forward-*buf_level;
		if(0==fseek(finfo.f,offset,SEEK_CUR)){
			result=fread(buf,1,buf_size,finfo.f);
			*buf_level=result;
			if(*buf_level<buf_size)
				memset(buf+*buf_level,0,buf_size-*buf_level);
		}
	}else{
		int len;
		int offset;
		len=*buf_level-forward;
		if(forward>0)
			memmove(buf,buf+forward,len);
		offset=len;
		len=buf_size-offset;
		result=fread(buf+offset,1,len,finfo.f);
		*buf_level=result+offset;
		if(*buf_level<buf_size)
			memset(buf+*buf_level,0,buf_size-*buf_level);
	}
	return result;
}




int seek_initial_offset(FILE_INFO finfo,int initial_offset)
{
	int result=FALSE;
	return result;
}

int save_file_offset(FILE_INFO finfo)
{
	DWORD get_tick_count();
	int get_msec(DWORD);
	int result=FALSE;
	static DWORD tick=0;
	DWORD delta;
#ifdef _WIN32
	const DWORD max=1000;
#else
	const DWORD max=5000;
#endif
	delta=get_tick_count()-tick;
	if(get_msec(delta)>5000){
		unsigned int divisor=finfo.flen/0xE0;
		tick=get_tick_count();
		if(divisor!=0){
			unsigned int tmp=ftell(finfo.f);
			tmp=tmp/divisor;
			if(tmp>0xFF)
				tmp=0xFF;
			result=write_cmos(CMOS_OFFSET,tmp);
		}
	}
	return result;
}
int seek_mp3(FILE_INFO finfo,int dir)
{
	int result=FALSE;
	return result;
}

int setup_data_buf(DATA_BUF *dbuf,int size)
{
	int result=FALSE;
	char *tmp;
	if(0==dbuf->data){
		tmp=calloc(1,size);
		if(tmp){
			dbuf->data=tmp;
			dbuf->data_size=size;
			result=TRUE;
		}
	}else{
		result=TRUE;
	}
	return result;
}
int fill_file_buf(DATA_BUF *buf,FILE *f)
{
	int amount;
	int position,size;
	char *tmp;
	int read;
	size=buf->data_size;
	position=buf->position;
	amount=size-position;
	tmp=buf->data+position;
	read=fread(tmp,1,amount,f);
	buf->position+=read;
	return read;
}

int fill_audio_buf(DATA_BUF *buf,char *audio,int alen)
{
	int len;
	int position,size;
	char *tmp;
	size=buf->data_size;
	position=buf->position;
	len=size-position;
	if(len>alen)
		len=alen;
	tmp+=buf->data+position;
	memcpy(tmp,audio,len);
	return len;
}

int play_mp3(const char *fname,int initial_offset)
{
	int result=FALSE;
	FILE *f;
	mp3dec_t mp3_dec={0};
	static DATA_BUF file_buf={0};
	static DATA_BUF audio_buf={0};
	if(!setup_data_buf(file_buf,0x10000))
		return result;
	if(!setup_data_buf(audio_buf,get_audio_buf_size()*4))
		return result;
	f=fopen(fname,"rb");
	if(0==f)
		return result;
	mp3dec_init(&mp3_dec);
	while(1){
		int fmt_bytes,frame_bytes;
	    mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
		mp3dec_frame_info_t frame_info;
		fill_file_buf(&file_buf,f);
		if(0==file_buf.position)
			break;
		do{
			char *tmp;
			int tmp_size;
			int samples;
			int fwd;
			tmp=file_buf.data;
			tmp_size=file_buf.position;
			memset(pcm,0xBB,sizeof(pcm));
			samples=mp3dec_decode_frame(&mp3_dec,tmp,tmp_size,pcm,&frame_info);
			if(0==samples)
				break;
			{
				int amount;
				samples*=2;
				amount=fill_audio_buf(audio_buf,pcm,samples);
				if(afill>=abuf_size){
					play_wav_buf(abuf,abuf_size);
					afill-=abuf_size;
				}
			}

			fwd=frame_info.frame_bytes;
			tmp+=fwd;
			len-=fwd;
			if(0==fwd)
				break;
			if(len<=0){
				break;
			}

		}while(1);

		

	}
	fclose(f);
	return result;
}

int mp3_test(const char *fname)
{
	return 0;
}

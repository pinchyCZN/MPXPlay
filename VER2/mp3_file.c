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



int fill_audio_buf(FILE_INFO finfo,unsigned char *abuf,unsigned int abuf_size,unsigned int *abuf_level,unsigned int need_amount)
{
	int result=FALSE;
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

int play_mp3(const char *fname,int initial_offset)
{
	int result=FALSE;
	FILE *f;
	static char *buf=0;
	int buf_size=0x10000;
	if(0==buf){
		buf=calloc(1,buf_size);
		if(0==buf)
			return result;
	}
	f=fopen(fname,"rb");
	if(0==f)
		return result;
	while(1){
		char *tmp;
		int len;
		int offset;
		len=fread(buf,1,buf_size,f);
		tmp=buf;
		if(len<=0)
			break;
		offset=mp3dec_skip_id3v2(tmp,len);
		if(offset>0){
			if(offset>len){
				fseek(f,offset-len,SEEK_CUR);
				continue;
			}else{
				memcpy(tmp,tmp+offset,len-offset);
				len-=offset;
			}
		}
		if(len<MAX_FREE_FORMAT_FRAME_SIZE){
			int x=fread(tmp+len,1,buf_size-len,f);
			len+=x;
		}
		

	}
	fclose(f);
	return result;
}

int mp3_test(const char *fname)
{
	return 0;
}

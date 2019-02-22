#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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


int mp3_test(const char *fname)
{
}

int fill_audio_buf(FILE_INFO finfo,unsigned char *abuf,unsigned int abuf_size,unsigned int *abuf_level,unsigned int need_amount)
{
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
			result=write_cmos(CMOS_VAL.OFFSET,tmp);
		}
	}
	return result;
}
int seek_nearest_frame(FILE_INFO finfo)
{
	static unsigned char *buf=0;
	const int buf_size=0x4000;
	int result=FALSE;
	uint current_offset;
	int read,count;
	if(0==buf)
		buf=(unsigned char*)malloc(buf_size);
	if(0==buf)
		return result;
	printf("sn =%08X\n",ftell(finfo.f));
	for(count=0;count<64;count++){
		const unsigned int rwind=3;
		current_offset=ftell(finfo.f);
		if(current_offset>=rwind){
			if(0==fseek(finfo.f,-rwind,SEEK_CUR))
				current_offset-=rwind;
		}
		if((current_offset+buf_size)>=finfo.flen){
			break;
		}
		read=fread(buf,1,buf_size,finfo.f);
		if(read > 4){
			int i;
			for(i=0;i<read;i++){
				int val;
				if((i+3)>=read)
					break;
				val=buf[i];
				if(val==0xFF){ //frame sync
					val=buf[i+1];
					if((val&0b111_000_00)==0b111_000_00){ //frame sync
						if((val&0b000_11_000)!=0b000_01_000){ //audio version id
							if((val&0b110)==0b010){ //layer descript III
								val=buf[i+2];
								if((val&0b1111_0000)!=0b1111_0000){ //bitrate index
									if((val&0b1100)!=0b1100){ //sample rate index
										val=buf[i+3];
										if((val&0b11)!=0b10)
											result=TRUE;
									}
								}
							}
						}
					}
				}
				if(result){
					fseek(finfo.f,current_offset+i,SEEK_SET);
					goto exit;
				}
			}
		}
		else{
			break;
		}
	}
exit:
	printf("sn2=%08X\n",ftell(finfo.f));
	return result;
}
int seek_mp3(FILE_INFO finfo,int dir)
{
	int result=FALSE;
	uint tmp=finfo.flen/36;
	printf("fln=%08X\n",finfo.flen);
	if(tmp!=0){
		uint offset=ftell(finfo.f);
		if(dir>=0){ //forward
			if((offset+tmp)>finfo.flen){
				printf("-----seeked to end\n");
				fseek(finfo.f,0,SEEK_END);
				return FALSE;
			}
		}
		else{ //reverse
			tmp<<=1;
			if(tmp>offset)
				tmp=offset;
			tmp=-tmp;
		}
		if(0==fseek(finfo.f,tmp,SEEK_CUR)){
			seek_nearest_frame(finfo);
			result=TRUE;
		}
	}
	return result;
}
extern (C)
int play_mp3(const char *fname,int initial_offset)
{
	int result=FALSE;
	FILE_INFO finfo;
	uint flen;
	finfo.f=fopen(fname,"rb");
	if(finfo.f is null){
		printf("unable to open file:%s\n",fname);
		return result;
	}
	import main:get_flen;
	finfo.flen=get_flen(finfo.f);
	seek_initial_offset(finfo,initial_offset);

	__gshared static unsigned char *buf;
	const uint buf_size=0x10000;
	uint buf_level;
	uint full_level;

	uint __tmp;
	fill_audio_buf(finfo,null,0,__tmp,0);

	full_level=get_audio_buf_size();
	if(full_level>buf_size)
		full_level=buf_size;
	buf_level=0;

	FILE *fout=null;
	//fout=fopen("1.wav","rb");
	if(fout !is null){
		char tmp[1024];
		fread(tmp.ptr,1,1024,fout);
		fclose(fout);
		fout=fopen("out.wav","wb");
		fwrite(tmp.ptr,1,16*4,fout);
	}
	DWORD tick=get_tick_count();
	if(buf is null)
		buf=cast(unsigned char*)malloc(buf_size);
	if(buf is null)
		goto exit;

	printf("444\n");
	while(1){
		if(fill_audio_buf(finfo,buf,buf_size,buf_level,full_level)){
			play_wav_buf(buf,full_level);
			if(fout !is null)
				fwrite(buf,full_level,1,fout);
			if(buf_level>buf_size)
				buf_level=buf_size;
			if(buf_level>full_level){
				int len;
				len=buf_level-full_level;
				memmove(buf,buf+full_level,len);
				buf_level-=full_level;
			}else
				buf_level=0;
			int len;
			len=ftell(finfo.f);
			//printf("len=%06i %06i\n",len,buf_level);
		}
		else
			break;
		
		save_file_offset(finfo);

		DWORD delta=get_tick_count()-tick;
		if(get_msec(delta)>200){
			tick=get_tick_count();
			int vkey,ext;
			vkey=dos_get_key(&ext);
			switch(vkey){
			case VK_BACKSPACE:
			case VK_4:
				if(seek_mp3(finfo,-1)){
					fill_audio_buf(finfo,null,0,__tmp,0);
					buf_level=0;
				}
				tick>>=2;
				break;
			case VK_6:
				if(seek_mp3(finfo,1)){
					fill_audio_buf(finfo,null,0,__tmp,0);
					buf_level=0;
				}
				else{
					goto exit;
				}
				tick>>=2;
				break;
			case VK_5:
				set_silence();
				while(1){
					vkey=dos_get_key(&ext);
					if(vkey!=0){
						break;
					}
				}
				break;
			case VK_1:
			case VK_2:
			case VK_3:
			case VK_7:
			case VK_8:
			case VK_9:
				break;
			case VK_0:
				version(windows_exe){
					import core.stdc.stdlib:exit;
					exit(0);
					dos_put_key(vkey);
					goto exit;
				}
				break;
			case VK_TAB:
			case VK_FWDSLASH:
			case VK_ASTERISK:
			case VK_PLUS:
			case VK_MINUS:
			case VK_ENTER:
				dos_put_key(vkey);
				goto exit;
			default:
				break;
			}
			int count=0;
			while(vkey){
				vkey=dos_get_key(&ext);
				count++;
				if(count>100)
					break;
			}
		}
	}
	memset(buf,0,buf_size);
	
exit:
	fclose(finfo.f);
	return result;
}

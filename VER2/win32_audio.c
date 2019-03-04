#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

/*
int fill_buffer(FILE *f,BYTE *buf,int buf_size,int buf_level,int forward)
{
	int result=0;
	if(buf_level>buf_size)
		buf_level=buf_size;
	if(forward>buf_level){
		int offset;
		offset=forward-buf_level;
		if(0==fseek(f,offset,SEEK_CUR)){
			result=fread(buf,1,buf_size,f);
			buf_level=result;
			if(buf_level<buf_size)
				memset(buf+buf_level,0,buf_size-buf_level);
		}
	}else{
		int len;
		int offset;
		len=buf_level-forward;
		if(forward>0)
			memmove(buf,buf+forward,len);
		offset=len;
		len=buf_size-offset;
		result=fread(buf+offset,1,len,f);
		buf_level=result+offset;
		if(buf_level<buf_size)
			memset(buf+buf_level,0,buf_size-buf_level);
	}
	return result;
}
int skip_tags(FILE *f,BYTE *buf,int buf_size,int buf_level)
{
	int result=FALSE;
	if(buf_level>=10){
		if(buf[0]=='I' && buf[1]=='D' && buf[2]=='3' && buf[3]!=0xFF && buf[4]!=0xFF
			&& ((buf[6]|buf[7]|buf[8]|buf[9])&0x80)==0){
			unsigned int size=(buf[9]|(buf[8]<<7)|(buf[7]<<14)|(buf[6]<<21))+10;
			if(size>0){
				if(0<fill_buffer(f,buf,buf_size,buf_level,size))
					result=TRUE;
			}
		}
	}
	return result;
}
*/
/*
static
int mp3_test(const char *fname)
{
	short *buffer;
	int buffer_len=0x10000;
	BYTE *fbuf;
	int fbuf_len=0x10000;
	int data_size;
	FILE *fout=0;
	FILE *f=(FILE*)fopen(fname,"rb");
	printf("fname=%s\n",fname);
	if(0==f)
		return 0;
	mp3_decode_init();
	
	mp3_context_t mp3;
	//fout=fopen("1.wav","rb");
	if(fout !is null){
		char tmp[1024];
		fread(tmp.ptr,1,1024,fout);
		fclose(fout);
		fout=fopen("out.wav","wb");
		fwrite(tmp.ptr,1,16*4,fout);
	}
	fbuf=(BYTE*)malloc(fbuf_len);
	buffer=(short*)malloc(buffer_len);
	while(1){
		int buf_level=0;
		fill_buffer(f,fbuf,fbuf_len,buf_level,0);
		if(buf_level<=0)
			break;
		skip_tags(f,fbuf,fbuf_len,buf_level);
		DWORD tick,delta;
		tick=clock();

		while(1){
			int consumed=0;
			int written=0;
			int result;
			result=mp3_decode_frame(&mp3,buffer,written,fbuf,fbuf_len,consumed);
			if(result){
				if(fout !is null)
					fwrite(buffer,1,written,fout);
				delta=clock()-tick;
				delta/=CLOCKS_PER_SEC;
				if(delta>1){
					tick=clock();
					DWORD p=ftell(f);
					printf("%08X\n",p);
				}
				int extended=0;
				int key=0;
				key=get_key(extended);
				if(key!=0){
					printf("key=%02X %i\n",key,key);
					goto exit;
				}
				fill_buffer(f,fbuf,fbuf_len,buf_level,consumed);
				skip_tags(f,fbuf,fbuf_len,buf_level);
			}else{
				break;
			}
		}
	}
exit:
	printf("done\n");
	if(fout !is null)
		fclose(fout);
	fclose(f);
	return 0;
}
*/
/*
int fill_audio_buf(FILE *f,BYTE *abuf,DWORD abuf_size,DWORD abuf_level,DWORD need_amount)
{
	int result=FALSE;
	static BYTE *fbuf;
	const int fbuf_size=0x10000;
	static BYTE *buf;
	const int buf_size=0x10000;

	static int buf_level=0;
	static mp3_context_t mp3;

	mp3_decode_init();

	if(0==f || 0==abuf){
		buf_level=0;
		memset(&mp3,0,mp3.sizeof);
		return result;
	}


	if(0==fbuf)
		fbuf=(BYTE*)malloc(fbuf_size);
	if(0==buf)
		buf=(BYTE*)malloc(buf_size);
	if(0==fbuf || 0==buf)
		return result;

	fill_buffer(f,fbuf,fbuf_size,buf_level,0);
	if(buf_level==0)
		return result;
	skip_tags(f,fbuf,fbuf_size,buf_level);
	if(abuf_level>=need_amount){
		result=true;
		return result;
	}
	while(1){
		int consumed=0;
		int written=0;
		int mp3_res;
		mp3_res=mp3_decode_frame(&mp3,(short*)buf,written,fbuf,fbuf_size,consumed);
		if(mp3_res){
			if(written>0){
				if(abuf_level>abuf_size)
					abuf_level=abuf_size;
				if(need_amount>abuf_size)
					need_amount=abuf_size;

				int len;
				len=written+abuf_level;
				if(len>abuf_size)
					len=abuf_size-abuf_level;
				else
					len=written;

				memmove(abuf+abuf_level,buf,len);

				abuf_level+=len;
			}
			fill_buffer(f,fbuf,fbuf_size,buf_level,consumed);
			skip_tags(f,fbuf,fbuf_size,buf_level);
			if(abuf_level>=need_amount){
				result=true;
				break;
			}
		}else{
			break;
		}
	}
	if(!result)
		buf_level=0;

	return result;
}



static
int play_mp3(const char *fname)
{
	int result=FALSE;
	FILE *f;
	f=fopen(fname,"rb");
	if(f is null){
		printf("unable to open file:%s\n",fname);
		return result;
	}
	static BYTE *buf;
	const DWORD buf_size=0x10000;
	DWORD buf_level;
	DWORD full_level;

	DWORD __tmp;
	fill_audio_buf(null,null,0,__tmp,0);

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
		buf=(BYTE*)malloc(buf_size);
	if(buf is null)
		goto exit;

	while(1){
		if(fill_audio_buf(f,buf,buf_size,buf_level,full_level)){
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
			len=ftell(f);
			printf("len=%06i %06i\n",len,buf_level);
		}
		else
			break;
		DWORD delta=get_tick_count()-tick;
		if(get_msec(delta)>200){
			tick=get_tick_count();
			if(kbhit())
				break;
		}
	}
	memset(buf,0,buf_size);
	
exit:
	fclose(f);
	return result;
}

*/

static WAVEFORMATEX wf = {
    1,  // wFormatTag
    0,  // nChannels
    0,  // nSamplesPerSec
    0,  // nAvgBytesPerSec
    4,  // nBlockAlign
    16, // wBitsPerSample
    sizeof(WAVEFORMATEX) // cbSize
};
static WAVEHDR wh_template = {
    NULL, // lpData
    0, // dwBufferLength
    0, // dwBytesRecorded
    0, // dwUser
    0, // dwFlags
    1, // dwLoops
    NULL, // lpNext
    0 // reserved
};
#define BUFFER_COUNT 2
static WAVEHDR wh[BUFFER_COUNT];


void WINAPI AudioCallback(
	HWAVEOUT hwo,      
	DWORD uMsg,         
	DWORD *dwInstance,  
	DWORD dwParam1,    
	DWORD dwParam2     
)
{
	extern int base_reg;
	extern int OSD0LPIB;
	int get_audio_buf_size();
	int write_32(DWORD,DWORD);
	LPWAVEHDR wh=(LPWAVEHDR) dwParam1;
	int size=sizeof(WAVEHDR);
	if(!wh)
		return;
	waveOutUnprepareHeader(hwo,wh,size);
	waveOutPrepareHeader(hwo,wh,size);
	waveOutWrite(hwo,wh,size);
	if(base_reg!=0){
		int tmp=0;
		if(!wh->dwUser)
			tmp=get_audio_buf_size();
		write_32(base_reg+OSD0LPIB,tmp);
	}
}

int thread_exit=FALSE;
DWORD WINAPI hw_thread(void *param)
{
#define HDA_COUNT 400
	extern int hda_registers[HDA_COUNT];
	while(1){
		if(thread_exit){
			break;
		}else{
			memset(hda_registers,0,HDA_COUNT*sizeof(int));
			Sleep(0);
		}

	}
	return 0;
}
int win32_setup_audio()
{
	int init_hda();
	int start_audio();
	int* get_audio_buf(int);
	HWAVEOUT hwo;
	DWORD tid=0;
	CreateThread(NULL,0,&hw_thread,NULL,0,&tid);
	init_hda();
	start_audio();
	printf("audio setup done\n");
	thread_exit=TRUE;
	wf.nSamplesPerSec=44100;
	wf.nChannels=2;
	waveOutOpen(&hwo,WAVE_MAPPER,&wf,(DWORD)&AudioCallback,0,CALLBACK_FUNCTION);
	if(hwo!=0){
		int i;
		for(i=0;i<2;i++){
			wh[i]=wh_template;
			wh[i].dwBufferLength=get_audio_buf_size();
			wh[i].lpData=(char*)get_audio_buf(i);
			wh[i].dwUser=i;
			AudioCallback(hwo,0,0,(DWORD)&wh[i],0);
		}
	}
	return 0;
}

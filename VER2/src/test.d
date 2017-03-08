module test;

import core.stdc.stdio: FILE;
import core.sys.windows.windows;

import dummy_;
import libc_map:get_key;
import minimp3;
import intel_hda;
import dos_map;
import main:process_file;

alias memset=_memset;
alias memmove=_memmove;
alias fread=_fread;
alias fwrite=_fwrite;
alias fseek=_fseek;
alias ftell=_ftell;
alias fopen=_fopen;
alias fclose=_fclose;
alias malloc=_malloc;
alias printf=_printf;
alias clock=_clock;
alias kbhit=_kbhit;

@nogc:
nothrow:

int fill_buffer(FILE *f,ubyte *buf,int buf_size,ref int buf_level,int forward)
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
int skip_tags(FILE *f,ubyte *buf,int buf_size,ref int buf_level)
{
	int result=false;
	if(buf_level>=10){
		if(buf[0]=='I' && buf[1]=='D' && buf[2]=='3' && buf[3]!=0xFF && buf[4]!=0xFF
			&& ((buf[6]|buf[7]|buf[8]|buf[9])&0x80)==0){
			uint size=(buf[9]|(buf[8]<<7)|(buf[7]<<14)|(buf[6]<<21))+10;
			if(size>0){
				if(0<fill_buffer(f,buf,buf_size,buf_level,size))
					result=true;
			}
		}
	}
	return result;
}

static
int mp3_test(const char *fname)
{
	printf("fname=%s\n",fname);
	FILE *f=cast(FILE*)fopen(fname,"rb");
	if(f is null)
		return 0;
	mp3_decode_init();
	
	mp3_context_t mp3;
	short *buffer;
	int buffer_len=0x10000;
	ubyte *fbuf;
	int fbuf_len=0x10000;
	int data_size;
	FILE *fout=null;
	//fout=fopen("1.wav","rb");
	if(fout !is null){
		char tmp[1024];
		fread(tmp.ptr,1,1024,fout);
		fclose(fout);
		fout=fopen("out.wav","wb");
		fwrite(tmp.ptr,1,16*4,fout);
	}
	fbuf=cast(ubyte*)malloc(fbuf_len);
	buffer=cast(short*)malloc(buffer_len);
	while(1){
		int buf_level=0;
		fill_buffer(f,fbuf,fbuf_len,buf_level,0);
		if(buf_level<=0)
			break;
		skip_tags(f,fbuf,fbuf_len,buf_level);
		uint tick,delta;
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
					uint p=ftell(f);
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

int fill_audio_buf(FILE *f,ubyte *abuf,uint abuf_size,ref uint abuf_level,uint need_amount)
{
	int result=false;
	__gshared static ubyte *fbuf;
	const int fbuf_size=0x10000;
	__gshared static ubyte *buf;
	const int buf_size=0x10000;

	__gshared static int buf_level=0;
	__gshared static mp3_context_t mp3;

	mp3_decode_init();

	if(f is null || abuf is null){
		buf_level=0;
		memset(&mp3,0,mp3.sizeof);
		return result;
	}


	if(fbuf is null)
		fbuf=cast(ubyte*)malloc(fbuf_size);
	if(buf is null)
		buf=cast(ubyte*)malloc(buf_size);
	if(fbuf is null || buf is null)
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
		mp3_res=mp3_decode_frame(&mp3,cast(short*)buf,written,fbuf,fbuf_size,consumed);
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
	int result=false;
	FILE *f;
	f=fopen(fname,"rb");
	if(f is null){
		printf("unable to open file:%s\n",fname);
		return result;
	}
	__gshared static ubyte *buf;
	const uint buf_size=0x10000;
	uint buf_level;
	uint full_level;

	uint __tmp;
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
		buf=cast(ubyte*)malloc(buf_size);
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
		if(delta>200){
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
__gshared static WAVEFORMATEX wf = {
    1,  // wFormatTag
    0,  // nChannels
    0,  // nSamplesPerSec
    0,  // nAvgBytesPerSec
    4,  // nBlockAlign
    16, // wBitsPerSample
    WAVEFORMATEX.sizeof // cbSize
};
__gshared static WAVEHDR wh_template = {
    NULL, // lpData
    0, // dwBufferLength
    0, // dwBytesRecorded
    0, // dwUser
    0, // dwFlags
    1, // dwLoops
    NULL, // lpNext
    0 // reserved
};
enum BUFFER_COUNT=2;
__gshared static WAVEHDR wh[BUFFER_COUNT];

extern (Windows)
void AudioCallback(
	HWAVEOUT hwo,      
	UINT uMsg,         
	DWORD_PTR dwInstance,  
	DWORD dwParam1,    
	DWORD dwParam2     
)
{
	LPWAVEHDR wh=cast(LPWAVEHDR) dwParam1;
	int size=WAVEHDR.sizeof;
	if(!wh)
		return;
	waveOutUnprepareHeader(hwo,wh,size);
	waveOutPrepareHeader(hwo,wh,size);
	waveOutWrite(hwo,wh,size);
	if(base_reg!=0){
		int tmp=0;
		if(!wh.dwUser)
			tmp=get_audio_buf_size();
		write_32(base_reg+OSD0LPIB,tmp);
	}
}

__gshared int thread_exit=false;
extern (Windows) DWORD hw_thread(void *param)
{
	while(1){
		if(thread_exit){
			break;
		}else{
			memset(hda_registers.ptr,0,hda_registers.sizeof);
			Sleep(0);
		}

	}
	return 0;
}

extern(C)
int test_d(const char *fname)
{
	DWORD tid=0;
	CreateThread(null,0,&hw_thread,null,0,&tid);
	init_hda();
	start_audio();
	printf("audio setup done\n");
	thread_exit=true;
	HWAVEOUT hwo;
	wf.nSamplesPerSec=44100;
	wf.nChannels=2;
	waveOutOpen(&hwo,WAVE_MAPPER,&wf,cast(DWORD)&AudioCallback,0,CALLBACK_FUNCTION);
	if(hwo !is null){
		int i;
		for(i=0;i<2;i++){
			wh[i]=wh_template;
			wh[i].dwBufferLength=get_audio_buf_size();
			wh[i].lpData=cast(char*)get_audio_buf(i);
			wh[i].dwUser=i;
			AudioCallback(hwo,0,0,cast(DWORD)&wh[i],0);
		}
	}
	//mp3_test(fname);
	//play_mp3(fname);
	process_file(fname);
	set_silence();
	return 0;
}
int main(string[] args)
{
	int result=0;
	if(args.length<2)
		return result;
	char fname[256];
	string s=args[1];
	if(s.length>=256){
		printf("fname too long\n");
		return result;
	}
	fname[0..s.length]=s[0..s.length];
	fname[s.length]=0;
	test_d(fname.ptr);
	return result;		
}

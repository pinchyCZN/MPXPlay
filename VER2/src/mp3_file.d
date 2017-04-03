module mp3_file;
import core.stdc.stdio: FILE,SEEK_CUR,SEEK_SET,SEEK_END;

import minimp3;
import libc_map;
import intel_hda;


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
alias kbhit=__kbhit;

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

extern (C)
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
	int trys=0;
	DWORD tick;
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
			version(Windows){
				printf("\n");
			}
			if(trys==0){
				tick=get_tick_count();
			}else{
				DWORD delta=get_tick_count()-tick;
				if(get_msec(delta)>2500){
					break;
				}
			}
			trys++;
			if(trys>1000){
				break;
			}
			if(trys==0)
				memset(&mp3,0,mp3.sizeof);
			else if(trys>3)
			{
				int tmp=fbuf_size/2;
				if(buf_level>4){
					tmp=buf_level-4;
				}
				fseek(f,-tmp,SEEK_CUR);
				memset(&mp3,0,mp3.sizeof);
			}
			buf_level=0;
			if(!seek_nearest_frame(f))
				break;
			skip_tags(f,buf,buf_size,buf_level);
			if(0==fill_buffer(f,fbuf,fbuf_size,buf_level,0))
				break;
			version(Windows){
				printf("try %i\n",trys);
				printf("offset after fill=%08X\n",ftell(f));
			}
		}
		version(Windows){
			printf("offset=%08X    \r",ftell(f));
		}
	}
	if(!result)
		buf_level=0;

	return result;
}

int seek_initial_offset(FILE *f,int flen,int initial_offset)
{
	int result=false;
	if(initial_offset>0 && initial_offset<=0xFF){
		uint tmp=flen;
		tmp=tmp/0xFF;
		tmp=tmp*initial_offset;
		if(0 == fseek(f,tmp,SEEK_SET)){
			seek_nearest_frame(f);
			result=true;
		}
	}
	return result;
}

int save_file_offset(FILE *f,int flen)
{
	int result=false;
	__gshared static DWORD tick=0;
	DWORD delta;
	version(Windows){
		const DWORD max=1000;
	}else{
		const DWORD max=5000;
	}
	delta=get_tick_count()-tick;
	if(get_msec(delta)>5000){
		tick=get_tick_count();
		uint divisor=flen/0xE0;
		if(divisor!=0){
			uint tmp=ftell(f);
			tmp=tmp/divisor;
			if(tmp>0xFF)
				tmp=0xFF;
			import main: write_cmos,CMOS_VAL;
			result=write_cmos(CMOS_VAL.OFFSET,tmp);
		}
	}
	return result;
}
int seek_nearest_frame(FILE *f)
{
	__gshared static ubyte *buf=null;
	const int buf_size=0x4000;
	int result=false;
	uint current_offset;
	int read,count;
	if(buf is null)
		buf=cast(ubyte*)malloc(buf_size);
	if(buf is null)
		return result;
	version(Windows){
		printf("sn =%08X\n",ftell(f));
	}
	for(count=0;count<64;count++){
		fseek(f,-3,SEEK_CUR);
		current_offset=ftell(f);
		read=fread(buf,1,buf_size,f);
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
											result=true;
									}
								}
							}
						}
					}
				}
				if(result){
					fseek(f,current_offset+i,SEEK_SET);
					goto exit;
				}
			}
		}
		else
			break;
	}
exit:
	version(Windows){
		printf("sn2=%08X\n",ftell(f));
	}
	return result;
}
int seek_mp3(FILE *f,int flen,int dir)
{
	int result=false;
	int tmp=flen/36;
	if(tmp!=0){
		if(dir<0){
			uint offset=ftell(f);
			tmp<<=1;
			if(tmp>offset)
				tmp=offset;
			tmp=-tmp;
		}
		if(0==fseek(f,tmp,SEEK_CUR)){
			seek_nearest_frame(f);
			result=true;
		}
	}
	return result;
}
extern (C)
int play_mp3(const char *fname,int initial_offset)
{
	int result=false;
	FILE *f;
	int flen;
	f=fopen(fname,"rb");
	if(f is null){
		printf("unable to open file:%s\n",fname);
		return result;
	}
	import main:get_flen;
	flen=get_flen(f);
	seek_initial_offset(f,flen,initial_offset);

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
			//printf("len=%06i %06i\n",len,buf_level);
		}
		else
			break;
		
		save_file_offset(f,flen);

		DWORD delta=get_tick_count()-tick;
		if(get_msec(delta)>200){
			tick=get_tick_count();
			int vkey,ext;
			vkey=dos_get_key(&ext);
			switch(vkey){
			case VK_BACKSPACE:
			case VK_4:
				if(seek_mp3(f,flen,-1)){
					fill_audio_buf(null,null,0,__tmp,0);
					buf_level=0;
				}
				tick>>=2;
				break;
			case VK_6:
				if(seek_mp3(f,flen,1)){
					fill_audio_buf(null,null,0,__tmp,0);
					buf_level=0;
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
		}
	}
	memset(buf,0,buf_size);
	
exit:
	fclose(f);
	return result;
}

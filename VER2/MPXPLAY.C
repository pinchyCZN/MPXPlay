#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <i86.h>

char DOS4GOPTIONS[] = "dos4g=StartupBanner:OFF\n";	// for DOS4G v2.xx

__declspec(__cdecl) FILE* _fopen(const char *name, const char *param)
{
	return fopen(name,param);
}
__declspec(__cdecl) int _fclose(FILE *f)
{
	return fclose(f);
}
__declspec(__cdecl) int _fread(void *buf, int size, int rec, FILE *f)
{
	return fread(buf,size,rec,f);
}
__declspec(__cdecl) int _fwrite(const void *buf, int size, int rec, FILE *f)
{
	return fwrite(buf,size,rec,f);
}
__declspec(__cdecl) int _fseek(FILE* stream,long offset,int whence)
{
	return fseek(stream,offset,whence);
}
__declspec(__cdecl) unsigned int _ftell(FILE* stream)
{
	return ftell(stream);
}
__declspec(__cdecl) void* _memset(void *buf, int val, int size)
{
	return memset(buf,val,size);
}
__declspec(__cdecl) void* _memcpy(void *s1,const void *s2,int size)
{
	return memcpy(s1,s2,size);
}
__declspec(__cdecl) void* _memmove(void *s1, const void *s2, int size)
{
	return memmove(s1,s2,size);
}

__declspec(__cdecl) void* _malloc(int size)
{
	return malloc(size);
}
__declspec(__cdecl) void* _realloc(void *buf,int size)
{
	return realloc(buf,size);
}
__declspec(__cdecl) void _free(void *buf)
{
	free(buf);
}

__declspec(__cdecl) double _pow(double a,double b)
{
	return pow(a,b);
}
__declspec(__cdecl) double _sin(double a)
{
	return sin(a);
}
__declspec(__cdecl) double _cos(double a)
{
	return cos(a);
}
__declspec(__cdecl) double _tan(double a)
{
	return tan(a);
}
__declspec(__cdecl) double _sqrt(double a)
{
	return sqrt(a);
}
__declspec(__cdecl) double _frexp(double a,int *e)
{
	return frexp(a,e);
}
__declspec(__cdecl) unsigned int _clock()
{
	return clock();
}
__declspec(__cdecl) int _getch()
{
	return getch();
}
__declspec(__cdecl) int _kbhit()
{
	return kbhit();
}
__declspec(__cdecl) int get_key(int *extended);


__declspec(__cdecl) int dos_get_key(int *extended)
{
	union REGPACK r={0};
	int key;
	r.h.ah=1;
	intr(0x16,&r);
	if(r.w.flags&INTR_ZF)
		return 0;
	memset(&r,0,sizeof(r));
	r.h.ah=0;
	intr(0x16,&r);
	key=r.h.al;
	*extended=0;
	if(key==0){
		memset(&r,0,sizeof(r));
		r.h.ah=0;
		intr(0x16,&r);
		key=r.h.al;
		*extended=1;
	}
	return key;
}


__declspec(__cdecl) int _printf(const char* fmt,...)
{
	int result;
	va_list args;
	va_start(args,fmt);
	result=vprintf(fmt,args);
	va_end(args);
	return result;
}
__declspec(__cdecl) int _fltused=0;
__declspec(__cdecl) int D15TypeInfo_Struct6__vtblZ=0;
__declspec(__cdecl) int D7minimp37__arrayZ()
{
	printf("array error\n");
}


__declspec(__cdecl) int test_d(const char *fname);

int test(const char *fname)
{
	int i;
	i=test_d(fname);
	printf("D result=%i\n",i);
}

int play_file(char *fname)
{
	FILE *f;
	f=fopen(fname,"rb");
	if(f!=0){
		char *buf;
		int i,size;
		int buf_size;
		buf_size=get_buf_size();
		buf=malloc(buf_size);
		if(0==buf)
			return 0;
		fseek(f,0,SEEK_END);
		size=ftell(f);
		fseek(f,0,SEEK_SET);
		for(i=0;i<size;i+=buf_size){
			int r;
			r=fread(buf,1,buf_size,f);
			if(r>0){
				if(r>buf_size)
					r=buf_size;
				play_wav_buf(buf,r);
			}
			if(r<buf_size)
				break;
			{
				static int vol=0x1f;
				int ext;
				int key=dos_get_key(&ext);
				if(key==0x1b)
					break;
				if(key==0x2d)
					vol++;
				else if(key==0x2b)
					vol--;
				if(key!=0){
					printf("vol=0x%03X\n",vol);
					set_volume(vol);
				}
				key=get_key(&ext);
				if(0x1b==key)
					break;
			}
		}
		fclose(f);
		if(0!=buf)
			free(buf);
	}
	return 0;
}

__declspec(__cdecl) int play_mp3(const char *fname);

int main(int argc,char **argv)
{
	int i;
	int result=0;
	printf("args:\n");
	for(i=1;i<argc;i++){
		printf("%s\n",argv[i]);
	}
	if(argc>1){
		char *fname=argv[1];
		audio_setup();
		//play_file(fname);
		play_mp3(fname);
		set_silence();
	}
	return result;
}
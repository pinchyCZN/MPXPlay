#include <stdio.h>
#include <string.h>
#include <stdarg.h>

char DOS4GOPTIONS[] = "dos4g=StartupBanner:OFF\n";	// for DOS4G v2.xx

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
		}
		fclose(f);
		if(0!=buf)
			free(buf);
	}
	return 0;
}
__declspec(__cdecl) int test_d(int);

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
__declspec(__cdecl) int _tls_index=0;
__declspec(__cdecl) int _tls_array=0;
__declspec(__cdecl) int D15TypeInfo_Struct6__vtblZ=0;
__declspec(__cdecl) int D7minimp37__arrayZ=0;

int test()
{
	int i=55;
	i=test_d(i);
	printf("D result=%i\n",i);
}

int main(int argc,char **argv)
{
	int i;
	int result=0;
	printf("args:\n");
	for(i=1;i<argc;i++){
		printf("%s\n",argv[i]);
	}
	test();
	return 0;
	if(argc>1){
		char *fname=argv[1];
		audio_setup();
		play_file(fname);
	}
	return result;
}
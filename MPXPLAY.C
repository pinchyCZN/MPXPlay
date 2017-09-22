#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <i86.h>
#include <math.h>
#include <time.h>
#include <conio.h>

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
__declspec(__cdecl) unsigned int _inp(int port)
{
	return inp(port);
}
__declspec(__cdecl) unsigned int _outp(int port,int val)
{
	return outp(port,val);
}
__declspec(__cdecl) unsigned int _clock()
{
	return clock();
}
__declspec(__cdecl) int __getch()
{
	return getch();
}
__declspec(__cdecl) int __kbhit()
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
__declspec(__cdecl) int dos_put_key(int key)
{
	return ungetch(key);
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
__declspec(__cdecl) int _vprintf(const char* fmt,va_list list)
{
	return vprintf(fmt,list);
}
__declspec(__cdecl) int _int386(int cmd,union REGS *r,union REGS *s)
{
	return int386(cmd,r,s);
}
__declspec(__cdecl) int _strlen(const char *s)
{
	return strlen(s);
}
__declspec(__cdecl) char * _strncpy(char *dst,const char *src,int len)
{
	return strncpy(dst,src,len);
}
__declspec(__cdecl) int __stricmp(char *a,const char *b)
{
	return stricmp(a,b);
}
__declspec(__cdecl) int _fltused=0;
__declspec(__cdecl) int D13TypeInfo_Enum6__vtblZ=0;
__declspec(__cdecl) int D15TypeInfo_Struct6__vtblZ=0;
__declspec(__cdecl) int D10TypeInfo_i6__initZ;
__declspec(__cdecl) int _d_local_unwind2()
{
	return 0;
}
__declspec(__cdecl) int _except_list=0;
__declspec(__cdecl) int _d_framehandler()
{
	return 0;
}


__declspec(__cdecl) int play_mp3(const char *fname);
__declspec(__cdecl) int init_hda();
__declspec(__cdecl) int start_audio();
__declspec(__cdecl) int set_silence();

__declspec(__cdecl) int d_main(int argc,char **argv);

int main(int argc,char **argv)
{
	return d_main(argc,argv);
}

/*
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
		init_hda();
		start_audio();
		printf("audio setup done\n");
		
		//play_file(fname);
		play_mp3(fname);
		set_silence();
	}
	return result;
}
*/

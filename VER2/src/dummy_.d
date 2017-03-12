module dummy_;

import core.stdc.stdio;
import core.stdc.string;
import core.stdc.ctype;
import core.stdc.stdlib;
import core.stdc.stdarg;
import core.stdc.math;
import core.stdc.time;

extern (C):
@nogc:
nothrow:

FILE* _fopen(const char *name, const char *param)
{
	return fopen(name,param);
}
int _fclose(FILE *f)
{
	return fclose(f);
}
int _fread(void *buf, int size, int rec, FILE *f)
{
	return fread(buf,size,rec,f);
}
int _fwrite(const void *buf, int size, int rec, FILE *f)
{
	return fwrite(buf,size,rec,f);
}
int _fseek(FILE* stream,long offset,int whence)
{
	return fseek(stream,cast(int)offset,whence);
}
uint _ftell(FILE* stream)
{
	return ftell(stream);
}
void* _memset(void *buf, int val, int size)
{
	return memset(buf,val,size);
}
void* _memmove(void *s1, const void *s2, int size)
{
	return memmove(s1,s2,size);
}
void* _memcpy(void *s1, const void *s2, int size)
{
	return memcpy(s1,s2,size);
}


void* _malloc(int size)
{
	return malloc(size);
}
void* _realloc(void *ptr,int size)
{
	return realloc(ptr,size);
}
void _free(void *buf)
{
	free(buf);
}

int _printf(const char* fmt,...)
{
	va_list list;
	va_start(list,fmt);
	vprintf(fmt,list);
	return 0;
}
int _vprintf(const char* fmt,va_list list)
{
	return vprintf(fmt,list);
}

int _strlen(const char *s)
{
	return strlen(s);
}
char *_strncpy(char *dst,const char *src,int len)
{
	return strncpy(dst,src,len);
}
int __stricmp(char *s1,char *s2)
{
	while (*s2 != 0 && toupper(*s1) == toupper(*s2))
		s1++, s2++;
	return (toupper(*s1) - toupper(*s2));

}
double _sin(double x)
{
	return sin(x);
}
double _cos(double x)
{
	return cos(x);
}
double _tan(double x)
{
	return tan(x);
}
double _sqrt(double x)
{
	return sqrt(x);
}
double _frexp(double x,int *y)
{
	return frexp(x,y);
}
double _pow(double x,double y)
{
	return pow(x,y);
}

uint _clock()
{
	return clock();
}
enum CLOCKS_PER_SEC=1000;

extern (C) @nogc nothrow int getch();
int _getch()
{
	return getch();
}
extern (C) @nogc nothrow int kbhit();
int _kbhit()
{
	return kbhit();
}
int __kbhit()
{
	return kbhit();
}
int __getch()
{
	return getch();
}
int dos_get_key(int *extended)
{
	int result=0;
	if(kbhit()){
		result=getch();
		if(result==0){
			result=getch();
			*extended=1;
		}
	}
	return result;
}
extern (C) @nogc nothrow int ungetch(int key);
int dos_put_key(int key)
{
	return ungetch(key);
}
static int cmos=0;
int _outp(int port,int val)
{
	int result=0;
	if(port==0x70){
		cmos=val;
		return result;
	}else{
		import core.sys.windows.windows;
		char key[80];
		char tmp[80];
		_snprintf(key.ptr,key.length,"OFFSET%i",cmos);
		_snprintf(tmp.ptr,tmp.length,"%i",val);
		WritePrivateProfileStringA("DATA".ptr,key.ptr,tmp.ptr,".\\MPXPLAY_.INI".ptr);
	}
	return result;
}
int _inp(int port)
{
	int result=0;
	import core.sys.windows.windows;
	char key[80];
	char tmp[80];
	_snprintf(key.ptr,key.length,"OFFSET%i",cmos);
	tmp[0]=0;
	GetPrivateProfileStringA("DATA".ptr,key.ptr,"".ptr,tmp.ptr,tmp.length,".\\MPXPLAY_.INI".ptr);
	result=atoi(tmp.ptr);
	return result;
}

module dummy_;

import core.stdc.stdio;
import core.stdc.string;
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

int dos_get_key(int *extended)
{
	return 0;
}


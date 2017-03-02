module dummy_;

import core.stdc.stdio;
import core.stdc.string;
import core.stdc.stdlib;
import core.stdc.stdarg;
import core.stdc.math;
import core.stdc.time;


extern (C) @nogc nothrow FILE* _fopen(const char *name, const char *param)
{
	return fopen(name,param);
}
extern (C) @nogc nothrow int _fclose(FILE *f)
{
	return fclose(f);
}
extern (C) @nogc nothrow int _fread(void *buf, int size, int rec, FILE *f)
{
	return fread(buf,size,rec,f);
}
extern (C) @nogc nothrow int _fwrite(const void *buf, int size, int rec, FILE *f)
{
	return fwrite(buf,size,rec,f);
}
extern (C) @nogc nothrow int _fseek(FILE* stream,long offset,int whence)
{
	return fseek(stream,cast(int)offset,whence);
}
extern (C) @nogc nothrow uint _ftell(FILE* stream)
{
	return ftell(stream);
}
extern (C) @nogc nothrow void* _memset(void *buf, int val, int size)
{
	return memset(buf,val,size);
}
extern (C) @nogc nothrow void* _memmove(void *s1, const void *s2, int size)
{
	return memmove(s1,s2,size);
}
extern (C) @nogc nothrow void* _memcpy(void *s1, const void *s2, int size)
{
	return memcpy(s1,s2,size);
}


extern (C) @nogc nothrow void* _malloc(int size)
{
	return malloc(size);
}
extern (C) @nogc nothrow void* _realloc(void *ptr,int size)
{
	return realloc(ptr,size);
}
extern (C) @nogc nothrow void _free(void *buf)
{
	free(buf);
}

extern (C) @nogc nothrow int _printf(const char* fmt,...)
{
	va_list list;
	va_start(list,fmt);
	vprintf(fmt,list);
	return 0;
}

extern (C) @nogc nothrow double _sin(double x)
{
	return sin(x);
}
extern (C) @nogc nothrow double _cos(double x)
{
	return cos(x);
}
extern (C) @nogc nothrow double _tan(double x)
{
	return tan(x);
}
extern (C) @nogc nothrow double _sqrt(double x)
{
	return sqrt(x);
}
extern (C) @nogc nothrow double _frexp(double x,int *y)
{
	return frexp(x,y);
}
extern (C) @nogc nothrow double _pow(double x,double y)
{
	return pow(x,y);
}

extern (C) @nogc nothrow uint _clock()
{
	return clock();
}
enum CLOCKS_PER_SEC=1000;

extern (C) @nogc nothrow int _getch()
{
//	return getch();
	return 0;
}
extern (C) @nogc nothrow int _kbhit()
{
//	return kbhit();
	return 0;
}

extern (C) @nogc nothrow int dos_get_key(int *extended)
{
	return 0;
}


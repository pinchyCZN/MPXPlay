import std.stdio;
import std.file;
import std.string;
import std.conv;


import minimp3;

@nogc:
nothrow:

extern (C){
	void * fopen(const char *, const char *);
	int fread(void *,int,int,void *);
}
void *f;

	auto reader = delegate (void[] buf) {
		int count;
		int size;
		void *ptr;
		size=buf.length;
		ptr=buf.ptr;
		count=fread(ptr,1,size,f);
		return count;
	};

extern(C)

int test_d(const char *fname)
{

	f=fopen(fname,"rb");

	auto mp3 = MP3DecoderNoGC(reader);

	if (!mp3.valid) {
		return 0;
	}

	while (mp3.valid) {
		mp3.decodeNextFrame(reader);
	}
	return 0;
}

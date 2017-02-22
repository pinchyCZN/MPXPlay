import std.stdio;
import std.file;
import std.string;
import std.conv;
import std.array;
import std.algorithm;
import core.stdc.stdlib;
import core.stdc.stdio;
import core.stdc.string;
import minimp3;


extern (C) int getch();

struct PROMPT{
	int a,b;
	string desc;
};
PROMPT prompts[]=[
	{0,0,"No prompts"},
	{1,0,"Odometer DriverID"},
	{1,1,"Odometer VehicleID"},
	{1,2,"Odometer"},
	{1,3,"DriverID VehicleID"},
	{1,4,"DriverID"},
	{1,5,"VehicleID"},
	{1,6,"DriverID JobNumber"},
	{1,7,"VehicleID JobNumber"},
	{1,8,"Odometer VehicleID DriverID"},
	{1,9,"Odometer DriverID JobNumber"},
	{2,0,"Odometer VehicleID JobNumber"},
	{2,1,"Odometer UserID JobNumber"},
	{2,2,"Odometer DriverID Data"},
	{2,3,"Odometer VehicleID Data"},
	{2,4,"Data DriverID JobNumber"},
	{2,5,"Data VehicleID JobNumber"},
	{2,6,"UserID"},
	{2,7,"Odometer UserID"},
	{2,8,"Odometer DriverID UserID"},
	{2,9,"Odometer VehicleID UserID"},
	{3,0,"Odometer UserID Data"},
	{3,1,"Odometer Data UserID"},
	{3,2,"UserID JobNumber"},
	{3,3,"VehicleID UserID"},
	{3,4,"DriverID UserID"},
	{3,5,"DriverID Department"},
	{3,6,"UserID Department"},
	{3,7,"VehicleID Department"},
	{3,8,"Odometer DriverID Department"},
	{3,9,"Odometer UserID Department"},
	{4,0,"Odometer VehicleID Department"},
	{4,1,"Department"},
	{4,2,"Data UserID Department"},
	{4,3,"Data VehicleID Department"},
	{4,4,"Data DriverID Department"},
	{4,5,"Data DriverID UserID"},
	{4,6,"Data UserID DriverLicenseNumber"},
	{4,7,"Data VehicleID DriverLicenseNumber"},
	{4,8,"Data"},
	{4,9,"DriverID Data"},
	{5,0,"UserID Data"},
	{5,1,"VehicleID Data"},
];
int get_prompt_str(int a,int b,ref string desc)
{
	int result=false;
	foreach(p;prompts){
		if(p.a==a && p.b==b){
			desc=p.desc;
			result=true;
			break;
		}
	}
	return result;
}
nothrow:
@nogc:

int fill_buffer(FILE *f,ubyte *buf,int buf_size,int buf_level,int forward)
{
	int result=0;
	int offset;
	offset=forward;
	if(buf_level>buf_size)
		buf_level=buf_size;
	if(offset>buf_level){
		offset=offset-buf_level;
		if(0==fseek(f,offset,SEEK_CUR)){
			result=fread(buf,1,buf_size,f);
		}
	}else{
		int len;
		len=buf_level-offset;
		memmove(buf,buf+offset,len);
	}
	
	return result;
}
int skip_tags(FILE *f,ubyte *buf,int buf_size,ref int data_len)
{
	int result=false;
	if(data_len>=10){
		if(buf[0]=='I' && buf[1]=='D' && buf[2]=='3' && buf[3]!=0xFF && buf[4]!=0xFF
			&& ((buf[6]|buf[7]|buf[8]|buf[9])&0x80)==0){
			uint size=(buf[9]|(buf[8]<<7)|(buf[7]<<14)|(buf[6]<<21))+10;
			if(size>0){
				if(data_len>buf_size)
					data_len=buf_size;
				if(size>data_len){
					int offset=size-data_len;
					fseek(f,offset,SEEK_CUR);
					data_len=fread(buf,1,buf_size,f);
					if(data_len>0)
						result=true;
				}else{
					int len,offset;
					len=data_len-size;
					if(len>buf_size)
						len=buf_size;
					memmove(buf,buf+size,len);
					offset=buf_size-len;
					data_len=fread(buf+offset,1,len,f);
					if(data_len>0)
						result=true;
				}
			}
		}
	}
	return result;
}


int main(string[] args)
{
	if(args.length<2)
		return 0;
	const char *fname=args[1].ptr;
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
	FILE *fout;
	fout=fopen("1.wav","rb");
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
		data_size=fread(fbuf,1,fbuf_len,f);
		if(data_size<=0)
			break;
		skip_tags(f,fbuf,fbuf_len,data_size);
		int offset=data_size;
		while(1){
			int consumed=0;
			int res=mp3_decode_frame(&mp3,buffer,consumed,fbuf,fbuf_len);
			if(res>=0){
				if(fout !is null)
					fwrite(buffer,1,data_size,fout);
				int len;
				offset=mp3.frame_size;
				if(offset>fbuf_len)
					offset=fbuf_len;
				else if(offset<0)
					offset=0;
				len=fbuf_len-offset;
				memmove(fbuf,fbuf+offset,len);
				offset-=mp3.frame_size;
				if(offset<=0){
					offset=offset;
					break;
				}
			}else{
				break;
			}
		}
	}
	printf("done\n");
	if(fout !is null)
		fclose(fout);
	fclose(f);
	return 0;
}
/*
int main(string[] argv)
{
	File f,fout;
	f=File("c:\\temp\\wex.txt","r");

	string swipe[];
	int maxlen=0;
	while(!f.eof()){
		string line=strip(f.readln());
		string words[];
		if(line.length==0)
			continue;
		words=split(line,",");
		if(words.length>=2){
			string left,right;
			string track[];
			left=words[0];
			right=words[1];
			track=split(right,"=");
			if(track.length>=2){
				string act,id,data;
				act=track[0];
				id=act[act.length-4..act.length];
				data=track[1];
				
				//swipe~="WEX "~id~" "~left[4..left.length]~","~right;
				//5th=ptable key
				//last char before ? POS prompt
				int a,b;
				if(data.length>5){
					string tmp,desc;
					a=to!int(data[4]-'0');
					b=to!int(data[data.length-2]-'0');
					get_prompt_str(a,b,desc);
					tmp="WEX "~id~" "~desc;
					if(tmp.length>maxlen)
						maxlen=tmp.length;
					//tmp="WEX "~desc~","~act~"="~data;
					tmp~=","~act~"="~data;
					swipe~=tmp;
					writeln(tmp);
				}
				
			}
		}
	}
	f.close();
	sort!("toUpper(a) < toUpper(b)")(swipe);
	fout=File("c:\\temp\\out.txt","w");
	foreach(s;swipe){
		string words[];
		string desc,data;
		words=split(s,",");
		if(words.length>=2){
			desc=words[0];
			data=words[1];
			if(desc.length<maxlen){
				desc=desc~replicate(" ",maxlen-desc.length);
			}
			s=desc~","~data;
		}
		writeln(s);
		fout.writefln(s);

	}
	fout.close();
	
	writeln("done");
	getch();
    return 0;
}
*/

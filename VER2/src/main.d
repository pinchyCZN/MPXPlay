module main;

import core.stdc.stdio: FILE,SEEK_SET,SEEK_END;
import libc_map;
import intel_hda;
import mp3_file;

alias strncpy=_strncpy;
alias strlen=_strlen;
enum MAX_PATH=256;

extern (C):
@nogc:
nothrow:
int tolower(char a)
{
	int result=a;
	if(a>='A' && a<='Z')
		a+=0x20;
	return result;
}
char * strstri(const char *str,const char *substr)
{
	int i,j,k;
	for(i=0;str[i];i++)
		for(j=i,k=0;tolower(str[j])==tolower(substr[k]);j++,k++)
			if(!substr[k+1])
				return cast(char *)(str+i);
	return null;
}

int extract_fname(const char *fpath,char *fname,int fname_len)
{
	int i,len,result;
	len=strlen(fpath);
	for(i=len-1;i>0;i--){
		ubyte a=fpath[i];
		if(a=='\\' || a=='/'){
			i++;
			break;
		}
	}
	strncpy(fname,fpath+i,fname_len);
	if(fname_len>0)
		fname[fname_len-1]=0;
	return 0;
}
int extract_ext(const char *fname,char *ext,int ext_len)
{
	int i,len,result;
	len=strlen(fname);
	for(i=len-1;i>0;i--){
		ubyte a=fname[i];
		if(a=='.' || a=='\\' || a=='/'){
			i++;
			break;
		}
	}
	strncpy(ext,fname+i,ext_len);
	if(ext_len>0)
		ext[ext_len-1]=0;
	return 0;
}
int get_flen(FILE *f)
{
	uint offset,len;
	offset=ftell(f);
	fseek(f,SEEK_END,0);
	len=ftell(f);
	fseek(f,SEEK_SET,offset);
	return len;
}
int get_line_count(const char *buf,int buf_size)
{
	int i,result=0;
	for(i=0;i<buf_size;i++){
		char a=buf[i];
		if(a=='\n')
			result++;
	}
	return result;
}
enum SEEK_TYPE {NEWLINE,ALPHANUMERIC}
int seek_char_type(const char *buf,uint buf_size,SEEK_TYPE type,ref uint offset,int dir)
{
	int result=false;
	while(1){
		if(offset>=buf_size){
			if(buf_size!=0)
				offset=buf_size-1;
			else
				offset=0;
			if(dir>=0)
				break;
		}
		else if(offset==0){
			offset=0;
			if(dir<0)
				break;
		}
		ubyte tmp=buf[offset];
		if(type==SEEK_TYPE.NEWLINE){
			if(tmp=='\n'){
				result=true;
				break;
			}
		}else{
			if(tmp>' '){
				result=true;
				break;
			}
		}
		if(dir>=0)
			offset++;
		else
			offset--;
	}
	return result;
}
int seek_line(const char *buf,uint buf_size,int line,ref uint offset)
{
	int result=false;
	int count=0;
	int found_start=false;
	while(1){
		ubyte a;
		if(offset>=buf_size){
			if(buf_size!=0)
				offset=buf_size-1;
			else
				offset=0;
			if(line>=0)
				break;
		}
		else if(offset==0){
			offset=0;
			if(line<0)
				break;
		}
		int dir=1;
		if(line==0){
			if(seek_char_type(buf,buf_size,SEEK_TYPE.ALPHANUMERIC,offset,dir))
				result=true;
			break;
		}
		else if(line>0){
			dir=1;
			if(seek_char_type(buf,buf_size,SEEK_TYPE.NEWLINE,offset,dir)){
				if(seek_char_type(buf,buf_size,SEEK_TYPE.ALPHANUMERIC,offset,dir)){
					count+=dir;
					if(count==line){
						result=true;
						break;
					}
				}else{
					break;
				}
			}else{
				break;
			}
		}else{ //reverse
			dir=-1;
			if(seek_char_type(buf,buf_size,SEEK_TYPE.NEWLINE,offset,dir)){
				if(seek_char_type(buf,buf_size,SEEK_TYPE.ALPHANUMERIC,offset,dir)){
					seek_char_type(buf,buf_size,SEEK_TYPE.NEWLINE,offset,dir);
					seek_char_type(buf,buf_size,SEEK_TYPE.ALPHANUMERIC,offset,1);
					count+=dir;
					if(count==line){
						result=true;
						break;
					}
				}else{
					break;
				}
			}else{
				break;
			}
		}
	}
	return result;
}
int extract_line(const char *buf,char *line,int line_size)
{
	int i;
	for(i=0;i<line_size;i++){
		ubyte a=buf[i];
		if(a<' '){
			break;
		}
		line[i]=a;
	}
	if(i<line_size)
		line[i]=0;
	if(line_size>0)
		line[line_size-1]=0;
	return i;
}
int process_playlist(const char *fname)
{
	FILE *f;
	char *playlist;
	uint len;
	f=fopen(fname,"rb");
	if(f is null){
		printf("unable to open file:%s\n",fname);
		return 0;
	}
	len=get_flen(f)+1;
	playlist=cast(char*)malloc(len);
	if(playlist !is null){
		fread(playlist,1,len,f);
		playlist[len-1]=0;
	}
	fclose(f);
	if(playlist is null){
		printf("unable to allocate mem for playlist\n");
		return 0;
	}
	uint offset=0,line=0;
	while(1){
		seek_line(playlist,len,line,offset);
		char current_line[256];
		extract_line(playlist+offset,current_line.ptr,current_line.length);
		
		
	}
}
int process_file(const char *fname)
{
	char tmp[MAX_PATH];
	extract_ext(fname,tmp.ptr,tmp.length);
	if(strstri(tmp.ptr,"mp3".ptr)){
		printf("playing mp3 file:%s\n",fname);
		play_mp3(fname);		
	}else if(strstri(tmp.ptr,"txt".ptr)){
		process_playlist(fname);
	}
	return 0;
}

int d_main(int argc,char **argv)
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
		
		process_file(fname);
		//play_mp3(fname);
		set_silence();
	}
	return result;
}

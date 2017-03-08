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
int seek_char(const char *buf,uint buf_size,char a,ref int offset,int dir)
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
		else if(offset<=0){
			offset=0;
			if(dir<0)
				break;
		}
		char tmp=buf[offset];
		if(tmp==a){
			result=true;
			break;
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
			break;
		}
		else if(offset<=0){
			offset=0;
		}
		if(count==line)
			break;
		a=buf[offset];
		if(a=='\n'){
			
		}
	}
	return 0;
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

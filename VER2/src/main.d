module main;

import core.stdc.stdio: FILE,SEEK_SET,SEEK_END;
import libc_map;
import intel_hda;
import mp3_file;

alias strncpy=_strncpy;
alias strlen=_strlen;
alias stricmp=__stricmp;
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
	fseek(f,0,SEEK_END);
	len=ftell(f);
	fseek(f,offset,SEEK_SET);
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
enum SEEK_TYPE {NONALPHANUMERIC,ALPHANUMERIC}
int seek_char_type(const char *buf,uint buf_size,SEEK_TYPE type,ref uint offset,int dir)
{
	int result=false;
	uint orig_offset=offset;
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
		if(type==SEEK_TYPE.NONALPHANUMERIC){
			if(tmp<' '){
				result=true;
				break;
			}
		}else{
			if(tmp>=' '){
				result=true;
				break;
			}
		}
		if(dir>=0)
			offset++;
		else
			offset--;
	}
	if(!result)
		offset=orig_offset;
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
			uint orig_offset=offset;
			dir=1;
			if(seek_char_type(buf,buf_size,SEEK_TYPE.NONALPHANUMERIC,offset,dir)){
				if(seek_char_type(buf,buf_size,SEEK_TYPE.ALPHANUMERIC,offset,dir)){
					count+=dir;
					if(count==line){
						result=true;
						break;
					}
				}else{
					offset=orig_offset;
					break;
				}
			}else{
				break;
			}
		}else{ //reverse
			dir=-1;
			int have_char=false;
			if(!seek_char_type(buf,buf_size,SEEK_TYPE.NONALPHANUMERIC,offset,dir))
				break;
			if(seek_char_type(buf,buf_size,SEEK_TYPE.ALPHANUMERIC,offset,dir))
				have_char=true;
			if(!seek_char_type(buf,buf_size,SEEK_TYPE.NONALPHANUMERIC,offset,dir)){
				if(have_char)
					offset=0;
			}
			if(seek_char_type(buf,buf_size,SEEK_TYPE.ALPHANUMERIC,offset,1)){
				count+=dir;
				if(count==line){
					result=true;
					break;
				}
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
int extract_folder(const char *path,char *folder,int folder_size)
{
	int result=false;
	int i,len;
	int start=0,end=0;
	len=strlen(path);
	for(i=len-1;i>=0;i--){
		char a=path[i];
		if(a=='\\' || a=='/'){
			if(end==0)
				end=i;
			else if(start==0){
				start=i;
				break;
			}
		}
	}
	if(folder_size>0)
		folder[0]=0;
	if(start!=end){
		result=true;
		len=end-start;
		if(len>folder_size)
			len=folder_size;
		strncpy(folder,path+start,len);
		if(len>0 && len<folder_size)
			folder[len]=0;
		if(folder_size>0)
			folder[folder_size-1]=0;
	}
	return result;
}
int seek_next_folder(const char *buf,uint buf_size,ref uint offset,int dir)
{
	int result=false;
	char line[256];
	char *ptr;
	if(offset>=buf_size){
		if(dir>=0)
			return result;
		if(buf_size>0)
			offset=buf_size-1;
		else
			offset=0;
	}
	char folder[256];
	ptr=cast(char*)buf+offset;
	extract_line(ptr,line.ptr,line.length);
	extract_folder(line.ptr,folder.ptr,folder.length);
	printf("current folder:%s\n",folder.ptr);
	if(dir>=0)
		dir=1;
	else
		dir=-1;
	int rounds=0;
	if(dir>0){
		while(1){
			char tmp[256];
			if(seek_line(buf,buf_size,dir,offset)){
next:
				ptr=cast(char*)buf+offset;
				extract_line(ptr,line.ptr,line.length);
				extract_folder(line.ptr,tmp.ptr,tmp.length);
				if(0==stricmp(folder.ptr,tmp.ptr))
					continue;
				else{
					result=true;
					break;
				}
			}else{
				if(rounds>0)
					break;
				rounds++;
				offset=0;
				goto next;
			}
		}
	}else{
		int rcount=0;
		while(1){
			char tmp[256];
			if(seek_line(buf,buf_size,dir,offset)){
				ptr=cast(char*)buf+offset;
				extract_line(ptr,line.ptr,line.length);
				extract_folder(line.ptr,tmp.ptr,tmp.length);
				if(0==stricmp(folder.ptr,tmp.ptr))
					continue;
				else{
					strncpy(folder.ptr,tmp.ptr,folder.length);
					if(folder.length>0)
						folder[folder.length-1]=0;
					if(rcount==0){
						rcount++;
						continue;
					}
					seek_line(buf,buf_size,1,offset);
					result=true;
					break;
				}
			}else{
				if(rcount>0)
					break;
				if(rounds>0)
					break;
				rounds++;
				offset=buf_size;
			}
		}
	}

	return result;
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
	uint offset=0;
	int dir=0;
	while(1){
		if(!seek_line(playlist,len,dir,offset)){
			if(dir<0){
				offset=len;
				seek_line(playlist,len,dir,offset);
			}else if(dir>0){
				offset=0;
			}
		}
loop:
		char current_line[256];
		current_line[0]=0;
		extract_line(playlist+offset,current_line.ptr,current_line.length);
		printf("%08X %s\n",offset,current_line.ptr);
		play_mp3(current_line.ptr);
		dir=1;
		int vkey,ext;
		vkey=dos_get_key(&ext);
		switch(vkey){
		case VK_FWDSLASH:
			seek_next_folder(playlist,len,offset,-1);
			goto loop;
			break;
		case VK_ASTERISK:
			seek_next_folder(playlist,len,offset,-1);
			goto loop;
			break;
		case VK_PLUS:
			dir=1;
			break;
		case VK_MINUS:
			dir=-1;
			break;
		case VK_ENTER:
			dir=0;
			break;
		case VK_0:
			goto exit;
			break;
		default:
			break;
		}
	}
exit:
	printf("done\n");
	return 0;
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

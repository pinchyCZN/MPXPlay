#include <stdio.h>
#include <string.h>

#define FALSE 0
#define TRUE 1

int get_fname_ext(const char *str,const char **out)
{
	int result=FALSE;
	int len=strlen(str);
	int i;
	for(i=0;i<len;i++){
		char a=str[len-1-i];
		if('.'==a){
			*out=str+len-i;
			result=TRUE;
			break;
		}else if('\\'==a){
			break;
		}
	}
	return result;
}
char lower(char a)
{
	if(a>='A' && a<='Z'){
		a+=0x20;
	}
	return a;
}
const char* strstri(const char *haystack,const char *needle)
{
	const char *result=0;
	int index=0;
	int x=0;
	while(1){
		char a,b;
		a=haystack[index];
		b=needle[x];
		a=lower(a);
		b=lower(b);
		if(a==b){
			if(0==a){
				result=haystack+index-x;
			}
			x++;
		}else{
			if(0==b && index>0){
				result=haystack+index-x;
			}
			if(0==a)
				break;
			x=0;
		}
		index++;
		if(result)
			break;
	}
	return result;
}

int stricmp(const char *a,const char *b)
{
	int result=0;
	int index=0;
	while(1){
		char x,y;
		x=a[index];
		y=b[index];
		x=lower(x);
		y=lower(y);
		result=x-y;
		if(result!=0)
			break;
		if(0==x)
			break;
		else if(0==y)
			break;
		index++;
	}
	return result;
}

int play_mp3_file(const char *fname)
{
	int offset=0;
	int res;
	res=play_mp3(fname,offset);
	return res;
}
int play_flac_file(const char *fname)
{
	return 0;
}
int play_list(const char *fname)
{
	return 0;
}

int init_audio()
{
	int result=FALSE;
#ifdef _WIN32
	win32_setup_audio();
	result=TRUE;
#else
	if(init_hda()){
		start_audio();
		result=TRUE;
	}
#endif
	return result;
}
int stop_audio()
{
#ifdef _WIN32
#else
	set_silence();
#endif
	return TRUE;
}

int main(int argc,const char **argv)
{
	const char *arg1=0;
	char *fext=0;
	if(argc<=1){
		printf("no input file given\n");
		return -1;
	}
	arg1=argv[1];

	if(!get_fname_ext(arg1,&fext)){
		printf("unable to get file extension\n");
		return -1;
	}
	if(!init_audio()){
		printf("unable to init audio\n");
		return -1;
	}
	if(0==stricmp(fext,"mp3")){
		play_mp3_file(arg1);
	}else if(0==stricmp(fext,"flac")){
		play_flac_file(arg1);
	}else if(0==stricmp(fext,"txt")){
		play_list(arg1);
	}else{
		printf("unhandled file:%s\n",arg1);
	}
	return 0;
}
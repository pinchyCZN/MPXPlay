#include <stdio.h>
#include <string.h>
char DOS4GOPTIONS[] = "dos4g=StartupBanner:OFF\n";	// for DOS4G v2.xx

int play_wav_buf(unsigned char *buf,int len);

int play_file(char *fname)
{
	FILE *f;
	f=fopen(fname,"rb");
	if(f!=0){
		char buf[1024];
		int i,size;
		const int buf_size=sizeof(buf);
		fseek(f,0,SEEK_END);
		size=ftell(f);
		fseek(f,0,SEEK_SET);
		for(i=0;i<size;i+=buf_size){
			int r;
			r=fread(buf,1,buf_size,f);
			if(r>0){
				if(r>buf_size)
					r=buf_size;
				play_wav_buf(buf,r);
			}
			if(r<buf_size)
				break;
		}
		fclose(f);
	}
}
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
		play_file(fname);
	}
	return result;
}
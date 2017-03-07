module main;

import libc_map;
import intel_hda;
import mp3_file;

extern (C):
@nogc:
nothrow:

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
		
		//play_file(fname);
		play_mp3(fname);
		set_silence();
	}
	return result;
}

#include "dosstuff.h"
#include "au_cards.h"

mpxplay_audioout_info_s au_infos={0};

int play_wav_buf(unsigned char *buf,int len)
{
	return 0;
}

extern one_sndcard_info IHD_sndcard_info;
int detect_card()
{
	if(IHD_sndcard_info.card_detect(&au_infos))
		IHD_sndcard_info.card_info(&au_infos);
}

int test_sound()
{
	static short buf[1024];
	int i;
	const int bcount=sizeof(buf)/sizeof(short);
	const int bsize=sizeof(buf);
	printf("test sound\n");
	dump_card_data();
	IHD_sndcard_info.card_start(&au_infos);
	for(i=0;i<5;i++){
		int j;
		for(j=0;j<bcount;j++){
			buf[j]=rand();
		}
		IHD_sndcard_info.cardbuf_writedata(&au_infos,(char*)buf,bsize);
	}
	dump_card_data();

	printf("test sound exit\n");
}
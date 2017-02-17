#include "dosstuff.h"
#include "au_cards.h"
#include <math.h>

mpxplay_audioout_info_s au_infos={0};

int play_wav_buf(unsigned char *buf,int len)
{
	return 0;
}

/*
extern one_sndcard_info IHD_sndcard_info;
int detect_card()
{
	if(IHD_sndcard_info.card_detect(&au_infos))
		IHD_sndcard_info.card_info(&au_infos);
}
*/

int latency_size=0;
int volume=0;
/*
int init_audio(int samplerate)
{
	//Start HD audio
	if(!w_sound_device_init(28, samplerate/2))
	{
		return 0;
	}
	w_set_device_master_volume(1);
	latency_size = w_get_nominal_sample_rate() * 0.025;
	w_lock_mixing_buffer(latency_size);
	w_mixing_zero();
	w_unlock_mixing_buffer();
	return 0;
}
int end_audio()
{
	w_sound_device_exit();
	return 0;
}
int set_volume(int vol)
{
	volume=vol;
	w_set_device_master_volume(vol);
	return 0;
}
int write_audio(unsigned char *data,int len)
{
	int samples;
	
	do
	{
//		samples = w_get_buffer_size() - w_get_latency() - latency_size;
	}while(samples < len);
	printf("uuu\n");
	
	w_lock_mixing_buffer(len);
	w_mixing_stereo((short int *)data, len, volume, volume);
	printf("222\n");
	w_unlock_mixing_buffer();
	printf("333\n");

}

*/
int test_sound()
{
#define PI (3.141592653589793)
	short buf2[1024];
	short *buf;
	int i,j;
	int count;
	count=44100*2;
	//count=sizeof(buf2)/sizeof(short);
	
	buf=malloc(count*sizeof(short));
	if(0==buf){
		printf("failed to allocate buf\n");
		return 0;
	}
	printf("address=%08X\n",buf);
	printf("address2=%08X\n",buf2);
	for(i=0;i<count;i++){
		buf[i]=sin(i/PI*4000)*1000;
	}
	w_sound_device_init(28,44100);
//	init_hda();
//	play_data();
//	init_audio(88200);
//	set_volume(31);
//	printf("write audio\n");
//	write_audio((unsigned char*)buf,count/8);
//	end_audio();
	free(buf);
	printf("test sound done\n");
	return 0;
}


#include "dosstuff.h"
#include "au_cards.h"

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
int init_audio(int samplerate)
{
	//Start HD audio
	if(!w_sound_device_init(28, samplerate/2))
	{
		printf("%s\n",w_get_error_message());
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
		samples = w_get_buffer_size() - w_get_latency() - latency_size;
	}while(samples < len);
	
	w_lock_mixing_buffer(len);
	w_mixing_stereo((short int *)data, len, volume, volume);
	w_unlock_mixing_buffer();

}


int test_sound()
{
	short buf[1024];
	int i,j;
	int count;
	count=sizeof(buf)/sizeof(short);
	for(i=0;i<count;i++){
		buf[i]=rand();
	}
	init_audio(88200);
	return 0;
}

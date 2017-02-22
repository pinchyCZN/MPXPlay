#include "dosstuff.h"
#include "au_cards.h"
#include <math.h>

int audio_setup()
{
//	w_sound_device_init(28,44100);
	init_hda();
	start_audio();
	printf("audio setup done\n");
	return 0;
}


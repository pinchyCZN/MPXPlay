
int play_wav_buf(unsigned char *buf,int len)
{
	int i;
	for(i=0;i<len;i++){
		buf[i]++;
	}
	return 0;
}
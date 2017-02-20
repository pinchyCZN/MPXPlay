#include <stdio.h>
#include <stdlib.h>
#include <i86.h>
#include <conio.h>
#include <dpmi.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>

typedef unsigned char		BYTE;
typedef unsigned short		WORD;
typedef unsigned long		DWORD;
#define TRUE (1)
#define FALSE (0)
#define CLOCK_RATE 300000000

DWORD base_reg=0;
int device_found=0;

#define PCICMD		0x04
#define MSE 		0x02
#define BME			0x04

//HDA PCI CONFIG REGISTERS
#define HDBARL 		0x10
//HDA MEM MAP CONFIG REGISTERS
#define GCAP		0x00
#define GCTL		0x08
#define STATESTS	0x0E
#define HDA_IC		0x60
#define HDA_IR		0x64
#define HDA_IRS		0x68
#define OSD0CTL		0xA0
#define OSD0STS		0xA3
#define OSD0LPIB	0xA4
#define OSD0CBL		0xA8
#define OSD0LVI		0xAC
#define OSD0FMT		0xB2
#define OSD0BDPL	0xB8
#define OSD0BDPU	0xBC

//ctrl bits
#define SRST		0x01
#define RUN			0x02
#define BCIS		0x04

#define CRST 		0x01

#define ICB			0x01 //immediate command busy
#define IRV			0x02 //immediate result valid

#define SET_AMP_GAIN	0x003
#define SET_MUTE		0x0080
#define SET_LEFT_AMP	0x2000
#define SET_RIGHT_AMP	0x1000
#define SET_OUT_AMP		0x8000


void log_msg(const char *fmt,...)
{
	va_list arg;
	va_start(arg,fmt);
	vprintf(fmt,arg);
	va_end(arg);
}

DWORD get_tick_count()
{
	DWORD tick;
	_asm{
		rdtsc;
		mov tick,eax;
	}
	return tick;
}

void udelay(int usec)
{
	DWORD tick,delta;
	tick=get_tick_count();
	while(1){
		delta=get_tick_count()-tick;
		delta=delta/(CLOCK_RATE/(1000*1000));
		if(delta>usec)
			break;
	}
	return;
}
int get_msec(DWORD ticks)
{
	return (ticks/(CLOCK_RATE/1000));
}

int find_pci_device(WORD ven_id,WORD dev_id,BYTE *bus_num,BYTE *dev_num)
{
	union REGS r={0};
	r.w.ax=0xB102; //FIND PCI DEVICE
	r.w.cx=dev_id;
	r.w.dx=ven_id;
	r.w.si=0;
	int386(0x1a,&r,&r);
	if(r.w.cflag!=0){
		return FALSE;
	}
	*bus_num=r.h.bh;
	*dev_num=r.h.bl;
	return TRUE;
}
int read_config(BYTE bus_num,BYTE dev_num,int reg_num,DWORD *out,int size)
{
	int result=FALSE;
	union REGS r={0};
	int cmd;
	switch(size){
	case 1:
		cmd=0xB108;
		break;
	case 2:
		cmd=0xB109;
		break;
	default:
	case 4:
		cmd=0xB10A;
		break;
	}
	r.w.ax=cmd;
	r.h.bh=bus_num;
	r.h.bl=dev_num;
	r.w.di=reg_num;
	int386(0x1a,&r,&r);
	if(r.w.cflag!=0){
		return result;
	}
	result=TRUE;
	*out=r.x.ecx;
	return result;
}
int read_config_32(BYTE bus_num,BYTE dev_num,int reg_num,DWORD *out)
{
	int result=FALSE;
	result=read_config(bus_num,dev_num,reg_num,out,4);
	return result;
}
int read_config_16(BYTE bus_num,BYTE dev_num,int reg_num,WORD *out)
{
	int result=FALSE;
	DWORD _out=0;
	result=read_config(bus_num,dev_num,reg_num,&_out,2);
	*out=_out;
	return result;
}
int read_config_8(BYTE bus_num,BYTE dev_num,int reg_num,BYTE *out)
{
	int result;
	DWORD _out=0;
	result=read_config(bus_num,dev_num,reg_num,&_out,1);
	*out=_out;
	return result;
}
int write_config(BYTE bus_num,BYTE dev_num,int reg_num,DWORD data,int size)
{
	int result=FALSE;
	union REGS r={0};
	int cmd;
	switch(size){
	case 1:
		cmd=0xB10B;
		r.h.cl=data;
		break;
	case 2:
		cmd=0xB10C;
		r.w.cx=data;
		break;
	default:
	case 4:
		cmd=0xB10D;
		r.x.ecx=data;
		break;
	}
	r.w.ax=cmd;
	r.h.bh=bus_num;
	r.h.bl=dev_num;
	r.w.di=reg_num;
	int386(0x1a,&r,&r);
	if(r.w.cflag!=0){
		return result;
	}
	result=TRUE;
	return result;
}
int write_config_16(BYTE bus_num,BYTE dev_num,int reg_num,WORD data)
{
	int result;
	DWORD _data=data;
	result=write_config(bus_num,dev_num,reg_num,_data,2);
	return result;
}
int write_config_8(BYTE bus_num,BYTE dev_num,int reg_num,BYTE data)
{
	int result;
	DWORD _data=data;
	result=write_config(bus_num,dev_num,reg_num,_data,1);
	return result;
}

int log_mem(char *fmt,...)
{
	va_list arg;
	va_start(arg,fmt);
	vprintf(fmt,arg);
	va_end(arg);
}
static DWORD last_read=0,last_data=0;

int write_32(DWORD addr,DWORD data)
{
	DWORD *ptr=(DWORD *)addr;
	ptr[0]=data;
	return TRUE;
}
DWORD read_32(DWORD addr)
{
	DWORD result,*ptr=(DWORD *)addr;
	result=ptr[0];
	return result;
}
int write_16(DWORD addr,WORD data)
{
	WORD *ptr=(WORD*)addr;
	ptr[0]=data;
	return TRUE;
}
WORD read_16(DWORD addr)
{
	WORD result,*ptr=(WORD*)addr;
	result=ptr[0];
	return result;
}
int write_08(DWORD addr,BYTE data)
{
	BYTE *ptr=(BYTE*)addr;
	ptr[0]=data;
	return TRUE;
}
int read_08(DWORD addr)
{
	BYTE result,*ptr=(BYTE*)addr;
	result=ptr[0];
	return result;
}

int wait_reset()
{
	int result=TRUE;
	DWORD tick,delta;
	tick=get_tick_count();
	while((read_32(base_reg+GCTL)&1)==0){
		delta=get_tick_count()-tick;
		if(get_msec(delta)>500){
			log_msg("wait reset timeout\n");
			result=FALSE;
			break;
		}
	}
	return result;
}
int hda_stop()
{
	int tmp;
	tmp=read_08(base_reg+OSD0CTL);
	tmp&=~RUN;
	write_08(base_reg+OSD0CTL,tmp);
	while((read_16(base_reg+OSD0CTL)&RUN)!=0)
		;
	tmp=read_08(base_reg+OSD0CTL);
	tmp|=SRST;
	write_08(base_reg+OSD0CTL,tmp);
	/*
	if((read_16(base_reg+OSD0CTL)&SRST)!=0){
		DWORD tick,delta;
		tmp=read_08(base_reg+OSD0CTL);
		tmp|=SRST;
		write_08(base_reg+OSD0CTL,tmp);
		tmp=read_08(base_reg+OSD0CTL);
		tmp&=~SRST;
		write_08(base_reg+OSD0CTL,tmp);
	}
	*/
	{
		DWORD tick,delta;
		tick=get_tick_count();
		while((read_16(base_reg+OSD0CTL)&SRST)==0){
			delta=get_tick_count()-tick;
			if(get_msec(delta)>50){
				printf("stop timeout\n");
				break;
			}
		}
	}
	return 0;
}

int hda_run()
{
	int tmp;
	DWORD tick;
	tmp=read_08(base_reg+OSD0CTL);
	tmp|=RUN;
	write_08(base_reg+OSD0CTL,tmp);
	tick=get_tick_count();
	while((read_08(base_reg+OSD0CTL)&RUN)==0){
		DWORD delta=get_tick_count()-tick;
		if(get_msec(delta)>10){
			printf("--run timeout--\n");
			break;
		}
	}
	return 0;
}
int hda_send_codec_cmd(DWORD param)
{
	while((read_16(base_reg+HDA_IRS)&ICB)!=0)
		;
	write_32(base_reg+HDA_IC, param);
	write_16(base_reg+HDA_IRS,ICB|IRV);
	return 0;
}
int hda_get_codec_resp(DWORD *data)
{
	int result=TRUE;
	DWORD tmp,tick,delta;
	tick=get_tick_count();
	while((read_16(base_reg+HDA_IRS)&(ICB|IRV))!=IRV){
		delta=get_tick_count()-tick;
		if(get_msec(delta)>10){
			printf("codec resp timeout\n");
			result=FALSE;
			break;
		}
	}
	*data=read_32(base_reg+HDA_IR);
	return result;
}

int get_hda_param(int addr,int nodeid,int verbid,int payload)
{
	return ((addr<<28)|(nodeid<<20)|(verbid<<16)|payload);
}
int set_volume(int vol)
{
	DWORD tmp;
	if(vol>31)
		vol=31;
	else if(vol<0)
		vol=0;
	tmp=(SET_OUT_AMP|vol) & ~SET_MUTE;
	hda_send_codec_cmd(get_hda_param(0,0xE,SET_AMP_GAIN,SET_RIGHT_AMP|tmp));
	hda_send_codec_cmd(get_hda_param(0,0xE,SET_AMP_GAIN,SET_LEFT_AMP|tmp));
	return 0;
}
int reset_hda()
{
	DWORD tmp,tick,delta;
	tmp=read_32(base_reg+GCTL);
	tmp&=~CRST;
	write_32(base_reg+GCTL,tmp);

	tick=get_tick_count();
	while((read_32(base_reg+GCTL)&CRST)!=0){
		delta=get_tick_count()-tick;
		if(get_msec(delta)>10){
			printf("reset1 timeout\n");
			break;
		}
	}
	tmp=read_32(base_reg+GCTL);
	tmp|=CRST;
	write_32(base_reg+GCTL,tmp);

	tick=get_tick_count();
	while((read_32(base_reg+GCTL)&CRST)==0){
		delta=get_tick_count()-tick;
		if(get_msec(delta)>10){
			printf("reset2 timeout\n");
			break;
		}
	}
	tmp=read_32(base_reg+GCTL);
	if(tmp==0)
		printf("error controller not read\n");
	tmp=read_16(base_reg+STATESTS);
	if(tmp==0)
		printf("error no codec found after reset\n");
	tmp=read_16(base_reg+GCAP);
	tmp=tmp>>12;
	if(tmp==0)
		printf("error no output streams supported\n");
	

	write_08(base_reg+OSD0STS,BCIS);
	return TRUE;
}

int init_pci_access(BYTE bus_num,BYTE dev_num)
{
	WORD tmp16=0;
	tmp16=0;
	read_config_16(bus_num,dev_num,0x4,&tmp16);
	write_config_16(bus_num,dev_num,0x4,tmp16|(MSE|BME));
	return TRUE;
}

int init_hda()
{
	int result=FALSE;
	const WORD ven_id=0x17F3;
	const WORD dev_id=0x3010;
	BYTE bus_num=0,dev_num=0;
	if(!find_pci_device(ven_id,dev_id,&bus_num,&dev_num)){
		log_msg("ERROR finding device %04X:%04X\n",ven_id,dev_id);
		return result;
	}
	log_msg("Found vortex audio device at %i:%i\n",bus_num,dev_num);
	if(!read_config_32(bus_num,dev_num,HDBARL,&base_reg)){
		log_msg("ERROR getting base address\n");
		return result;
	}
	device_found=TRUE;
	log_msg("BASE ADDRESS:%08X\n",base_reg);
	init_pci_access(bus_num,dev_num);

	reset_hda();
	send_all_commands();
	return result;
}

int dump_address(int *ptr,int count)
{
	int i;
	for(i=0;i<count;i++){
		if((i%4)==0)
			printf("%03X:",((DWORD)&ptr[i])&0xFFF); //i*4);
		printf("%08X ",ptr[i]);
		if(i>0){
			if(((i+1)%4)==0)
				printf("\n");
		}
	}
	return 0;
}
int test_mem()
{	
	int count=0;
	int *ptr;
	DWORD tick,delta;
	while(1){
		ptr=0xFBFF0060;
		dump_address(ptr,8*4);
		tick=get_tick_count();
		while(1){
			delta=get_tick_count()-tick;
			if(get_msec(delta)>100)
				break;
		}
		count++;
		if(count>2)
			break;

	}
	return 0;
}


static int *memory_chunk=0;
static int *bdl_list=0;
static int *buffer1=0;
static int *buffer2=0;
const int buf_size=0x1000;
int current_buf=0;

int get_buf_size()
{
	return buf_size;
}
int start_audio()
{
	DWORD tmp;
	int result=FALSE;
	int i;
	const int bdl_entries=2;
	if(base_reg==0)
		return result;
	if(memory_chunk==0){
		DWORD tmp;
		memory_chunk=calloc(1,buf_size*16);
		if(memory_chunk==0){
			printf("unable to allocate memory\n");
			return result;
		}
		tmp=memory_chunk;
		tmp=(tmp+0x7F)&(-0x80);
		bdl_list=tmp;
		buffer1=tmp+buf_size;
		buffer2=tmp+buf_size*2;
	}
	printf("bdl_list=%08X\n",bdl_list);
	printf("buffer1=%08X\n",buffer1);
	for(i=0;i<bdl_entries;i++){
		tmp=buffer1;
		bdl_list[i*4]=tmp+i*buf_size;
		bdl_list[i*4+1]=0;
		bdl_list[i*4+2]=buf_size;
		bdl_list[i*4+3]=0;
	}

	hda_stop();
	tmp=read_08(base_reg+OSD0CTL);
	tmp&=~SRST;
	write_08(base_reg+OSD0CTL,tmp);
	{
		DWORD tick,delta;
		tick=get_tick_count();
		while((read_08(base_reg+OSD0CTL)&SRST)!=0){
			delta=get_tick_count()-tick;
			if(get_msec(delta)>1){
				printf("--srst timeout--\n");
				break;
			}
		}
	}

	tmp=read_32(base_reg+OSD0CTL);
	tmp&=0xff0fffff;
	tmp|=(1<<20);
	write_32(base_reg+OSD0CTL,tmp);
	write_32(base_reg+OSD0CBL,buf_size*bdl_entries);
	write_16(base_reg+OSD0LVI,bdl_entries-1);
	write_16(base_reg+OSD0FMT,(1<<14)|(1<<4)|(1)); //44.1khz 16bit 2 channels
	write_32(base_reg+OSD0BDPL,bdl_list);
	write_32(base_reg+OSD0BDPU,0);
	printf("running\n");
	hda_run();
}

int set_bit_rate(int rate)
{
	DWORD tmp;
	tmp=read_16(base_reg+OSD0FMT);
	if(44100==rate)
		tmp|=1<<14;
	else
		tmp&=~(1<<14);
	write_16(base_reg+OSD0FMT,tmp);
	return 0;
}

int codec_commands[]={
	0x000F0000,
	0x000F0004,
	0x001F0005,
	0x00170500,
	0x001F0012,
	0x001F000D,
	0x001F0004,
	0x002F0009,
	0x003F0009,
	0x004F0009,
	0x005F0009,
	0x006F0009,
	0x007F0009,
	0x008F0009,
	0x009F0009,
	0x00AF0009,
	0x00BF0009,
	0x00CF0009,
	0x00DF0009,
	0x00EF0009,
	0x00FF0009,
	0x010F0009,
	0x011F0009,
	0x012F0009,
	0x013F0009,
	0x014F0009,
	0x015F0009,
	0x016F0009,
	0x017F0009,
	0x018F0009,
	0x019F0009,
	0x01AF0009,
	0x01BF0009,
	0x01CF0009,
	0x01DF0009,
	0x01EF0009,
	0x01FF0009,
	0x020F0009,
	0x021F0009,
	0x022F0009,
	0x023F0009,
	0x024F0009,
	0x002F000B,
	0x002F000A,
	0x00270610,
	0x00220011,
	0x00270500,
	0x003F000B,
	0x003F000A,
	0x00370610,
	0x00320011,
	0x00370500,
	0x00BF000E,
	0x00BF0200,
	0x00BF0204,
	0x00CF000E,
	0x00CF0200,
	0x00C70100,
	0x00CF0012,
	0x00C39080, //vol
	0x00C3A080, //vol
	0x00CF000D,
	0x00C35000,
	0x00C36000,
	0x00DF000E,
	0x00DF0200,
	0x00D70100,
	0x00DF0012,
	0x00D39080, //vol
	0x00D3A080, //vol
	0x00DF000D,
	0x00D35000,
	0x00D36000,
	0x00EF000E,
	0x00EF0200,
	0x00E70100,
	0x00EF0012,
	0x00E39080, //vol
	0x00E3A080, //vol
	0x00EF000D,
	0x00E35000,
	0x00E36000,
	0x012F1C00,
	0x014F1C00,
	0x014F000C,
	0x014F0700,
	0x014707E0,
	0x014F0C00,
	0x01470C00,
	0x014F000E,
	0x014F0200,
	0x01470100,
	0x01470500,
	0x014F0012,
	0x0143907F, //main vol
	0x0143A07F, //main vol
	0x014F000D,
	0x01435003,
	0x01436003,
	0x015F1C00,
	0x016F1C00,
	0x018F1C00,
	0x019F1C00,
	0x01AF1C00,
	0x01BF1C00,
	0x01CF1C00,
	0x01DF1C00,
	0x022F000E,
	0x022F0200,
	0x022F0204,
	0x022F0208,
	0x023F000E,
	0x023F0200,
	0x023F0204,
	0x023F0208,
	0x024F000E,
	0x024F0200,
	0x024F0204,
	0x024F0208,
};
int send_all_commands()
{
	int i;
	int count=sizeof(codec_commands)/sizeof(int);
	log_msg("writing codec commands\n");
	for(i=0;i<count;i++){
		int cmd=codec_commands[i];
		hda_send_codec_cmd(cmd);
	}
	return 0;
}


int play_wav_buf(char *buf,int len)
{
	DWORD tick,delta;
	
	if(0==device_found){
		printf("device not found\n");
		return 0;
	}
	tick=get_tick_count();
	while(1){
		DWORD pos;
		int ready=FALSE;
		delta=get_tick_count()-tick;
		if(get_msec(delta)>100){
			printf("timeout waiting for buffer\n");
			break;
		}
		pos=read_32(base_reg+OSD0LPIB);
		switch(current_buf){
		default:
		case 0:
			if(pos>=buf_size)
				ready=TRUE;
			break;
		case 1:
			if(pos<buf_size)
				ready=TRUE;
			break;
		}
		if(ready)
			break;
	}
	{
		char *mem;
		if(0==current_buf)
			mem=(char*)buffer1;
		else
			mem=(char*)buffer2;
		if(len>buf_size)
			len=buf_size;
		else if(len<0)
			len=0;
		memcpy(mem,buf,len);
		current_buf^=1;
	}
	return 0;
}
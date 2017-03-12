import dos_map;
import libc_map;

alias printf=_printf;
alias int386=_int386;
alias vprintf=_vprintf;
alias malloc=_malloc;
alias memset=_memset;
alias memcpy=_memcpy;

@nogc:
nothrow:

alias ubyte BYTE;
alias ushort WORD;
alias uint DWORD;
alias int BOOL;
enum : BOOL {
    FALSE = 0,
    TRUE  = 1
}

const CLOCK_RATE = 300000000;
__gshared DWORD base_reg;
__gshared int device_found;

const PCICMD = 0x04;
const MSE = 0x02;

const BME = 0x04;
//HDA PCI CONFIG REGISTERS
//HDA MEM MAP CONFIG REGISTERS
const HDBARL = 0x10;
const GCAP = 0x00;
const GCTL = 0x08;
const STATESTS = 0x0E;
const HDA_IC = 0x60;
const HDA_IR = 0x64;
const HDA_IRS = 0x68;
const OSD0CTL = 0xA0;
const OSD0STS = 0xA3;
const OSD0LPIB = 0xA4;
const OSD0CBL = 0xA8;
const OSD0LVI = 0xAC;
const OSD0FMT = 0xB2;
const OSD0BDPL = 0xB8;

const OSD0BDPU = 0xBC;
//ctrl bits
const SRST = 0x01;
const RUN = 0x02;

const BCIS = 0x04;

const CRST = 0x01;
const ICB = 0x01;

const IRV = 0x02;
const SET_AMP_GAIN = 0x003;
const SET_MUTE = 0x0080;
const SET_LEFT_AMP = 0x2000;
const SET_RIGHT_AMP = 0x1000;

const SET_OUT_AMP = 0x8000;

void log_msg(const char *fmt,...)
{
	import core.stdc.stdarg;
	import core.stdc.stdio;
	va_list arg;
	va_start(arg,fmt);
	vprintf(fmt,arg);
}

DWORD get_tick_count()
{
	DWORD tick;
	version(windows_exe){
		import core.sys.windows.windows;
		tick=GetTickCount();
	}else{
		asm{
			rdtsc;
			mov tick,EAX;
		}
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
	version(windows_exe){
		return ticks;
	}else{
		return (ticks/(CLOCK_RATE/1000));
	}
}

int find_pci_device(WORD ven_id,WORD dev_id,BYTE *bus_num,BYTE *dev_num)
{
	REGS r;
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
int read_config(BYTE bus_num,BYTE dev_num,WORD reg_num,DWORD *result_data,int size)
{
	int result=FALSE;
	REGS r;
	WORD cmd;
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
	*result_data=r.x.ecx;
	return result;
}
int read_config_32(BYTE bus_num,BYTE dev_num,WORD reg_num,DWORD *result_data)
{
	int result=FALSE;
	result=read_config(bus_num,dev_num,reg_num,result_data,4);
	return result;
}
int read_config_16(BYTE bus_num,BYTE dev_num,WORD reg_num,WORD *result_data)
{
	int result=FALSE;
	DWORD _out=0;
	result=read_config(bus_num,dev_num,reg_num,&_out,2);
	*result_data=cast(WORD)_out;
	return result;
}
int read_config_8(BYTE bus_num,BYTE dev_num,WORD reg_num,BYTE *result_data)
{
	int result;
	DWORD _out=0;
	result=read_config(bus_num,dev_num,reg_num,&_out,1);
	*result_data=cast(BYTE)_out;
	return result;
}
int write_config(BYTE bus_num,BYTE dev_num,WORD reg_num,DWORD data,int size)
{
	int result=FALSE;
	REGS r;
	WORD cmd;
	switch(size){
	case 1:
		cmd=0xB10B;
		r.h.cl=cast(BYTE)data;
		break;
	case 2:
		cmd=0xB10C;
		r.w.cx=cast(WORD)data;
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
int write_config_16(BYTE bus_num,BYTE dev_num,WORD reg_num,WORD data)
{
	int result;
	DWORD _data=data;
	result=write_config(bus_num,dev_num,reg_num,_data,2);
	return result;
}
int write_config_8(BYTE bus_num,BYTE dev_num,WORD reg_num,BYTE data)
{
	int result;
	DWORD _data=data;
	result=write_config(bus_num,dev_num,reg_num,_data,1);
	return result;
}

void log_mem(char *fmt,...)
{
	import core.stdc.stdarg: va_list,va_start;
	va_list arg;
	va_start(arg,fmt);
	vprintf(fmt,arg);
}
static DWORD last_read=0,last_data=0;

int write_32(DWORD addr,DWORD data)
{
	DWORD *ptr=cast(DWORD *)addr;
	ptr[0]=data;
	return TRUE;
}
DWORD read_32(DWORD addr)
{
	DWORD result;
	DWORD *ptr=cast(DWORD *)addr;
	result=ptr[0];
	return result;
}
int write_16(DWORD addr,WORD data)
{
	WORD *ptr=cast(WORD*)addr;
	ptr[0]=data;
	return TRUE;
}
WORD read_16(DWORD addr)
{
	WORD result;
	WORD *ptr=cast(WORD*)addr;
	result=ptr[0];
	return result;
}
int write_08(DWORD addr,BYTE data)
{
	BYTE *ptr=cast(BYTE*)addr;
	ptr[0]=data;
	return TRUE;
}
BYTE read_08(DWORD addr)
{
	BYTE result;
	BYTE *ptr=cast(BYTE*)addr;
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
	BYTE tmp;
	tmp=read_08(base_reg+OSD0CTL);
	tmp&=~RUN;
	write_08(base_reg+OSD0CTL,tmp);
	while((read_16(base_reg+OSD0CTL)&RUN)!=0){
	}
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
	BYTE tmp;
	tmp=read_08(base_reg+OSD0CTL);
	tmp|=RUN;
	write_08(base_reg+OSD0CTL,tmp);
	DWORD tick;
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
	while((read_16(base_reg+HDA_IRS)&ICB)!=0){
	}
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

extern (C)
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
			printf("%03X:",(cast(DWORD)&ptr[i])&0xFFF); //i*4);
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
		ptr=cast(int*)0xFBFF0060;
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


__gshared static int *memory_chunk=null;
__gshared static int *bdl_list=null;
__gshared static int *buffer1=null;
__gshared static int *buffer2=null;
version(windows_exe)
	__gshared const int buf_size=0x2000;
else
	__gshared const int buf_size=0x1000;
__gshared int current_buf=0;

int get_audio_buf_size()
{
	return buf_size;
}
int* get_audio_buf(int which)
{
	if(which)
		return buffer2;
	else
		return buffer1;
}
extern (C)
int start_audio()
{
	DWORD tmp;
	int result=FALSE;
	int i;
	const int bdl_entries=2;
	if(base_reg==0)
		return result;
	if(memory_chunk is null){
		const int chunk_size=buf_size*16;
		memory_chunk=cast(int*)malloc(chunk_size);
		if(memory_chunk is null){
			printf("unable to allocate memory\n");
			return result;
		}
		memset(memory_chunk,0,chunk_size);
		tmp=cast(DWORD)memory_chunk;
		tmp=(tmp+0x7F)&(-0x80);
		bdl_list=cast(int*)tmp;
		buffer1=cast(int*)(tmp+buf_size);
		buffer2=cast(int*)(tmp+buf_size*2);
	}
	printf("bdl_list=%08X\n",bdl_list);
	printf("buffer1=%08X\n",buffer1);
	for(i=0;i<bdl_entries;i++){
		tmp=cast(DWORD)buffer1;
		bdl_list[i*4]=tmp+i*buf_size;
		bdl_list[i*4+1]=0;
		bdl_list[i*4+2]=buf_size;
		bdl_list[i*4+3]=0;
	}

	hda_stop();
	tmp=read_08(base_reg+OSD0CTL);
	tmp&=~SRST;
	write_08(base_reg+OSD0CTL,cast(BYTE)tmp);
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
	write_32(base_reg+OSD0BDPL,cast(DWORD)bdl_list);
	write_32(base_reg+OSD0BDPU,0);
	hda_run();
	printf("audio running\n");
	result=TRUE;
	return result;
}

int set_bit_rate(int rate)
{
	DWORD tmp;
	tmp=read_16(base_reg+OSD0FMT);
	if(44100==rate)
		tmp|=1<<14;
	else
		tmp&=~(1<<14);
	write_16(base_reg+OSD0FMT,cast(WORD)tmp);
	return 0;
}

const int codec_commands[]=[
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
	0x00C3901F, //main vol
	0x00C3A01F, //main vol
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
	0x0143907F, //amp
	0x0143A07F, //amp
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
];
int send_all_commands()
{
	int i;
	int count=codec_commands.length;
	log_msg("writing codec commands\n");
	for(i=0;i<count;i++){
		int cmd=codec_commands[i];
		hda_send_codec_cmd(cmd);
	}
	return 0;
}

extern (C)
int set_volume(int vol)
{
	int cmds[2]=[
		0x00C39020, //main vol
		0x00C3A020, //main vol
	];
	vol&=0xFF;
	if(vol>=0x1F)
		vol=0x1F;
	cmds[0]|=vol;
	cmds[1]|=vol;
	hda_send_codec_cmd(cmds[0]);
	hda_send_codec_cmd(cmds[1]);
	return 0;
}

int wait_buffer()
{
	int result=false;
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
		if(ready){
			result=true;
			break;
		}
		version(windows_exe){
			import core.sys.windows.windows;
			Sleep(0);
		}
	}
	return result;
}
int play_wav_buf(ubyte *buf,int len)
{
	if(0==device_found){
		printf("device not found\n");
		return 0;
	}
	wait_buffer();
	char *mem;
	if(0==current_buf)
		mem=cast(char*)buffer1;
	else
		mem=cast(char*)buffer2;
	if(len>buf_size)
		len=buf_size;
	else if(len<0)
		len=0;
	memcpy(mem,buf,len);
	current_buf^=1;
	return 0;
}

extern (C)
int set_silence()
{
	int count=0;
	while(count<2){
		int *b=null;
		wait_buffer();
		if(current_buf==0)
			b=buffer1;
		else
			b=buffer2;
		if(b !is null)
			memset(b,0,buf_size);
		current_buf^=1;
		count++;
	}
	return 0;
}
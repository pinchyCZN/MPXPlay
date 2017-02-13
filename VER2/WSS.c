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

//HDA PCI CONFIG REGISTERS
#define HDBARL 		0x10
//HDA MEM MAP CONFIG REGISTERS
#define GCTL		0x8
#define HDA_IC		0x60
#define HDA_IRS		0x68
#define OSD0CTL		0xA0
#define OSD0STS		0xA3
#define OSD0CBL		0xA8
#define OSD0LVI		0xAC
#define OSD0FMT		0xB2
#define OSD0BDPL	0xB8
#define OSD0BDPU	0xBC

//ctrl bits
#define SRST		0x01
#define RUN			0x02
#define BCIS		0x04

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

int read_config_dword(BYTE bus_num,BYTE dev_num,int reg_num,DWORD *out)
{
	int result=FALSE;
	union REGS r={0};
	r.w.ax=0xB10A; //READ CONFIG DWORD
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

int write_dword(DWORD addr,DWORD data)
{
	DWORD *ptr=(DWORD *)addr;
	ptr[0]=data;
	return TRUE;
}
DWORD read_dword(DWORD addr)
{
	DWORD *ptr=(DWORD *)addr;
	return ptr[0];
}
int write_word(DWORD addr,WORD data)
{
	WORD *ptr=(WORD*)addr;
	ptr[0]=data;
	return TRUE;
}
WORD read_word(DWORD *addr)
{
	WORD *ptr=(WORD*)addr;
	return ptr[0];
}
int write_byte(DWORD addr,BYTE data)
{
	BYTE *ptr=(BYTE*)addr;
	ptr[0]=data;
	return TRUE;
}
int read_byte(DWORD addr)
{
	BYTE *ptr=(BYTE*)addr;
	return ptr[0];
}

int wait_reset()
{
	int result=TRUE;
	DWORD tick,delta;
	tick=get_tick_count();
	while((read_dword(base_reg+GCTL)&1)==0){
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
	tmp=read_byte(base_reg+OSD0CTL);
	tmp&=~RUN;
	write_byte(base_reg+OSD0CTL,tmp);
	while((read_word(base_reg+OSD0CTL)&RUN)!=0)
		;
	if((read_word(base_reg+OSD0CTL)&SRST)!=0){
		DWORD tick,delta;
		tmp=read_byte(base_reg+OSD0CTL);
		tmp|=SRST;
		write_byte(base_reg+OSD0CTL,tmp);
		tmp=read_byte(base_reg+OSD0CTL);
		tmp&=~SRST;
		write_byte(base_reg+OSD0CTL,tmp);
	}
	while((read_word(base_reg+OSD0CTL)&SRST)!=0)
		;
	return 0;
}

int hda_run()
{
	int tmp;
	tmp=read_byte(base_reg+OSD0CTL);
	tmp|=RUN;
	write_byte(base_reg+OSD0CTL,tmp);
	return 0;
}
int hda_send_codec_cmd(DWORD param)
{
	while((read_dword(base_reg+HDA_IRS)&ICB)!=0)
		;
	write_dword(base_reg+HDA_IC, param);
	write_word(base_reg+HDA_IRS,ICB|IRV);
	return 0;
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
//	hda_send_codec_cmd(HDAPARAM2(codecaddr, nid, SET_AMP_GAIN, SET_RIGHT_AMP | tmp), FALSE);
//	hda_send_codec_cmd(HDAPARAM2(codecaddr, nid, SET_AMP_GAIN, SET_LEFT_AMP  | tmp), FALSE);
	return 0;
}
int reset_hda()
{
	wait_reset();
	write_dword(base_reg+GCTL,1);
	wait_reset();
	write_byte(base_reg+OSD0STS,BCIS);
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
	if(!read_config_dword(bus_num,dev_num,HDBARL,&base_reg)){
		log_msg("ERROR getting base address\n");
		return result;
	}
	log_msg("BASE ADDRESS:%08X\n",base_reg);
	reset_hda();
	return result;
}

int dump_address(int *ptr)
{
	int i;
	for(i=0;i<0xC0/4;i++){
		if((i%4)==0)
			printf("%03X:",i*4);
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
	int i;
	int *ptr;
	int log[10]={0};
	int log2[10]={0};
	int quit=0;
	DWORD tick,delta;
	ptr=0xFBFF0000;
	tick=get_tick_count();
	while(1){
		ptr=0xFBFF00A0;
		for(i=0;i<8;i++){
			log[i]=ptr[i];
		}
		for(i=0;i<8;i++){
			int x=ptr[i];
			log2[i]=x;
			if(log[i]!=x){
				quit=1;
			}
		}
		if(quit){
			dump_address(&log);
			printf("--\n");
			dump_address(&log2);
			printf("[[\n");
			dump_address(0xFBFF0000);
			break;
		}
		delta=get_tick_count()-tick;
		if(get_msec(delta)>500)
			break;

	}
	return 0;
}


static int *memory_chunk=0;
static int *bdl_list=0;
static int *buffer1=0;
static int *buffer2=0;
int play_data(char *data,int len)
{
	DWORD tmp;
	int result=FALSE;
	if(base_reg==0)
		return result;
	if(memory_chunk==0){
		DWORD tmp;
		memory_chunk=calloc(1,0x10000*4);
		if(memory_chunk==0)
			return result;
		{
			int i;
			char *ptr=(char*)memory_chunk;
			for(i=0;i<0x10000*4;i++){
				ptr[i]=rand();
			}
		}
		tmp=memory_chunk;
		tmp=(tmp+0x7F)&(-0x80);
		bdl_list=tmp;
		buffer1=tmp+0x10000;
		buffer2=tmp+0x20000;
	}

	bdl_list[0]=buffer1;
	bdl_list[1]=0;
	bdl_list[2]=0x1000;
	bdl_list[3]=0;
	bdl_list[4]=buffer2;
	bdl_list[5]=0;
	bdl_list[6]=0x1000;
	bdl_list[7]=0;
	send_all_commands();
	set_volume(31);
	hda_stop();
	tmp=read_dword(base_reg+OSD0CTL);
	tmp&=0xff0fffff;
	tmp|=(1<<20);
	write_dword(base_reg+OSD0CTL,tmp);
	write_dword(base_reg+OSD0CBL,0x4000);
	write_word(base_reg+OSD0LVI,1);
	write_word(base_reg+OSD0FMT,(1<<14)|(1<<4)|(1));
	write_dword(base_reg+OSD0BDPL,bdl_list);
	write_dword(base_reg+OSD0BDPU,0);
	hda_run();
	test_mem();
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
	0x00C3901F, //vol
	0x00C3A01F, //vol
	0x00CF000D,
	0x00C35000,
	0x00C36000,
	0x00DF000E,
	0x00DF0200,
	0x00D70100,
	0x00DF0012,
	0x00D3901F, //vol
	0x00D3A01F, //vol
	0x00DF000D,
	0x00D35000,
	0x00D36000,
	0x00EF000E,
	0x00EF0200,
	0x00E70100,
	0x00EF0012,
	0x00E3901F, //vol
	0x00E3A01F, //vol
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
	0x0143901F, //vol
	0x0143A01F, //vol
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
	for(i=0;i<count;i++){
		hda_send_codec_cmd(codec_commands[i]);
	}
}
#if 0

typedef struct {
	int 	initialized;
	void	(*device_exit)(void);
	void	(*pcm_upload)(void);
	DWORD	(*get_current_pos)(void);
	DWORD	playback_rate;
	int 	pcm_format;
	char	*device_name;
	int 	irq;
	int 	isa_port;
	int 	isa_dma;
	int 	isa_hdma;
} WAVEDEVICE;

static WAVEDEVICE wd = { FALSE, NULL, NULL, NULL, 0, _16BITSTEREO, NULL, -1, -1, -1, -1};

static void wavedevice_struct_init(void)
{
	wd.initialized = FALSE;
	wd.device_exit = NULL;
	wd.pcm_upload  = NULL;
	wd.get_current_pos = NULL;
	wd.playback_rate = 0;
	wd.pcm_format  = _16BITSTEREO;
	wd.device_name = NULL;
	wd.irq		= -1;
	wd.isa_port = -1;
	wd.isa_dma	= -1;
	wd.isa_hdma = -1;
}

static BOOL pci_read_config_byte(PCI_DEV *pci, int idx, BYTE *data)
{
	union REGS r={0};
	BOOL result = TRUE;

	r.w.ax = 0xB108;						/* read config byte */
	r.x.ebx = (DWORD)pci->device_bus_number;
	r.w.di = (DWORD)idx;
	int386(0x1a,&r,&r);
	if(r.x.cflag!=0){
		log_msg("pci read config byte failed\n");
		result = FALSE;
		r.w.cx = 0;
	}
	*data = (BYTE)r.h.cl;
	return result;
}

static BOOL pci_read_config_word(PCI_DEV *pci, int idx, WORD *data)
{
	union REGS r={0};
	BOOL result = TRUE;

	r.w.ax = 0xB109;						/* read config word */
	r.x.ebx = (DWORD)pci->device_bus_number;
	r.w.di = (DWORD)idx;
	int386(0x1a,&r,&r);
	if( r.h.ah != 0 ){
		log_msg("pci read config word failed\n");
		result = FALSE;
		r.w.cx = 0;
	}
	*data = (WORD)r.w.cx;
	return result;
}

static BOOL pci_read_config_dword(PCI_DEV *pci, int idx, DWORD *data)
{
	union REGS r={0};
	BOOL result = TRUE;

	r.w.ax = 0xB10A;						/* read config dword */
	r.x.ebx = (DWORD)pci->device_bus_number;
	r.w.di = (DWORD)idx;
	int386(0x1a,&r,&r);
	if(r.x.cflag!=0){
		log_msg("pci read config dword failed\n");
		result = FALSE;
		r.w.cx = 0;
	}
	*data = (DWORD)r.x.ecx;
	return result;
}

static BOOL pci_write_config_byte(PCI_DEV *pci, int idx, BYTE data)
{
	union REGS r={0};
	BOOL result = TRUE;

	r.w.ax = 0xB10B;						/* write config byte */
	r.x.ebx = (DWORD)pci->device_bus_number;
	r.w.cx = (DWORD)data;
	r.w.di = (DWORD)idx;
	int386(0x1a,&r,&r);
	if( r.h.ah != 0 ){
		log_msg("pci write config byte failed\n");
		result = FALSE;
	}
	return result;
}

static BOOL pci_write_config_word(PCI_DEV *pci, int idx, WORD data)
{
	union REGS r={0};
	BOOL result = TRUE;

	r.w.ax = 0xB10C;						/* write config word */
	r.x.ebx = (DWORD)pci->device_bus_number;
	r.w.cx = (DWORD)data;
	r.w.di = (DWORD)idx;
	int386(0x1a,&r,&r);
	if( r.h.ah != 0 ){
		log_msg("pci write config word failed\n");
		result = FALSE;
	}
	return result;
}

static BOOL pci_write_config_dword(PCI_DEV *pci, int idx, DWORD data)
{
	union REGS r={0};
	BOOL result = TRUE;

	r.w.ax = 0xB10D;						/* write config dword */
	r.x.ebx = (DWORD)pci->device_bus_number;
	r.w.cx = (DWORD)data;
	r.w.di = (DWORD)idx;
	int386(0x1a,&r,&r);
	if( r.h.ah != 0 ){
		log_msg("pci write config dword failed\n");
		result = FALSE;
	}
	return result;
}
static int read_byte(char *addr)
{
	return addr[0];
}
static BOOL find_pci_device(PCI_DEV *pci)
{
	union REGS r={0};
	WORD wdata;

	r.w.ax = 0xB102;					// PCI BIOS - FIND PCI DEVICE
	r.w.cx = pci->device_id;				// device ID
	r.w.dx = pci->vender_id;				// vendor ID
	r.x.esi = 0x00000000;					// device index
	int386(0x1a,&r,&r);
	if(r.x.cflag!=0){
		return FALSE;						// no specified device found
	}
	pci->device_bus_number = r.w.bx;		// save the device & bus number
	return TRUE;
}



#define SAMPLECNT	  16384
#define SAMPLECNTMASK 0x3FFF

static DWORD g_prev_play_cursor = 0;
static DWORD g_write_cursor = 0;
static DWORD g_latency	= 48000/30;
static float g_latencym = 2.2;
static DWORD g_dma_average_cnt = (48000/60) << 8;
static int	 g_dma_dt = 0;
static int	 g_dma_overflow  = 0;
static int	 g_dma_underflow = 0;
#define DMA_AVERAGE 			256
#define DMA_AVERAGE_MASK		0xFF
#define DMA_AVERAGE_SHIFT_COUNT 8

static DWORD g_dma_remainder = 0;
static long  mixing_buff[SAMPLECNT*2];
static DWORD g_current_req = 0;
static DWORD g_next_req = 0;
static int	 g_master_volume = 256;
static DWORD g_samples_per_frame = 0;
static int stereo_reverse_flag=0;

static int g_wss_dma_sel = 0;
static unsigned long g_wss_dma_addr = 0;
static DWORD g_dosmem64k_phys_table[16];
static int g_dma_buff_size;
static int g_dmacnt_shift_count;

/* dos memory 4k bytes for work area */
static BOOL allocate_dosmem4k(void);
static void free_dosmem4k(void);
static DWORD get_address_dosmem4k(void);
static int g_dosmem4k_sel = 0;
static unsigned long g_dosmem4k_addr = 0;
static DWORD g_dosmem4k_phys_table[1];


static int _dma_allocate_mem(int *sel, unsigned long *phys)
{
	int result=FALSE;
	union REGS regs={0};
	regs.x.eax = 0x0100;
	regs.x.ebx = 131072 >> 4;
	regs.x.cflag = 1;
	int386(0x31, &regs, &regs);
	if(regs.x.cflag)
		return result;
	*sel = regs.x.edx;
	*phys = (regs.x.eax << 4);
	result=TRUE;
	return result;

}
static BOOL _dma_allocate_mem4k(int *sel, unsigned long *phys)
{
	int result=FALSE;
	union REGS regs;
	regs.x.eax = 0x0100;
	regs.x.ebx = 8192 >> 4;
	regs.x.cflag = 1;
	int386(0x31, &regs, &regs);
	if(regs.x.cflag)
		return result;
	*sel = regs.x.edx;
	*phys = (regs.x.eax << 4);
	result=TRUE;
	return result;
}

static BOOL allocate_dosmem4k(void)
{
	if(g_dosmem4k_sel  != 0) return FALSE;
	if(g_dosmem4k_addr != 0) return FALSE;
	if(_dma_allocate_mem4k(&g_dosmem4k_sel, &g_dosmem4k_addr) == FALSE){
		return FALSE;
	}
//	  info4k.size	 = 4096;
//	  info4k.address = g_dosmem4k_addr;
//	  __dpmi_lock_linear_region(&info4k);

	return TRUE;
}

static void free_dosmem4k(void)
{
	union REGS regs={0};
	if(g_dosmem4k_sel  == 0) return;
	if(g_dosmem4k_addr == 0) return;

	regs.x.eax = 0x0101;
	regs.x.edx = g_dosmem4k_sel;
	regs.x.cflag = 1;
	int386(0x31, &regs, &regs);
	g_dosmem4k_sel = 0;
	g_dosmem4k_addr = 0;
}

static DWORD get_address_dosmem4k(void)
{
	return g_dosmem4k_addr;
}


static BOOL allocate_dosmem64k_for_dma(int format)
{
	BYTE *a0;
	int d0;
	BOOL lockflag = FALSE;

/*
	g_dma_buff_size = SAMPLECNT;
	switch(format){
		case _8BITMONO:
			g_dma_buff_size *= 1;
			g_dmacnt_shift_count = 0;
			break;
		case _8BITSTEREO:
			g_dma_buff_size *= 2;
			g_dmacnt_shift_count = 1;
			break;
		case _16BITSTEREO:
			g_dma_buff_size *= 4;
			g_dmacnt_shift_count = 2;
			break;
		default:
			g_dma_buff_size = 0;
	}
	if(g_dma_buff_size == 0) return FALSE;
	a0 = (BYTE*)malloc(g_dma_buff_size);
	if(a0 == NULL) return FALSE;
	if (_dma_allocate_mem(&g_wss_dma_sel, &g_wss_dma_addr) == FALSE)
		return FALSE;
	d0 = 0;
	while(d0 < g_dma_buff_size){
		if(format == _8BITSTEREO || format == _8BITMONO)
			a0[d0] = 0x80;
		else
			a0[d0] = 0x00;
		d0 += 1;
	}
	memcpy((void*)g_wss_dma_addr,a0,g_dma_buff_size);
	free(a0);

	if(lockflag == TRUE){
		info64k.size	= 65536;
		info64k.address = g_wss_dma_addr;
		__dpmi_lock_linear_region(&info64k);
	}else{
		info64k.address = 0;
	}
*/
	return TRUE;

}

static void free_dosmem64k_for_dma(void)
{
	union REGS regs={0};
	if(g_wss_dma_sel  == 0) return;
	if(g_wss_dma_addr == 0) return;

	regs.x.eax=0x0101;
	regs.x.edx=g_wss_dma_sel;
	int386(0x31,&regs,&regs);
	g_wss_dma_sel = 0;
	g_wss_dma_addr = 0;
}

static DWORD get_address_dosmem64k_for_dma(void)
{
	return g_wss_dma_addr;
}

BYTE get_8bit_pcm_value(long value)
{
	BYTE d0;

	if(value >	32767 - 128) value =  32767 - 128;
	if(value < -32768)		 value = -32768;
	d0 = (value + 128 - ((value >> 15) & 0x0001)) >> 8;
	return (d0 + 0x80);
}

static void common_pcm_upload_func(void)
{
	int d0;
	long d1;
	WORD d2;
	DWORD d3;
	DWORD start;
	DWORD end;

	printf("upload\n");
	start = -1;
	d0 = 0;
	while(d0 < g_current_req){
		g_write_cursor = g_write_cursor & SAMPLECNTMASK;
		if(wd.pcm_format == _16BITSTEREO){
			int *ptr;
			d1	= ((mixing_buff[(d0*2) + 0] * g_master_volume)/256);
			if(d1 >  32767) d1 =  32767;
			if(d1 < -32768) d1 = -32768;
			d3	= d1 & 0xFFFF;
			d1	= ((mixing_buff[(d0*2) + 1] * g_master_volume)/256);
			if(d1 >  32767) d1 =  32767;
			if(d1 < -32768) d1 = -32768;
			d3 |= d1 << 16;
			ptr=(int*)(g_wss_dma_addr+g_write_cursor * 4);
			ptr[0]=d3;
		}else if(wd.pcm_format == _8BITSTEREO){
			short *ptr;
			d1	= ((mixing_buff[(d0*2) + 0] * g_master_volume)/256);
			d2	= get_8bit_pcm_value(d1);
			d1	= ((mixing_buff[(d0*2) + 1] * g_master_volume)/256);
			d2 |= get_8bit_pcm_value(d1) << 8;
			ptr=(short*)(g_wss_dma_addr + (g_write_cursor * 2));
			ptr[0]=d2;
		}else if(wd.pcm_format == _8BITMONO){
			char *ptr;
			d1	= ((mixing_buff[(d0*2) + 0] * g_master_volume)/256);
			d1 += ((mixing_buff[(d0*2) + 1] * g_master_volume)/256);
			d1	= d1 / 2;
			d2	= get_8bit_pcm_value(d1);
			ptr=(char*)(g_wss_dma_addr + (g_write_cursor * 1));
			ptr[0]=d2;
		}
		if(start == -1)
			start = g_write_cursor;
		g_write_cursor += 1;
		d0 += 1;
	}
	end = g_write_cursor;
}

/********************************************************************
 *						 		HDA									*
 ********************************************************************/
 
#define DEV_ICH		0
#define DEV_PCH		1
#define DEV_SCH		2
#define DEV_ATI		3
#define DEV_ATIHDMI	4
#define DEV_VIA		5
#define DEV_SIS		6
#define DEV_ULI		7
#define DEV_NVIDIA	8
#define DEV_TERA	9
#define DEV_CREAT	10

#define MSE 		0x02
#define BME			0x04

#define PCICMD		0x04
#define HDBARL 		0x10
#define HDBARU 		0x14
#define HDCTL		0x40
#define DEVC		0x78
#define HDAIOLEN 16*1024

#define CRST 		0x01
#define ICB			0x01
#define IRV			0x02
#define SRST		0x01
#define RUN			0x02

#define HDAGCAP		0x00
#define HDAGCTL		0x08
#define HDASTATESTS	0x0e
#define HDACORBSIZE	0x4e
#define	HDARIRBSTS	0x5d
#define HDAICOI		0x60
#define HDAICII		0x64
#define HDAICIS		0x68
#define HDADPLBASE	0x70
#define HDADPUBASE	0x74	

#define HDASDBASE	0x80
#define HDASDSIZE	0x20

#define HDA_MAX_CODECS	4
#define BDL_ENTRIES		256

typedef struct{
	unsigned short vender_id;
	unsigned short device_id;
	char *name;
}DEVICE_INFO;

DEVICE_INFO hda_dev_list[] = {
	{ 0x17F3, 0x3010, "vortex MXplus"},
	{ 0x0000, 0x0000, "" }
};

typedef struct
{
	DWORD nid;
	DWORD caps;
	DWORD type;
	int checked;
}HDAFUNCINFO;

static int sound_device_master_volume=-1;
static DWORD actual_sample_rate=0;
static int hdaiosel = 0;
static DWORD codecmask = 0;
static DWORD osdbase = 0;
static DWORD outampcap = 0, inampcap = 0;
static DWORD codecvid = 0;
static DWORD codecdid = 0;
static HDAFUNCINFO * nodes = NULL;
static int optspeak = 0;

//stream registers
#define	OSDCTL		(osdbase + 0x00)
#define OSDSTS		(osdbase + 0x03)
#define OSDLPIB		(osdbase + 0x04)
#define OSDCBL		(osdbase + 0x08)
#define OSDLVI		(osdbase + 0x0c)
#define OSDFIFOS	(osdbase + 0x10)
#define	OSDFMT		(osdbase + 0x12)
#define OSDBDPL		(osdbase + 0x18)
#define OSDBDPU		(osdbase + 0x1c)

#define BCIS	0x04
#define FIFOE	0x08
#define DESE	0x10		

unsigned char hda_rd_reg8(int offset)
{
	char *ptr;
	ptr=(unsigned char*)(g_pci.base0+offset);
	return ptr[0];
}
unsigned short hda_rd_reg16(int offset)
{
	unsigned short *ptr;
	ptr=(unsigned short*)(g_pci.base0+offset);
	return ptr[0];
}
unsigned int hda_rd_reg32(int offset)
{
	unsigned int *ptr;
	ptr=(unsigned int*)(g_pci.base0+offset);
	return ptr[0];
}
int hda_wr_reg8(int offset,unsigned char data)
{
	char *ptr;
	ptr=(char*)(g_pci.base0+offset);
	ptr[0]=data;
	return 1;
}
int hda_wr_reg16(int offset,unsigned short data)
{
	unsigned short *ptr;
	ptr=(unsigned short*)(g_pci.base0+offset);
	ptr[0]=data;
	return 1;
}
int hda_wr_reg32(int offset,unsigned int data)
{
	unsigned int *ptr;
	ptr=(unsigned int*)(g_pci.base0+offset);
	ptr[0]=data;
	return 1;
}


#define SET_CVT_FORM	0x002
#define SET_AMP_GAIN	0x003
#define GET_CVT_FORM	0x00a
#define GET_AMP_GAIN	0x00b
#define SET_CON_SEL 	0x701
#define SET_POW_STATE	0x705
#define SET_CVT_STRM	0x706
#define SET_PIN_CTRL	0x707
#define SET_PIN_SENSE	0x709
#define SET_PIN_EAPDBTL	0x70c
#define SET_GPIO_DATA	0x715
#define SET_GPIO_MASK	0x716
#define SET_GPIO_DIR	0x717
#define	FG_RESET		0x7ff

#define GET_PARAM		0xf00
#define GET_CON_LST_ENT	0xf02
#define GET_POW_STATE	0xf05
#define GET_PIN_CTRL	0xf07
#define GET_PIN_SENSE	0xf09
#define GET_PIN_EAPDBTL	0xf0c
#define GET_GPIO_DATA	0xf15
#define GET_GPIO_MASK	0xf16
#define GET_GPIO_DIR	0xf17
#define GET_PIN_CFG		0xf1c

#define VENDOR_ID		0x00
#define NODE_CNT		0x04
#define FUNC_TYPE		0x05
#define WIDGET_CAP		0x09
#define PCM_SUPP		0x0a
#define STREAM_SUPP		0x0b
#define PIN_CAP			0x0c
#define IN_AMP_CAP		0x0d
#define CON_LIST		0x0e
#define OUT_AMP_CAP		0x12	

#define AUDIO_FUNC		0x01	
#define	AUDIO_PCM		0x01
#define AUDIO_4816		0x20040

#define AUDIO_OUTPUT	0x00
#define AUDIO_INPUT		0x01
#define MIXER			0x02
#define SELECTOR		0x03
#define PIN_CMPLX		0x04
#define POWER_WIDG		0x05
#define VOL_KNOB		0x06

#define	STEREO_WIDG		0x001	
#define	HAS_IN_AMP		0x002	
#define HAS_OUT_AMP		0x004
#define HAS_AMP_PARAM	0x008
#define HAS_FORM_INFO	0x010
#define HAS_CON_LST		0x100
#define IS_DIGITAL		0x200
#define HAS_POW_CTRL	0x400

#define ASK_OUT_AMP		0x8000
#define ASK_IN_AMP		0x0000
#define ASK_LEFT_AMP	0x2000
#define ASK_RIGHT_AMP	0x0000

#define SET_MUTE		0x0080
#define SET_OUT_AMP		0x8000
#define SET_IN_AMP		0x4000
#define SET_LEFT_AMP	0x2000
#define SET_RIGHT_AMP	0x1000

#define PIN_IN_ENAB		0x20
#define PIN_OUT_ENAB	0x40
#define PIN_HP_ENAB		0x80

#define DEV_ATTACHED	0x80000000
#define STATE_D0		0x00
#define EAPD_ENABLE		0x02
#define BTL_ENABLE		0x01

#define PIN_PD_CAP		0x00004
#define PIN_HP_CAP		0x00008
#define PIN_OUT_CAP		0x00010
#define PIN_IN_CAP		0x00020
#define PIN_BIO_CAP		0x00040
#define PIN_EAPD_CAP	0x10000

#define LINE_OUT		0x00
#define SPEAKER			0x01
#define	HP_OUT			0x02
#define	AUX				0x09
#define UNKNOWN			0x0F

#define LONG_CON_LST	0x80
#define SET_AUDIO_48162	   0x0011 //48kHz
//vic
#define SET_AUDIO_441162   0x4011 //44.1kHz
#define SET_AUDIO_96162    0x0811 //96kHz
#define SET_AUDIO_882162   0x4811 //88.2kHz
#define SET_AUDIO_32162    0x0A11 //32kHz
#define SET_AUDIO_144162   0x1011 //144kHz
#define SET_AUDIO_192162   0x1811 //192kHz
#define SET_AUDIO_1764162  0x5811 //176.4kHz
#define SET_AUDIO_24162    0x0111 //24kHz
#define SET_AUDIO_2205162  0x4111 //22.05kHz
#define SET_AUDIO_16162    0x0211 //16kHz
#define SET_AUDIO_11025162 0x4311 //11.025kHz
#define SET_AUDIO_9_6162   0x0411 //9.6kHz
#define SET_AUDIO_8162     0x0511 //8kHz
#define SET_AUDIO_6162     0x0711 //6kHz

//vic
static unsigned short hda_set_rate()
{
	if(wd.playback_rate == 44100)
		return SET_AUDIO_441162;
	else if(wd.playback_rate == 96000)
		return SET_AUDIO_96162;
	else if(wd.playback_rate == 88200)
		return SET_AUDIO_882162;
	else if(wd.playback_rate == 32000)
		return SET_AUDIO_32162;
	else if(wd.playback_rate == 144000)
		return SET_AUDIO_144162;
	else if(wd.playback_rate == 192000)
		return SET_AUDIO_192162;
	else if(wd.playback_rate == 176400)
		return SET_AUDIO_1764162;
	else if(wd.playback_rate == 24000)
		return SET_AUDIO_24162;
	else if(wd.playback_rate == 22050)
		return SET_AUDIO_2205162;
	else if(wd.playback_rate == 16000)
		return SET_AUDIO_16162;
	else if(wd.playback_rate == 11025)
		return SET_AUDIO_11025162;
	else if(wd.playback_rate == 9600)
		return SET_AUDIO_9_6162;
	else if(wd.playback_rate == 8000)
		return SET_AUDIO_8162;
	else if(wd.playback_rate == 6000)
		return SET_AUDIO_6162;
	else
		return SET_AUDIO_48162;
}

#define HDAPARAM1(addr, nodeid, verbid, payload) ((addr << 28) | (nodeid << 20) | (verbid << 8) | payload)
#define HDAPARAM2(addr, nodeid, verbid, payload) ((addr << 28) | (nodeid << 20) | (verbid << 16) | payload)


static void hda_init_pci_regs(int type)
{
	/*taken from OSS*/
	
	DWORD tmp;
	
	switch(type)
	{								
		case DEV_SCH:
		case DEV_PCH:
			pci_read_config_word(&g_pci, DEVC, (WORD *) &tmp);				
			pci_write_config_word(&g_pci, DEVC, (tmp & (~0x0800)) );
			break;
		case DEV_ATI:
			pci_read_config_byte (&g_pci, 0x42, (BYTE *)&tmp);
			pci_write_config_byte (&g_pci, 0x42, (tmp & 0xf8) | 0x2);
			break;
		case DEV_NVIDIA:
			pci_read_config_byte(&g_pci, 0x4e, (BYTE *) &tmp);
			pci_write_config_byte(&g_pci, 0x4e, (tmp & 0xf0) | 0x0f);
			pci_read_config_byte(&g_pci, 0x4d, (BYTE *) &tmp);
			pci_write_config_byte(&g_pci, 0x4d, (tmp & 0xfe) | 0x01);
			pci_read_config_byte(&g_pci, 0x4c, (BYTE *) &tmp);
			pci_write_config_byte(&g_pci, 0x4c, (tmp & 0xfe) | 0x01);
			break;
		case DEV_ULI:
			pci_read_config_word (&g_pci, HDCTL,(WORD *) &tmp);
			pci_write_config_word (&g_pci, HDCTL, tmp | 0x10);
			pci_write_config_dword (&g_pci, HDBARU, 0);
			break;
	}
	
	pci_read_config_word(&g_pci, PCICMD, (WORD *)&tmp);
	pci_write_config_word(&g_pci, PCICMD, tmp | (MSE | BME));
	
	if (g_pci.vender_id != 0x1002 /*ATI VENDOR ID*/)
    {
		pci_read_config_byte(&g_pci, 0x44, (BYTE *)&tmp);
		pci_write_config_byte(&g_pci, 0x44, tmp & 0xf8);
	}
}

static BOOL hda_reset(void)
{
	int tmout;
	DWORD d0;
	
	hda_wr_reg32(HDAGCTL,(hda_rd_reg32(HDAGCTL) & ~CRST));
	tmout = 1000;
	while(((hda_rd_reg32(HDAGCTL) & CRST) != 0) && --tmout)	
		udelay(1000);
	udelay(1000);
	hda_wr_reg32(HDAGCTL,(hda_rd_reg32(HDAGCTL) | CRST));	
	tmout = 1000;
	while(((hda_rd_reg32(HDAGCTL) & CRST) == 0) && --tmout)	
		udelay(1000);
	udelay(1000);
	if(hda_rd_reg32(HDAGCTL) == 0)
	{
		log_msg("HDA: controller not ready.\n");
		return FALSE;
	}	
	if ((codecmask = (DWORD)hda_rd_reg16(HDASTATESTS)) == 0)
	{
		log_msg("HDA: no codec found after reset.\n");
		return FALSE;
	}
	d0 = (DWORD)hda_rd_reg16(HDAGCAP);
	if((d0 >> 12) == 0)
	{
		log_msg("HDA: no output streams supported.\n");
		return FALSE;
	}
	
	osdbase = (((d0 >> 8) & 0x0f) * HDASDSIZE) + HDASDBASE;
	hda_wr_reg8(OSDSTS, BCIS);
	/*hda_wr_reg8(OSDSTS, BCIS | FIFOE | DESE);
	hda_wr_reg16(HDASTATESTS, 0x01 | 0x02 | 0x04);
	hda_wr_reg8(HDARIRBSTS, 0x01 | 0x04);
	hda_wr_reg32(HDADPLBASE, 0);
	hda_wr_reg32(HDADPUBASE, 0);*/	
		
	log_msg("HDA: reset was succesful, codec mask: %04X, osd base %04X.\n", codecmask, osdbase);

	return TRUE;
}

static DWORD hda_send_codec_cmd(DWORD param, BOOL needanswer)
{
	int tmout;
	
	tmout = 1000;
	while(((hda_rd_reg16(HDAICIS) & ICB) != 0) && --tmout)	
		udelay(1000);
	udelay(1000);
	hda_wr_reg32(HDAICOI, param);
	hda_wr_reg16(HDAICIS, ICB | IRV);

	if(needanswer)
	{
		tmout = 1000;
		while(((hda_rd_reg16(HDAICIS) & (ICB | IRV)) != IRV) && --tmout)	
			udelay(1000);
		udelay(1000);
		return hda_rd_reg32(HDAICII);
	}	

	return 0;
}

static void hda_set_input_volume(DWORD codecaddr, DWORD nid, DWORD vol, int index)
{
	DWORD d0;
	
	if((sound_device_master_volume != -1) && (vol != 0))
		vol = vol - (sound_device_master_volume / ((float)31 / vol));
	
	d0 = (SET_IN_AMP | (index << 8) | vol) & ~SET_MUTE;
	hda_send_codec_cmd(HDAPARAM2(codecaddr, nid, SET_AMP_GAIN, SET_RIGHT_AMP | d0), FALSE);
	hda_send_codec_cmd(HDAPARAM2(codecaddr, nid, SET_AMP_GAIN, SET_LEFT_AMP  | d0), FALSE);
}

static void hda_set_output_volume(DWORD codecaddr, DWORD nid, DWORD vol)
{
	DWORD d0;
	
	if((sound_device_master_volume != -1) && (vol != 0))
		vol = vol - (sound_device_master_volume / ((float)31 / vol));
	
	d0 = (SET_OUT_AMP | vol) & ~SET_MUTE;
	hda_send_codec_cmd(HDAPARAM2(codecaddr, nid, SET_AMP_GAIN, SET_RIGHT_AMP | d0), FALSE);
	hda_send_codec_cmd(HDAPARAM2(codecaddr, nid, SET_AMP_GAIN, SET_LEFT_AMP  | d0), FALSE);
}

static HDAFUNCINFO * hda_find_node_in_list(DWORD nid, int subnodes)
{
	int d0 = 0;	
	while(d0 < subnodes)
	{
		if(nodes[d0].nid == nid)
			return &nodes[d0];
		d0 += 1;
	}
	return NULL;
}
				
static BOOL hda_parse_node(DWORD codecaddr, HDAFUNCINFO * nodeinfo, int subnodes)
{

	DWORD nid, caps, type, pintype, pincap, pinwig, pineapd, pdcapovrrd, subnid, d0;
	int chck, index, ncons;
	int d1, d2, shift, step, mask;
	HDAFUNCINFO * entry, * selentry = NULL;
		
	if((chck = nodeinfo->checked) != 0)
		return (chck & 0x02) ? TRUE : FALSE;
	
	nodeinfo->checked |= 0x01;
	
	nid = nodeinfo->nid;
	caps = nodeinfo->caps;
	type = nodeinfo->type;

	log_msg("HDA: parsing node id %04X.\n", nid);
	
	if(caps & IS_DIGITAL)
		return FALSE;
	
	if(type > VOL_KNOB)
		return FALSE;
		
	switch(type)
	{
		case (AUDIO_OUTPUT):
			if(caps & HAS_FORM_INFO)
			{
				d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, STREAM_SUPP), TRUE);
				if ((d0 & AUDIO_PCM) == 0)
					return FALSE;
		
				d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, PCM_SUPP), TRUE);
				if ((d0 & AUDIO_4816) != AUDIO_4816)
					return FALSE;
			}
			hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, SET_CVT_STRM, 0x10), FALSE);
			//vic
			hda_send_codec_cmd(HDAPARAM2(codecaddr, nid, SET_CVT_FORM, hda_set_rate()), FALSE);
			log_msg("HDA: found DAC, node id %04X.\n", nid);
			break;
		
		case (AUDIO_INPUT):		
			return FALSE;	
		
		case (PIN_CMPLX):
			d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PIN_CFG, 0), TRUE);	
			
			if((d0 >> 30) & 1)
				return FALSE;
			
			pdcapovrrd = ((d0 >> 8) & 1);
			
			pintype = (d0 >> 20) & 0xf;

			if(pintype > HP_OUT)
				return FALSE;	

			if((pintype == SPEAKER) && (optspeak == 1))
				return FALSE;
			
			pincap = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, PIN_CAP), TRUE);
			
			pinwig = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PIN_CTRL, 0), TRUE);
			if(pincap & PIN_OUT_CAP)
			{
				pinwig |= PIN_OUT_ENAB;
				if(pincap & PIN_HP_CAP)
					pinwig |= PIN_HP_ENAB;
			}
			else
				return FALSE;
			hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, SET_PIN_CTRL, pinwig), FALSE);

			pineapd = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PIN_EAPDBTL, 0), TRUE);
			if(pincap & PIN_EAPD_CAP)
				pineapd |= EAPD_ENABLE;
			/*if(pincap & PIN_BIO_CAP)
				pineapd |= BTL_ENABLE;*/
			hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, SET_PIN_EAPDBTL, pineapd), FALSE);		
				
			log_msg("HDA: found pin type ");
			switch(pintype)
			{
				case (LINE_OUT):
					log_msg("LINE OUT");
					break;
				case (SPEAKER):
					log_msg("SPEAKER");
					break;
				case (HP_OUT):
					log_msg("HP OUT");
					break;
			}
			log_msg(", node id %04X.\n", nid);
			
			if((pincap & PIN_PD_CAP) && (pdcapovrrd == 0))
			{
				if(hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PIN_SENSE, 0), TRUE) & DEV_ATTACHED)
					log_msg("HDA: a jack is plugged-in.\n");
				else
					log_msg("HDA: no jack is plugged-in.\n");
			}
			else
				log_msg("HDA: this pin doesn't support presence detection.\n");	
			
			break;
	}
	
	index = 0;
	if(caps & HAS_CON_LST)
	{		
		d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, CON_LIST), TRUE);
		if (d0 & LONG_CON_LST)
		{
			shift = 16;
			step = 2;
			mask = 0x7fff;
		}
		else
		{
			shift = 8;
			step = 4;
			mask = 0x7f;
		}
		ncons = d0 & 0x7f;
		log_msg("HDA: the number of connections for node id %04X is %d.\n", nid, ncons);	
		
		d1 = 0;
		while(d1 < ncons)
		{
			d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_CON_LST_ENT, d1), TRUE);
			d2 = 0;
			while(((d1+d2) < ncons) && (d2 < step))
			{
				subnid = (d0 >> (d2 * shift)) & mask;
				log_msg("HDA: index %d has node id %04X.\n", d1 + d2, subnid);
				entry = hda_find_node_in_list(subnid, subnodes);
				if(entry && (entry->type != PIN_CMPLX))
				{	
					if(hda_parse_node(codecaddr, entry, subnodes) == TRUE)
					{
						if((selentry == NULL) || ((selentry->type == AUDIO_OUTPUT) && 
							((entry->type == MIXER) || (entry->type == SELECTOR))))
						{
							index = d1 + d2;
							selentry = entry;
						}
					}
				}	
				d2 += 1;
			}
			d1 += step;
		}
		
		if(selentry == NULL)
			return FALSE;
		
		log_msg("HDA: selected connection for node id %04X is index %d.\n", nid, index);
		hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, SET_CON_SEL, index), FALSE);
	}
	
	if(caps & HAS_POW_CTRL)
		hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, SET_POW_STATE, STATE_D0), FALSE);
			
	if(caps & HAS_OUT_AMP)
	{
		if(caps & HAS_AMP_PARAM) 
			d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, OUT_AMP_CAP), TRUE);
		else
			d0 = outampcap;
		hda_set_output_volume(codecaddr, nid, (d0 >> 8) & 0x7f);
	}
	
	if(caps & HAS_IN_AMP)
	{
		if(caps & HAS_AMP_PARAM)
			d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, IN_AMP_CAP), TRUE);
		else
			d0 = inampcap;
		hda_set_input_volume(codecaddr, nid, (d0 >> 8) & 0x7f, index & 0x0f);
	}
	
	nodeinfo->checked |= 0x02;

	return TRUE;
}

static BOOL hda_node_init(DWORD codecaddr, DWORD nid)
{
	DWORD d0;
	int d1, d2, d3, firstnode, subnodes;
	BOOL b0 = FALSE;
	
	d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, FUNC_TYPE), TRUE);
	if ((d0 & AUDIO_FUNC) == 0)
		return FALSE;
		
	log_msg("HDA: node id %04X supports audio functions.\n", nid);
	
	hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, SET_POW_STATE, STATE_D0), FALSE);
		
	outampcap = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, OUT_AMP_CAP), TRUE);
	inampcap = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, IN_AMP_CAP), TRUE);

	d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, NODE_CNT), TRUE);
	
	d1 = (int)((d0 >> 16) & 0xff);
	subnodes = (int)(d0 & 0xff);
	
	log_msg("HDA: starting function/widget id %04X, total number of functions/widgets %d.\n", d1, subnodes);
	
	if((nodes = (HDAFUNCINFO *)malloc(subnodes * sizeof(HDAFUNCINFO))) == 0)
	{
		log_msg("HDA: not enough memory.\n");
		return FALSE;
	}
	
	d2 = d1 + subnodes;
	d3 = 0;
	while(d1 < d2) 
	{
		nodes[d3].nid = d1;
		d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, d1, GET_PARAM, WIDGET_CAP), TRUE);
		nodes[d3].caps = d0;
		nodes[d3].type = (d0 >> 20) & 0x0f;
		nodes[d3].checked = 0;
		d1 += 1;
		d3 += 1;
	}
	
	d3 = 0;
	while(d3 < subnodes)
	{
		if (hda_parse_node(codecaddr, &nodes[d3], subnodes) == TRUE)
			b0 = TRUE;		
		d3 += 1;
	}
	free(nodes);
	printf("done node init\n");
	
	return b0;
}

static BOOL hda_codec_init(DWORD codecaddr)
{
	DWORD d0;
	int d1, d2;
	
	d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, 0, GET_PARAM, VENDOR_ID), TRUE);
	
	codecvid = d0 >> 16;
	codecdid = d0 & 0xffff;
	
	log_msg("HDA: codec found, address %04X, vendor id %04X, device id %04X.\n", codecaddr, codecvid, codecdid);

	d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, 0, GET_PARAM, NODE_CNT), TRUE);
	
	d1 = (int)((d0 >> 16) & 0xff);
	d2 = (int)(d0 & 0xff);
	
	log_msg("HDA: starting node id %04X, total number of nodes %d.\n", d1, d2);
	
	d2 += d1;
	
	while(d1 < d2) 
	{
		if(hda_node_init(codecaddr, d1))
			return TRUE;
		d1 += 1;		
	}
	
	return FALSE;
}

static BOOL hda_init(void)
{
	
	DWORD d0;
	
	if(hda_reset() == FALSE)
		return FALSE;
	//codec skip
	return TRUE;

	d0 = 0;	
	while(d0 < HDA_MAX_CODECS)
	{
		if(codecmask & (1 << d0))
			if (hda_codec_init(d0) == TRUE)
				return TRUE;
		d0 += 1;
	}	
	log_msg("HDA: no suitable codec was found.\n");
	
	return FALSE;
}


static int hda_get_iobase(void)
{
	DWORD address=0;
	
	/* get io base address */
	pci_read_config_dword(&g_pci, HDBARL, &address);

	if((address &= 0xfffffff9) == 0)
	{
		log_msg("HDA: device found, but disabled.\n");
		return FALSE;
	}
	
	log_msg("HDA: i/o base found at %08X.\n", address);
	
	g_pci.base0 = address;
	return TRUE;
}

static void hda_free_dma_mem(void)
{
	/*
	if(vds_helper_unlock(get_address_dosmem64k_for_dma(), &g_dosmem64k_phys_table[0], 16) == FALSE){
		log_msg("HDA: vds_helper_unlock error.\n");
	}
	if(vds_helper_unlock(get_address_dosmem4k(), &g_dosmem4k_phys_table[0], 1) == FALSE){
		log_msg("HDA: vds_helper_unlock error.\n");
	}
	free_dosmem64k_for_dma();
	free_dosmem4k();	
	*/
}
	
static BOOL hda_alloc_dma_mem()
{
	/*
	if(allocate_dosmem64k_for_dma(_16BITSTEREO) == FALSE)
	{
		log_msg("HDA: allocate_dosmem64k_for_dma() error.\n");
		return FALSE;
	}
	if(vds_helper_lock(get_address_dosmem64k_for_dma(), &g_dosmem64k_phys_table[0], 16) == FALSE)
	{
		log_msg("HDA: vds_helper_lock() error.\n");
		return FALSE;
	}
*/
	if(allocate_dosmem4k() == FALSE)
	{
		log_msg("HDA: allocate_dosmem4k() error.\n");
		return FALSE;
	}
/*	
	if(vds_helper_lock(get_address_dosmem4k(), &g_dosmem4k_phys_table[0], 1) == FALSE)
	{
		log_msg("HDA: vds_helper_lock() error.\n");
		return FALSE;
	}
	*/
	return TRUE;
}

static DWORD hda_current_pos(void)
{	
	DWORD pos=0;
/*
	w_enter_critical();
	pos = hda_rd_reg32(OSDLPIB); 
	w_exit_critical();
	
	if((pos == 0) || (pos > g_dma_buff_size))
		pos = g_dma_buff_size;
*/
	return (g_dma_buff_size - pos);	
	
}

static void hda_stop(void)
{
	int tmout;
	
	hda_wr_reg8(OSDCTL, (hda_rd_reg8(OSDCTL) & ~RUN));
	tmout = 1000;
	while(((hda_rd_reg16(OSDCTL) & RUN) != 0) && --tmout)	
		udelay(1000);
	udelay(1000);
	hda_wr_reg8(OSDCTL, (hda_rd_reg8(OSDCTL) | SRST));
	tmout = 1000;
	while(((hda_rd_reg16(OSDCTL) & SRST) == 0) && --tmout)	
		udelay(1000);
	udelay(1000);
}

static void hda_run(void)
{
	int tmout;
	
	hda_wr_reg8(OSDCTL, (hda_rd_reg8(OSDCTL) | RUN));
	tmout = 1000;
	while(((hda_rd_reg8(OSDCTL) & RUN) == 0) && --tmout)	
		udelay(1000);
	udelay(1000);
}
	
static void hda_exit(void)
{
	hda_stop();
	hda_free_dma_mem();
	//hda_remove_mapping();
	wavedevice_struct_init();
}

static void hda_prepare(void)
{
	DWORD a0;
	int d0, tmout;
	
	a0 = get_address_dosmem4k();
	d0 = 0;
	while(d0 < BDL_ENTRIES)
	{
		int *ptr=(int*)a0;
		ptr[0]=g_dosmem64k_phys_table[d0 & 0x0F];	/*low base*/
		ptr[1]=0;									/*high base*/		
		ptr[2]=0x1000;							/*4KB length in bytes*/
		ptr[3]=0;								/*IOC & reserved*/		
		a0 += 16;
		d0 += 1;
	}
		
	hda_wr_reg8(OSDCTL, (hda_rd_reg8(OSDCTL) & ~SRST));
	tmout = 1000;
	while(((hda_rd_reg8(OSDCTL) & SRST) != 0) && --tmout)	
		udelay(1000);
	udelay(1000);
	hda_wr_reg32(OSDCTL, (hda_rd_reg32(OSDCTL) & 0xff0fffff) |  (1 << 20));	/*set stream*/
	hda_wr_reg32(OSDCBL, g_dma_buff_size);
	hda_wr_reg16(OSDLVI, BDL_ENTRIES - 1);
	//vic
	hda_wr_reg16(OSDFMT, hda_set_rate());	/*48 kHz, 16 bits per sample, 2 channels*/
	hda_wr_reg32(OSDBDPL, get_address_dosmem4k());
	hda_wr_reg32(OSDBDPU, 0);
}

static BOOL hda_start(int rate)
{
	static char * device_name_hda = NULL;
	int d0;
	
	
	
	memset(&g_pci, 0, sizeof(PCI_DEV));

	if(check_pci_bios() == FALSE)
	{
		log_msg("HDA: No PCI BIOS found.\n");
		return FALSE;
	}

	/* detection */
	d0 = 0;
	while(hda_dev_list[d0].vender_id != 0x0000)
	{
		g_pci.vender_id 	= hda_dev_list[d0].vender_id;
		g_pci.device_id 	= hda_dev_list[d0].device_id;
		g_pci.sub_vender_id = 0;
		g_pci.sub_device_id = 0;
		if(find_pci_device(&g_pci) == TRUE)
		{
			log_msg("HDA: %s found.\n", hda_dev_list[d0].name);
			break;
		}
		d0 += 1;
	}
	if(hda_dev_list[d0].vender_id == 0x0000){
		log_msg("HDA: no compatble device found.\n");
		return FALSE;
	}
	
	device_name_hda = hda_dev_list[d0].name;
	
	hda_init_pci_regs(DEV_ATI);
	
	if (hda_get_iobase() == FALSE)
		return FALSE;

	if((hda_init() == FALSE) || (hda_alloc_dma_mem() == FALSE))
	{	
		//hda_remove_mapping();
		return FALSE;
	}
	
	//vic
	wd.playback_rate   = rate;
	
	hda_stop();
	hda_prepare();
	hda_run();	

	wd.device_name	   = device_name_hda;
	wd.pcm_format	   = _16BITSTEREO;
	wd.device_exit	   = hda_exit;
	wd.pcm_upload	   = common_pcm_upload_func;
	wd.get_current_pos = hda_current_pos;
	wd.initialized = TRUE;
	return TRUE;
}

static BOOL hda_start_no_speaker(int rate)
{
	optspeak = 1;
	return hda_start(rate);
}









void w_set_watermark(float latency, DWORD samples_per_frame)
{
	if(wd.initialized == FALSE)
		return;

	if(latency < 1.0) latency = 1.0;
	g_latencym = latency;
	g_latency = samples_per_frame * latency;
	g_dma_average_cnt = samples_per_frame << DMA_AVERAGE_SHIFT_COUNT;
	g_samples_per_frame = samples_per_frame;
	w_reset_write_cursor(0);

	w_clear_buffer();
	printf("clear done\n");
}

void w_set_master_volume(int volume)
{
	g_master_volume = volume;
}

DWORD w_get_current_req(void)
{
	if(wd.initialized == FALSE) return 0;
	return g_current_req;
}

DWORD w_get_next_req(void)
{
	if(wd.initialized == FALSE) return 0;
	return g_next_req;
}

static void calc_next_req(void)
{
	if(wd.initialized == FALSE) return;

	g_dma_remainder += g_dma_average_cnt;
	g_dma_remainder += g_dma_dt;
	g_dma_dt = 0;
	g_current_req = g_next_req;
	g_next_req = g_dma_remainder >> DMA_AVERAGE_SHIFT_COUNT;
	g_dma_remainder = g_dma_remainder & DMA_AVERAGE_MASK;

	if(g_current_req == 0 || g_current_req > SAMPLECNT) g_current_req = 1;
	if(g_next_req	 == 0 ||	g_next_req > SAMPLECNT) g_next_req = 1;
}


void w_reverse_stereo(int flag)
{
	stereo_reverse_flag = (flag) ? TRUE : FALSE;
}

void w_lock_mixing_buffer(int currentsamplecount)
{
	DWORD d0;

	if(wd.initialized == FALSE) return;

	if(currentsamplecount != 0)
		g_current_req = currentsamplecount;

	if(g_current_req > SAMPLECNT)
		g_current_req = SAMPLECNT;
	if(g_current_req == 0)
		g_current_req = 1;

	d0 = 0;
	while(d0 < g_current_req){
		mixing_buff[(d0*2) + 0] = 0;
		mixing_buff[(d0*2) + 1] = 0;
		d0 += 1;
	}
}

void w_mixing_zero(void)
{
	int d1;
	int leftindex  = 0;
	int rightindex = 1;

	if(stereo_reverse_flag == TRUE){
		leftindex  = 1;
		rightindex = 0;
	}

	if(wd.initialized == FALSE) return;
	if(g_current_req > SAMPLECNT) return;

	d1 = 0;
	while(d1 < g_current_req){
		mixing_buff[(d1*2) + leftindex ] = 0;
		mixing_buff[(d1*2) + rightindex] = 0;
		d1 += 1;
	}
}

void w_mixing8(char data[], DWORD length, int leftvol, int rightvol)
{
	DWORD dt;
	DWORD d0;
	int d1;
	long d2;
	int leftindex  = 0;
	int rightindex = 1;

	if(stereo_reverse_flag == TRUE){
		leftindex  = 1;
		rightindex = 0;
	}

	if(wd.initialized == FALSE) return;
	if(g_current_req > SAMPLECNT) return;

	dt = (length << 16) / g_current_req;
	d0 = 0;
	d1 = 0;
	while(d1 < g_current_req){
		d2 = (long)data[d0 >> 16];
		d2 = d2 << 8;
		mixing_buff[(d1*2) + leftindex ] += (d2 * leftvol)	/ 256;
		mixing_buff[(d1*2) + rightindex] += (d2 * rightvol) / 256;
		d0 += dt;
		d1 += 1;
	}
}

void w_mixing(short data[], DWORD length, int leftvol, int rightvol)
{
	DWORD dt;
	DWORD d0;
	int d1;
	long d2;
	int leftindex  = 0;
	int rightindex = 1;

	if(stereo_reverse_flag == TRUE){
		leftindex  = 1;
		rightindex = 0;
	}

	if(wd.initialized == FALSE) return;
	if(g_current_req > SAMPLECNT) return;

	dt = (length << 16) / g_current_req;
	d0 = 0;
	d1 = 0;
	while(d1 < g_current_req){
		d2 = (long)data[d0 >> 16];
		mixing_buff[(d1*2) + leftindex ] += (d2 * leftvol)	/ 256;
		mixing_buff[(d1*2) + rightindex] += (d2 * rightvol) / 256;
		d0 += dt;
		d1 += 1;
	}
}

void w_mixing_stereo(short data[], DWORD length, int leftvol, int rightvol)
{
	DWORD dt;
	DWORD d0;
	int d1;
	long left;
	long right;
	int leftindex  = 0;
	int rightindex = 1;

	if(stereo_reverse_flag == TRUE){
		leftindex  = 1;
		rightindex = 0;
	}

	if(wd.initialized == FALSE) return;
	if(g_current_req > SAMPLECNT) return;

	dt = (length << 16) / g_current_req;
	d0 = 0;
	d1 = 0;
	while(d1 < g_current_req){
		left  = (long)data[((d0 >> 16) * 2)];
		right = (long)data[((d0 >> 16) * 2) + 1];
		mixing_buff[(d1*2) + leftindex ] += (left  * leftvol)  / 256;
		mixing_buff[(d1*2) + rightindex] += (right * rightvol) / 256;
		d0 += dt;
		d1 += 1;
	}
}


void w_unlock_mixing_buffer(void)
{
	if(wd.pcm_upload != NULL){
		(*wd.pcm_upload)();
	}
}
/*
void w_set_sb_cursor_offset(int offset)
{
	sb_cursor_offset = offset & SAMPLECNTMASK;
}
*/
static DWORD common_dma_current_pos(void)
{
	DWORD d0=0;
	/*
	w_enter_critical();
	d0 = _dma_todo(wd.isa_dma);
	w_exit_critical();
*/
	return d0;
}

static DWORD _dma_counter(void)
{
	DWORD d0 = 0;

	if(wd.get_current_pos != NULL){
		d0 = (*wd.get_current_pos)();
	}
	return d0;
}

static DWORD get_played_sample_count(void)
{
	DWORD curr;
	DWORD d0;

	if(wd.initialized == FALSE) return 0;

	curr = w_get_play_cursor();
	d0 = w_calc_distance(curr, g_prev_play_cursor);
	g_prev_play_cursor = curr;

	return d0;
}


DWORD w_get_buffer_size(void)
{
	return SAMPLECNT;
}

DWORD w_get_play_cursor(void)
{
	return (SAMPLECNT - 1) - (_dma_counter() >> g_dmacnt_shift_count);
}

DWORD w_get_write_cursor(void)
{
	return g_write_cursor;
}

void w_set_write_cursor(DWORD position)
{
	g_write_cursor = w_calc_position(position, 0);
}

void w_reset_watermark(void)
{
	w_reset_write_cursor(0);
}

void w_reset_watermark_ex(DWORD samplecount)
{
	w_reset_write_cursor(samplecount);
}

void w_reset_write_cursor(DWORD samplecount)
{
	DWORD current;

	if(wd.initialized == FALSE) return;

	current 		   = w_get_play_cursor();
	g_prev_play_cursor = current;

	if(samplecount <= g_latency){
		samplecount = g_latency - samplecount;
	}else{
		samplecount = 0;
	}
	w_set_write_cursor( w_calc_position(current, samplecount) );

	g_dma_remainder = 0;
	g_dma_dt = 0;
	calc_next_req();
	calc_next_req();
}


DWORD w_calc_distance(DWORD cursor1, DWORD cursor2)
{
	return (cursor1 - cursor2) & SAMPLECNTMASK;
}

DWORD w_calc_position(DWORD cursor1, DWORD cursor2)
{
	return (cursor1 + cursor2) & SAMPLECNTMASK;
}


DWORD w_get_requested_sample_count(void)
{
	DWORD d0;

	d0 = w_get_latency();
	if(d0 > w_get_buffer_size()/2){
		w_reset_watermark();
		return 0;
	}
	if(d0 >= g_latency) return 0;
	return g_latency - d0;
}


DWORD w_get_latency(void)
{
	DWORD d0;

	if(wd.initialized == FALSE){
		d0 = 0;
	}else{
		d0 = w_calc_distance(w_get_write_cursor(), w_get_play_cursor());
	}
	return d0;
}

int w_get_watermark_status(void)
{
	DWORD d0;
	int result;

	if(wd.initialized == FALSE) return 0;

	d0 = w_get_latency();

	if(d0 >= (DWORD)(g_latency*2.5)){
		result = -2;							// overrun
	}else if(d0 >= (DWORD)(g_latency*1.5)){
		result = 2; 							// full
	}else if(d0 >= (DWORD)g_latency){
		result = 1; 							// good
	}else if(d0 >= g_samples_per_frame){
		result = 0; 							// ok
	}else if(d0 >= g_samples_per_frame/8){
		result = -1;							// lack
	}else{
		result = -2;							// overrun
	}

	return result;
}


static DWORD ofdt = 0;
static DWORD ufdt = 0;
static int dma_dt = 0;

DWORD w_adjust_latency_for_vsync(void)
{
	DWORD d0;

	if(wd.initialized == FALSE) return 0;

	d0 = w_calc_distance(w_get_write_cursor(), w_get_play_cursor());
	if( d0 >= (g_latency + 1) ){
		ofdt = d0 - g_latency;
		g_dma_overflow	+= 1;
		g_dma_underflow  = 0;
	}else if( d0 <= (g_latency - 1) ){
		ufdt = g_latency - d0;
		g_dma_overflow	 = 0;
		g_dma_underflow += 1;
	}else{
		g_dma_overflow	 = 0;
		g_dma_underflow  = 0;
	}
	if(g_dma_overflow > 16)
		g_dma_overflow = 16;
	if(g_dma_underflow > 16)
		g_dma_underflow = 16;
	ofdt = ofdt  * (g_dma_overflow	& 0xFE);
	ufdt = ufdt  * (g_dma_underflow & 0xFE);
	if(ofdt > (wd.playback_rate  >> 5))
		ofdt = (wd.playback_rate  >> 5);
	if(ufdt > (wd.playback_rate  >> 5))
		ufdt = (wd.playback_rate  >> 5);

	g_dma_dt = ufdt - ofdt;
	dma_dt = g_dma_dt;			// for debug

	calc_next_req();

	return d0;
}


float w_calc_samples_per_vsync(void (*__vsync)(void), int vsync_count)
{
	DWORD d0;
	DWORD result[2];
	int d1;
	int count;
	int limit;
	int d7;

	if(wd.initialized == FALSE) return 0;
	limit = 16;
	while(limit > 0){
		d7 = 0;
		while(d7 < 2){
			d1 = 0;
			while(d1 < 4){
				count = 0;
				while(count < vsync_count){
					(*__vsync)();
					count += 1;
				}
				get_played_sample_count();
				d1 += 1;
			}
			d0 = 0;
			d1 = 0;
			while(d1 < (256/8)){
				count = 0;
				while(count < vsync_count){
					(*__vsync)();
					count += 1;
				}
				d0 += get_played_sample_count();
				d1 += 1;
			}
			result[d7] = d0;
			d7 += 1;
		}
		if(result[0] > result[1]){
			d0 = result[0] - result[1];
		}else{
			d0 = result[1] - result[0];
		}
		if(d0 < 16) break;
		limit -= 1;
	}

	d0 = (result[0] + result[1]) * 4;
	g_latency = (d0 * g_latencym) / 256;
	g_dma_average_cnt = d0;
	calc_next_req();
	calc_next_req();

	return (float)d0/256;
}


void w_clear_buffer(void)
{
	int d0;

	if(wd.initialized == FALSE) return;

	for(d0 = 0; d0 <= SAMPLECNT; d0 += 1024){
		w_lock_mixing_buffer(1024);
		w_mixing_zero();
		w_unlock_mixing_buffer();
	}
}

DWORD w_get_nominal_sample_rate(void)
{
	if(wd.initialized == FALSE) return 0;
	return wd.playback_rate;
}

char *w_get_device_name(void)
{
	if(wd.initialized == FALSE) return NULL;
	return wd.device_name;
}


int w_sound_device_init(int device_no, int rate)
{
	int result;

	if(wd.initialized == TRUE){
		log_msg("already initialized.\n");
		return FALSE;
	}
	if(rate < 0)
		rate = 22050;
	if(rate > 64000)
		rate = 64000;

	switch(device_no){
		case 28:
			result = hda_start(rate);
			break;
		default:
			result = FALSE;
			log_msg("invalid sound device number.\n");
	}
	w_set_watermark(2.2, 1200);

	return result;
}

void w_sound_device_exit(void)
{
	w_clear_buffer();
	if(wd.device_exit != NULL){
		(*wd.device_exit)();
	}
}


DWORD w_get_actual_sample_rate(void)
{
	DWORD d0;
	DWORD result[2];
	int d1;
	int d7;
	int limit;
	clock_t prev;
	clock_t curr;

	if(wd.initialized == FALSE) return 0;

	w_set_watermark(2.0, w_get_nominal_sample_rate()/60);

	prev = clock();
	while(1){
		if( (clock() - prev) >= CLOCKS_PER_SEC/2 ) break;
		get_played_sample_count();
	}

	limit = 16;
	while(limit > 0){
		d7 = 0;
		while(d7 < 2){
			d0 = 0;
			d1 = 65536;
			get_played_sample_count();
			while(d1 > 0){
                if(get_played_sample_count() != 0) break;
				d1 -= 1;
			}
			prev = clock();
			while(1){
				d0 += get_played_sample_count();
				curr = clock();
				if( (curr - prev) >= CLOCKS_PER_SEC/2 ) break;
			}
			result[d7] = d0;
			d7 += 1;
		}
		if(result[0] > result[1]){
			d0 = result[0] - result[1];
		}else{
			d0 = result[1] - result[0];
		}
		if(d0 <= 8) break;
		limit -= 1;
	}
	actual_sample_rate = result[0] + result[1];
	return actual_sample_rate;
}


void w_set_device_master_volume(int volume)
{
	//		   max	  min
	// volume:	 0	-  31, or  -1(default)
	/*
	if( (0 <= volume) && (volume <= 31) ){
		sound_device_master_volume	= volume;
	}else{
		sound_device_master_volume	= -1;
	}*/
	
	if(volume < 0)
		volume = 0;
	else if(volume > 31)
		volume = 31;
	sound_device_master_volume = volume ^ 0x1f;
}



#endif

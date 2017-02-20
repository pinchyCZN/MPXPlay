#include <stdio.h>
#include <stdlib.h>
#include <i86.h>
#include <conio.h>
#include <dpmi.h>
#include <time.h>
#include <string.h>
#include <math.h>
//#include <sys/farptr.h>
//#include <pc.h>
#include <ctype.h>
#include <stdarg.h>
#include "WSS.h"

#ifndef ALLEGRO_DJGPP
#pragma off (unreferenced);
typedef  unsigned char uint8_t;
typedef  unsigned short uint16_t;
typedef  unsigned int uint32_t;
   #define _farnspokeb(addr, val)   (*((uint8_t  *)(addr)) = (val))
   #define _farnspokew(addr, val)   (*((uint16_t *)(addr)) = (val))
   #define _farnspokel(addr, val)   (*((uint32_t *)(addr)) = (val))
	#define _farpeekb(seg,addr) (*((uint8_t  *)(addr)))
   #define _farnspeekb(addr)        (*((uint8_t  *)(addr)))
   #define _farnspeekw(addr)        (*((uint16_t *)(addr)))
   #define _farnspeekl(addr)        (*((uint32_t *)(addr)))
   #define __attribute__()
   #define __inline__ _inline
   #define __dpmi_int(a,b) int386(a,b,b)
   #define __dpmi_regs union REGS
   int _dos_ds=0;
#endif

static void logerror_(const char *text,...)
{
	  va_list arg;
	  va_start(arg,text);
//	  vfprintf(stdout,text,arg);
	  va_end(arg);
}
int _farsetsel(short sel)
{
	return 0;
}
/***********************	TICKER	*************************/

typedef signed long long	   	 INT64;
typedef unsigned long long	UINT64;
typedef INT64 cycles_t;

	
#define uclock_t cycles_t
#define osd_cycles() clock()
#define osd_cycles_per_second() CLOCKS_PER_SEC



/***********************  COMMON  *************************/


#define _8BITMONO		1
#define _8BITSTEREO 	2
#define _16BITSTEREO	3


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


static void common_pcm_upload_func(void);
static DWORD common_dma_current_pos(void);



static BOOL _dma_allocate_mem4k(int *sel, unsigned long *phys);
static int _dma_allocate_mem(int *sel, unsigned long *phys);
static void _dma_start(int channel, unsigned long addr, int size, int auto_init, int input);
static void _dma_stop(int channel);
static DWORD _dma_todo(int channel);
static int _dma_count;


static DWORD _dma_counter(void);

/* dos memory 4k bytes for work area */
static BOOL allocate_dosmem4k(void);
static void free_dosmem4k(void);
static DWORD get_address_dosmem4k(void);
static int g_dosmem4k_sel = 0;
static unsigned long g_dosmem4k_addr = 0;
static DWORD g_dosmem4k_phys_table[1];
//static __dpmi_meminfo info4k;

/* dos memory 64k cyclic playing buffer */
static BOOL allocate_dosmem64k_for_dma(int format);
static void free_dosmem64k_for_dma(void);
static DWORD get_address_dosmem64k_for_dma(void);
static int g_wss_dma_sel = 0;
static unsigned long g_wss_dma_addr = 0;
static DWORD g_dosmem64k_phys_table[16];
static int g_dma_buff_size;
static int g_dmacnt_shift_count;
static __dpmi_meminfo info64k;



#define SAMPLECNT	  16384
#define SAMPLECNTMASK 0x3FFF


static _go32_dpmi_seginfo old_handler, new_handler;
static int old_handler_irq_vector = -1;

static int tick = 0;

static DWORD actual_sample_rate = 0;
static int sound_device_master_volume  = -1;	// 0:max - 31:min, or -1(default)

static int stereo_reverse_flag = FALSE;


/*	function  */

static DWORD get_played_sample_count(void);
static void calc_next_req(void);


/* error message func */

#define ERROR_MESSAGE_BUFFER_SIZE	1024
static int error_message_pointer = 0;
static char error_message[ERROR_MESSAGE_BUFFER_SIZE] = "\n";

static void set_error_message(char *message)
{
	while(1){
		if(error_message_pointer >= (ERROR_MESSAGE_BUFFER_SIZE - 1)) break;
		if(*message == '\0') break;
		error_message[error_message_pointer++] = *message++;
	}
	error_message[error_message_pointer] = '\0';
}

static char* get_error_message(void)
{
	return &error_message[0];
}

static void clear_error_message(void)
{
	error_message[0] = '\0';
	error_message_pointer = 0;
}


/***********************  COMMON END  *********************/







/***********************  ULTRASOUND  *********************/

static char device_name_ultrasound[]  = "Ultrasound";

static int ultrasound_start(int rate_no);
static void ultrasound_exit(void);

static int ultramax_start(int rate_no);

static int get_ultrasnd(void);
static DWORD u_get_current_location(BYTE voice);
static void u_upload(DWORD addr, BYTE data);
static volatile int u_dma_upload(DWORD addr, WORD count, DWORD gusaddr,
					int flag, DWORD addr2, WORD count2, DWORD gusaddr2);
static void u_start_dma_upload(DWORD addr, WORD count, DWORD gusaddr);
static DWORD convert_to_16bit(DWORD address);
static void u_abort_dma_upload(void);


static int u_prim_voice = 0;
static int u_base = -1;
static int u_cmd;
static int u_datal;
static int u_datah;
static int u_page;
static int u_status;
static int u_dram;
static DWORD u_freq_value = 0;
static volatile int u_dma_busy_flag = FALSE;
static volatile int u_twoflag = FALSE;
static DWORD u_nextaddr;
static WORD  u_nextcount;
static DWORD u_nextgusaddr;



/********************************************************************
 *	PCI BIOS helper funtions
 ********************************************************************/

#define PCI_ANY_ID	((WORD)(~0))

typedef struct {
	WORD vender_id;
	WORD device_id;
	WORD sub_vender_id;
	WORD sub_device_id;
	WORD device_bus_number;
	DWORD base0;
	DWORD base1;
	DWORD base2;
	DWORD base3;
	DWORD base4;
	DWORD base5;
} PCI_DEV;

static PCI_DEV g_pci;


int dump_audio()
{
	int i,j;
	char *buf;
	buf=g_pci.base0;
	for(j=0;j<8;j++){
		for(i=0;i<0x180;i++){
			unsigned char a;
			if((i%16)==0 && i>0)
				printf("\n");
			if((i%16)==0)
				printf("%04X:",i);
			else if((i%4)==0 && i>0)
				printf(" ");
			a=buf[i];
			printf("%02X",a);
		}
		printf("\n");
	}
}


static BOOL pci_read_config_byte(PCI_DEV *pci, int idx, BYTE *data)
{
	__dpmi_regs r;
	BOOL result = TRUE;

	r.w.ax = 0xB108;						/* read config byte */
	r.x.ebx = (DWORD)pci->device_bus_number;
	r.w.di = (DWORD)idx;
	__dpmi_int(0x1a, &r);
	if(r.x.cflag!=0){
		logerror_("pci read config byte failed\n");
		result = FALSE;
		r.w.cx = 0;
	}
	*data = (BYTE)r.h.cl;
	return result;
}

static BOOL pci_read_config_word(PCI_DEV *pci, int idx, WORD *data)
{
	__dpmi_regs r;
	BOOL result = TRUE;

	r.w.ax = 0xB109;						/* read config word */
	r.x.ebx = (DWORD)pci->device_bus_number;
	r.w.di = (DWORD)idx;
	__dpmi_int(0x1a, &r);
	if( r.h.ah != 0 ){
		logerror_("pci read config word failed\n");
		result = FALSE;
		r.w.cx = 0;
	}
	*data = (WORD)r.w.cx;
	return result;
}

static BOOL pci_read_config_dword(PCI_DEV *pci, int idx, DWORD *data)
{
	__dpmi_regs r;
	BOOL result = TRUE;

	r.w.ax = 0xB10A;						/* read config dword */
	r.x.ebx = (DWORD)pci->device_bus_number;
	r.w.di = (DWORD)idx;
	__dpmi_int(0x1a, &r);
	if(r.x.cflag!=0){
		logerror_("pci read config dword failed\n");
		result = FALSE;
		r.w.cx = 0;
	}
	*data = (DWORD)r.x.ecx;
	return result;
}

static BOOL pci_write_config_byte(PCI_DEV *pci, int idx, BYTE data)
{
	__dpmi_regs r;
	BOOL result = TRUE;

	r.w.ax = 0xB10B;						/* write config byte */
	r.x.ebx = (DWORD)pci->device_bus_number;
	r.w.cx = (DWORD)data;
	r.w.di = (DWORD)idx;
	__dpmi_int(0x1a, &r);
	if( r.h.ah != 0 ){
		logerror_("pci write config byte failed\n");
		result = FALSE;
	}
	return result;
}

static BOOL pci_write_config_word(PCI_DEV *pci, int idx, WORD data)
{
	__dpmi_regs r;
	BOOL result = TRUE;

	r.w.ax = 0xB10C;						/* write config word */
	r.x.ebx = (DWORD)pci->device_bus_number;
	r.w.cx = (DWORD)data;
	r.w.di = (DWORD)idx;
	__dpmi_int(0x1a, &r);
	if( r.h.ah != 0 ){
		logerror_("pci write config word failed\n");
		result = FALSE;
	}
	return result;
}

static BOOL pci_write_config_dword(PCI_DEV *pci, int idx, DWORD data)
{
	__dpmi_regs r;
	BOOL result = TRUE;

	r.w.ax = 0xB10D;						/* write config dword */
	r.x.ebx = (DWORD)pci->device_bus_number;
	r.w.cx = (DWORD)data;
	r.w.di = (DWORD)idx;
	__dpmi_int(0x1a, &r);
	if( r.h.ah != 0 ){
		logerror_("pci write config dword failed\n");
		result = FALSE;
	}
	return result;
}

static BOOL set_pci_hardware_interrupt(PCI_DEV *pci, BYTE int_pin, BYTE irq)
{
	union REGPACK r;
	BOOL result = TRUE;

	memset(&r,0,sizeof(r));
	r.w.ax = 0xB10F;						/* set pci hardware interrupt */
	r.w.bx = (DWORD)pci->device_bus_number;
	r.h.cl	= int_pin;
	r.h.ch	= irq;
	r.x.ds	= 0xF000;
	intr(0x1a, &r);
	if( r.h.ah != 0 ){
		logerror_("set pci hardware interrupt failed.\n");
		result = FALSE;
	}
	pci_write_config_byte(pci, 0x3C, irq);
	return result;
}


static BOOL check_pci_bios(void)
{
	__dpmi_regs r;
	BOOL result = TRUE;

	r.w.ax = 0xB101;					// PCI BIOS - INSTALLATION CHECK
	r.w.di = 0x00000000;
	__dpmi_int(0x1a, &r);
	if( r.x.edx != 0x20494350 ){			// ' ICP'
		result = FALSE;
	}
	return result;
}


static BOOL find_pci_device(PCI_DEV *pci)
{
	__dpmi_regs r;
	WORD wdata;

	r.w.ax = 0xB102;					// PCI BIOS - FIND PCI DEVICE
	r.w.cx = pci->device_id;				// device ID
	r.w.dx = pci->vender_id;				// vendor ID
	r.x.esi = 0x00000000;					// device index
	__dpmi_int(0x1a, &r);
	if( r.h.ah != 0 ){
		return FALSE;						// no specified device found
	}
	pci->device_bus_number = r.w.bx;		// save the device & bus number
	if(pci->sub_vender_id != PCI_ANY_ID){
		/* get subsystem vender id */
		if(pci_read_config_word(pci, 0x2C, &wdata) == FALSE) return FALSE;
		if(wdata != pci->sub_vender_id) return FALSE;
	}
	if(pci->sub_device_id != PCI_ANY_ID){
		/* get subsystem device id */
		if(pci_read_config_word(pci, 0x2E, &wdata) == FALSE) return FALSE;
		if(wdata != pci->sub_device_id) return FALSE;
	}
	/* device found */
	return TRUE;
}






/********************************************************************
 *	helper func
 ********************************************************************/

static DWORD tasmania = 0;

void w_enter_critical(void)
{
	__dpmi_regs r;

	tasmania += 1;
	if(tasmania == 1){
		r.w.ax = 0x1681;
		__dpmi_int(0x2F, &r);
		__asm {
			cli
			};
	}
}

void w_exit_critical(void)
{
	__dpmi_regs r;

	tasmania -= 1;
	if(tasmania == 0){
		r.w.ax = 0x1682;
		__dpmi_int(0x2F, &r);
		__asm { sti };
	}
}

static DWORD get_tick_count()
{
	DWORD tick;
	_asm{
		rdtsc;
		mov tick,eax;
	}
	return tick;
}

static void udelay(int usec)
{
#define CLOCK_RATE 300000000
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

static void copy_to_dos_memory(DWORD dos_address, BYTE *src, DWORD length)
{
	int d0;

	_farsetsel(_dos_ds);
	d0 = 0;
	while(d0 < length){
		_farnspokeb(dos_address + d0, *(src + d0));
		d0 += 1;
	}
}

static void copy_from_dos_memory(DWORD dos_address, BYTE *dest, DWORD length)
{
	int d0;

	_farsetsel(_dos_ds);
	d0 = 0;
	while(d0 < length){
		*(dest + d0) = _farnspeekb(dos_address + d0);
		d0 += 1;
	}
}

static void eoi(int irq)
{
	outp(0x20, 0x20);
	if (irq > 7) outp(0xA0, 0x20);
}

static void unmask_irq(int irq)
{
	BYTE d0;

	d0 = inp(0x21);

	if (irq > 7) {
		outp(0x21, d0 & 0xFB);
		d0 = inp(0xA1);
		outp(0xA1, d0 & ~(1<<(irq-8)) );
   }else{
	  outp(0x21, d0 & (~(1<<irq)) );
   }
}

static void mask_irq(int irq)
{
	BYTE d0;

	if (irq > 7) {
		d0 = inp(0xA1);
		outp(0xA1, d0 | (1<<(irq-8)));
	}else{
		d0 = inp(0x21);
		outp(0x21, d0 | (1<<irq));
	}
}

/*
static int install_irq_handler(int irq, void *my_handler)
{
	int bl = TRUE;
	int irq_vector = 0;

	if( !(0 <= irq && irq <= 15) ) return FALSE;

	if(irq < 8){
		irq_vector = irq + 0x08;
	}else{
		irq_vector = irq - 8 + 0x70;
	}

	if(old_handler_irq_vector == -1){
		old_handler_irq_vector = irq_vector;
		_go32_dpmi_get_protected_mode_interrupt_vector(irq_vector, &old_handler);
		new_handler.pm_offset = (long)my_handler;
		_go32_dpmi_allocate_iret_wrapper(&new_handler);
		_go32_dpmi_set_protected_mode_interrupt_vector(irq_vector, &new_handler);
	}else{
		bl = FALSE;
	}

	return bl;
}

static void restore_irq_handler(void)
{
	if(old_handler_irq_vector != -1){
		_go32_dpmi_set_protected_mode_interrupt_vector(old_handler_irq_vector, &old_handler);
		_go32_dpmi_free_iret_wrapper(&new_handler);
		old_handler_irq_vector = -1;
	}
}
*/

/********************************************************************
 *	VDS helper func
 ********************************************************************/
int __dpmi_free_dos_memory(int sel)
{
	union REGS regs={0};
	regs.x.eax=0x0101;
	regs.x.edx=sel;
	int386(0x31,&regs,&regs);
	return 0;
}
int __dpmi_free_ldt_descriptor(int sel)
{
	union REGS regs={0};
	regs.x.eax=0x0001;
	regs.x.ebx=sel;
	int386(0x31,&regs,&regs);
	return 0;
}
int __dpmi_allocate_ldt_descriptors(int num)
{
	int result=-1;
	union REGS regs={0};
	regs.x.eax=0x0000;
	regs.x.ecx=num;
	int386(0x31,&regs,&regs);
	if(regs.x.cflag==0)
		result=regs.x.eax;
	return result;
}
int __dpmi_unlock_linear_region(__dpmi_meminfo *minfo)
{
	union REGS regs={0};
	regs.x.eax=0x0601;
	regs.w.bx=minfo->address>>16;
	regs.w.cx=minfo->address&0xFFFF;
	regs.w.si=minfo->size>>16;
	regs.w.di=minfo->size&0xFFFF;
	int386(0x31,&regs,&regs);
	return 0;
}
int __dpmi_lock_linear_region(__dpmi_meminfo *minfo)
{
	int result=1;
	union REGS regs={0};
	regs.x.eax=0x0600;
	regs.w.bx=minfo->address>>16;
	regs.w.cx=minfo->address&0xFFFF;
	regs.w.si=minfo->size>>16;
	regs.w.di=minfo->size&0xFFFF;
	int386(0x31,&regs,&regs);
	if(regs.x.cflag==0){
		result=0;
	}
	return result;
}

int __dpmi_free_physical_address_mapping(__dpmi_meminfo *minfo)
{
	union REGS regs={0};
	regs.x.eax=0x0801;
	regs.w.bx=minfo->address>>16;
	regs.w.cx=minfo->address&0xFFFF;
	int386(0x31,&regs,&regs);
	return 0;
}
int __dpmi_physical_address_mapping(__dpmi_meminfo *minfo)
{
	int result=FALSE;
	union REGS regs={0};
	regs.x.eax=0x0800;
	regs.w.bx=minfo->address>>16;
	regs.w.cx=minfo->address&0xFFFF;
	int386(0x31,&regs,&regs);
	if(regs.x.cflag==0){
		minfo->address=(regs.w.bx<<16)+regs.w.cx;
		result=TRUE;
	}
	return result;
}
int __dpmi_set_segment_base_address(int sel,unsigned long base)
{
	int result=1;
	union REGS regs={0};
	regs.x.eax=0x0007;
	regs.x.ebx=sel;
	regs.w.cx=base>>16;
	regs.w.dx=base&0xFFFF;
	int386(0x31,&regs,&regs);
	if(regs.x.cflag==0)
		result=0;
	return result;
}
int __dpmi_set_segment_limit(int sel,unsigned long limit)
{
	int result=1;
	union REGS regs={0};
	regs.x.eax=0x0008;
	regs.x.ebx=sel;
	regs.w.cx=limit>>16;
	regs.w.dx=limit&0xFFFF;
	int386(0x31,&regs,&regs);
	if(regs.x.cflag==0)
		result=0;
	return result;
}

_Packed
typedef struct {
	DWORD Region_Size		__attribute__ ((packed));
	DWORD Offset			__attribute__ ((packed));
	WORD  Seg_or_Select 	__attribute__ ((packed));
	WORD  Buffer_ID 		__attribute__ ((packed));
	DWORD Physical_Address	__attribute__ ((packed));
} DDS;

static BOOL is_vds_available(void)
{
	__dpmi_regs r;
	int available;
	BOOL flag = FALSE;

	available = (_farpeekb (_dos_ds, 0x0047b) & 0x20) != 0;

	if(available != 0){
		// get vds verson nunber
		r.w.ax = 0x8102;
		r.w.dx = 0x0000;
		__dpmi_int(0x4B, &r);
		if((r.x.cflag & 0x01) == 0){		// carry is clear if successful
			if(r.w.ax != 0x8102){
				flag = TRUE;
			}
		}
	}

	return flag;
}

static BOOL vds_helper_lock(DWORD buffer_addr, DWORD *physical_addr_table, int pages)
{
	union REGPACK r;
	DWORD temp;
	DDS dds;
	int d0;
	int sel;
	unsigned long dos_addr;
	BOOL vds_available;

	memset(&r,0,sizeof(r));
	vds_available = is_vds_available();

	if(_dma_allocate_mem4k(&sel, &dos_addr) == FALSE){
		set_error_message("vds_helper_lock: _dma_allocate_mem4k() error.\n");
		return FALSE;
	}
	memset(&dds, 0, sizeof(dds));

	d0 = 0;
	while(d0 < pages){
		temp = buffer_addr + (4096 * d0);
		if(vds_available == TRUE){
			/* vds available */
			dds.Region_Size   = 4096;
			dds.Offset		  = temp & 0x0F;
			dds.Seg_or_Select = temp >> 4;
//			printf("%08x : %08x : %08x\n", temp, dds.Offset, dds.Seg_or_Select);
			copy_to_dos_memory(dos_addr, (BYTE*)&dds, sizeof(dds));
			r.w.ax = 0x8103;			// LOCK DMA BUFFER REGION
			r.w.dx = 0x0000;
			r.w.di = dos_addr & 0xF;
			r.x.es = dos_addr >> 4;
			intr(0x4B, &r);
			if((r.x.flags & 0x01) != 0){
				set_error_message("vds_helper_lock: LOCK DMA BUFFER REGION error\n");
				__dpmi_free_dos_memory(sel);
				return FALSE;
			}
			copy_from_dos_memory(dos_addr, (BYTE*)&dds, sizeof(dds));
			if(dds.Buffer_ID != 0){
				set_error_message("vds_helper_lock: Buffer_ID allocated. error\n");
				__dpmi_free_dos_memory(sel);
				return FALSE;
			}
			physical_addr_table[d0] = dds.Physical_Address;
		}else{
			/* vds not available */
			physical_addr_table[d0] = temp;
		}
		d0 += 1;
	}
	__dpmi_free_dos_memory(sel);

	return TRUE;
}

static BOOL vds_helper_unlock(DWORD buffer_addr, DWORD *physical_addr_table, int pages)
{
	union REGPACK r;
	DWORD temp;
	DDS dds;
	int d0;
	int sel;
	unsigned long dos_addr;
	BOOL vds_available;

	memset(&r,0,sizeof(r));
	vds_available = is_vds_available();

	if(_dma_allocate_mem4k(&sel, &dos_addr) == FALSE){
		set_error_message("vds_helper_unlock: _dma_allocate_mem4k() error.\n");
		return FALSE;
	}
	memset(&dds, 0, sizeof(dds));

	d0 = 0;
	while(d0 < pages){
		if(vds_available == TRUE){
			/* vds available */
			temp = buffer_addr + (4096 * d0);
			dds.Region_Size   = 4096;
			dds.Offset		  = temp & 0x0F;
			dds.Seg_or_Select = temp >> 4;
			dds.Buffer_ID	  = 0;
			dds.Physical_Address = physical_addr_table[d0];
//			printf("%08x : %08x : %08x\n", temp, dds.Offset, dds.Seg_or_Select);
			copy_to_dos_memory(dos_addr, (BYTE*)&dds, sizeof(dds));
			r.w.ax = 0x8104;			// UNLOCK DMA BUFFER REGION
			r.w.dx = 0x0000;
			r.w.di = dos_addr & 0xF;
			r.x.es = dos_addr >> 4;
			intr(0x4B, &r);
			if((r.w.flags & 0x01) != 0){
				set_error_message("vds_helper_unlock: UNLOCK DMA BUFFER REGION error\n");
				__dpmi_free_dos_memory(sel);
				return FALSE;
			}
		}
		physical_addr_table[d0] = 0;
		d0 += 1;
	}
	__dpmi_free_dos_memory(sel);

	return TRUE;
}


/**********************************************************
						   dma.c  from allegro
***********************************************************/


// 8bit DMAC
#define DMA1_STAT		0x08
#define DMA1_WCMD		0x08
#define DMA1_WREQ		0x09
#define DMA1_SNGL		0x0A
#define DMA1_MODE		0x0B
#define DMA1_CLRFF		0x0C
#define DMA1_MCLR		0x0D
#define DMA1_CLRM		0x0E
#define DMA1_WRTALL 	0x0F

// 16bit DMAC
#define DMA2_STAT		0xD0
#define DMA2_WCMD		0xD0
#define DMA2_WREQ		0xD2
#define DMA2_SNGL		0xD4
#define DMA2_MODE		0xD6
#define DMA2_CLRFF		0xD8
#define DMA2_MCLR		0xDA
#define DMA2_CLRM		0xDC
#define DMA2_WRTALL 	0xDE


#define DMA0_ADDR		0x00
#define DMA0_CNT		0x01
#define DMA1_ADDR		0x02
#define DMA1_CNT		0x03
#define DMA2_ADDR		0x04
#define DMA2_CNT		0x05
#define DMA3_ADDR		0x06
#define DMA3_CNT		0x07
#define DMA4_ADDR		0xC0
#define DMA4_CNT		0xC2
#define DMA5_ADDR		0xC4
#define DMA5_CNT		0xC6
#define DMA6_ADDR		0xC8
#define DMA6_CNT		0xCA
#define DMA7_ADDR		0xCC
#define DMA7_CNT		0xCE

#define DMA0_PAGE		0x87
#define DMA1_PAGE		0x83
#define DMA2_PAGE		0x81
#define DMA3_PAGE		0x82
#define DMA4_PAGE		0x8F
#define DMA5_PAGE		0x8B
#define DMA6_PAGE		0x89
#define DMA7_PAGE		0x8A


typedef struct {
   BYTE dma_disable;
   BYTE dma_enable;
   WORD page;
   WORD addr;
   WORD count;
   WORD single;
   WORD mode;
   WORD clear_ff;
   BYTE write;
   BYTE read;
} DMA_S;



static DMA_S mydma[] =
{
   /* channel 0 */
   { 0x04, 0x00, DMA0_PAGE, DMA0_ADDR, DMA0_CNT,
	 DMA1_SNGL, DMA1_MODE, DMA1_CLRFF, 0x48, 0x44 },

   /* channel 1 */
   { 0x05, 0x01, DMA1_PAGE, DMA1_ADDR, DMA1_CNT,
	 DMA1_SNGL,DMA1_MODE,DMA1_CLRFF,0x49,0x45 },

   /* channel 2 */
   { 0x06, 0x02, DMA2_PAGE, DMA2_ADDR, DMA2_CNT,
	 DMA1_SNGL, DMA1_MODE, DMA1_CLRFF, 0x4A, 0x46 },

   /* channel 3 */
   { 0x07, 0x03, DMA3_PAGE, DMA3_ADDR, DMA3_CNT,
	 DMA1_SNGL, DMA1_MODE, DMA1_CLRFF, 0x4B, 0x47 },

   /* channel 4 */
   { 0x04, 0x00, DMA4_PAGE, DMA4_ADDR, DMA4_CNT,
	 DMA2_SNGL, DMA2_MODE, DMA2_CLRFF, 0x48, 0x44 },

   /* channel 5 */
   { 0x05, 0x01, DMA5_PAGE, DMA5_ADDR, DMA5_CNT,
	 DMA2_SNGL, DMA2_MODE, DMA2_CLRFF, 0x49, 0x45 },

   /* channel 6 */
   { 0x06, 0x02, DMA6_PAGE, DMA6_ADDR, DMA6_CNT,
	 DMA2_SNGL, DMA2_MODE, DMA2_CLRFF, 0x4A, 0x46 },

   /* channel 7 */
   { 0x07, 0x03, DMA7_PAGE, DMA7_ADDR, DMA7_CNT,
	 DMA2_SNGL, DMA2_MODE, DMA2_CLRFF, 0x4B, 0x47 }
};



__inline__ BYTE get_8bit_pcm_value(long value)
{
	BYTE d0;

	if(value >	32767 - 128) value =  32767 - 128;
	if(value < -32768)		 value = -32768;
	d0 = (value + 128 - ((value >> 15) & 0x0001)) >> 8;

	return (d0 + 0x80);
}


static int _dma_allocate_mem(int *sel, unsigned long *phys)
{
	int result=FALSE;
	union REGS regs;
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
	/*
	int seg;

	seg = __dpmi_allocate_dos_memory(131072 >> 4, sel); 	// 128K

	if (seg < 0) {
	   *sel = 0;
	   *phys = 0;
	   return FALSE;
	}

   *phys = seg << 4;

   *phys = (*phys + 65535) & ~0xFFFF ;						// 64K boundary
   return TRUE;
	*/
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
/*
	int seg;

	seg = __dpmi_allocate_dos_memory(8192 >> 4, sel);	  // 8K

	if (seg < 0) {
	   *sel = 0;
	   *phys = 0;
	   return FALSE;
	}

   *phys = seg << 4;

   *phys = (*phys + 4095) & ~0x0FFF ;					   // 4K boundary

   return TRUE;
*/
}



static void _dma_start(int channel, unsigned long addr, int size, int auto_init, int input)
{
   DMA_S *tdma;
   unsigned long page, offset;
   int mode;

   tdma = &mydma[channel];
   page = addr >> 16;

   if (channel >= 4) {
	  addr >>= 1;
	  size >>= 1;
   }

   offset = addr & 0xFFFF;
   size--;

   _dma_count = size;

   mode = (input) ? tdma->read : tdma->write;
   if (auto_init)
	  mode |= 0x10;

   outp(tdma->single, tdma->dma_disable);
   outp(tdma->mode, mode);
   outp(tdma->clear_ff, 0);
   outp(tdma->addr, offset & 0xFF);
   outp(tdma->addr, offset >> 8);
   outp(tdma->page, page);
   outp(tdma->clear_ff, 0);
   outp(tdma->count, size & 0xFF);
   outp(tdma->count, size >> 8);
   outp(tdma->single, tdma->dma_enable);
}

static void _dma_stop(int channel)
{
   DMA_S *tdma = &mydma[channel];
   outp(tdma->single, tdma->dma_disable);
}

static DWORD _dma_todo(int channel)
{
	int val1, val2 = 0;
	DMA_S *tdma = &mydma[channel];
	int _dma_todo_limit = 0;

	outp(tdma->clear_ff, 0xff);
	while(_dma_todo_limit < 1024){
		val1  = inp(tdma->count);
		val1 |= inp(tdma->count) << 8;
		val2  = inp(tdma->count);
		val2 |= inp(tdma->count) << 8;
		val1 -= val2;
		if(0 <= val1 && val1 < 8) break;
		_dma_todo_limit += 1;
	}
	if(val2 > _dma_count) val2 = _dma_count;
	if(channel > 3) val2 <<= 1;
	return val2;
}


/**********************************************************
						dma.c end
***********************************************************/


/**********************************************************
						AC97
***********************************************************/

#define AC97_RESET				0x00	/* Reset */
#define AC97_MASTER 			0x02	/* Master Volume */
#define AC97_HEADPHONE			0x04	/* Headphone Volume (optional) */
#define AC97_PCM				0x18	/* PCM Volume */
#define AC97_EXTENDED_ID		0x28	/* Extended Audio ID */
#define AC97_EXTENDED_STATUS		0x2a	/* Extended Audio Status */
#define AC97_PCM_FRONT_DAC_RATE 	0x2c	/* PCM Front DAC Rate */

#define AC97_EA_VRA 	0x0001	/* Variable bit rate enable bit */


typedef struct {
	WORD vender_id;
	WORD device_id;
	WORD sub_vender_id;
	WORD sub_device_id;
	int  type;
	char *string;
} DEVICE_LIST;









/********************************************************************
 *							ac97 autodetect							*
 ********************************************************************/
/*
static BOOL ac97_auto_detect_start(int rate, BOOL initialize)
{
	clear_error_message();
	if(via686_start(rate, initialize) == TRUE) return TRUE;

//	  clear_error_message();
	if(intel_ich_start(rate, initialize) == TRUE) return TRUE;

//	  clear_error_message();
	set_error_message("AC97_AUTODETECT: no supported device found.\n");
	return FALSE;
}
*/
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

static DEVICE_LIST hda_dev_list[] = {
	{ 0x8086, 0x2668, PCI_ANY_ID, PCI_ANY_ID, DEV_ICH, "I82801FB/FBM/FR/FW/FRW (ICH6 Family) HD Audio Controller" },
	{ 0x8086, 0x269a, PCI_ANY_ID, PCI_ANY_ID, DEV_ICH, "631xESB/632xESB HD Audio Controller" },
	{ 0x8086, 0x27d8, PCI_ANY_ID, PCI_ANY_ID, DEV_ICH, "N10 (ICH7 Family) HD Audio Controller" },
	{ 0x8086, 0x284b, PCI_ANY_ID, PCI_ANY_ID, DEV_ICH, "82801H (ICH8 Family) HD Audio Controller" },
	{ 0x8086, 0x2911, PCI_ANY_ID, PCI_ANY_ID, DEV_ICH, "ICH9 HD Audio Controller" },
	{ 0x8086, 0x293e, PCI_ANY_ID, PCI_ANY_ID, DEV_ICH, "I82801I (ICH9 Family) HD Audio Controller" },
	{ 0x8086, 0x293f, PCI_ANY_ID, PCI_ANY_ID, DEV_ICH, "I82801I (ICH9 Family) HD Audio Controller" },
	{ 0x8086, 0x3a3e, PCI_ANY_ID, PCI_ANY_ID, DEV_ICH, "82801JI (ICH10 Family) HD Audio Controller" },
	{ 0x8086, 0x3a6e, PCI_ANY_ID, PCI_ANY_ID, DEV_ICH, "82801JD/DO (ICH10 Family) HD Audio Controller"	},
	{ 0x8086, 0x3b56, PCI_ANY_ID, PCI_ANY_ID, DEV_PCH, "5 Series/3400 Series Chipset HD Audio" },
	{ 0x8086, 0x3b57, PCI_ANY_ID, PCI_ANY_ID, DEV_PCH, "5 Series/3400 Series Chipset HD Audio" },
	{ 0x8086, 0x1c20, PCI_ANY_ID, PCI_ANY_ID, DEV_PCH, "6 Series/C200 Series Chipset Family HD Audio Controller" },
    { 0x8086, 0x1d20, PCI_ANY_ID, PCI_ANY_ID, DEV_PCH, "7 Series Chipset HD Audio" },
	{ 0x8086, 0x811b, PCI_ANY_ID, PCI_ANY_ID, DEV_SCH, "System Controller Hub (SCH Poulsbo) HD Audio Controller" },
	{ 0x1002, 0x437b, PCI_ANY_ID, PCI_ANY_ID, DEV_ATI, "IXP SB4x0 High Definition Audio Controller" },
	{ 0x1002, 0x4383, PCI_ANY_ID, PCI_ANY_ID, DEV_ATI, "SBx00 Azalia (Intel HDA)" },
	{ 0x1002, 0x793b, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "RS600 audio device [Radeon Xpress 12xx Series]" },
	{ 0x1002, 0x7919, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "Radeon X1200 Series Audio Controller" },
	{ 0x1002, 0x960f, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "RS780 HDMI Audio [Radeon HD 3000-3300 Series]" },
	{ 0x1002, 0x970f, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "RS880 HDMI Audio [Radeon HD 4200 Series]" },
	{ 0x1002, 0xaa00, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "R600 Audio Device [Radeon HD 2900 Series]" },
	{ 0x1002, 0xaa08, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "RV630 audio device [Radeon HD 2600 Series]" },
	{ 0x1002, 0xaa10, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "RV610 audio device [Radeon HD 2400 PRO]" },
	{ 0x1002, 0xaa18, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "RV670/680 HDMI Audio [Radeon HD 3690/3800 Series]" },
	{ 0x1002, 0xaa20, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "RV635 HDMI Audio [Radeon HD 3600 Series]" },
	{ 0x1002, 0xaa28, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "RV620 HDMI Audio [Radeon HD 3400 Series]" },
	{ 0x1002, 0xaa30, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "RV770 HDMI Audio [Radeon HD 4850/4870]" },
	{ 0x1002, 0xaa38, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "RV710/730 HDMI Audio [Radeon HD 4000 series]" },
	{ 0x1002, 0xaa40, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "ATI HDMI Audio" },
	{ 0x1002, 0xaa48, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "ATI HDMI Audio" },
	{ 0x1002, 0xaa50, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "Cypress HDMI Audio [Radeon HD 5800 Series]" },
	{ 0x1002, 0xaa58, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "Juniper HDMI Audio [Radeon HD 5700 Series]" },
	{ 0x1002, 0xaa60, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "Redwood HDMI Audio [Radeon HD 5600 Series]" },
	{ 0x1002, 0xaa68, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "Manhattan HDMI Audio [Mobility Radeon HD 5000 Series]" },
	{ 0x1002, 0xaa88, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "Barts HDMI Audio [Radeon HD 6800 Series]" },
	{ 0x1002, 0xaa98, PCI_ANY_ID, PCI_ANY_ID, DEV_ATIHDMI, "Caicos HDMI Audio [Radeon HD 6400 Series]" },
	{ 0x1106, 0x3288, PCI_ANY_ID, PCI_ANY_ID, DEV_VIA, "VT8237A/VT8251 HDA Controller" },	
	{ 0x1039, 0x7502, PCI_ANY_ID, PCI_ANY_ID, DEV_SIS, "Azalia Audio Controller" },
	{ 0x10b9, 0x5461, PCI_ANY_ID, PCI_ANY_ID, DEV_ULI, "HD Audio Controller" },
	{ 0x10de, 0x026c, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP51 High Definition Audio" },
	{ 0x10de, 0x0371, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP55 High Definition Audio" },
	{ 0x10de, 0x03e4, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP61 High Definition Audio" },
	{ 0x10de, 0x03f0, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP61 High Definition Audio" },
	{ 0x10de, 0x044a, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP65 High Definition Audio" },
	{ 0x10de, 0x044b, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP65 High Definition Audio" },
	{ 0x10de, 0x055c, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP67 High Definition Audio" },
	{ 0x10de, 0x055d, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP67 High Definition Audio" },
	{ 0x10de, 0x0774, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP72XE/MCP72P/MCP78U/MCP78S High Definition Audio" },
	{ 0x10de, 0x0775, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP72XE/MCP72P/MCP78U/MCP78S High Definition Audio" },
	{ 0x10de, 0x0776, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP72XE/MCP72P/MCP78U/MCP78S High Definition Audio" },
	{ 0x10de, 0x0777, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP72XE/MCP72P/MCP78U/MCP78S High Definition Audio" },
	{ 0x10de, 0x07fc, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP73 High Definition Audio" },
	{ 0x10de, 0x07fd, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP73 High Definition Audio" },
	{ 0x10de, 0x0ac0, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP79 High Definition Audio" },
	{ 0x10de, 0x0ac1, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP79 High Definition Audio" },
	{ 0x10de, 0x0ac2, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP79 High Definition Audio" },
	{ 0x10de, 0x0ac3, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP79 High Definition Audio" },
	{ 0x10de, 0x0d94, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP89 High Definition Audio" },
	{ 0x10de, 0x0d95, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP89 High Definition Audio" },
	{ 0x10de, 0x0d96, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP89 High Definition Audio" },
	{ 0x10de, 0x0d97, PCI_ANY_ID, PCI_ANY_ID, DEV_NVIDIA, "MCP89 High Definition Audio" },
	{ 0x6549, 0x1200, PCI_ANY_ID, PCI_ANY_ID, DEV_TERA, "TERA1200 PC-over-IP Host" },
	{ 0x1102, 0x0009, PCI_ANY_ID, PCI_ANY_ID, DEV_CREAT, "[SB X-Fi Xtreme Audio] CA0110-IBG" },
	{ 0x17F3, 0x3010, PCI_ANY_ID, PCI_ANY_ID, DEV_ATI, "vortex MXplus"},
	{ 0x0000, 0x0000, PCI_ANY_ID, PCI_ANY_ID, 0, "" }
};

typedef struct
{
	DWORD nid;
	DWORD caps;
	DWORD type;
	int checked;
}HDAFUNCINFO;

static int hda_device_type = DEV_ICH;
static int hdaiosel = 0;
static DWORD codecmask = 0;
static DWORD osdbase = 0;
static DWORD outampcap = 0, inampcap = 0;
static DWORD codecvid = 0;
static DWORD codecdid = 0;
static HDAFUNCINFO * nodes = NULL;
static int optspeak = 0;

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

/*
#define hda_sel()			(_farsetsel((WORD)hdaiosel))
#define hda_rd_reg8(x)		(_farnspeekb(x))
#define hda_rd_reg16(x)		(_farnspeekw(x))
#define hda_rd_reg32(x)		(_farnspeekl(x))
#define hda_wr_reg8(x,y)	(_farnspokeb(x,(BYTE)y))
#define hda_wr_reg16(x,y) 	(_farnspokew(x,(WORD)y))
#define hda_wr_reg32(x,y) 	(_farnspokel(x,(DWORD)y))
*/
int log_mem(char *fmt,...)
{
	va_list arg;
	va_start(arg,fmt);
	vfprintf(stdout,fmt,arg);
	va_end(arg);
}
static int last_read=0,last_data=0;

int hda_sel()
{
	return 0;
}
unsigned char hda_rd_reg8(int offset)
{
	char *ptr,result;
	ptr=g_pci.base0+offset;
	result=ptr[0];
	if(ptr!=last_read){
		printf("r8 %08X %08X\n",last_read,last_data);
	}
	last_read=ptr;
	last_data=result;
	return result;
}
unsigned short hda_rd_reg16(int offset)
{
	unsigned short *ptr,result;
	ptr=g_pci.base0+offset;
	result=ptr[0];
	if(ptr!=last_read){
		printf("r16 %08X %08X\n",last_read,last_data);
	}
	last_read=ptr;
	last_data=result;
	return result;
}
unsigned int hda_rd_reg32(int offset)
{
	unsigned int *ptr,result;
	ptr=g_pci.base0+offset;
	result=ptr[0];
	if(ptr!=last_read){
		printf("r32 %08X %08X\n",last_read,last_data);
	}
	last_read=ptr;
	last_data=result;
	return result;
}
int hda_wr_reg8(int offset,unsigned char data)
{
	char *ptr;
	ptr=g_pci.base0+offset;
	ptr[0]=data;
	log_mem("%08X %08X (8)\n",ptr,data);
	return 1;
}
int hda_wr_reg16(int offset,unsigned short data)
{
	unsigned short *ptr;
	ptr=g_pci.base0+offset;
	ptr[0]=data;
	log_mem("%08X %08X (16)\n",ptr,data);
	return 1;
}
int hda_wr_reg32(int offset,unsigned int data)
{
	unsigned int *ptr;
	ptr=g_pci.base0+offset;
	ptr[0]=data;
	log_mem("%08X %08X (32)\n",ptr,data);
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


static void hda_init_pci_regs()
{
	/*taken from OSS*/
	
	DWORD tmp;
	
	switch(hda_device_type)			
	{								
		case DEV_SCH:
		case DEV_PCH:
			pci_read_config_word(&g_pci, DEVC, (WORD *) &tmp);				
			pci_write_config_word(&g_pci, DEVC, (tmp & (~0x0800)) );
			break;
		case DEV_ATI:
			pci_read_config_byte (&g_pci, 0x42, (BYTE *)&tmp);
			pci_write_config_byte (&g_pci, 0x42, (tmp & 0xf8) | 0x2);
			logerror_("ATI CODE=%08X\n",tmp);
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
		logerror_("special vender ID, code=%08X\n",tmp);
	}
}

static BOOL hda_reset(void)
{
	int tmout;
	DWORD d0;
	
	hda_sel();
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
		set_error_message("HDA: controller not ready.\n");
		return FALSE;
	}	
	if ((codecmask = (DWORD)hda_rd_reg16(HDASTATESTS)) == 0)
	{
		set_error_message("HDA: no codec found after reset.\n");
		return FALSE;
	}
	d0 = (DWORD)hda_rd_reg16(HDAGCAP);
	if((d0 >> 12) == 0)
	{
		set_error_message("HDA: no output streams supported.\n");
		return FALSE;
	}
	
	osdbase = (((d0 >> 8) & 0x0f) * HDASDSIZE) + HDASDBASE;
	hda_wr_reg8(OSDSTS, BCIS);
	/*hda_wr_reg8(OSDSTS, BCIS | FIFOE | DESE);
	hda_wr_reg16(HDASTATESTS, 0x01 | 0x02 | 0x04);
	hda_wr_reg8(HDARIRBSTS, 0x01 | 0x04);
	hda_wr_reg32(HDADPLBASE, 0);
	hda_wr_reg32(HDADPUBASE, 0);*/	
		
	logerror_("HDA: reset was succesful, codec mask: %04X, osd base %04X.\n", codecmask, osdbase);

	return TRUE;
}

int log_codec_cmds(DWORD param,BOOL need,int dump)
{
	static int *buffer=0;
	static int count=0;
	if(buffer==0)
		buffer=malloc(0x10000);
	if(buffer==0)
		return 0;
	if(dump){
		int i;
		for(i=0;i<count;i+=2){
			int index=i;
			printf("0x%08X,%i,\n",buffer[index],buffer[index+1]);
		}
		return 0;
	}
	buffer[count]=param;
	buffer[count+1]=need;
	count+=2;
	return 0;
}
static DWORD hda_send_codec_cmd(DWORD param, BOOL needanswer)
{
	int tmout,result=0;
	
	hda_sel();
	tmout = 1000;
//#define HDAICOI		0x60
//#define HDAICII		0x64
//#define HDAICIS		0x68
	
	while(((hda_rd_reg16(HDAICIS) & ICB) != 0) && --tmout)	
		udelay(1000);
	udelay(1000);
	hda_wr_reg32(HDAICOI, param);
	hda_wr_reg16(HDAICIS, ICB | IRV);

	log_codec_cmds(param,needanswer,0);
	if(needanswer)
	{
		tmout = 1000;
		while((hda_rd_reg16(HDAICIS) & (ICB | IRV)) != IRV){
			tmout--;
			if(tmout==0){
				printf("timeout waiting for response\n");
				break;
			}
			udelay(1000);
		}
		udelay(1000);
		tmout= hda_rd_reg32(HDAICII);
		result=tmout;
	}
	/*
	printf("param=%08X",param);
	if(needanswer)
		printf(" response=%08X",result);
	printf("\n");
	*/
	return result;
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
	
	vol=7;
	d0 = (SET_OUT_AMP | vol) & ~SET_MUTE;
	logerror_("--output volume codeaddr:%08X nid:%08X vol %08X\n",codecaddr,nid,vol);
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

//	logerror_("HDA: parsing node id %04X.\n", nid);
	
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
			//logerror_("HDA: found DAC, node id %04X.\n", nid);
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
				
			logerror_("HDA: found pin type ");
			switch(pintype)
			{
				case (LINE_OUT):
					logerror_("LINE OUT");
					break;
				case (SPEAKER):
					logerror_("SPEAKER");
					break;
				case (HP_OUT):
					logerror_("HP OUT");
					break;
			}
			logerror_(", node id %04X.\n", nid);
			
			if((pincap & PIN_PD_CAP) && (pdcapovrrd == 0))
			{
				if(hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PIN_SENSE, 0), TRUE) & DEV_ATTACHED)
					logerror_("HDA: a jack is plugged-in.\n");
				else
					logerror_("HDA: no jack is plugged-in.\n");
			}
			else
				logerror_("HDA: this pin doesn't support presence detection.\n");	
			
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
//		logerror_("HDA: the number of connections for node id %04X is %d.\n", nid, ncons);	
		
		d1 = 0;
		while(d1 < ncons)
		{
			d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_CON_LST_ENT, d1), TRUE);
			d2 = 0;
			while(((d1+d2) < ncons) && (d2 < step))
			{
				subnid = (d0 >> (d2 * shift)) & mask;
//				logerror_("HDA: index %d has node id %04X.\n", d1 + d2, subnid);
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
		
		logerror_("HDA: selected connection for node id %04X is index %d.\n", nid, index);
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
		
	logerror_("HDA: node id %04X supports audio functions.\n", nid);
	
	hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, SET_POW_STATE, STATE_D0), FALSE);
		
	outampcap = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, OUT_AMP_CAP), TRUE);
	inampcap = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, IN_AMP_CAP), TRUE);

	d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, nid, GET_PARAM, NODE_CNT), TRUE);
	
	d1 = (int)((d0 >> 16) & 0xff);
	subnodes = (int)(d0 & 0xff);
	
	logerror_("HDA: starting function/widget id %04X, total number of functions/widgets %d.\n", d1, subnodes);
	
	if((nodes = (HDAFUNCINFO *)malloc(subnodes * sizeof(HDAFUNCINFO))) == 0)
	{
		set_error_message("HDA: not enough memory.\n");
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
	
	logerror_("HDA: codec found, address %04X, vendor id %04X, device id %04X.\n", codecaddr, codecvid, codecdid);

	d0 = hda_send_codec_cmd(HDAPARAM1(codecaddr, 0, GET_PARAM, NODE_CNT), TRUE);
	
	d1 = (int)((d0 >> 16) & 0xff);
	d2 = (int)(d0 & 0xff);
	
	logerror_("HDA: starting node id %04X, total number of nodes %d.\n", d1, d2);
	
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
		
	d0 = 0;	
	while(d0 < HDA_MAX_CODECS)
	{
		if(codecmask & (1 << d0))
			if (hda_codec_init(d0) == TRUE)
				return TRUE;
		d0 += 1;
	}	
	set_error_message("HDA: no suitable codec was found.\n");
	
	return FALSE;
}


static void hda_remove_mapping(void)
{
	__dpmi_meminfo minfo;
/*
	if(hdaiosel != 0)
	{
		__dpmi_free_ldt_descriptor(hdaiosel);
		hdaiosel = 0;
	}
	if(g_pci.base0 != 0)
	{
		minfo.size = HDAIOLEN;
		minfo.address = g_pci.base0;
		__dpmi_unlock_linear_region(&minfo);
		__dpmi_free_physical_address_mapping(&minfo);
		g_pci.base0 = 0;
	}
*/
}

static int hda_map_iobase(void)
{
	__dpmi_meminfo minfo;
	
	/* get io base address */
	pci_read_config_dword(&g_pci, HDBARL, &minfo.address);

	if((minfo.address &= 0xfffffff9) == 0)
	{
		set_error_message("HDA: device found, but disabled.\n");
		return FALSE;
	}
	
	logerror_("HDA: i/o base found at %08X.\n", minfo.address);
	
	minfo.size = HDAIOLEN;
	
	/*
	if(__dpmi_physical_address_mapping(&minfo))
	{
		set_error_message("HDA: __dpmi_physical_address_mapping error.\n");
		return FALSE;
	}
	printf("physical map=%08X\n",minfo.address);
	*/
	/*
	
	if(__dpmi_lock_linear_region(&minfo))
	{
		__dpmi_free_physical_address_mapping(&minfo);
		set_error_message("HDA: __dpmi_lock_linear_region error.\n");
		return FALSE;
	}

	if (((hdaiosel = __dpmi_allocate_ldt_descriptors(1)) < 0) || 
		 __dpmi_set_segment_base_address(hdaiosel, minfo.address) ||
		 __dpmi_set_segment_limit(hdaiosel, minfo.address + HDAIOLEN - 1)) 
	{
		if(hdaiosel > 0)
			__dpmi_free_ldt_descriptor(hdaiosel);
		hdaiosel = 0;
		__dpmi_unlock_linear_region(&minfo);
		__dpmi_free_physical_address_mapping(&minfo);
		set_error_message("HDA: selector allocation or initialization error.\n");
		return FALSE;
	}
	*/
	g_pci.base0 = minfo.address;
		
	logerror_("HDA: i/o base mapped to %08X, selector %04X.\n", g_pci.base0, hdaiosel);
	
	return TRUE;
}

static void hda_free_dma_mem(void)
{
	if(vds_helper_unlock(get_address_dosmem64k_for_dma(), &g_dosmem64k_phys_table[0], 16) == FALSE){
		set_error_message("HDA: vds_helper_unlock error.\n");
	}
	if(vds_helper_unlock(get_address_dosmem4k(), &g_dosmem4k_phys_table[0], 1) == FALSE){
		set_error_message("HDA: vds_helper_unlock error.\n");
	}
	free_dosmem64k_for_dma();
	free_dosmem4k();	
}
	
static BOOL hda_alloc_dma_mem()
{
	return TRUE;
	/* playing buffer in dos memory */
	if(allocate_dosmem64k_for_dma(_16BITSTEREO) == FALSE)
	{
		set_error_message("HDA: allocate_dosmem64k_for_dma() error.\n");
		return FALSE;
	}
	/* get physical address every 4KB */
	if(vds_helper_lock(get_address_dosmem64k_for_dma(), &g_dosmem64k_phys_table[0], 16) == FALSE)
	{
		set_error_message("HDA: vds_helper_lock() error.\n");
		return FALSE;
	}

	/* scatter/gather table buffer in dos memory */
	if(allocate_dosmem4k() == FALSE)
	{
		set_error_message("HDA: allocate_dosmem4k() error.\n");
		return FALSE;
	}
	
	/* get physical address */
	if(vds_helper_lock(get_address_dosmem4k(), &g_dosmem4k_phys_table[0], 1) == FALSE)
	{
		set_error_message("HDA: vds_helper_lock() error.\n");
		return FALSE;
	}
	
	return TRUE;
}

static DWORD hda_current_pos(void)
{	
	DWORD pos;

	w_enter_critical();
	hda_sel();
	pos = hda_rd_reg32(OSDLPIB); 
	w_exit_critical();
	
	if((pos == 0) || (pos > g_dma_buff_size))
		pos = g_dma_buff_size;
	return (g_dma_buff_size - pos);	
	
}

static void hda_stop(void)
{
	int tmout;
	
	hda_sel();	
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
	
	hda_sel();
	hda_wr_reg8(OSDCTL, (hda_rd_reg8(OSDCTL) | RUN));
	tmout = 1000;
	while(((hda_rd_reg8(OSDCTL) & RUN) == 0) && --tmout)	
		udelay(1000);
	udelay(1000);
}
	
static void hda_exit(void)
{
	hda_stop();
	//hda_free_dma_mem();
	//hda_remove_mapping();
	//wavedevice_struct_init();
}

static void hda_prepare(void)
{
	DWORD a0;
	int d0, tmout;
	int bdl_entries=BDL_ENTRIES;

	bdl_entries=2;

	//a0 = get_address_dosmem4k();
	a0=0x004C2D00;
	d0 = 0;
	while(d0 < bdl_entries)
	{
		DWORD addr;
		/*
		DWORD tmp=g_dosmem64k_phys_table[d0 & 0x0F];
		{
			int i;
			short *ptr=(short*)tmp;
			for(i=0;i<0x1000;i++){
				float f=0;
				ptr[i]=100*sin(f);
				f+=3.14/180;
			}
		}
		addr=g_dosmem64k_phys_table[d0 & 0x0F];
		*/
		addr=0x4000;
		_farnspokel(a0, addr);		/*low base*/
		_farnspokel(a0 + 4, 0);									/*high base*/		
		_farnspokel(a0 + 8, 0x1000);							/*4KB length in bytes*/
		_farnspokel(a0 + 12, 0);								/*IOC & reserved*/		
		a0 += 16;
		d0 += 1;
	}
	{
		int i;
		int *buf;
		//buf=get_address_dosmem4k();
		buf=0x004C2D00;
		printf("bdl_list=%08X\n",buf);
		for(i=0;i<bdl_entries;i++){
			int j;
			for(j=0;j<4;j++){
				printf(" %08X\n",buf[i*4+j]);
			}
		}
	}
		
	hda_sel();
	hda_wr_reg8(OSDCTL, (hda_rd_reg8(OSDCTL) & ~SRST));
	tmout = 1000;
	while(((hda_rd_reg8(OSDCTL) & SRST) != 0) && --tmout)	
		udelay(1000);
	udelay(1000);
	d0=(hda_rd_reg32(OSDCTL) & 0xff0fffff) |  (1 << 20);
	printf("set stream=%08X\n",d0);
	hda_wr_reg32(OSDCTL, (hda_rd_reg32(OSDCTL) & 0xff0fffff) |  (1 << 20));	/*set stream*/
	hda_wr_reg32(OSDCBL, g_dma_buff_size);
	hda_wr_reg16(OSDLVI, bdl_entries - 1);
	printf("dma buffer size=%08X\n",g_dma_buff_size);
	printf("buffer entries=%i\n",bdl_entries);
	//vic
	a0=hda_set_rate();
	printf("rate=%08X\n",a0);
	hda_wr_reg16(OSDFMT, a0);	/*48 kHz, 16 bits per sample, 2 channels*/
	//hda_wr_reg32(OSDBDPL, get_address_dosmem4k());
	hda_wr_reg32(OSDBDPL, 0x004C2D00);
	hda_wr_reg32(OSDBDPU, 0);
}

BOOL hda_start(int rate)
{
	static char * device_name_hda = NULL;
	int d0;
	
	
	
	memset(&g_pci, 0, sizeof(PCI_DEV));

	if(check_pci_bios() == FALSE)
	{
		set_error_message("HDA: No PCI BIOS found.\n");
		return FALSE;
	}

	/* detection */
	d0 = 0;
	while(hda_dev_list[d0].vender_id != 0x0000)
	{
		g_pci.vender_id 	= hda_dev_list[d0].vender_id;
		g_pci.device_id 	= hda_dev_list[d0].device_id;
		g_pci.sub_vender_id = hda_dev_list[d0].sub_vender_id;
		g_pci.sub_device_id = hda_dev_list[d0].sub_device_id;
		if(find_pci_device(&g_pci) == TRUE)
		{
			logerror_("HDA: %s found.\n", hda_dev_list[d0].string);
			break;
		}
		d0 += 1;
	}
	if(hda_dev_list[d0].vender_id == 0x0000){
		set_error_message("HDA: no compatble device found.\n");
		return FALSE;
	}
	
	hda_device_type = hda_dev_list[d0].type;
	device_name_hda = hda_dev_list[d0].string;
	logerror_("hda type=%08X\n",hda_device_type);
	
	hda_init_pci_regs();
	
	if (hda_map_iobase() == FALSE)
		return FALSE;

	if((hda_init() == FALSE) || (hda_alloc_dma_mem() == FALSE))
	{	
		//hda_remove_mapping();
		return FALSE;
	}
	
	//vic
	wd.playback_rate   = rate;
	
	/* start playing*/
	hda_stop();
	hda_prepare();
	hda_run();
	printf("OSDBASE=%08X\n",osdbase);

	/*
	wd.device_name	   = device_name_hda;
	wd.pcm_format	   = _16BITSTEREO;
	wd.device_exit	   = hda_exit;
	wd.pcm_upload	   = common_pcm_upload_func;
	wd.get_current_pos = hda_current_pos;
	wd.initialized = TRUE;
	*/
	return TRUE;
}

static BOOL hda_start_no_speaker(int rate)
{
	optspeak = 1;
	return hda_start(rate);
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
	union REGS regs;
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
	copy_to_dos_memory(g_wss_dma_addr, a0, g_dma_buff_size);
	free(a0);

	if(lockflag == TRUE){
		info64k.size	= 65536;
		info64k.address = g_wss_dma_addr;
		__dpmi_lock_linear_region(&info64k);
	}else{
		info64k.address = 0;
	}

	return TRUE;
}

static void free_dosmem64k_for_dma(void)
{
	union REGS regs;
	if(g_wss_dma_sel  == 0) return;
	if(g_wss_dma_addr == 0) return;
	if(info64k.address != 0){
		/*
		regs.x.eax=0x0005;
		regs.x.ebx=
		__dpmi_unlock_linear_region(&info64k);
		*/
	}

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

static void u_delay(void)
{
	int d0;

	d0 = 0;
	while(d0 < 7){
		inp(u_dram);
		d0 += 1;
	}
}

static void u_voice_no(BYTE data)
{
	outp(u_page, data);
}

static void u_cmdwb(BYTE _index, BYTE data)
{
	outp(u_cmd,   _index);
	outp(u_datah, data);
}

static BYTE u_cmdrb(BYTE _index)
{
	outp(u_cmd, _index);
	return inp(u_datah);
}

static void u_cmdww(BYTE _index, WORD data)
{
	outp(u_cmd,   _index);
	outpw(u_datal, data);
}
static WORD u_cmdrw(BYTE _index)
{
	outp(u_cmd,   _index);
	return inpw(u_datal);
}

static BYTE u_irq_status(void)
{
	return inp(u_status);
}

// pan 0 to 7 to 15
//
static void u_set_pan(BYTE voice, BYTE pan)
{
	u_voice_no(voice);
	u_cmdwb(0x0C, pan);
}

static void u_set_freq(BYTE voice, DWORD freq)
{
	WORD fc;

	fc = (WORD)( ((freq << 9) + (u_freq_value >> 1)) / u_freq_value);
	fc = fc << 1;
	u_voice_no(voice);
	u_cmdww(0x01, fc);
}

static void u_set_freq_stereo(BYTE voice)
{
	WORD fc;

	fc = 2 << 10;
	u_voice_no(voice);
	u_cmdww(0x01, fc);
}

static void u_set_volume(BYTE voice, WORD volume)
{
	u_voice_no(voice);
	u_cmdww(0x09, volume);
	u_delay();
	u_cmdww(0x09, volume);
}

static void u_16bit_stereo_voice_start(DWORD length, WORD volume)
{
	int d0;
	int voice = 0;
	DWORD current;
	DWORD start;
	DWORD end;

	u_prim_voice = voice;

	d0 = 0;
	while(d0 < 2){
		u_voice_no(voice + d0);
		current = d0;
		end 	= ((length + 31) * 2);
		start	= 31*2;
		current = current << 9;
		start	= start   << 9;
		end 	= end	  << 9;
		u_cmdww(0x0A, (current >> 16) & 0xFFFF);
		u_cmdww(0x0B, current & 0xFFFF);
		u_cmdww(0x02, (start >> 16) & 0xFFFF);
		u_cmdww(0x03, start & 0xFFFF);
		u_cmdww(0x04, (end >> 16) & 0xFFFF);
		u_cmdww(0x05, end & 0xFFFF);

		u_set_pan(voice + d0, (d0 == 0) ? 0 : 15);
		u_set_freq_stereo(voice + d0);
		u_set_volume(voice + d0, volume);

		d0 += 1;
	}

	w_enter_critical();
	u_voice_no(voice);
	u_cmdwb(0x00, 0x0C);
	u_voice_no(voice + 1);
	u_cmdwb(0x00, 0x0C);
	w_exit_critical();
}


static void u_voice_stop(BYTE voice)
{
	u_set_volume(voice, 0);
	u_voice_no(voice);
	u_cmdwb(0x00, 0x03);		// turn voice off
}


static DWORD u_get_current_location(BYTE voice)
{
	DWORD d0;
	DWORD d1;
	int d2;

	u_voice_no(voice);
	d2 = 0;
	while(d2 < 1024){
		d0	= (u_cmdrw(0x8A) & 0x1FFF) << 7;
		d0 |= u_cmdrw(0x8B) >> 9;
		d1	= (u_cmdrw(0x8A) & 0x1FFF) << 7;
		d1 |= u_cmdrw(0x8B) >> 9;
		if( (d1 - d0) == 0 || (d1 - d0) == 1 ) break;
		d2 += 1;
	}
	return d1;
}

static void u_upload(DWORD addr, BYTE data)
{
	static BYTE gusdramh = -1;

	if( ((addr >> 16) & 0x0F) != gusdramh ){
		gusdramh = (addr >> 16) & 0x0F;
		u_cmdwb(0x44, gusdramh);
	}
	u_cmdww(0x43, addr & 0xFFFF);
	outp(u_base + 0x107, data);
}

static void u_set_dma_address(DWORD addr)
{
	if(wd.isa_dma > 3)
		addr = convert_to_16bit(addr);
	u_cmdww(0x42, (addr >> 4) & 0xFFFF);
}

/*	from GUSDK ROCK.C  */
static DWORD convert_to_16bit(DWORD address)
{
	DWORD hold_address;

	hold_address = address;

	/* Convert to 16 translated address. */
	address = address >> 1;

	/* Zero out bit 17. */
	address &= 0x0001ffffL;

	/* Reset bits 18 and 19. */
	address |= (hold_address & 0x000c0000L);

	return(address);
}

static volatile int u_dma_upload(DWORD addr, WORD count, DWORD gusaddr,
					int flag, DWORD addr2, WORD count2, DWORD gusaddr2)
{
	DWORD d0;

	d0 = 0;
	while(d0 < 65536*4){
		if(u_dma_busy_flag == FALSE) break;
		u_delay();
		d0 += 1;
	}
	w_enter_critical();
	if(u_dma_busy_flag == TRUE){
		u_abort_dma_upload();
		u_dma_busy_flag = FALSE;
	}
	w_exit_critical();

	u_twoflag = flag;
	if(u_twoflag == TRUE){
		d0 = gusaddr2 - (gusaddr2 & ~31);
		u_nextgusaddr = gusaddr2 & ~31;
		u_nextaddr	  = addr2  - d0;
		u_nextcount   = count2 + d0;
	}

	d0 = gusaddr - (gusaddr & ~31);
	gusaddr = gusaddr & ~31;
	w_enter_critical();
	u_dma_busy_flag = TRUE;
	u_start_dma_upload(addr - d0, count + d0, gusaddr);
	w_exit_critical();
	return TRUE;
}

static void u_start_dma_upload(DWORD addr, WORD count, DWORD gusaddr)
{
	BYTE d1;

	_dma_start(wd.isa_dma, addr, count, FALSE, FALSE);
	u_set_dma_address(gusaddr);
	d1 = 0x61;
	if(wd.isa_dma > 3) d1 |= 0x04;
	u_cmdwb(0x41, d1);
}

static void u_abort_dma_upload(void)
{
	BYTE d1;

	d1 = 0x60;
	if(wd.isa_dma > 3) d1 |= 0x04;
	u_cmdwb(0x41, d1);
	_dma_stop(wd.isa_dma);
	u_cmdrb(0x41);
}

static int u_gf1_irq_handler(void)
{
	BYTE d0;
	BYTE irq_status;

	tick += 1;

	while(1){
		irq_status = inp(u_base + 0x006) & 0xE0;
		if(irq_status == 0) break;
		if( (irq_status & 0x80) != 0 ){
			d0 = u_cmdrb(0x41);
			if(u_twoflag == TRUE){
				u_twoflag = FALSE;
				u_start_dma_upload(u_nextaddr, u_nextcount, u_nextgusaddr);
			}else{
				u_dma_busy_flag = FALSE;
			}
		}
		if( (irq_status & 0x60) != 0 ){
			d0 = u_cmdrb(0x8F);
		}
	}
	eoi(wd.irq);
	return 0;
}

static void u_init(int base, DWORD freq)
{
	int voice_no;
	DWORD d0;
	BYTE d1;

	u_base	 = base;
	u_page	 = u_base + 0x102;
	u_cmd	 = u_base + 0x103;
	u_datal  = u_base + 0x104;
	u_datah  = u_base + 0x105;
	u_status = u_base + 0x006;
	u_dram	 = u_base + 0x107;

	u_cmdwb(0x4C, 0);				// reset
	u_delay();
	u_delay();
	u_cmdwb(0x4C, 1);				// enable
	u_delay();
	u_delay();

	u_cmdwb(0x41, 0);				// DMA off
	u_cmdwb(0x45, 0);				// disable TIMER irq
	u_cmdwb(0x49, 0);				// sampling control reg.

	if(freq == 44100){
		u_cmdwb(0x0E, (14-1) | 0xC0);	  // active voice 14 44100Hz
		u_freq_value = 44100;
	}else{
		u_cmdwb(0x0E, (28-1) | 0xC0);	  // active voice 28 22050Hz
		u_freq_value = 22050;
	}
	u_irq_status();

	u_cmdrb(0x41);
	u_cmdrb(0x49);
	u_cmdrb(0x8F);

	voice_no = 0;
	while(voice_no < 32){
		u_voice_stop(voice_no);
		u_voice_no(voice_no);
		u_cmdwb(0x0D, 0x03);		// turn ramp off
		voice_no += 1;
	}

	u_cmdrb(0x41);
	u_cmdrb(0x49);
	u_cmdrb(0x8F);
	u_cmdwb(0x4C, 0x07);
	u_delay();
	u_cmdwb(0x4C, 0x07);

	d1 = 0x00;
	d0 = 0;
	while(d0 < 65536*2){
		u_upload(d0, d1);
		d0 += 1;
	}

	outp(u_base, 0x08);
}


/*
 *	ultrasound
 */

void ultrasound_pcm_upload_func(void);

static int get_ultrasnd(void)
{
	int midiirq;

	char *wsscfg = getenv("ULTRASND");
	if (wsscfg != NULL) {
		sscanf(wsscfg, "%x,%d,%d,%d,%d",
			&wd.isa_port, &wd.isa_dma, &wd.isa_hdma, &wd.irq, &midiirq);
	}
	if( (wd.isa_port < 0) || (wd.isa_dma < 0) || (wd.isa_hdma < 0) || (wd.irq < 0) ) return FALSE;

	return TRUE;
}

static DWORD ultrasound_current_pos(void)
{
	DWORD d0;

	w_enter_critical();
	d0 = (SAMPLECNT*2) - (u_get_current_location(u_prim_voice) - (31*2));
	w_exit_critical();

	return d0;
}
/*
static int ultrasound_start(int rate)
{
	DWORD curr;

	if(rate <= 22050){
		rate = 22050;
	}else{
		rate = 44100;
	}

	if( get_ultrasnd() == FALSE ){
		set_error_message("ULTRASND not found.\n");
		return FALSE;
	}
	if(allocate_dosmem64k_for_dma(_16BITSTEREO) == FALSE){
		set_error_message("allocate_dosmem64k_for_dma() error.\n");
		return FALSE;
	}
	g_dmacnt_shift_count = 1;
	u_init(wd.isa_port, rate);
	unmask_irq(wd.irq);
	install_irq_handler(wd.irq, (void*)u_gf1_irq_handler);

	u_16bit_stereo_voice_start(SAMPLECNT, 0xFFFF);

	wd.device_name	   = device_name_ultrasound;
	wd.playback_rate   = rate;
	wd.pcm_format	   = _16BITSTEREO;
	wd.device_exit	   = ultrasound_exit;
	wd.pcm_upload	   = ultrasound_pcm_upload_func;
	wd.get_current_pos = ultrasound_current_pos;
	wd.initialized = TRUE;

	while(1){
		w_enter_critical();
		curr = u_get_current_location(u_prim_voice);
		w_exit_critical();
		if( curr > (32*4) ) break;
	}

	return TRUE;
}

static void ultrasound_exit(void)
{
	int d0;
	d0 = 0;
	while(d0 < 2){
		u_voice_stop(d0);
		d0 += 1;
	}
	_dma_stop(wd.isa_dma);
	mask_irq(wd.irq);
	restore_irq_handler();
	u_init(wd.isa_port, 44100);

	free_dosmem64k_for_dma();
	wavedevice_struct_init();
}
*/

/*	from Allegro sndscape.c  */
static int get_ini_config_entry(char *entry, char *dest, FILE *fp)
{
	char str[83];
	char tokstr[33];
	char *p;

	/* make a local copy of the entry, upper-case it */
	strcpy(tokstr, entry);
	strupr(tokstr);

	/* rewind the file and try to find it... */
	rewind(fp);

	for (;;) {
		/* get the next string from the file */
		fgets(str, 83, fp);
		if (feof(fp)) {
			fclose(fp);
			return -1;
		}

		/* properly terminate the string */
		for (p=str; *p; p++) {
			if (isspace(*p)) {
				*p = 0;
				break;
			}
		}

		/* see if it's an 'equate' string; if so, zero the '=' */
		p = strchr(str, '=');
		if (!p)
			continue;

		*p = 0;

		/* upper-case the current string and test it */
		strupr(str);

		if (strcmp(str, tokstr))
			continue;

		/* it's our string - copy the right-hand value to buffer */
		strcpy(dest, str+strlen(str)+1);
		break;
	}

	return 0;
}

/*	from Allegro sndscape.c  */
static int soundscape_get_init_config()
{
	FILE *fp = NULL;
	char str[78];
	char *ep;

	/* get the environment var and build the filename, then open it */
	if (!(ep = getenv("SNDSCAPE"))){
		set_error_message("SNDSCAPE not found.");
		return FALSE;
	}
	strcpy(str, ep);

	if (str[strlen(str)-1] == '\\')
		str[strlen(str)-1] = 0;

	strcat(str, "\\SNDSCAPE.INI");

	if (!(fp = fopen(str, "r"))){
		set_error_message("SNDSCAPE.INI not found.");
		return FALSE;
	}
   /* read all of the necessary config info ... */
   if (get_ini_config_entry("Product", str, fp)) {
	  fclose(fp);
	  set_error_message("error in SNDSCAPE.INI.");
	  return FALSE;
   }

   if (get_ini_config_entry("WavePort", str, fp)) {
	  fclose(fp);
	  set_error_message("error in SNDSCAPE.INI.");
	  return FALSE;
   }

   wd.isa_port = strtol(str, NULL, 16) - 4;
   if (get_ini_config_entry("SBIRQ", str, fp)) {
	  fclose(fp);
	  set_error_message("error in SNDSCAPE.INI.");
	  return FALSE;
   }

   wd.irq = strtol(str, NULL, 10);

   if (wd.irq == 2)
	  wd.irq = 9;

   if (get_ini_config_entry("DMA", str, fp)) {
	  fclose(fp);
	  set_error_message("error in SNDSCAPE.INI.");
	  return FALSE;
   }

   wd.isa_dma = strtol(str, NULL, 10);

   fclose(fp);
   return TRUE;
}



/*
 *	eternal silence
 */

static cycles_t es_tick_per_hz = 0;
static cycles_t es_remain = 0;
static cycles_t es_prev = 0;
static DWORD  es_count;

static int eternal_silence_start(int rate_no);
static void eternal_silence_exit(void);
static DWORD eternal_silence_current_pos(void);
static char device_name_eternal_silence[] = "Eternal Silence";

static int eternal_silence_start(int rate)
{
	if(allocate_dosmem64k_for_dma(_16BITSTEREO) == FALSE){
		set_error_message("allocate_dosmem64k_for_dma() error.\n");
		return FALSE;
	}
	es_tick_per_hz = osd_cycles_per_second() / rate;
	es_remain = 0;
	es_prev = osd_cycles();
	es_count = 0;

	wd.device_name	   = device_name_eternal_silence;
	wd.playback_rate   = rate;
	wd.pcm_format	   = _16BITSTEREO;
	wd.device_exit	   = eternal_silence_exit;
	wd.pcm_upload	   = common_pcm_upload_func;
	wd.get_current_pos = eternal_silence_current_pos;
	wd.initialized = TRUE;

	return TRUE;
}

static void eternal_silence_exit(void)
{
	free_dosmem64k_for_dma();
	wavedevice_struct_init();
}

static DWORD eternal_silence_current_pos(void)
{
	cycles_t curr;

	curr = osd_cycles();
	es_remain += curr - es_prev;
	es_prev = curr;
	while(es_remain >= es_tick_per_hz){
		es_remain -= es_tick_per_hz;
		es_count -= 1;
	}
	es_count = es_count & SAMPLECNTMASK;
	return es_count << 2;						// _16BITSTEREO ˆµ‚¢
}

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

void w_set_watermark(float latency, DWORD samples_per_frame)
{
	if(wd.initialized == FALSE) return;

	if(latency < 1.0) latency = 1.0;
	g_latencym = latency;
	g_latency = samples_per_frame * latency;
	g_dma_average_cnt = samples_per_frame << DMA_AVERAGE_SHIFT_COUNT;
	g_samples_per_frame = samples_per_frame;
	w_reset_write_cursor(0);

	w_clear_buffer();
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

static void common_pcm_upload_func(void)
{
	int d0;
	long d1;
	WORD d2;
	DWORD d3;
	DWORD start;
	DWORD end;

	start = -1;
	_farsetsel(_dos_ds);
	d0 = 0;
	while(d0 < g_current_req){
		g_write_cursor = g_write_cursor & SAMPLECNTMASK;
		if(wd.pcm_format == _16BITSTEREO){
			d1	= ((mixing_buff[(d0*2) + 0] * g_master_volume)/256);
			if(d1 >  32767) d1 =  32767;
			if(d1 < -32768) d1 = -32768;
			d3	= d1 & 0xFFFF;
			d1	= ((mixing_buff[(d0*2) + 1] * g_master_volume)/256);
			if(d1 >  32767) d1 =  32767;
			if(d1 < -32768) d1 = -32768;
			d3 |= d1 << 16;
			_farnspokel(g_wss_dma_addr + (g_write_cursor * 4), d3);
		}else if(wd.pcm_format == _8BITSTEREO){
			d1	= ((mixing_buff[(d0*2) + 0] * g_master_volume)/256);
			d2	= get_8bit_pcm_value(d1);
			d1	= ((mixing_buff[(d0*2) + 1] * g_master_volume)/256);
			d2 |= get_8bit_pcm_value(d1) << 8;
			_farnspokew(g_wss_dma_addr + (g_write_cursor * 2), d2);
		}else if(wd.pcm_format == _8BITMONO){
			d1	= ((mixing_buff[(d0*2) + 0] * g_master_volume)/256);
			d1 += ((mixing_buff[(d0*2) + 1] * g_master_volume)/256);
			d1	= d1 / 2;
			d2	= get_8bit_pcm_value(d1);
			_farnspokeb(g_wss_dma_addr + (g_write_cursor * 1), (BYTE)d2);
		}
		if(start == -1) start = g_write_cursor;
		g_write_cursor += 1;
		d0 += 1;
	}
	end = g_write_cursor;
}

void ultrasound_pcm_upload_func(void)
{
	int d0;
	long d1;
	WORD d2;
	DWORD d3;
	DWORD start;
	DWORD end;
	DWORD start2;
	DWORD end2;

	if(wd.initialized == FALSE) return;

	// dma upload
	start = -1;
	_farsetsel(_dos_ds);
	d0 = 0;
	while(d0 < g_current_req){
		g_write_cursor = g_write_cursor & SAMPLECNTMASK;
		if(wd.pcm_format == _16BITSTEREO){
			d1	= ((mixing_buff[(d0*2) + 0] * g_master_volume)/256);
			if(d1 >  32767) d1 =  32767;
			if(d1 < -32768) d1 = -32768;
			d3	= d1 & 0xFFFF;
			d1	= ((mixing_buff[(d0*2) + 1] * g_master_volume)/256);
			if(d1 >  32767) d1 =  32767;
			if(d1 < -32768) d1 = -32768;
			d3 |= d1 << 16;
			_farnspokel(g_wss_dma_addr + (g_write_cursor * 4), d3);
		}else if(wd.pcm_format == _8BITSTEREO){
			d1	= ((mixing_buff[(d0*2) + 0] * g_master_volume)/256);
			d2	= get_8bit_pcm_value(d1);
			d1	= ((mixing_buff[(d0*2) + 1] * g_master_volume)/256);
			d2 |= get_8bit_pcm_value(d1) << 8;
			_farnspokew(g_wss_dma_addr + (g_write_cursor * 2), d2);
		}else if(wd.pcm_format == _8BITMONO){
			d1	= ((mixing_buff[(d0*2) + 0] * g_master_volume)/256);
			d1 += ((mixing_buff[(d0*2) + 1] * g_master_volume)/256);
			d1	= d1 / 2;
			d2	= get_8bit_pcm_value(d1);
			_farnspokeb(g_wss_dma_addr + (g_write_cursor * 1), (BYTE)d2);
		}
		if(start == -1) start = g_write_cursor;
		g_write_cursor += 1;
		d0 += 1;
	}
	end = g_write_cursor;
	if(start <= end){
		start = start * 4;
		end   = end * 4;
		u_dma_upload(g_wss_dma_addr + start, end - start, start + (32*4),
				FALSE, 0, 0, 0);
	}else{
		d3 = _farnspeekl(g_wss_dma_addr + (SAMPLECNTMASK * 4) );
		u_upload( (31*4) + 0,  (d3 >>  0) & 0xFF);
		u_upload( (31*4) + 1,  (d3 >>  8) & 0xFF);
		u_upload( (31*4) + 2,  (d3 >> 16) & 0xFF);
		u_upload( (31*4) + 3,  (d3 >> 24) & 0xFF);

		start2 = 0;
		end2 = end * 4;
		start = start * 4;
		end  = SAMPLECNT * 4;
		u_dma_upload(g_wss_dma_addr + start, end - start, start + (32*4),
				TRUE, g_wss_dma_addr + start2, end2 - start2, start2 + (32*4));
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
	DWORD d0;
	w_enter_critical();
	d0 = _dma_todo(wd.isa_dma);
	w_exit_critical();

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
		set_error_message("already initialized.\n");
		return FALSE;
	}
	if(rate < 0)
		rate = 22050;
	if(rate > 64000)
		rate = 64000;

	switch(device_no){
			/*
		case 0:
			result = eternal_silence_start(rate);
			break;
		case 1:
			result = sb_auto_detect_start(rate);
			break;
		case 2:
			result = ac97_auto_detect_start(rate, FALSE);
			break;
		case 3:
			result = ac97_auto_detect_start(rate, TRUE);
			break;
		case 4:
			result = ultramax_start(rate);
			break;
		case 5:
			result = ultrasound_start(rate);
			break;
		case 6:
			result = wss_start(rate);
			break;
		case 7:
			result = soundscape_start(rate);
			break;
		case 8:
			result = ess_start(rate);
			break;
		case 9:
			result = wdm_sbpro_start(rate);
			break;
		case 10:
			result = sb201_interrupt_driven_start(rate);
			break;
		case 11:
			result = sb100_interrupt_driven_start(rate);
			break;
		case 12:
			result = sb201_start(rate);
			break;
		case 13:
			result = sbpro_start(rate);
			break;
		case 14:
			result = sb16_start(rate);
			break;
		case 15:
			result = sbpro_ex_start(rate);
			break;
		case 16:
			result = sb16_ex_start(rate);
			break;
		case 17:
			result = cmi8x38_as_sb16_start(rate);
			break;
		case 21:
			result = trid4dwave_as_sb16_start(rate);
			break;
		case 23:
			result = sbpro_interrupt_driven_start(rate);
			break;
		case 24:
			result = via686_start_chip_init(rate);
			break;
		case 25:
			result = via686_start_no_chip_init(rate);
			break;
		case 26:
			result = intel_ich_start_chip_init(rate);
			break;
		case 27:
			result = intel_ich_start_no_chip_init(rate);
			break;
			*/

		case 28:
			result = hda_start(rate);
			break;
/*
		case 29:
			result = hda_start_no_speaker(rate);
			break;
*/
		default:
			result = FALSE;
			set_error_message("invalid sound device number.\n");
	}

//	w_set_watermark(2.2, 1200);

	//log_codec_cmds(0,0,1);
	return result;
}

void w_sound_device_exit(void)
{
	w_clear_buffer();
	if(wd.device_exit != NULL){
		(*wd.device_exit)();
	}
}

void vga_vsync(void)
{
	w_enter_critical();
	while( (inp(0x3da) & 0x08) != 0 );
	while( (inp(0x3da) & 0x08) == 0 );
	w_exit_critical();
}

DWORD w_get_actual_sample_rate(void)
{
	DWORD d0;
	DWORD result[2];
	int d1;
	int d7;
	int limit;
	cycles_t prev;
	cycles_t curr;

	if(wd.initialized == FALSE) return 0;

	w_set_watermark(2.0, w_get_nominal_sample_rate()/60);

	prev = osd_cycles();
	while(1){
		if( (osd_cycles() - prev) >= osd_cycles_per_second()/2 ) break;
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
			prev = osd_cycles();
			while(1){
				d0 += get_played_sample_count();
				curr = osd_cycles();
				if( (curr - prev) >= osd_cycles_per_second()/2 ) break;
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


char *w_get_error_message(void)
{
	return get_error_message();
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

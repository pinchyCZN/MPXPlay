#include <i86.h>
#include <stdio.h>
#include <stdarg.h>
#include "dosstuff.h"
#include "au_cards.h"

unsigned int crossfadepart, intsoundconfig, intsoundcontrol;

void (__far __interrupt * oldint08_handler) ();

int mpxplay_close_program()
{
	exit(0);
}
void pds_memxch(char *addr1, char *addr2, unsigned int len)
{
	while(len--) {
		char tmp1 = *addr1, tmp2 = *addr2;
		*addr1 = tmp2;
		*addr2 = tmp1;
		addr1++;
		addr2++;
	}
}
void AU_start()
{
}
void AU_stop()
{
}

void pds_textdisplay_printf(char *str)
{
	printf("%s\n",str);
}

unsigned long pds_dpmi_map_physical_memory(unsigned long phys_addr, unsigned long memsize)
{
	union REGS regs;
	pds_newfunc_regs_clear(&regs);
	regs.w.ax = 0x0800;
	regs.w.bx = (phys_addr >> 16);
	regs.w.cx = (phys_addr & 0xffff);
	regs.w.di = (memsize & 0xffff);
	regs.w.si = (memsize >> 16);
	regs.w.cflag = 1;
	int386(0x31, &regs, &regs);
	if(regs.w.cflag)
		return 0;
	return ((regs.x.ebx << 16) | (regs.x.ecx & 0xffff));
}
void pds_dpmi_unmap_physycal_memory(unsigned long linear_addr)
{
	union REGS regs;
	if(!linear_addr)
		return;
	pds_newfunc_regs_clear(&regs);
	regs.w.ax = 0x0801;
	regs.w.bx = linear_addr >> 16;
	regs.w.cx = linear_addr & 0xffff;
	int386(0x31, &regs, &regs);
}

int pds_dpmi_dos_allocmem(dosmem_t * dm, unsigned int size)
{
	union REGS regs;
	if(dm->selector) {
		pds_dpmi_dos_freemem(dm);
		dm->selector = 0;
	}
	pds_newfunc_regs_clear(&regs);
	regs.x.eax = 0x0100;
	regs.x.ebx = (size + 15) >> 4;
	regs.x.cflag = 1;
	int386(0x31, &regs, &regs);
	if(regs.x.cflag)
		return 0;
	dm->selector = regs.x.edx;
	dm->segment = (unsigned short)regs.x.eax;
	dm->linearptr = (void *)(regs.x.eax << 4);
	return 1;
}

void pds_dpmi_dos_freemem(dosmem_t * dm)
{
	union REGS regs;
	if(dm->selector) {
		pds_newfunc_regs_clear(&regs);
		regs.x.eax = 0x0101;
		regs.x.edx = dm->selector;
		int386(0x31, &regs, &regs);
		dm->selector = dm->segment = 0;
		dm->linearptr = NULL;
	}
}

void far *pds_dos_getvect(unsigned int intno)	// protected mode vector
{
	union REGPACK regp;
	pds_memset((void *)&regp, 0, sizeof(union REGPACK));
	regp.x.eax = 0x3500 | (intno & 0xff);
	intr(0x21, &regp);
	return MK_FP(regp.x.es, regp.x.ebx);
	//return _dos_getvect(intno);
}
void pds_dos_setvect(unsigned int intno, void far * vect)
{
	union REGPACK regp;
	if(!vect)
		return;
	pds_memset((void *)&regp, 0, sizeof(union REGPACK));
	regp.x.eax = 0x2500 | (intno & 0xff);
	regp.x.edx = FP_OFF(vect);
	regp.x.ds = FP_SEG(vect);
	intr(0x21, &regp);
	//_dos_setvect(intno,vect);
}
#define INT08_DIVISOR_DEFAULT  65536
#define INT08_CYCLES_DEFAULT   (1000.0/55.0)	// 18.181818
#define INT08_DIVISOR_NEW      10375	// = 18.181818*65536 / (3 * 44100/1152)

void pds_delay_10us(unsigned int ticks)	//each tick is 10us
{
#ifdef __DOS__
	unsigned int divisor = (oldint08_handler) ? INT08_DIVISOR_NEW : INT08_DIVISOR_DEFAULT;	// ???
	unsigned int i, oldtsc, tsctemp, tscdif;

	for(i = 0; i < ticks; i++) {
		_disable();
		outp(0x43, 0x04);
		oldtsc = inp(0x40);
		oldtsc += inp(0x40) << 8;
		_enable();

		do {
			_disable();
			outp(0x43, 0x04);
			tsctemp = inp(0x40);
			tsctemp += inp(0x40) << 8;
			_enable();
			if(tsctemp <= oldtsc)
				tscdif = oldtsc - tsctemp;	// handle overflow
			else
				tscdif = divisor + oldtsc - tsctemp;
		} while(tscdif < 12);	//wait for 10us  (12/(65536*18) sec)
	}
#else
	unsigned int oldclock = clock();
	while(oldclock == clock()) {
	}							// 1ms not 0.01ms (10us)
#endif
}

static void __interrupt __loadds newhandler_08(void)
{
}

#define MPXPLAY_TIMER_INT      0x08
void newfunc_newhandler08_init(void)
{
	if(!oldint08_handler) {
		oldint08_handler = (void (__far __interrupt *) ())pds_dos_getvect(MPXPLAY_TIMER_INT);
		pds_dos_setvect(MPXPLAY_TIMER_INT, newhandler_08);
		outp(0x43, 0x34);
		outp(0x40, (INT08_DIVISOR_NEW & 0xff));
		outp(0x40, (INT08_DIVISOR_NEW >> 8));
	}
}



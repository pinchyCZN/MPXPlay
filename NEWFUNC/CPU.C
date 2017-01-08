//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: cpu control
//some routines are based on Linux,MPlayer and VGA13 (by RayeR/Martin Rehak)

#include <stdlib.h>
#include <dos.h>
#include "newfunc.h"

//#define NEWFUNC_CPU_DEBUG 1

int mpxplay_cpu_capables_std, mpxplay_cpu_capables_ext, mpxplay_cpu_capables_mm;
int mpxplay_cpu_cores;
static unsigned int mpxplay_cpu_initialized;

void newfunc_cpu_init(void)
{
	int eax, ebx, ecx, edx, cap_std, cap_ext, cap_mm;
#ifdef MPXPLAY_WIN32
	//SYSTEM_INFO si;
#endif

	if(mpxplay_cpu_initialized)
		return;
	mpxplay_cpu_initialized = 1;

	if(!pds_cpu_cpuid_test())
		return;

	cap_ext = cap_mm = 0;

	//if(pds_cpu_cpuid_get(1, &ebx, &ecx, &edx)>=1){
	if(pds_cpu_cpuid_get(1, &ebx, &ecx, &cap_std) >= 1) {
		//pds_cpu_cpuid_get(1, &ebx, &ecx, &cap_std); // ???
		mpxplay_cpu_capables_std = cap_std;
		if(cap_std & CPU_X86_STDCAP_MMX)
			cap_mm |= CPU_X86_MMCAP_MMX;
		if(cap_std & CPU_X86_STDCAP_SSE1)
			cap_mm |= CPU_X86_MMCAP_MMXEXT | CPU_X86_MMCAP_SSE;
		if(cap_std & CPU_X86_STDCAP_SSE2)
			cap_mm |= CPU_X86_MMCAP_SSE2;
	}
	if(pds_cpu_cpuid_get(0x80000000, &ebx, &ecx, &edx) >= 0x80000001) {
		pds_cpu_cpuid_get(0x80000001, &ebx, &ecx, &cap_ext);
		mpxplay_cpu_capables_ext = cap_ext;
		if(cap_ext & CPU_X86_EXTCAP_3DNOW1)
			cap_mm |= CPU_X86_MMCAP_3DNOW;
		if(cap_ext & CPU_X86_EXTCAP_3DNOW2)
			cap_mm |= CPU_X86_MMCAP_3DNOWEXT;
		if(cap_ext & CPU_X86_EXTCAP_MMXEXT)
			cap_mm |= CPU_X86_MMCAP_MMX;
	}

	eax = pds_cpu_cpuid_get(0, &ebx, &ecx, &edx);
	if(ebx == 0x68747541 && edx == 0x69746e65 && ecx == 0x444d4163) {	// AMD
		if(cap_ext & (1 << 22))
			cap_mm |= CPU_X86_MMCAP_MMXEXT;
	} else if(ebx == 0x746e6543 && edx == 0x48727561 && ecx == 0x736c7561) {	//  CentaurHauls
		if(cap_ext & (1 << 24))	// VIA C3
			cap_mm |= CPU_X86_MMCAP_MMXEXT;
	} else if(ebx == 0x69727943 && edx == 0x736e4978 && ecx == 0x64616574) {
		if(eax >= 2)
			if(cap_ext & (1 << 24))
				cap_mm |= CPU_X86_MMCAP_MMXEXT;
	}
	mpxplay_cpu_capables_mm = cap_mm;

#ifdef MPXPLAY_WIN32
	//GetSystemInfo(&si);
	//mpxplay_cpu_cores=si.dwNumberOfProcessors;
#endif

#ifdef NEWFUNC_CPU_DEBUG
	fprintf(stdout, "cpu features: MMX:%d MMX2(iSSE):%d SSE:%d SSE2:%d 3dNow:%d 3dNow2:%d\n\n", ((cap_mm & CPU_X86_MMCAP_MMX) ? 1 : 0), ((cap_mm & CPU_X86_MMCAP_MMXEXT) ? 1 : 0),
			((cap_mm & CPU_X86_MMCAP_SSE) ? 1 : 0), ((cap_mm & CPU_X86_MMCAP_SSE2) ? 1 : 0), ((cap_mm & CPU_X86_MMCAP_3DNOW) ? 1 : 0), ((cap_mm & CPU_X86_MMCAP_3DNOWEXT) ? 1 : 0));
#endif							//NEWFUNC_CPU_DEBUG
}

int asm_cpu_testcpuid(void);
#ifdef WIN32
int asm_cpu_testcpuid(void)
{
	__asm {
	pushad pushfd pop eax mov ecx, eax xor eax, 0x200000 push eax popfd pushfd pop eax cmp eax, ecx popad setnz al and eax, 1}
}

#else
#pragma aux asm_cpu_testcpuid=\
 "pushad"\
 "pushfd"\
 "pop     eax"\
 "mov     ecx, eax"\
 "xor     eax, 0x200000"\
 "push    eax"\
 "popfd"\
 "pushfd"\
 "pop     eax"\
 "cmp     eax, ecx"\
 "popad"\
 "setnz   al"\
 "and     eax,1"\
 value[eax] modify[eax ecx];
#endif							//WIN32

int asm_cpu_getcpuid(unsigned int eax_p, int *ebx_p, int *ecx_p, int *edx_p);

#ifdef WIN32
int asm_cpu_getcpuid(unsigned int eax_p, int *ebx_p, int *ecx_p, int *edx_p)
{
	__asm {
mov eax, eax_p cpuid mov edi, ebx_p mov[edi], ebx mov edi, ecx_p mov[edi], ecx mov edi, edx_p mov[edi], edx}}
#else
#pragma aux asm_cpu_getcpuid=\
 "push ebx"\
 "push ecx"\
 "push edx"\
 "cpuid"\
 "pop edi"\
 "mov [edi],edx"\
 "pop edi"\
 "mov [edi],ecx"\
 "pop edi"\
 "mov [edi],ebx"\
 parm[eax][ebx][ecx][edx] value[eax] modify[ebx ecx edx edi];
#endif							//WIN32

unsigned int pds_cpu_cpuid_test(void)
{
	return asm_cpu_testcpuid();
}

int pds_cpu_cpuid_get(unsigned int eax_p, int *ebx_p, int *ecx_p, int *edx_p)
{
	return asm_cpu_getcpuid(eax_p, ebx_p, ecx_p, edx_p);
}

//--------------------------------------------------------------------------
//MTRR (cache) setting

#ifdef __DOS__

#define MTRR_REG_CAP            0x0fe
#define MTRR_REG_DEFTYPE        0x2ff
#define MTRR_REG_PHYSBASE(reg) (0x200 + 2 * (reg))
#define MTRR_REG_PHYSMASK(reg) (0x200 + 2 * (reg) + 1)
#define MTRR_REG_FIX16K_A0000   0x259
#define MTRR_REG_PAT            0x277

#define MTRR_MODE_UC 0x00		//
#define MTRR_MODE_WC 0x01		// write-combining

#define MTRR_GET_PB_MODE(v)        ((v)&0xff)	// type of memory
#define MTRR_PUT_PB_MODE(r,v)      (((r)&(~0xff))|(v))
#define MTRR_GET_PB_PHYS_BASE(v)   (((v)>>12)&0xffffff)	// 36-bit phys_base address (low bits=0)
#define MTRR_PUT_PB_PHYS_BASE(r,v) (((r)&(~((mpxp_int64_t)0xffffff<<12)))|((mpxp_int64_t)((v)&0xffffff)<<12))

#define MTRR_GET_PM_VALID(v)       (((v)>>11)&0x1)	// MTRR var register is valid (used)
#define MTRR_PUT_PM_VALID(r,v)     (((r)&(~(1<<11)))|(((v)&1)<<11))
#define MTRR_GET_PM_PHYS_MASK(v)   (((v)>>12)&0xffffff)	// 36-bit phys_mask (low bits=0)
#define MTRR_PUT_PM_PHYS_MASK(r,v) (((r)&(~((mpxp_int64_t)0xffffff<<12)))|((mpxp_int64_t)((v)&0xffffff)<<12))

#define MTRR_PUT_PAT(r,pa,v)   (((r)&(~((mpxp_int64_t)0xff<<(pa*8))))|((mpxp_int64_t)((v)&0xff)<<(pa*8)))

unsigned long asm_read_cr0(void);
void asm_write_cr0(unsigned long);
unsigned long asm_read_cr3(void);
void asm_write_cr3(unsigned long);
unsigned long asm_read_cr4(void);
void asm_write_cr4(unsigned long);
void asm_wbinvd(void);
void asm_flush_tlb(void);

#pragma aux asm_read_cr0 value[eax] = "mov eax,cr0"
#pragma aux asm_write_cr0 parm[eax] = "mov cr0,eax"
#pragma aux asm_read_cr3 value[eax] = "mov eax,cr3"
#pragma aux asm_write_cr3 parm[eax] = "mov cr3,eax"
#pragma aux asm_read_cr4 value[eax] = "mov eax,cr4"
#pragma aux asm_write_cr4 parm[eax] = "mov cr4,eax"
#pragma aux asm_wbinvd = "wbinvd"
#pragma aux asm_flush_tlb = "mov eax,cr3" "mov cr3,eax"

#define ENTER_CRITICAL IRQ_PUSH_OFF()
void IRQ_PUSH_OFF(void);
#pragma aux IRQ_PUSH_OFF = \
	"pushfd",          \
	"cli"              \
	modify [esp];

#define LEAVE_CRITICAL IRQ_POP()
void IRQ_POP(void);
#pragma aux IRQ_POP =   \
		"popfd" \
		"sti"   \
		modify [esp];

unsigned long asm_mtrr_read(unsigned long index, unsigned long *high);

static void pds_cpu_mtrr_read(unsigned long index, mpxp_int64_t * hilo)
{
#ifdef __WATCOMC__
	unsigned long hi, lo;
#pragma aux asm_mtrr_read=\
 "mov ecx,eax"\
 "push edx"\
 "xor eax,eax"\
 "xor edx,edx"\
 "rdmsr"\
 "pop ecx"\
 "mov dword ptr [ecx],edx"\
 parm[eax][edx] value[eax] modify[ecx];
	lo = asm_mtrr_read(index, &hi);
	*hilo = (((mpxp_int64_t) hi) << 32) | ((mpxp_int64_t) lo);
#else
	*hilo = 0;
#endif
}

void asm_mtrr_write(unsigned long index, unsigned long lo, unsigned long hi);

static void pds_cpu_mtrr_write(unsigned long index, mpxp_int64_t hilo)
{
#ifdef __WATCOMC__
#pragma aux asm_mtrr_write=\
 "wrmsr"\
 parm[ecx][eax][edx] modify[];
	asm_mtrr_write(index, (unsigned long)(hilo & 0xffffffff), (unsigned long)(hilo >> 32));
#endif
}

static unsigned long cpu_reg_cr4;
static mpxp_int64_t mtrr_deftype;

static void pds_cpu_mtrr_cache_disable(void)
{
	unsigned long cr0;

	// Enter the no-fill (CD=1, NW=0) cache mode and flush caches.
	cr0 = asm_read_cr0() | 0x40000000;	// set CD flag
	asm_wbinvd();
	asm_write_cr0(cr0);
	asm_wbinvd();
	//  Save value of CR4 and clear Page Global Enable (bit 7)
	if(mpxplay_cpu_capables_std & CPU_X86_STDCAP_PGE) {
		cpu_reg_cr4 = asm_read_cr4();
		asm_write_cr4(cpu_reg_cr4 & (unsigned char)~(1 << 7));
	}
	asm_flush_tlb();			// Flush all TLBs
	pds_cpu_mtrr_read(MTRR_REG_DEFTYPE, &mtrr_deftype);	//  Save MTRR state
	pds_cpu_mtrr_write(MTRR_REG_DEFTYPE, mtrr_deftype & 0xffffffff0000f300);	//  Disable MTRRs, and set the default type to uncached
}

static void pds_cpu_mtrr_cache_enable(void)
{
	asm_wbinvd();				// flush caches
	asm_flush_tlb();			// flush TLBs
	pds_cpu_mtrr_write(MTRR_REG_DEFTYPE, mtrr_deftype);	// Intel (P6) standard MTRRs
	asm_write_cr0(asm_read_cr0() & 0xbfffffff);	// enable caches
	if(mpxplay_cpu_capables_std & CPU_X86_STDCAP_PGE)
		asm_write_cr4(cpu_reg_cr4);	// Restore value of CR4
}

#endif

//#define MTRR_DEBUG 1

#ifdef __DOS__
static unsigned int pds_cpu_mtrr_setmode(unsigned long phys_ptr, unsigned long size_kb, unsigned int mode)
{
	unsigned long phys_base_ptr_shifted = phys_ptr >> 12;
	mpxp_int64_t hilo, mtrr_var_pb, mtrr_var_pm;
	int index = -1;
	unsigned long lo, hi, mtrr_var_cnt, i;
	union char_to_int64_t {
		unsigned char c[8];
		mpxp_int64_t i;
	} fix16k;
	unsigned long pb_phys_base, pm_valid;

	if(pds_mswin_getver() != 0)
		return 0;

	newfunc_cpu_init();

	if(!(mpxplay_cpu_capables_std & CPU_X86_STDCAP_MSR) || !(mpxplay_cpu_capables_std & CPU_X86_STDCAP_MTRR))
		return 0;

	pds_cpu_mtrr_read(MTRR_REG_CAP, &hilo);
	hi = hilo >> 32;
	lo = hilo & 0xffffffff;
	mtrr_var_cnt = lo & 0xff;
#ifdef MTRR_DEBUG
	fprintf(stdout, "cap: %8.8X %8.8X cnt:%d psy_s:%2X\n", lo, hi, mtrr_var_cnt, phys_base_ptr_shifted);
#endif
	if(!(lo & 0x400) || !(lo & 0x100))	// WC and fixMTRR modes are supported?
		return 0;

	//size_kb=(size_kb+4095)&(~4095); // ??? page aligned (Causeway works on this way only)

	if(phys_base_ptr_shifted == 0xA0) {	// if A0000h memory area used
		pds_cpu_mtrr_read(MTRR_REG_FIX16K_A0000, &fix16k.i);	// BC000-B8000-B4000-B0000-AC000-A8000-A4000-A0000
		for(i = 0; i < 8; i++) {	// for 16kB block 0-7
			if(i < (size_kb / 16))	// limit by area size
				fix16k.c[i] = mode;
		}
		pds_cpu_mtrr_write(MTRR_REG_FIX16K_A0000, fix16k.i);	// write modified MSR
		return (1);
	}

	for(i = 0; i < mtrr_var_cnt; i++) {
		pds_cpu_mtrr_read(MTRR_REG_PHYSBASE(i), &mtrr_var_pb);
		pds_cpu_mtrr_read(MTRR_REG_PHYSMASK(i), &mtrr_var_pm);
		pb_phys_base = MTRR_GET_PB_PHYS_BASE(mtrr_var_pb);
		pm_valid = MTRR_GET_PM_VALID(mtrr_var_pm);
#ifdef MTRR_DEBUG
		fprintf(stdout, "%d. %8.8X %8.8X %8.8X %8.8X \n", i, (unsigned long)(mtrr_var_pb >> 32), (unsigned long)(mtrr_var_pb & 0xffffffff),
				(unsigned long)(mtrr_var_pm >> 32), (unsigned long)(mtrr_var_pm & 0xffffffff));
#endif
		if(pb_phys_base == phys_base_ptr_shifted) {	// MTRR is allready set for our PB
			index = i;
			break;
		}
		if(!pm_valid && (index < 0))
			index = i;
	}
	if(index < 0)				// no free MTRR found
		return 0;

	ENTER_CRITICAL;

	pds_cpu_mtrr_cache_disable();

	//update and write back mtrr info
	mtrr_var_pb = MTRR_PUT_PB_MODE(mtrr_var_pb, mode);
	mtrr_var_pb = MTRR_PUT_PB_PHYS_BASE(mtrr_var_pb, phys_base_ptr_shifted);
	pds_cpu_mtrr_write(MTRR_REG_PHYSBASE(index), mtrr_var_pb);
	mtrr_var_pm = MTRR_PUT_PM_VALID(mtrr_var_pm, ((size_kb) ? 1 : 0));
	mtrr_var_pm = MTRR_PUT_PM_PHYS_MASK(mtrr_var_pm, (~(phys_base_ptr_shifted ^ (phys_base_ptr_shifted + (size_kb >> 2) - 1))) & 0xFFFFFF);
	pds_cpu_mtrr_write(MTRR_REG_PHYSMASK(index), mtrr_var_pm);

#ifdef MTRR_DEBUG
	pds_cpu_mtrr_read(MTRR_REG_DEFTYPE, &hilo);
	fprintf(stdout, "deftype old %8.8X %8.8X \n", (unsigned long)(hilo >> 32), (unsigned long)(hilo & 0xffffffff));
#endif

	//set page attribute table
	if((mpxplay_cpu_capables_std & CPU_X86_STDCAP_PAT) && (index < 8)) {
		pds_cpu_mtrr_read(MTRR_REG_PAT, &hilo);
		hilo = MTRR_PUT_PAT(hilo, index, mode);
		pds_cpu_mtrr_write(MTRR_REG_PAT, hilo);
	}
	//enable mtrr's
	if(mode)
		mtrr_deftype |= 0x800;
	pds_cpu_mtrr_cache_enable();

	LEAVE_CRITICAL;

#ifdef MTRR_DEBUG
	pds_cpu_mtrr_read(MTRR_REG_DEFTYPE, &hilo);
	fprintf(stdout, "deftype new %8.8X %8.8X \n", (unsigned long)(hilo >> 32), (unsigned long)(hilo & 0xffffffff));
	fprintf(stdout, "index: %d pb_phys_base:%8.8X pm_valid:%d\n", index, pb_phys_base, pm_valid);
	fprintf(stdout, "w: %8.8X %8.8X %8.8X %8.8X \n", (unsigned long)(mtrr_var_pb >> 32), (unsigned long)(mtrr_var_pb & 0xffffffff),
			(unsigned long)(mtrr_var_pm >> 32), (unsigned long)(mtrr_var_pm & 0xffffffff));
	pds_cpu_mtrr_read(MTRR_REG_PHYSBASE(index), &mtrr_var_pb);
	pds_cpu_mtrr_read(MTRR_REG_PHYSMASK(index), &mtrr_var_pm);
	fprintf(stdout, "r: %8.8X %8.8X %8.8X %8.8X \n", (unsigned long)(mtrr_var_pb >> 32), (unsigned long)(mtrr_var_pb & 0xffffffff),
			(unsigned long)(mtrr_var_pm >> 32), (unsigned long)(mtrr_var_pm & 0xffffffff));
#endif

	return 1;
}
#endif

unsigned int pds_cpu_mtrr_enable_wc(unsigned long phys_ptr, unsigned long size_kb)
{
#ifdef __DOS__
	return pds_cpu_mtrr_setmode(phys_ptr, size_kb, MTRR_MODE_WC);
#else
	return 0;
#endif
}

void pds_cpu_mtrr_disable_wc(unsigned long phys_ptr)
{
#ifdef __DOS__
	pds_cpu_mtrr_setmode(phys_ptr, 0, MTRR_MODE_UC);
#endif
}

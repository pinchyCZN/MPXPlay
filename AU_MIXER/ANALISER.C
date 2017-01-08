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
//function: volume level/pcm spectrum analiser
//fixme : works properly on stereo data only!

#include "au_mixer.h"
#include "newfunc\newfunc.h"

extern unsigned int analtabnum;
extern unsigned long analtab[5][32], volnum[5][2];

extern unsigned int displaymode;
extern unsigned int SOUNDLIMITvol;

//--------------------------------------------------------------------------

//void asm_get_volumelevel_maxsign(short *,unsigned int);
void asm_get_volumelevel_average(short *, unsigned int);

void mixer_get_volumelevel(short *pcm_sample, unsigned int samplenum, unsigned int channelnum)
{
	unsigned int channelskip = (channelnum > 2) ? ((channelnum - 2) * 2 + 2) : 2;
	samplenum = samplenum / channelnum * 2;
/*#pragma aux asm_get_volumelevel_maxsign=\
 "mov edi,eax"\
 "shr edx,1"\
 "xor ebx,ebx"\
 "xor esi,esi"\
 "mov ecx,dword ptr channelskip"\
 "vload1:movsx eax,word ptr [edi]"\
  "test eax,eax"\
  "jge vpositivl"\
  "neg eax"\
  "vpositivl:cmp ebx,eax"\
  "jge nochl"\
   "mov ebx,eax"\
  "nochl:"\
  "add edi,2"\
  "movsx eax,word ptr [edi]"\
  "test eax,eax"\
  "jge vpositivr"\
  "neg eax"\
  "vpositivr:cmp esi,eax"\
  "jge nochr"\
   "mov esi,eax"\
  "nochr:"\
  "add edi,ecx"\
  "dec edx"\
 "jnz vload1"\
 "mov edi,dword ptr analtabnum"\
 "shl edi,3"\
 "add edi,offset volnum"\
 "mov dword ptr [edi],ebx"\
 "mov dword ptr 4[edi],esi"\
 parm[eax][edx] modify [ebx ecx edi esi];

 asm_get_volumelevel_maxsign(pcm_sample,samplenum);*/
#ifdef WIN32
	__asm {
		mov edx, samplenum mov edi, pcm_sample shr edx, 1 mov ecx, edx xor ebx, ebx xor esi, esi vload1:movsx eax, word ptr[edi]
		test eax, eax jge vpositivl neg eax vpositivl:add ebx, eax add edi, 2 movsx eax, word ptr[edi]
test eax, eax
			jge vpositivr
			neg eax
			vpositivr:add esi, eax
			add edi, dword ptr channelskip
			dec edx
			jnz vload1
			shr ecx, 5
			test ecx, ecx
			jz nodiv
			mov eax, ebx
			xor edx, edx
			div ecx
			mov ebx, eax mov eax, esi xor edx, edx div ecx mov esi, eax nodiv: mov edi, dword ptr analtabnum shl edi, 3 add edi, offset volnum mov dword ptr[edi], ebx mov dword ptr 4[edi], esi}}
#else
#pragma aux asm_get_volumelevel_average=\
 "mov edi,eax"\
 "shr edx,1"\
 "mov ecx,edx"\
 "xor ebx,ebx"\
 "xor esi,esi"\
 "vload1:movsx eax,word ptr [edi]"\
  "test eax,eax"\
  "jge vpositivl"\
  "neg eax"\
  "vpositivl:add ebx,eax"\
  "add edi,2"\
  "movsx eax,word ptr [edi]"\
  "test eax,eax"\
  "jge vpositivr"\
  "neg eax"\
  "vpositivr:add esi,eax"\
  "add edi,dword ptr channelskip"\
  "dec edx"\
 "jnz vload1"\
 "shr ecx,5"\
 "test ecx,ecx"\
 "jz nodiv"\
  "mov eax,ebx"\
  "xor edx,edx"\
  "div ecx"\
  "mov ebx,eax"\
  "mov eax,esi"\
  "xor edx,edx"\
  "div ecx"\
  "mov esi,eax"\
 "nodiv:"\
 "mov edi,dword ptr analtabnum"\
 "shl edi,3"\
 "add edi,offset volnum"\
 "mov dword ptr [edi],ebx"\
 "mov dword ptr 4[edi],esi"\
 parm[eax][edx] modify [ebx ecx edi esi];

	asm_get_volumelevel_average(pcm_sample, samplenum);
}
#endif							//WIN32

/*void get_volumelevel(short *pcm_sample,unsigned int samplenum)
{
 mixer_get_volumelevel_lq(pcm_sample,samplenum);
}

static int mixer_get_volumelevel_check(void)
{
 if((displaymode&DISP_TIMEPOS) || SOUNDLIMITvol)
  return 1;
 return 0;
}

one_mixerfunc_info MIXER_FUNCINFO_getvolume={
 "MIX_GETVOLUME",
 NULL,
 NULL,
 MIXER_INFOBIT_SWITCH|MIXER_INFOBIT_ANALISER16,
 0,0,0,0,
 NULL,
 &mixer_get_volumelevel_lq,
 &mixer_get_volumelevel_hq,
 &mixer_get_volumelevel_check,
 NULL
};*/

//--------------------------------------------------------------------------
//pcm (integer based) spectrum analiser (from the OpenCP)

#define POW         11
#define SAMPLES     (1<<POW)
#define SAMPLES2    (1<<(POW-1))

static long cossintab86[SAMPLES2][2] = { {268435455, 0}, {268434192, 823548}, {268430402, 1647088},
{268424086, 2470614}, {268415243, 3294115}, {268403873, 4117586},
{268389978, 4941018}, {268373556, 5764404}, {268354608, 6587735},
{268333134, 7411005}, {268309134, 8234204}, {268282610, 9057326},
{268253559, 9880363}, {268221985, 10703307}, {268187885, 11526150},
{268151261, 12348885}, {268112113, 13171503}, {268070442, 13993997},
{268026247, 14816360}, {267979530, 15638583}, {267930290, 16460659},
{267878529, 17282580}, {267824246, 18104339}, {267767442, 18925927},
{267708118, 19747337}, {267646274, 20568561}, {267581911, 21389591},
{267515029, 22210420}, {267445630, 23031040}, {267373713, 23851443},
{267299279, 24671622}, {267222330, 25491569}, {267142865, 26311275},
{267060886, 27130734}, {266976394, 27949938}, {266889388, 28768878},
{266799870, 29587548}, {266707841, 30405939}, {266613302, 31224044},
{266516253, 32041855}, {266416696, 32859365}, {266314631, 33676565},
{266210059, 34493448}, {266102982, 35310007}, {265993400, 36126233},
{265881315, 36942120}, {265766727, 37757658}, {265649637, 38572841},
{265530047, 39387661}, {265407958, 40202111}, {265283370, 41016182},
{265156286, 41829866}, {265026706, 42643158}, {264894631, 43456047},
{264760063, 44268528}, {264623003, 45080592}, {264483453, 45892232},
{264341413, 46703440}, {264196884, 47514208}, {264049870, 48324529},
{263900369, 49134395}, {263748385, 49943799}, {263593918, 50752733},
{263436971, 51561188}, {263277543, 52369159}, {263115638, 53176637},
{262951256, 53983614}, {262784399, 54790083}, {262615069, 55596036},
{262443267, 56401466}, {262268994, 57206365}, {262092254, 58010726},
{261913046, 58814541}, {261731373, 59617802}, {261547236, 60420502},
{261360638, 61222633}, {261171579, 62024188}, {260980063, 62825159},
{260786089, 63625539}, {260589662, 64425320}, {260390781, 65224495},
{260189450, 66023056}, {259985670, 66820995}, {259779442, 67618305},
{259570769, 68414979}, {259359654, 69211009}, {259146097, 70006387},
{258930101, 70801107}, {258711667, 71595160}, {258490799, 72388539},
{258267497, 73181237}, {258041765, 73973246}, {257813604, 74764559},
{257583016, 75555168}, {257350004, 76345066}, {257114569, 77134246},
{256876715, 77922699}, {256636443, 78710419}, {256393755, 79497398},
{256148653, 80283629}, {255901141, 81069104}, {255651220, 81853816},
{255398893, 82637758}, {255144162, 83420922}, {254887030, 84203301},
{254627498, 84984887}, {254365570, 85765673}, {254101247, 86545652},
{253834533, 87324816}, {253565430, 88103159}, {253293940, 88880672},
{253020066, 89657348}, {252743810, 90433181}, {252465175, 91208162},
{252184165, 91982285}, {251900780, 92755543}, {251615025, 93527927},
{251326901, 94299431}, {251036411, 95070047}, {250743559, 95839768},
{250448347, 96608588}, {250150777, 97376498}, {249850853, 98143491},
{249548577, 98909561}, {249243953, 99674700}, {248936982, 100438900},
{248627668, 101202156}, {248316014, 101964458}, {248002023, 102725801},
{247685698, 103486177}, {247367041, 104245579}, {247046056, 105004000},
{246722745, 105761432}, {246397113, 106517869}, {246069161, 107273304},
{245738893, 108027729}, {245406312, 108781136}, {245071421, 109533520},
{244734224, 110284873}, {244394723, 111035188}, {244052922, 111784458},
{243708823, 112532676}, {243362431, 113279835}, {243013748, 114025927},
{242662778, 114770946}, {242309523, 115514885}, {241953988, 116257736},
{241596176, 116999493}, {241236089, 117740150}, {240873733, 118479697},
{240509108, 119218130}, {240142220, 119955440}, {239773072, 120691622},
{239401667, 121426667}, {239028009, 122160570}, {238652100, 122893322},
{238273946, 123624918}, {237893548, 124355351}, {237510912, 125084613},
{237126040, 125812697}, {236738936, 126539598}, {236349604, 127265307},
{235958047, 127989818}, {235564270, 128713125}, {235168275, 129435220},
{234770066, 130156097}, {234369648, 130875749}, {233967024, 131594169},
{233562198, 132311350}, {233155173, 133027287}, {232745954, 133741970},
{232334544, 134455396}, {231920947, 135167555}, {231505167, 135878442},
{231087209, 136588051}, {230667075, 137296374}, {230244770, 138003404},
{229820298, 138709136}, {229393663, 139413562}, {228964868, 140116675},
{228533919, 140818470}, {228100818, 141518940}, {227665571, 142218077},
{227228181, 142915876}, {226788652, 143612330}, {226346988, 144307431},
{225903194, 145001175}, {225457273, 145693554}, {225009231, 146384561},
{224559070, 147074191}, {224106796, 147762436}, {223652413, 148449291},
{223195924, 149134748}, {222737335, 149818802}, {222276649, 150501445},
{221813871, 151182672}, {221349005, 151862476}, {220882056, 152540851},
{220413028, 153217789}, {219941925, 153893286}, {219468752, 154567334},
{218993513, 155239927}, {218516213, 155911059}, {218036856, 156580724},
{217555447, 157248914}, {217071990, 157915625}, {216586490, 158580849},
{216098952, 159244581}, {215609379, 159906814}, {215117778, 160567542},
{214624151, 161226758}, {214128504, 161884457}, {213630842, 162540632},
{213131169, 163195277}, {212629490, 163848386}, {212125809, 164499953},
{211620132, 165149972}, {211112463, 165798436}, {210602807, 166445340},
{210091169, 167090677}, {209577553, 167734441}, {209061965, 168376627},
{208544409, 169017227}, {208024890, 169656237}, {207503413, 170293650},
{206979983, 170929460}, {206454605, 171563661}, {205927283, 172196248},
{205398023, 172827213}, {204866830, 173456552}, {204333709, 174084258},
{203798664, 174710326}, {203261701, 175334749}, {202722825, 175957522},
{202182041, 176578639}, {201639354, 177198094}, {201094769, 177815881},
{200548291, 178431994}, {199999926, 179046428}, {199449678, 179659176},
{198897553, 180270234}, {198343555, 180879594}, {197787691, 181487253},
{197229965, 182093203}, {196670383, 182697439}, {196108950, 183299955},
{195545670, 183900746}, {194980551, 184499806}, {194413595, 185097130},
{193844811, 185692711}, {193274201, 186286545}, {192701772, 186878625},
{192127530, 187468946}, {191551479, 188057503}, {190973625, 188644290},
{190393974, 189229301}, {189812531, 189812531}
};

static unsigned short permtab[SAMPLES];
static long x86[SAMPLES][2];
static int fftinited;

/*unsigned long isqrt(unsigned long);
#pragma aux isqrt parm [ebx] value [edx] modify [eax ebx ecx edx] = \
"  mov ecx,40000000h" \
"fastloop:" \
"    cmp ebx,ecx" \
"    jae near goloop" \
"    shr ecx,2" \
"  jnz fastloop" \
"  xor edx,edx" \
"  jmp loopend" \
"" \
"goloop: " \
"  mov edx,ecx" \
"  sub ebx,ecx" \
"  xor eax,eax" \
"  toobig: "\
"      add ebx,eax" \
"    shr ecx,2" \
"    jz loopend" \
"  sqrtloop:"\
"    mov eax,edx" \
"    add eax,ecx" \
"    shr edx,1" \
"    sub ebx,eax" \
"    jb toobig"\
"      or edx,ecx"\
"    shr ecx,2"\
"    jnz sqrtloop" \
"loopend:"*/

#ifdef WIN32
void fftCalc(long *xi, long *cos, unsigned long d2)
{
	__asm {
		mov edx, d2 shl edx, 2 mov edi, cos mov esi, xi mov ebx,[esi]
		mov ecx,[esi + edx]
		mov eax, ebx add ebx, ecx sub eax, ecx sar ebx, 1 push eax push eax mov[esi], ebx mov ecx,[esi + edx + 4]
		mov ebx,[esi + 4]
		mov eax, ebx add ebx, ecx sub eax, ecx sar ebx, 1 mov ecx, eax mov[esi + 4], ebx add esi, edx mov edx,[edi + 4]
		xor ebx, ebx imul edx shrd eax, edx, 29 mov edx,[edi]
		sub ebx, eax pop eax imul edx shrd eax, edx, 29 add ebx, eax mov edx,[edi + 4]
		pop eax mov[esi], ebx imul edx shrd eax, edx, 29 mov ebx, eax mov eax,[edi]
imul ecx shrd eax, edx, 29 add ebx, eax mov[esi + 4], ebx}}
#else

void fftCalc(long *xi, long *cos, unsigned long d2);
#pragma aux fftCalc parm [esi][edi][edx] modify [eax ebx ecx edx esi edi] = \
"shl  edx, 2" \
"" \
"mov  ebx, [esi] " \
"mov  ecx, [esi+edx]" \
"mov  eax, ebx" \
"add  ebx, ecx" \
"sub  eax, ecx" \
"sar  ebx, 1" \
"push eax" \
"push eax" \
"mov  [esi], ebx" \
"" \
"mov  ecx, [esi+edx+4]" \
"mov  ebx, [esi+4] " \
"mov  eax, ebx" \
"add  ebx, ecx" \
"sub  eax, ecx" \
"sar  ebx, 1" \
"mov  ecx, eax" \
"mov  [esi+4], ebx" \
"" \
"add  esi, edx" \
"" \
"mov  edx, [edi+4]" \
"xor  ebx, ebx" \
"imul edx" \
"shrd eax, edx, 29" \
"mov  edx, [edi]" \
"sub  ebx, eax" \
"pop  eax" \
"imul edx" \
"shrd eax, edx, 29" \
"add  ebx, eax" \
"mov  edx, [edi+4]" \
"pop  eax" \
"mov  [esi], ebx" \
"imul edx" \
"shrd eax, edx, 29" \
"mov  ebx, eax" \
"mov  eax, [edi]" \
"imul ecx" \
"shrd eax, edx, 29" \
"add  ebx, eax" \
"mov  [esi+4], ebx"
#endif

static void fftInit(void)
{
	int i, j, k;
	j = 0;
	for(i = 0; i < SAMPLES; ++i) {
		permtab[i] = j;
		for(k = SAMPLES2; k && (k <= j); k >>= 1)
			j -= k;
		j += k;
	}
	for(i = SAMPLES2 / 4 + 1; i <= SAMPLES2 / 2; ++i) {
		cossintab86[i][0] = cossintab86[SAMPLES2 / 2 - i][1];
		cossintab86[i][1] = cossintab86[SAMPLES2 / 2 - i][0];
	}
	for(i = SAMPLES2 / 2 + 1; i < SAMPLES2; ++i) {
		cossintab86[i][0] = -cossintab86[SAMPLES2 - i][0];
		cossintab86[i][1] = cossintab86[SAMPLES2 - i][1];
	}
	fftinited = 1;
}

static void dofft86(long (*x)[2], const int n)
{
	int i, j;
	long *xe = x[1 << n], curcossin[2], *xi;

	for(i = POW - n; i < POW; ++i) {
		const unsigned long s2dk = SAMPLES2 >> i;
		const unsigned long d2 = s2dk << 1;
		for(j = 0; j < s2dk; ++j) {
			curcossin[0] = cossintab86[j << i][0];
			curcossin[1] = cossintab86[j << i][1];
			for(xi = x[j]; xi < xe; xi += (d2 << 1))
				fftCalc(xi, curcossin, d2);
		}
	}
}

void mixer_pcm_spectrum_analiser(short *pcm_sample, unsigned int samplenum, unsigned int channelnum)
{
	//const int outsamplenum=(samplenum>>1)
	unsigned long *ana = &analtab[analtabnum][0];
	const int bits[2] = { 10, 7 };	// 1024+128 (1<<10)+(1<<7) = 1152 samples
	const unsigned int outsamplenum[2] = { 1024, 128 };	//1<<bits[0],1<<bits[1]};
	const unsigned int half[2] = { 512, 64 };	//outsamplenum[0]>>1,outsamplenum[1]>>1};
	const unsigned int anashift[2] = { 4, 1 };	// 512>>4=32, 64>>1=32 subband channels
	int i, j;

	if(!fftinited)
		fftInit();

	for(i = 0; i < 32; i++)
		ana[i] = 0;

	if(samplenum < 2304) {
		short *pcm = pcm_sample + samplenum;
		i = 2304 - samplenum;
		while(i--)
			*pcm++ = 0;
	}

	for(j = 0; j < 2; j++) {
		for(i = 0; i < outsamplenum[j]; i++) {
			x86[i][0] = ((long)(pcm_sample[0]) + (long)(pcm_sample[1])) << 12;
			pcm_sample += channelnum;
			x86[i][1] = 0;
		}
		dofft86(x86, bits[j]);
		for(i = 1; i <= half[j]; ++i) {
			float ff;
			long x0, x1, index;
			index = permtab[i] >> (POW - bits[j]);
			x0 = x86[index][0] >> 12;
			x1 = x86[index][1] >> 12;
			x0 = x0 * x0 + x1 * x1;
			ff = sqrt((float)x0);
			pds_ftoi(ff, &x0);
			ana[(i - 1) >> anashift[j]] += x0 << 1;
		}
	}
}

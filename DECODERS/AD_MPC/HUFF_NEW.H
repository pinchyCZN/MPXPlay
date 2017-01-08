#ifndef _huff_new_h_
#define _huff_new_h

#define LARGEST_TABLE 63

typedef struct Huffmancode {
	unsigned int Code;
	unsigned int Length;
	int Value;
} HuffmanTyp;

typedef struct {
	unsigned int Code:16;		// >= 14 bit
	unsigned int Length:8;		// >=  4 bit
} HuffSrcTyp;

extern void mpcdec_Huffman_Decoder_SV7_init(void);

extern HuffmanTyp HuffHdr[10];
extern HuffmanTyp HuffSCFI[4];
extern HuffmanTyp HuffDSCF[16];
extern HuffmanTyp *HuffQ[2][8];

#endif

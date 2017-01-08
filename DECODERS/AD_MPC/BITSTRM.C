#include "bitstrm.h"

static unsigned long mask[] = {
	0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000f,
	0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff, 0x000001ff,
	0x000003ff, 0x000007ff, 0x00000fff, 0x00001fff, 0x00003fff,
	0x00007fff, 0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
	0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff,
	0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff, 0x1fffffff,
	0x3fffffff, 0x7fffffff, 0xffffffff
};

void mpcdec_bitstream_reset(struct mpc_decoder_data *mpci)
{
	mpci->bs_bitpos = mpci->bs_dword = mpci->bs_elemcount = mpci->bs_putbyteptr = 0;
	mpci->bs_elementptr = mpci->bs_forwardbits = mpci->bs_rewindbits = 0;
}

void mpcdec_bitstream_fill(struct mpc_decoder_data *mpci, unsigned long (*readfunc) (void *fbds, void *buf, unsigned long len), void *fbds, unsigned int num)
{
	long bytes = 0, i;

	i = MPC_BITSTREAM_BYTESIZE - mpci->bs_putbyteptr;

	if(i <= num) {
		bytes = readfunc(fbds, ((char *)(mpci->bitstream)) + mpci->bs_putbyteptr, i);
		mpci->bs_putbyteptr += bytes;
		mpci->bs_forwardbits += bytes * 8;
		if(mpci->bs_rewindbits > bytes * 8)
			mpci->bs_rewindbits -= bytes * 8;
		else
			mpci->bs_rewindbits = 0;
		if(bytes != i)
			return;
		num -= i;
		mpci->bs_putbyteptr = 0;
	}
	if(num) {
		bytes = readfunc(fbds, ((char *)(mpci->bitstream)) + mpci->bs_putbyteptr, num);
		mpci->bs_putbyteptr += bytes;
		mpci->bs_forwardbits += bytes * 8;
		if(mpci->bs_rewindbits > bytes * 8)
			mpci->bs_rewindbits -= bytes * 8;
		else
			mpci->bs_rewindbits = 0;
	}
	mpci->bs_dword = mpci->bitstream[mpci->bs_elementptr];
}

void mpcdec_bitstream_forward(struct mpc_decoder_data *mpci, unsigned int bits)
{
	if(bits > mpci->bs_forwardbits) {
		mpcdec_bitstream_reset(mpci);
	} else {
		mpci->bs_forwardbits -= bits;
		mpci->bs_rewindbits += bits;
		mpci->bs_bitpos += bits;
		mpci->bs_elemcount += mpci->bs_bitpos >> 5;
		mpci->bs_elementptr += mpci->bs_bitpos >> 5;
		mpci->bs_elementptr &= MPC_BITSTREAM_ELEMENTMASK;
		mpci->bs_bitpos &= 31;
		mpci->bs_dword = mpci->bitstream[mpci->bs_elementptr];
	}
}

void mpcdec_bitstream_rewind(struct mpc_decoder_data *mpci, unsigned int bits)
{
	unsigned int currbitpos = mpcdec_BitsRead(mpci), bitptr;
	if(bits > mpci->bs_rewindbits) {
		mpcdec_bitstream_reset(mpci);
	} else {
		mpci->bs_forwardbits += bits;
		mpci->bs_rewindbits -= bits;
		bitptr = mpci->bs_elementptr * 32 + mpci->bs_bitpos;
		if(bitptr >= bits)
			bitptr -= bits;
		else
			bitptr = bitptr + MPC_BITSTREAM_ELEMENTMASK * 32 - bits;
		mpci->bs_elementptr = bitptr / 32;
		currbitpos -= bits;
		mpci->bs_bitpos = currbitpos & 31;
		mpci->bs_elemcount = currbitpos / 32;
		mpci->bs_dword = mpci->bitstream[mpci->bs_elementptr];
	}
}

unsigned int mpcdec_BitsRead(struct mpc_decoder_data *mpci)
{
	return (32 * mpci->bs_elemcount + mpci->bs_bitpos);
}

unsigned int mpcdec_Bitstream_read(struct mpc_decoder_data *mpci, unsigned int bits)
{
	unsigned int out;

	if(mpci->bs_forwardbits < bits) {
		mpci->bs_forwardbits = 0;
		mpci->bs_rewindbits = 0;
		return 0;
	}
	mpci->bs_forwardbits -= bits;
	mpci->bs_rewindbits += bits;

	out = mpci->bs_dword;

	mpci->bs_bitpos += bits;

	if(mpci->bs_bitpos < 32) {
		out >>= (32 - mpci->bs_bitpos);
	} else {
		mpci->bs_elementptr++;
		mpci->bs_elementptr &= MPC_BITSTREAM_ELEMENTMASK;
		mpci->bs_dword = mpci->bitstream[mpci->bs_elementptr];
		mpci->bs_bitpos -= 32;
		if(mpci->bs_bitpos) {
			out <<= mpci->bs_bitpos;
			out |= mpci->bs_dword >> (32 - mpci->bs_bitpos);
		}
		mpci->bs_elemcount++;
	}

	return (out & mask[bits]);
}

int mpcdec_Huffman_Decode(struct mpc_decoder_data *mpci, HuffmanTyp * Table)
{
	unsigned int code, bits;

	code = mpci->bs_dword << mpci->bs_bitpos;
	if(mpci->bs_bitpos > 18)
		code |= mpci->bitstream[(mpci->bs_elementptr + 1) & MPC_BITSTREAM_ELEMENTMASK] >> (32 - mpci->bs_bitpos);
	while(code < Table->Code)
		Table++;

	bits = Table->Length;
	if(mpci->bs_forwardbits < bits) {
		mpci->bs_forwardbits = 0;
		mpci->bs_rewindbits = 0;
		return 0;
	}
	mpci->bs_forwardbits -= bits;
	mpci->bs_rewindbits += bits;

	if((mpci->bs_bitpos += Table->Length) >= 32) {
		mpci->bs_bitpos -= 32;
		mpci->bs_elementptr++;
		mpci->bs_elementptr &= MPC_BITSTREAM_ELEMENTMASK;
		mpci->bs_dword = mpci->bitstream[mpci->bs_elementptr];
		mpci->bs_elemcount++;
	}

	return Table->Value;
}

int mpcdec_Huffman_Decode_fast(struct mpc_decoder_data *mpci, HuffmanTyp * Table)
{
	unsigned int code, bits;

	code = mpci->bs_dword << mpci->bs_bitpos;

	if(mpci->bs_bitpos > 22)
		code |= mpci->bitstream[(mpci->bs_elementptr + 1) & MPC_BITSTREAM_ELEMENTMASK] >> (32 - mpci->bs_bitpos);
	while(code < Table->Code)
		Table++;

	bits = Table->Length;
	if(mpci->bs_forwardbits < bits) {
		mpci->bs_forwardbits = 0;
		mpci->bs_rewindbits = 0;
		return 0;
	}
	mpci->bs_forwardbits -= bits;
	mpci->bs_rewindbits += bits;

	if((mpci->bs_bitpos += Table->Length) >= 32) {
		mpci->bs_bitpos -= 32;
		mpci->bs_elementptr++;
		mpci->bs_elementptr &= MPC_BITSTREAM_ELEMENTMASK;
		mpci->bs_dword = mpci->bitstream[mpci->bs_elementptr];
		mpci->bs_elemcount++;
	}

	return Table->Value;
}

int mpcdec_Huffman_Decode_faster(struct mpc_decoder_data *mpci, HuffmanTyp * Table)
{
	unsigned int code, bits;

	code = mpci->bs_dword << mpci->bs_bitpos;
	if(mpci->bs_bitpos > 27)
		code |= mpci->bitstream[(mpci->bs_elementptr + 1) & MPC_BITSTREAM_ELEMENTMASK] >> (32 - mpci->bs_bitpos);
	while(code < Table->Code)
		Table++;

	bits = Table->Length;
	if(mpci->bs_forwardbits < bits) {
		mpci->bs_forwardbits = 0;
		mpci->bs_rewindbits = 0;
		return 0;
	}
	mpci->bs_forwardbits -= bits;
	mpci->bs_rewindbits += bits;

	if((mpci->bs_bitpos += Table->Length) >= 32) {
		mpci->bs_bitpos -= 32;
		mpci->bs_elementptr++;
		mpci->bs_elementptr &= MPC_BITSTREAM_ELEMENTMASK;
		mpci->bs_dword = mpci->bitstream[mpci->bs_elementptr];
		mpci->bs_elemcount++;
	}

	return Table->Value;
}

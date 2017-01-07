#ifndef _bitstream_decode_h_
#define _bitstream_decode_h_

#include "mpcdec.h"
#include "huff_new.h"

#define MPC_BITSTREAM_ELEMENTSIZE   8192
#define MPC_BITSTREAM_ELEMENTSIZE2 (MPC_BITSTREAM_ELEMENTSIZE/2)
#define MPC_BITSTREAM_ELEMENTMASK  (MPC_BITSTREAM_ELEMENTSIZE-1)

#define MPC_BITSTREAM_BYTESIZE  (MPC_BITSTREAM_ELEMENTSIZE*sizeof(unsigned int))
#define MPC_BITSTREAM_BYTESIZE2 (MPC_BITSTREAM_BYTESIZE/2)
#define MPC_BITSTREAM_BYTEMASK  (MPC_BITSTREAM_BYTESIZE-1)

void mpcdec_bitstream_reset(struct mpc_decoder_data *mpci);
void mpcdec_bitstream_fill(struct mpc_decoder_data *mpci,unsigned long (*readfunc)(void *fbds,void *buf,unsigned long len),void *fbds,unsigned int num);
void mpcdec_bitstream_forward(struct mpc_decoder_data *,unsigned int);
void mpcdec_bitstream_rewind(struct mpc_decoder_data *,unsigned int);

unsigned int mpcdec_BitsRead(struct mpc_decoder_data *);
unsigned int mpcdec_Bitstream_read(struct mpc_decoder_data *,unsigned int);
int mpcdec_Huffman_Decode       (struct mpc_decoder_data *,HuffmanTyp *);// works with maximum lengths up to 14
int mpcdec_Huffman_Decode_fast  (struct mpc_decoder_data *,HuffmanTyp *);// works with maximum lengths up to 10
int mpcdec_Huffman_Decode_faster(struct mpc_decoder_data *,HuffmanTyp *);// works with maximum lengths up to  5

#endif

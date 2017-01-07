#ifndef mpxplay_mpxinbuf_h
#define mpxplay_mpxinbuf_h

#include "mpxplay.h"

//prebuffertype and frp->buffertype controlbits
#define PREBUFTYPE_NONE      0 // -bn
#define PREBUFTYPE_SHORTRING 1 // -bs
#define PREBUFTYPE_LONGRING  2 // -bp
#define PREBUFTYPE_FULL      4 // -bl
#define PREBUFTYPE_RING      8
#define PREBUFTYPE_MASK      (PREBUFTYPE_SHORTRING|PREBUFTYPE_LONGRING|PREBUFTYPE_RING|PREBUFTYPE_FULL)
#define PREBUFTYPE_PRELOADNEXT   16 // -bpn
#define PREBUFTYPE_BACK          32 // -bb
#define PREBUFTYPE_INT           64 // use intsound (-bp,-bl)
#define PREBUFTYPE_PUT_BACKBUF_PERCENT(t,p) t|=((p&0x7f)<<24)
#define PREBUFTYPE_GET_BACKBUF_PERCENT(t)   (((t)>>24)&0x7f)
#define PREBUFTYPE_BACKBUF_PERCENT_MAX     98
//frp->buffertype flags
#define PREBUFTYPE_FILLED           256 // buffer is full
#define PREBUFTYPE_WRITEPROTECT     512 // don't write into buffer (only read)
#define PREBUFTYPE_LOADNEXT_OK     1024 //
#define PREBUFTYPE_LOADNEXT_FAILED 2048 //
#define PREBUFTYPE_LOADNEXT_MASK   (PREBUFTYPE_LOADNEXT_OK|PREBUFTYPE_LOADNEXT_FAILED)

#define PREBUFFERBLOCKSIZE_CHECK  2048  // was 512
#define PREBUFFERBLOCKSIZE_DECODE 32768

#define PREBUFFERBLOCKS_SHORTRING  4 // default number of blocks in normal (non -bp) mode
#define PREBUFFERBLOCKS_LONGRING  32 // default number of blocks in -bp mode

#define PREBUFFERSIZE_CHECK (PREBUFFERBLOCKS_SHORTRING*PREBUFFERBLOCKSIZE_DECODE)

#define PREBUFFER_SEEKRETRY_INVALID -1

extern void         mpxplay_mpxinbuf_init(struct mainvars *mvp);
extern void         mpxplay_mpxinbuf_assign_funcs(struct frame *frp);
extern void         mpxplay_mpxinbuf_prealloc(struct mainvars *mvp);
extern void         mpxplay_mpxinbuf_close(struct mainvars *mvp);
extern unsigned int mpxplay_mpxinbuf_alloc(struct mainvars *,struct frame *);
extern unsigned int mpxplay_mpxinbuf_buffer_check(struct frame *);
extern void         mpxplay_mpxinbuf_set_intsound(struct frame *frp,unsigned int intcfg);

extern struct frame *mpxplay_mpxinbuf_seekhelper_init(struct frame *frp);
extern void          mpxplay_mpxinbuf_seekhelper_close(struct frame *frp);

#endif // mpxplay_mpxinbuf_h

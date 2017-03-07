module dos_map;

extern (C):
nothrow:
@nogc:

struct BYTEREGS {
        ubyte al, ah ,f1,f2;
        ubyte bl, bh ,f3,f4;
        ubyte cl, ch ,f5,f6;
        ubyte dl, dh ,f7,f8;
};
struct WORDREGS {
		align (4):
        ushort ax;
        ushort bx;
        ushort cx;
        ushort dx;
        ushort si;
        ushort di;
        uint cflag;
};
struct DWORDREGS {
        uint eax;
        uint ebx;
        uint ecx;
        uint edx;
        uint esi;
        uint edi;
        uint cflag;
};


union REGS {
        DWORDREGS x;
        WORDREGS  w;
        BYTEREGS  h;
};

version(windows_exe){
__gshared int hda_registers[400];

int _int386(int cmd,REGS *r,REGS *s)
{
	switch(cmd){
	case 0x1A:
		switch(r.w.ax){
		case 0xB102:
			r.h.bl=1;
			r.h.bh=2;
			r.w.cflag=0;
			break;
		case 0xB10A:
			if(r.w.di==0x10) //HDBARL
			{
				s.x.ecx=cast(uint)&hda_registers;
				r.w.cflag=0;
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return 0;
}
}else{
extern (C) int _int386(int,REGS *,REGS*);
}
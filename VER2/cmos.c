#include <conio.h>
#include "cmos.h"

int read_cmos(int cmos_val)
{
	int result=0;
	int a;
	if(cmos_val==LINE_NUMBER){
		outp(0x70,1);
		a=inp(0x71);
		result=a<<8;
		outp(0x70,3);
		a=inp(0x71);
		result|=a;
	}else{
		outp(0x70,5);
		a=inp(0x71);
		result=a;
	}
	return result;
}
int write_cmos(int cmos_val,int data)
{
	int result=0;
	if(cmos_val==LINE_NUMBER){
		outp(0x70,1);
		outp(0x71,(data>>8)&0xFF);
		outp(0x70,3);
		outp(0x71,data&0xFF);
	}else{
		outp(0x70,5);
		outp(0x71,data&0xFF);
	}
	return result;
}
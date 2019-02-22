enum CMOS_VAL{
	LINE_NUMBER,
	OFFSET
};

int read_cmos(int cmos_val);
int write_cmos(int cmos_val,int data);
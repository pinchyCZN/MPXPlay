enum {
	CMOS_LINE_NUMBER,
	CMOS_OFFSET
};

int read_cmos(int cmos_val);
int write_cmos(int cmos_val,int data);
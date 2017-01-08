#ifndef control_h
#define control_h

#include <stdio.h>

// freeopts elements
#define MAXFREEOPTS    16
#define OPT_PROGNAME    0
#define OPT_INPUTFILE   1
#define OPT_OUTPUTFILE  2

//variable types (command line and mpxplay.ini)
#define ARG_NUM       1			// decimal number
#define ARG_HEX       2			// hexa number
#define ARG_FLO       4			// 32-bit float number
#define ARG_SIGNED   16			// signed number (can begin with '-')
#define ARG_SNUM    (ARG_NUM|ARG_SIGNED)
#define ARG_SFLO    (ARG_FLO|ARG_SIGNED)
#define ARG_CHAR     64			// character field (string)
#define ARG_POINTER 128			// pointer (to string)
#define ARG_PCHAR   (ARG_CHAR|ARG_POINTER)	// pointer to string

#define ARG1        256			// 1 argument
#define ARG2  (ARG1|512)		// 1 argument + 1 variable set

#define ARG_OR     2048			// bitwise OR  in variable (enable bit(s))
#define ARG_AND    4096			// bitwise AND in variable (unmask (disable) bit(s))

#define ARG_SAVE   8192			// save variable (mpxplay.ini)

#define KEY_ESC        0x011b
#define KEY_TAB        0x0f09
#define KEY_ENTER1     0x1c0d
#define KEY_ENTER2     0xe00d
#define KEY_BACKSPACE  0x0e08
#define KEY_UP_GRAY    0x48e0
#define KEY_UP_WHITE   0x4800
#define KEY_DOWN_GRAY  0x50e0
#define KEY_DOWN_WHITE 0x5000

#define MPXINI_MAX_LINES        768	// mpxplay.ini is ca. 350 lines at v1.50
#define MPXINI_MAX_CHARSPERLINE 512	// maybe startup/sublist requires more
#define MPXINI_MAX_SIZE      256000	// mpxplay.ini is ca. 20000 bytes at v1.50

typedef struct {
	char *oname;
	int flags;					//note: ARG_CHAR is allways pointer in opts[]
	void *var;
	int value;
	void *var2;
} topt;

// for keyboard configuration
typedef struct {
	char *name;					// function name
	unsigned int c;				// key-code
} keyconfig;

// for general loadini
typedef struct {
	char *name;					// variable name
	void *c;					// store value here
	unsigned int type;			// value type (ARG_NUM,ARG_HEX,ARG_CHAR,ARG_POINTER)
} mpxini_var_s;

// one mpxplay.ini line contains 3 parts
typedef struct {
	char *varnamep;				// variable name (without spaces)
	char *valuep;				// value (number or string) (without spaces)
	unsigned short varnamelen;	// original len (with spaces)
	unsigned short valuelen;	//
	char *commentp;				// comment part (after ';')
	void *storepoint;			//
} mpxini_line_t;

#define MPXINI_FLAG_LOADINI_POST 1	// load config after getting command line args

typedef struct mpxini_part_t {
	char *partname;				// "[keyboard]","[fastlists]",etc.
	unsigned int partbegin_linenum;	// line where the part begins (calculated)
	unsigned int partlinenum;	// number of lines (of the part) (calculated)
	long filepos;				// (mpxplay.ini) file position where the part begins (calculated)
	unsigned long flags;		// MPXINI_FLAG_
	void (*loadini) (mpxini_line_t *, struct mpxini_part_t *);	// load configuration of the part
	void (*saveini) (mpxini_line_t *, struct mpxini_part_t *, FILE *);	// save configuration of the part
	void (*init) (void);		// initialize part (hardware)
	void (*close) (void);		// close part (hardware)
} mpxini_part_t;

extern void mpxplay_control_general_loadini(mpxini_line_t * mpxini__lines,	// structure of all mpxplay.ini lines
											mpxini_part_t * partp,	// part info
											mpxini_var_s * vars_begin	// variable structure
	);
void mpxplay_control_general_saveini(mpxini_line_t * mpxini__lines,	// structure of all mpxplay.ini lines
									 mpxini_part_t * mpxini_partp,	// part info
									 FILE * conffile);

extern char *freeopts[MAXFREEOPTS];

#endif

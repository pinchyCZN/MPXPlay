#ifndef _resort_h_
#define _resort_h_

/* K O N S T A N T E N */
#define LARGEST_TABLE 63

/* T Y P E N D E F I N I T I O N */
typedef struct Huffmancode {
	unsigned int Code;
	unsigned int Length;
	int Value;
} HuffmanTyp;

/* P R O Z E D U R E N */
void Resort_HuffTables(const unsigned int, HuffmanTyp *, const int);

#endif

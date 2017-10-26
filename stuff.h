#ifndef STUFH
#define STUFH

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;

//************* hax macros
/* Macro reads a LH word from the image regardless of host convention */
/* Returns a 16 bit quantity, e.g. C000 is read into an Int as C000 */
//#define LH(p)  ((int16)((byte *)(p))[0] + ((int16)((byte *)(p))[1] << 8))
#define LH(p)    ((word)((byte *)(p))[0]  + ((word)((byte *)(p))[1] << 8))

struct header {				/*      EXE file header		 	 */
	u8	sigLo;			/* .EXE signature: 0x4D 0x5A	 */
	u8	sigHi;
	u16	lastPageSize;	/* Size of the last page		 */
	u16	numPages;		/* Number of pages in the file	 */
	u16	numReloc;		/* Number of relocation items	 */
	u16	numParaHeader;	/* # of paragraphs in the header */
	u16	minAlloc;		/* Minimum number of paragraphs	 */
	u16	maxAlloc;		/* Maximum number of paragraphs	 */
	u16	initSS;			/* Segment displacement of stack */
	u16	initSP;			/* Contents of SP at entry       */
	u16	checkSum;		/* Complemented checksum         */
	u16	initIP;			/* Contents of IP at entry       */
	u16	initCS;			/* Segment displacement of code  */
	u16	relocTabOffset;	/* Relocation table offset       */
	u16	overlayNum;		/* Overlay number                */
};

struct exefile {
	u32 siz;
	u8 *buf;	//whole contents
	struct header hdr;
};


#endif // STUFH

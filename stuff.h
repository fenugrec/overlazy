#ifndef STUFH
#define STUFH

/** these are not exports, just internal structs */

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

//************* hax macros
#define read_u16_LE(u8p)    ((u16)((u8 *)(u8p))[0] + ((u16)((u8 *)(u8p))[1] << 8))

// round up seg:ofs to next segment
#define nextseg(seg,ofs)	((((seg << 4) + ofs) + 16) >> 4)

/* this header isn't a perfect match with some MS headers
 * which have an extra u16 field at the end
 */
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

/** overlay descriptor */
struct ovl_desc {
	struct header hdr;
	u32 relocs_ofs;	//position in orig .exe
	u32 img_ofs;	//position in orig .exe
	u32 img_siz;	//in bytes (just image, no relocs or header)
};

/** relocation table entry */
struct reloc_entry {
	u16 ofs;
	u16 seg;
};

/* building blocks for relinking new exe with flattened overlays */
struct new_exe {
	struct header hdr;
	u8 *relocs;	//array of all reloc entries
	u32 imgsiz;
	u8 *img;	//image ("load module")
};


#endif // STUFH

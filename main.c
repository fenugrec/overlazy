/* Overlazy
 *
 * Crude tool to split overlayed DOS exe into chunks; one file per overlay.
 * Designed for binaries compiled with MS C compiler 5.1;
 * Others of similar vintage might work too.
 *
 * (c) fenugrec 2017
 * I use minor parts of dcc
 * (C. Cifuentes orig work; fork github.com/nemerle/dcc
 *
 * Assumes host is little-endian (x86 etc). This should compile with any C99 compliant compiler, for any target.
 *
 * Note : unsafe code - limited bounds checking, naive string processing. Run at your own risk
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "stuff.h"

static void print_header(const struct header *hdr) {

	printf(	"bytes lastpage\t"
			"File pages (512B)\t"
			"# relocs\t"
			"Offset to load image (parags)\t"
			"Minimum alloc (parags)\t"
			"Maximum alloc (parags)\t"
			"Initial SS:SP\t"
			"Initial CS:IP\t"
			"Overlay #\t"
			"\n");

	printf(	"%04X\t%04X\t%04X\t%04X\t%04X\t%04X\t"
			"%04X:%04X\t%04X:%04X\t%04X"
			"\n",
			hdr->lastPageSize,
			hdr->numPages,
			hdr->numReloc,
			hdr->numParaHeader,
			hdr->minAlloc,
			hdr->maxAlloc,

			hdr->initSS,
			hdr->initSP,
			hdr->initCS,
			hdr->initIP,
			hdr->overlayNum);

	return;
}

/* TODO: take care of endianness */
static void read_header(struct header *dest_header, const u8 *buf) {
	memcpy(dest_header, buf, sizeof(struct header));
	return;
}


// hax, get file length but restore position
u32 flen(FILE *hf) {
	long siz;
	long orig;

	if (!hf) return 0;
	orig = ftell(hf);
	if (orig < 0) return 0;

	if (fseek(hf, 0, SEEK_END)) return 0;

	siz = ftell(hf);
	if (siz < 0) siz=0;
		//the rest of the code just won't work if siz = UINT32_MAX
	#if (LONG_MAX >= UINT32_MAX)
		if ((long long) siz == (long long) UINT32_MAX) siz = 0;
	#endif

	if (fseek(hf, orig, SEEK_SET)) return 0;
	return (u32) siz;
}


void close_exe(struct exefile *exf) {
	if (exf->buf) {
		free(exf->buf);
		exf->buf = NULL;
	}
	return;
}

bool parse_header(struct exefile *exf) {
	struct header *hdr = &exf->hdr;
	u32 cb;

	bool validsig = (hdr->sigLo == 0x4D && hdr->sigHi == 0x5A);
	if (!validsig) {
		printf("bad MZ\n");
		return 0;
	}

	/* This is a typical DOS kludge! */
	if (hdr->relocTabOffset == 0x40) {
		printf("new exe\n");
		return 0;
	}

	/* Calculate the load module size.
	 * This is the number of pages in the file
	 * less the length of the header and reloc table
	 * less the number of bytes unused on last page
	*/
	cb = (dword)hdr->numPages * 512 - (dword)hdr->numParaHeader * 16;
	if (hdr->lastPageSize) {
		cb -= 512 - hdr->lastPageSize;
	}

	return 1;
}


/** init exefile struct with complete .exe read into it
 * exefile->buf must be free'd by caller
 */
bool load_exe(struct exefile *exf, const char *filename) {
	FILE   *fbin;
	u32 file_len;
	u8 *buf;

	/* Open the input file */
	if ((fbin = fopen(filename, "rb")) == NULL) {
		printf("CANNOT_OPEN\n");
		return 0;
	}

	file_len = flen(fbin);
	if ((!file_len) || (file_len > 2048*1024UL)) {
		printf("huge file (length %lu)\n", (unsigned long) file_len);
		fclose(fbin);
		return 0;
	}
	exf->siz = file_len;

	buf = malloc(file_len);
	if (!buf) {
		printf("malloc choke\n");
		fclose(fbin);
		return 0;
	}

	/* load whole ROM */
	if (fread(buf,1,file_len,fbin) != file_len) {
		printf("trouble reading\n");
		free(buf);
		fclose(fbin);
		return 0;
	}

	fclose(fbin);

	read_header(&exf->hdr, buf);
	if (!parse_header(exf)) {
		free(buf);
		return 0;
	}

	exf->buf = buf;
	return 1;
}

/** for every overlay, create a "prefix_XXXX.ovl" file
 */

void dump_ovls(const struct exefile *exf, const char *prefix) {
	char *fname;
	FILE *fbin;

	u32 chunksiz;
	struct header hdr;
	u32 ofs = 0;	//in main exe file
	u16 i=0;

	while (ofs < exf->siz) {
		char suffix[10];
		//1) get chunk header
		read_header(&hdr, &exf->buf[ofs]);

		//2) check MZ
		bool validsig = (hdr.sigLo == 0x4D && hdr.sigHi == 0x5A);
		if (!validsig) {
			printf("bad MZ @ %d\n", i);
			return;
		}

		chunksiz = 512 * hdr.numPages;

		snprintf(suffix, sizeof(suffix), "_%04X", i);
		fname = malloc(strlen(prefix) + strlen(suffix) + 1);
		if (!fname) {
			printf("malloc\n");
			return;
		}

		strcpy(fname, prefix);
		strcat(fname, suffix);

		fbin = fopen(fname, "wb");
		if (!fbin) {
			printf("fopen\n");
			free(fname);
			return;
		}
		free(fname);

		if (fwrite(&exf->buf[ofs], 512, hdr.numPages, fbin) != hdr.numPages) {
			printf("fwrite\n");
			fclose(fbin);
			return;
		}
		fclose(fbin);

		i++;
		ofs += chunksiz;
	}
}


/** raw search for all "int 0x3F" calls
 *
 * expect lots of spurious hits due to no filtering
 */
void dump_ovlcalls(const struct exefile *exf) {
#define PATLEN 5
#define MATCHLEN 2
	const u8 pat[MATCHLEN]={0xCD, 0x3F};	//pattern

	u32 ofs = 0;	//within exe file

	printf(	"file_ofs\t"
			"ovl_idx\t"
			"offs\n"
			);
	while ((ofs + PATLEN) <= exf->siz) {
		int i;
		bool match = 1;
		for (i=0; i < MATCHLEN; i++) {
			if (exf->buf[ofs + i] != pat[i]) {
				match = 0;
				break;
			}
		}
		if (match) {
			u16 ovl_offs = LH(&exf->buf[ofs+3]);
			printf("%04X\t%02X\t%04X\n",
					ofs, (unsigned) exf->buf[ofs+2], (unsigned) ovl_offs );
		}
		ofs++;
	}
	return;
}

/** print list of overlay chunks and their headers
*/
void list_ovls(const struct exefile *exf) {
	u32 chunksiz;
	struct header hdr;
	u32 ofs = 0;	//in main exe file
	u16 i=0;

	printf(	"OVL #\t"
			"start(file ofs)\t"
			"siz\t"

			"numpages (512B)\t"
			"# relocs\t"
			"Offset to load image (parags)\t"
			"Minimum alloc (parags)\t"
			"Maximum alloc (parags)\t"

			"Initial SS:SP\t"
			"Initial CS:IP\t"
			"\n");
	while (ofs < exf->siz) {
		//1) get chunk header
		read_header(&hdr, &exf->buf[ofs]);

		//2) check MZ
		bool validsig = (hdr.sigLo == 0x4D && hdr.sigHi == 0x5A);
		if (!validsig) {
			printf("bad MZ @ %d\n", i);
			return;
		}

		chunksiz = 512 * hdr.numPages;
		printf(	"%04X\t%08X\t%08X\t"
				"%04X\t%04X\t%04X\t%04X\t%04X\t"
				"%04X:%04X\t%04X:%04X"
				"\n",
				i, ofs, chunksiz,
				hdr.numPages,
				hdr.numReloc,
				hdr.numParaHeader,
				hdr.minAlloc,
				hdr.maxAlloc,

				hdr.initSS,
				hdr.initSP,
				hdr.initCS,
				hdr.initIP
				);
		i++;
		ofs += chunksiz;
	}
}


int main(int argc, char *argv[])
{
	struct exefile exf = {0};

	if (argc < 2) {
		printf(	"**** %s\n"
				"**** overlayed DOS exe tool\n"
				"**** (c) 2017 fenugrec\n"
				"Usage:\t%s <exefile> [-c]\n"
				"\nif \"-c\" is specified : don't dump overlays, just dump all int 0x3F calls.\n"
				, argv[0],argv[0]);
		return 0;
	}

	if (!load_exe(&exf, argv[1])) {
		printf("Trouble in loadexe\n");
		return -1;
	}

	switch (argc) {
		case 2:
			list_ovls(&exf);
			dump_ovls(&exf,argv[1]);
			break;
		case 3:
			if (argv[2][1]=='c') {
				dump_ovlcalls(&exf);
			}
			break;
		default:
			printf("bad args\n");
			break;
	}
	close_exe(&exf);

	return 0;
}

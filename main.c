/* Overlazy
 *
 * Crude CLI tool for overlayed DOS programs.
 * Run without arguments to print usage info.
 *
 * Designed for binaries compiled with MS C compiler 5.1;
 * Others of similar vintage might work too.
 *
 * (c) fenugrec 2017
 * Licensed under GPLv3
 *
 *
 * Assumptions:
 * - host is little-endian (x86 etc).
 * - C99 compliant compiler
 * - source .exe is a valid DOS program
 * - overlays don't have relative pointers to data/code outside their own image
 * - overlays don't store data outside their own image, within the OVL mapping area
 *
 * Note : unsafe code - limited bounds checking, naive string processing, etc. Run at your own risk !
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "stuff.h"

inline void write_u16_LE(u8 *dest, u16 val) {
	*dest++ = val & 0xFF;
	*dest = val >> 8;
	return;
}

void print_header(const struct header *hdr) {

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


/** init exefile struct with complete .exe read into it.
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
			u16 ovl_offs = read_u16_LE(&exf->buf[ofs+3]);
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
			"img siz\t"

			"# relocs\t"
			"Offset to load image (parags)\t"
			"Minimum alloc (parags)\t"
			"Maximum alloc (parags)\t"

			"Initial SS:SP\t"
			"Initial CS:IP\t"
			"\n");
	while (ofs < exf->siz) {
		u32 img_siz;
		//1) get chunk header
		read_header(&hdr, &exf->buf[ofs]);

		//2) check MZ
		bool validsig = (hdr.sigLo == 0x4D && hdr.sigHi == 0x5A);
		if (!validsig) {
			printf("bad MZ @ %d\n", i);
			return;
		}

		chunksiz = 512 * hdr.numPages;
		img_siz = (chunksiz + hdr.lastPageSize - 512) - (hdr.numParaHeader * 16);
		printf(	"%04X\t%08X\t%08X\t"
				"%04X\t%04X\t%04X\t%04X\t"
				"%04X:%04X\t%04X:%04X"
				"\n",
				i, ofs, img_siz,

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

/** count # of overlays in a given buf
 * excludes the root overlay (# 0)
 *
 * In case of malformed input, this just stops counting and so could return 0.
 */
u16 count_ovls(const u8 *buf, u32 siz) {
	u32 ofs = 0;
	struct header hdr;
	u16 i = 0;

	while (ofs < siz) {
		u32 chunksiz;
		//1) get chunk header
		read_header(&hdr, &buf[ofs]);

		i++;
		//2) check MZ
		bool validsig = (hdr.sigLo == 0x4D && hdr.sigHi == 0x5A);
		if (!validsig) {
			break;
		}

		chunksiz = 512 * hdr.numPages;

		ofs += chunksiz;
	}
	i--;
	return i;
}

/** parse all overlays, including root OVL_000
 * returns an array of "struct ovl_desc" that must be free'd by caller
 * Assumes no bad data.
 *
 * @param num_ovls number of overlays *excluding the root*.
 *
 * so the array index can be the overlay #, as intended.
 */
struct ovl_desc *parse_ovls(const u8 *buf, u32 siz, u16 num_ovls) {
	struct ovl_desc *od_arr;
	u16 i = 0;
	u32 ofs = 0;

	od_arr = malloc((num_ovls + 1) * sizeof(struct ovl_desc));
	if (!od_arr) return NULL;

	while (i <= num_ovls) {
		u32 chunksiz;
		struct ovl_desc *oda;
		struct header *hdr;	//shortcuts

		oda = &od_arr[i];
		hdr = &oda->hdr;

		//get chunk header
		read_header(hdr, &buf[ofs]);

		chunksiz = 512 * hdr->numPages;
		// fill in descriptor
		oda->img_ofs = ofs + (hdr->numParaHeader * 16);
		oda->img_siz = (chunksiz + hdr->lastPageSize - 512) - (hdr->numParaHeader * 16);
		oda->relocs_ofs = ofs + hdr->relocTabOffset;

		i++;
		ofs += chunksiz;
	}
	return od_arr;
}

/** fixup a set of relocs for a displaced chunk.
 * (removed) param imgbuf must start at IMG_BASE and contain the appended newchunk
 * @param relocbuf : destination for the new reloc table
 * (removed) param dest_ofs : offset into imgbuf where to write the new reloc entries
 * @param dispbase : offset into new load_image where the displaced chunk starts, in parags
 * @param origbase : offset into original image where the chunk was meant to be mapped
 * @param relocs : original reloc table, valid when newchunk was mapped at OVL_BASE
 *
 * All this does is change the reloc entries to point to the right items. The items themselves
 * need not be changed since they are absolute pointers already.
 */
void fixup_relocs(u8 *relocbuf, u16 dispbase, u16 origbase, const u8 *relocs, u16 num_relocs) {
	u16 i;
	u32 dest_ofs = 0;

	for (i = 0; i < num_relocs; i++) {
		u16 roffs = read_u16_LE(&relocs[(4 * i) + 0]);
		u16 rseg = read_u16_LE(&relocs[(4 * i) + 2]);	//segment, relative to chunk base, where the item is located
		//u32 r_lin = (rseg << 4) + roffs;	//"address" into displaced chunk of relocation item

		u16 r_newseg = rseg - origbase + dispbase;

		write_u16_LE(&relocbuf[dest_ofs], roffs);
		dest_ofs += 2;
		write_u16_LE(&relocbuf[dest_ofs], r_newseg);
		dest_ofs += 2;

	}
	return;
}

/** correct all overlay segment LUT entries that point to the given overlay #.
 *
 * @param seglutpos : offset in imgbuf of seg LUT
 * @param olutpos : offs in imgbuf of ovl # LUT
 * @param lut_entries : # of entries
 * @param seg_delta : value to add to LUT entry to point to new segment.
 */
void fixup_seglut(u8 *imgbuf, u32 seglutpos, u32 olutpos, u16 lut_entries, u16 ovlno, u16 seg_delta) {
	u16 i;

	for (i = 0; i < lut_entries; i++) {
		u8 test_no = imgbuf[olutpos + i];
		if (test_no != ovlno) continue;

		u16 seg = read_u16_LE(&imgbuf[seglutpos + (2*i) ]);
		write_u16_LE(&imgbuf[seglutpos + (2*i) ], seg + seg_delta);
	}
}

/** fixup INT 0x3F calls
 *
 * @param seglutpos : ofs in imgbuf of segment LUT
 * @param olutpos : ofs in imgbuf of ovl # LUT
 *
 * replaces "CD 3F" opcodes and following 3 bytes with a "call far ptr" to the correct destination
 * this must be done after the LUT has been corrected with the new mapping
 */
void fixup_int3f(u8 *imgbuf, u32 bufsiz, u32 seglutpos, u32 olutpos, u16 lut_entries) {
	u32 cur;

	for (cur = 0; (cur + 5) < bufsiz; cur++) {
		u8 ovl_id;	//not the same as overlay # !
		u16 seg, offs;
		if (imgbuf[cur] != 0xCD) continue;
		if (imgbuf[cur + 1] != 0x3F) continue;

		ovl_id = imgbuf[cur + 2];
		offs = read_u16_LE(&imgbuf[cur + 3]);
		seg = read_u16_LE(&imgbuf[seglutpos + (2 * ovl_id)]);

		imgbuf[cur] = 0x9A;	//opcode for "call (far ptr) seg:offs"
		write_u16_LE(&imgbuf[cur+1], offs);
		write_u16_LE(&imgbuf[cur+3], seg);
		cur += 4;	//not +5 since there's a "++" at the loop top
	}
}

/** tweak header fields for mostly correct info, and write out.
 *
 * @param rcur size (bytes) of all relocs in in nex.relocs[]
 * @param imgcur_parags size (parags) of load image
 *
 * checksum ignored
 * new header must be already filled except
 *	lastPageSize;
 *	numPages;
 *	numReloc;
 *	numParaHeader;
 */
void dump_newheader(FILE *outf, struct new_exe *nex, u32 rcur, u16 imgcur_parags) {
	u32 wcur = 0;
	u32 wlen;
	u32 padlen;
	const u8 pagebuf[512] = {0};	//just used to write padding 0 bytes

	nex->hdr.relocTabOffset = sizeof(struct header);	//1C != 1E !!
	nex->hdr.numReloc = rcur / 4;
	nex->hdr.numParaHeader = (sizeof(struct header) + rcur + 15) >> 4;	//round to next parag
	nex->hdr.lastPageSize = ((nex->hdr.numParaHeader + imgcur_parags) * 16) & 511;
    nex->hdr.numPages = (((nex->hdr.numParaHeader + imgcur_parags) * 16) + 511) / 512;

    //write hdr
    wlen = fwrite(&nex->hdr, 1, sizeof(struct header), outf);
    if (wlen != sizeof(struct header)) goto write_err;
    wcur += wlen;

    //write relocs
    wlen = fwrite(nex->relocs, 1, rcur, outf);
    if (wlen != rcur) goto write_err;
    wcur += wlen;

	//0-pad relocs if necessary
	padlen = (nex->hdr.numParaHeader * 16) - wcur;
	wlen = fwrite(pagebuf, 1, padlen, outf);
	if (wlen != padlen) goto write_err;
	wcur += wlen;

	//write image
	wlen = fwrite(nex->img, 1, imgcur_parags * 16, outf);
	if  (wlen != imgcur_parags * 16) goto write_err;
	wcur += wlen;

	//0-pad to 512-byte page if necessary
#if PADPAGE
	padlen = (nex->hdr.numPages * 512) - wcur;
	wlen = fwrite(pagebuf, 1, padlen, outf);
	if (wlen != padlen) goto write_err;
#endif

	return;

write_err:
	printf("fwrite err\n");
	return;
}

/** convert overlayed .exe to monolithic .exe with flattened overlays
 *
 * @param seglut_pos file offset of overlay segment LUT
 * @param olut_pos file offset of overlay number LUT
 * @param lut_entries
 * @param ovl_base : segment where overlays are loaded (relative to image base)
 * @param out_fname : filename for output
 *
 * The resulting .exe will probably not run properly anymore.
*/
void unfold_overlay(struct exefile *exf, u32 seglut_pos, u32 olut_pos, u16 lut_entries, u16 ovl_base, const char *out_fname) {
	u16 num_ovls;	//excluding root
	u16 num_relocs = 0;
	u32 imgsiz = 0;
	u16 i;
	struct ovl_desc *oda;	//array of descriptors
	struct new_exe nex;
	u32 imgcur_parags;
	u32 rcur;	//cursors into new img and reloc tables
	FILE *outf;

	printf("seglut @ %lX, ovllut @ %lX, entries=%X ovlbase %X:0000\n",
			(unsigned long) seglut_pos, (unsigned long) olut_pos,
			(unsigned) lut_entries, (unsigned) ovl_base);

	num_ovls = count_ovls(exf->buf, exf->siz);
	if (!num_ovls) {
		printf("no ovl\n");
		return;
	}

	//open output file
	outf = fopen(out_fname, "wb");
	if (!outf) {
		printf("can't create outf\n");
		return;
	}

	// fill ovl descriptors
	oda = parse_ovls(exf->buf, exf->siz, num_ovls);
	if (!oda) {
		printf("ovl parse fail\n");
		fclose(outf);
		return;
	}

	nex.img = NULL;
	nex.relocs = NULL;

	// gather ovl stats
	for (i=0; i <= num_ovls; i++) {
		num_relocs += oda[i].hdr.numReloc;
		imgsiz += oda[i].img_siz;
	}

	// check if it can be done by mapping OVLs *above* SS:SP.
	// Assume we need 1 parag padding for each ovl
	u16 required_segs = ((imgsiz - oda[0].img_siz) >> 4) + num_ovls;
	u16 availseg = (0xFFFF - nextseg(exf->hdr.initSS, exf->hdr.initSP));
	if (required_segs >= availseg) {
		printf("not enough addressing space to unroll that shit\n");
		goto fexit;
	}

	//allocate new data structures
	nex.relocs = malloc(num_relocs * 4);
	if (!nex.relocs) goto fexit;
		// max total size is (headers) + (reloc table) + (base size up to SS:SP) + (ovl img data) + 1 parag per ovl
	nex.img = calloc(	sizeof(struct header) +
						num_relocs * sizeof(struct reloc_entry) +
						(exf->hdr.initSS << 4) + exf->hdr.initSP + 16 +
						(imgsiz - oda[0].img_siz) +
						num_ovls * 16,
						1);
	if (!nex.img) goto fexit;

	// write in root OVL_000 image, including its relocs
	memcpy(nex.relocs, &exf->buf[oda[0].relocs_ofs], oda[0].hdr.numReloc * 4);
	memcpy(nex.img, &exf->buf[oda[0].img_ofs], oda[0].img_siz);
	rcur = oda[0].hdr.numReloc * 4;
	imgcur_parags = exf->hdr.initSS + ((exf->hdr.initSP + 15) >> 4);	//bring cursor after stack area

	//convert lut positions to "offset within image"
	seglut_pos -= (exf->hdr.numParaHeader * 16);
	olut_pos -= (exf->hdr.numParaHeader * 16);

	// masterloop (tm)
	for (i = 1; i <= num_ovls; i++) {
		u16 chunk_segdelta;	//distance (in parags) from new location to original mapping location OVL_BASE

		//copy ovl image, and append fixed up relocs to the main table
		memcpy(&nex.img[imgcur_parags * 16], &exf->buf[oda[i].img_ofs], oda[i].img_siz);
		fixup_relocs(&nex.relocs[rcur], imgcur_parags, ovl_base, &exf->buf[oda[i].relocs_ofs], oda[i].hdr.numReloc);

		//adjust overlay segment LUT
		chunk_segdelta = imgcur_parags - ovl_base;
		fixup_seglut(nex.img, seglut_pos, olut_pos, lut_entries, i, chunk_segdelta);

		printf("mapping OVL_%X @ %X0 within image\n", i, imgcur_parags);

		//advance cursors
		rcur += (oda[i].hdr.numReloc * 4);
		imgcur_parags += ((oda[i].img_siz + 15) >> 4);	//round to next parag
	}

	//fixup INT 3F calls
	fixup_int3f(nex.img, imgcur_parags * 16, seglut_pos, olut_pos, lut_entries);

	//regen new exe header. mostly same as orig
	memcpy(&nex.hdr, &exf->hdr, sizeof(struct header));
	dump_newheader(outf, &nex, rcur, imgcur_parags);

fexit:
	fclose(outf);
	if (nex.img) free(nex.img);
	if (nex.relocs) free(nex.relocs);
	free(oda);
	return;
}

void print_usage(const char *argv0) {
	printf(	"**** %s\n"
		"**** overlayed DOS exe tool\n"
		"**** (c) 2017 fenugrec\n"
		"Usage:\t%s <exefile> <command> [command options]]\n"
		"Commands and options:\n"
		"\tc : list all int 0x3F calls.\n"
		"\tl : list overlays\n"
		"\td : dump overlays to separate files\n"
		"\tu <SEGLUT_POS> <OVLLUT_POS> <OVL_BASE>: unfold overlays\n"
		"\t\tSEGLUT_POS : file offset of overlay segment LUT\n"
		"\t\tOVLLUT_POS : file offset of overlay number LUT\n"
		"\t\tLUT_ENTRIES : number of entries in LUT\n"
		"\t\tOVL_BASE : loaded overlay's segment (relative to image base)\n",
		argv0, argv0);
	return;

}

int main(int argc, char *argv[])
{
	struct exefile exf = {0};
	bool badargs = 1;

	if (argc < 3) {
		print_usage(argv[0]);
		return 0;
	}

	if (!load_exe(&exf, argv[1])) {
		printf("Trouble in loadexe\n");
		return -1;
	}

	switch (argv[2][0]) {
		case 'l':
			list_ovls(&exf);
			badargs = 0;
			break;
		case 'd':
			dump_ovls(&exf,argv[1]);
			badargs = 0;
			break;
		case 'c':
			dump_ovlcalls(&exf);
			badargs = 0;
			break;
		case 'u':
			if (argc != 7) {
				break;
			} else {
				unsigned long seglut, olut;
				unsigned lut_entries, ovlbase;
				if (sscanf(argv[3], "%lx", &seglut) != 1) break;
				if (sscanf(argv[4], "%lx", &olut) != 1) break;
				if (sscanf(argv[5], "%x", &lut_entries) != 1) break;
				if (sscanf(argv[6], "%x", &ovlbase) != 1) break;
				if (seglut > exf.siz) break;
				if (olut > exf.siz) break;
				if (ovlbase >= 0xFFFF) break;
				if (lut_entries > 0xFFFF) break;
				badargs = 0;
				unfold_overlay(&exf, (u32) seglut, (u32) olut, (u16) lut_entries, (u16) ovlbase, "test.ex_");
				break;
			}

			break;
		default:
			break;
	}
	if (badargs) {
		printf("bad args\n");
		print_usage(argv[0]);
	}
	close_exe(&exf);

	return 0;
}

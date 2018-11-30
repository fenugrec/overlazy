// Source : forum user "darkpanda"
//
// https://forums.civfanatics.com/threads/tutorial-civ-ida-part-2.522975/page-3

// script to automatically rename all the overlays segments to "ovrXX" format (instead of IDA's default of "segXXX", starting from "ovr01";
// this script simply looks for the segment named seg033 (the first overlay, since main CIV.EXE has 33 segments form seg000 to seg032),
// and form then on, renamed all segments accordingly: ovr01, ovr02, ..., ovr023

#include <idc.idc>

static main()
{
	auto seg, loc, ovrseg;
	auto off, base;
	auto xref;
	auto sn, sn0;
	auto b;
	auto segc, c;

	// init seg to seg000
	seg = FirstSeg();
	// skip to next segment until reaching seg033 (= ovr01)
	while(seg != BADADDR && SegName(seg)!="seg033")	{ seg = NextSeg(seg); }

	// 'c' is the overlay index
	c = 1;
	while(seg != BADADDR)
	{
		// retrieving the current segment name segXXX - just for logging
		sn = SegName(seg);
		// building the new segment name as "ovrXX"
		sn0 = "ovr"+((c<10)?"0":"")+ltoa(c,10);
		// logging
		Message("Renaming '%s' to '%s'\n",sn,sn0);
		// renaming the segment
		SegRename(seg,sn0);
		// setting the segment as CODE
		SegClass(seg,"CODE");
		// setting the segment addressing as 16-bit
		SetSegAddressing(seg,0); // 16-bit
		// force segment analysis
		AnalyzeArea(SegStart(seg),SegEnd(seg));

		// proceed to next segment
		seg = NextSeg(seg);
		c++;
	}
}
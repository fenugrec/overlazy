// Source : forum user "darkpanda"
//
// https://forums.civfanatics.com/threads/tutorial-civ-ida-part-2.522975/page-3

// script to replace all occurrences of INT 3Fh with far calls to the routine in the overlay segments

#include <idc.idc>

static main()
{
	auto seg, loc, ovrseg;
	auto off, base;
	auto xref;
	auto sn, sn0;
	auto b;
	auto segc, c;

	c = 0;
	seg = FirstSeg();
	segc = 0;
	while(seg != BADADDR )
	{
		loc = SegStart(seg);
		Message("Segment %s ...\n", SegName(seg));

		// scan the segment code, looking for "CD 3F" sequence
		// which stands for "int 3Fh"
		while(loc < SegEnd(seg)) {
			if( Byte(loc) == 0xCD && Byte(loc+1) == 0x3F)
			{
				Message("   found int 3Fh at %s - 0x%x(%d) [seg start: %d]\n", atoa(loc), loc,loc, SegStart(seg));

				MakeUnknown(loc,5,1);

				// Overlay call is CD 3F xx yy zz, where:
				//    [B]xx[/B] is the overlay number
				//    [B]yy zz[/B] is the function offset inside the overlay, little endian mode
				sn0 = "ovr"+(Byte(loc+2)<10?"0":"")+ ltoa(Byte(loc+2),10);
				Message("     - referenced overlay: 0x%x(%d) -> %s ; overlay function offset: 0x%x(%d)\n", Byte(loc+2),Byte(loc+2),sn0, Byte(loc+3)+(Byte(loc+4)<<8),Byte(loc+3)+(Byte(loc+4)<<8));
				
				ovrseg = SegByBase(SegByName(sn0));

				Message("     - overlay segment: %d; address: %d \n", ovrseg, GetSegmentAttr(ovrseg,SEGATTR_START));
				Message("     - far call patch (hex): %x %x %x %x %x\n", 0x9A, Byte(loc+3), Byte(loc+4), (ovrseg>>4)&0xFF, (ovrseg>>12)&0xFF );

				// Far call is 9A ww xx yy zz , where:
				//    [B]ww xx[/B] is the function offset inside the overlay, little endian mode = directly copied from CD 3F entry
				//    [B]yy zz[/B] is the function's segment base, little endian mode
				PatchByte(loc,0x9A);
				PatchByte(loc+1,Byte(loc+3));
				PatchByte(loc+2,Byte(loc+4));
				PatchByte(loc+3,(ovrseg>>4)&0xFF);
				PatchByte(loc+4,(ovrseg>>12)&0xFF);

				MakeCode(loc);
			
				loc = loc + 5;
				segc++;
				c++;
			} else {
				loc = loc + 1;
			}	
		}
		Message(" -> patched %d INT 3Fh overlay calls in segment %s\n",segc,SegName(seg));
		seg = NextSeg(seg);
		segc = 0;
	}
	Message(" Total patched INT 3Fh overlay calls: %d\n",c);
}
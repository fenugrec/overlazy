### overlazy
(c) fenugrec 2017
Licensed under GPLv3

overlazy is a crude tool to analyze, split and flatten overlayed DOS exe programs.
 
Designed for binaries compiled with MS C compiler 5.1;
Others of similar vintage might work too. Probably MSC from 4.x to 7.x, and Visual C 1.x ?

Note : this is relatively unsafe code : limited bounds checking, naive string processing, etc. Run at your own risk !
Should be harmless on well-formed , legitimate .exe files.

#### status
- works for at least some valid .exe files
- mapping the overlays above the top-of-stack could be a problem.

### compiling
I include a codeblocks project file but really not a requirement. Just
```gcc main.c```
should do the trick.

### what it does
(WIP)
An overlayed .exe is structured like this :

```
   main .exe                                             each OVL_xxx :
                           file offset
+-----------------------+  0                           +-----------------------+
| MZ header             |                              |  MZ header            |
| "overlay number"=0    |                              |  "overlay number" field
+-----------------------+  0x1E                        |      >= 0             |
| relocation table      |                              +-----------------------+
|                       |                              |  relocs               |
+-----------------------+  (hdr_parags * 0x10)         |                       |
| image                 |                              +-----------------------+
|                       |                              |  image                |
|                       |                              |                       |
|                       |                              +-----------------------+
++----------------------+  (img_pages * 0x200)
 |  OVL_001             |
 |                      |
 +----------------------+
 |  OVL_002             |
 |                      |
 +----------------------+
 |  OVL_...             |
 |                      |
 +----------------------+

```

Now, at run-time the memory map looks like this :

```
       layout in memory
       (when running)

+---------------------------+ IMG_BASE
|                           |
|    main code (overlay 000)|
|                           |
|                           |
+---------------------------+ OVL_BASE
  | overlays loaded here    |
  | (initially all 0x00)    |
  |                         |
  |                         |
+---------------------------+ OVL_BASE + max_ovl_size
|                           |
|   rest of main code       |
|   (ovl 000)               |
|                           |
|   also data segment etc.  |
|                           |
|                           |
+---------------------------+ SS:0000
|                           |
|       stack               |
|                           |
+---------------------------+ initial SS:SP (top of stack)

```

Naturally only one overlay can be loaded at OVL_BASE at a time. This makes static analysis (radare2, IDA, etc) troublesome.
The "unfold" mode of this tool cooks a new .exe with all the overlays appended after the stack, while also
 - combining and adjusting all the relocations into one reloc table
 - replacing all "int 3F" calls by a "call far xyz" opcode

Resulting .exe layout :

```
   new "flattened" .exe
                                    file offset
+---------------------------------+ 0
| MZ header                       |
+---------------------------------+ 0x1C
| relocation table:               |
|    - original OVL_000 relocs    |
|    - adjusted OVL_XXX relocs    |
|    - new relocs for call fixups |
|                                 |
|                                 |
+---------------------------------+ (hdr_parags * 0x10)
|  main/root image (OVL_000)      |
+---------------------------------+
|  blank area (0x00 filled)       |
|  (original stack area / BSS )   |
|                                 |
+---------------------------------+ (hdr_parags * 0x10) + (original SS:SP)
|  concatenated images of         |
|   OVL_001...OVL_XXX             |
+---------------------------------+

```


### example
Different outputs for a test ~ 700kB .exe containing 31 overlays.
In "dump mode", creates one file per overlay, plus a text summary of each overlay header.

List overlays :
```
> overlazy test.exe l
OVL #	start(file ofs)	siz	numpages (512B)	# relocs	Offset to load image (parags)	Minimum alloc (parags)	Maximum alloc (parags)	Initial SS:SP	Initial CS:IP	
0000	00000000	00071400	038A	247E	0940	0AFE	FFFF	67EE:AFC8	2CBD:2905
0001	00071400	00002000	0010	00DC	0040	0000	FFFF	67EE:AFC8	2CBD:2905
0002	00073400	00001200	0009	0058	0020	0000	FFFF	67EE:AFC8	2CBD:2905
....
```

finding all "int 0x3F" calls :
```
> overlazy test.exe c
file_ofs	ovl_idx	offs
942A	11	0000
9432	14	0004
943A	15	0000
9442	17	0008
944A	16	000E
....
```

Flattening an .exe for static analysis (the .exe created will NOT be executable !)
```
> overlazy test.exe u 6F2F4 6F37E 45 38CC

seglut @ 6F2F4, ovllut @ 6F37E, entries=45 ovlbase 38CC:0000
mapping OVL_1 @ 72EB0 within image
mapping OVL_2 @ 74A90 within image
mapping OVL_3 @ 75910 within image
mapping OVL_4 @ 76D40 within image
mapping OVL_5 @ 778F0 within image
....
```


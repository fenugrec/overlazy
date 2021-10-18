Minimal examples to build overlayed executables with Microsoft C 5.1 and Microsoft Visual C++ 1.52c .

# Microsoft C 5.1

Compiler and Linker only run in DosBox or any other 16 bit DOS environment

```
SET PATH=D:\MSC5.1\BIN
SET LIB=D:\MSC5.1\LIB
SET INCLUDE=D:\MSC5.1\INCLUDE
```

##### Compile:

`CL.EXE /c /AM ovl_test.c ovl1.c ovl2.c ovl3.c`

##### Link: Static Overlay Managment, using the parenthesis style

`LINK.EXE ovl_test.obj (ovl1.obj) (ovl2.obj) (ovl3.obj), ovl1.exe;`

Packing with /EXEPACK is possible

# Microsoft Visual C++ 1.52c

Compiler and Linker run under 32/64 bit windows
(Compiler/Linker can also run under 16 bit DOS environment using the Microsft DOS Extender in MSVC15\BIN\dosxnt.exe internally)

```
SET PATH=D:\MSVC15\BIN
SET LIB=D:\MSVC15\LIB
SET INCLUDE=D:\MSVC15\INCLUDE
```

#####  Compile: same as with Microsoft C 5.1

`CL.EXE /c /AM ovl_test.c ovl1.c ovl2.c ovl3.c`

#####  Link: Static Overlay Managment, using the deprecated parenthesis style
(same options as with MSC 5.1 but minor differences in the produced code)

`LINK.EXE /OLDOVERLAY ovl_test.obj (ovl1.obj) (ovl2.obj) (ovl3.obj), ovl2exp.exe;`

Packing with /EXEPACK is possible with /OLDOVERLAY

##### Link: using Overlay configuration from DEF-File, with dynamic Overlay Managment

`LINK.EXE /DYNAMIC ovl_test ovl1 ovl2 ovl3, ovl3.exe,,,ovl_test.def;`

Remark: /DYNAMIC is default for Overlays with MSVC 1.52c, EXEPACK is not possible

Should be more or less useable/available with older/newer Microsoft compilers

.ERASE
.EXTENSIONS:
.EXTENSIONS:	.obj	.c	.h

!ifndef	TARGET
TARGET	=	DOS
!endif

!ifeq	TARGET	DOS
!else
!ifeq	TARGET	WIN95
!else
!error	Invalid TARGET (must be DOS or WIN95)
!endif
!endif

!ifeq	TARGET	DOS
COPT	=	/zp4 /5s /fp3 /d2 /oneatx /i=include /i=.. /d2 /wx /bt=dos
OBJDIR	=	OBJS\
EXE	=	xlib.exe
!endif
!ifeq	TARGET	WIN95
COPT	=	/zp4 /5s /fp3 /d2 /oneatx /i=include /i=.. /d2 /wx /bt=nt
OBJDIR	=	OBJS95\
EXE	=	xlib95.exe
!endif

C	=	wcc386 $(COPT)

.c:		.
.h:		include
.obj:		$(OBJDIR)

OBJS	=	main.obj	library.obj

$(EXE)	:	$(OBJS)
		%create link.lnk
!ifeq	TARGET	DOS
		%append link.lnk system pmodew
!endif
!ifeq	TARGET	WIN95
		%append link.lnk system nt
!endif
		%append link.lnk option map
		%append link.lnk option stack=64k
		%append link.lnk debug watcom all
		%append link.lnk name $(EXE)
		for %i in ($(OBJS)) do %append link.lnk file $(OBJDIR)\%i
		wlink @link.lnk
!ifeq	TARGET	DOS
		d:\code\pmodew\pmwsetup /B0 $(EXE)
!endif

.c.obj	:	.AUTODEPEND
		$(C) -fo=$(OBJDIR)$^. $[.

clean:		.SYMBOLIC
		del $(OBJDIR)*.obj
		del *.err
		del $(EXE)
		del *.map
		del link.lnk
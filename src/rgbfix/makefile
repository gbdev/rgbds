.ERASE

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
OBJDIR	=	OBJS\
COPT	=	/zp4 /fp3 /d2 /oneatx /i=include -i=..\ /d2 /wx /bt=DOS
CC	=	wcc $(COPT)
EXE	=	rgbfix.exe
!endif
!ifeq	TARGET	WIN95
OBJDIR	=	OBJS95\
COPT	=	/zp4 /5s /fp3 /d2 /oneatx /i=include -i=..\ /d2 /wx /bt=NT
CC	=	wcc386 $(COPT)
EXE	=	rgbfix95.exe
!endif

OBJS	=	main.obj

.c:
.h:   	include
.obj:	$(OBJDIR)
.exe:

$(EXE):		$(OBJS)
		%create link.lnk
!ifeq	TARGET	DOS
		%append link.lnk system dos
!endif
!ifeq	TARGET	WIN95
		%append link.lnk system nt
!endif
		%append link.lnk option map
		%append link.lnk debug watcom all
		%append link.lnk name $(EXE)
		for %i in ($(OBJS)) do %append link.lnk file $(OBJDIR)\%i
		wlink @link.lnk

.c.obj:		.AUTODEPEND
		$(CC) -fo=$(OBJDIR)$^. $[@

clean:		.SYMBOLIC
		del $(OBJDIR)*.obj
		del *.err
		del $(EXE)
		del *.map
		del link.lnk
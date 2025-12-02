!IFNDEF CC
CC=cl
!ENDIF

!IFNDEF RC
RC=rc
!ENDIF

CFLAGS=/nologo /DUNICODE /D_UNICODE /W4 /EHsc /Zi /Od
LDFLAGS=/nologo
LIBS=user32.lib gdi32.lib comdlg32.lib comctl32.lib shell32.lib advapi32.lib

OBJS=binaries\retropad.obj binaries\file_io.obj binaries\retropad.res

all: binaries binaries\retropad.exe

binaries:
	@if not exist binaries mkdir binaries

binaries\retropad.exe: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) binaries\retropad.obj binaries\file_io.obj binaries\retropad.res $(LIBS) /Fe:$@ /Fd:binaries\

binaries\retropad.obj: retropad.c resource.h file_io.h
	$(CC) $(CFLAGS) /c retropad.c /Fo:$@ /Fd:binaries\

binaries\file_io.obj: file_io.c file_io.h resource.h
	$(CC) $(CFLAGS) /c file_io.c /Fo:$@ /Fd:binaries\

binaries\retropad.res: retropad.rc resource.h res\retropad.ico
	$(RC) /fo $@ retropad.rc

clean:
	-del /q binaries\*.exe binaries\*.obj binaries\*.res binaries\*.pdb binaries\*.ilk 2> NUL

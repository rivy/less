# Makefile for less.
# Windows 32 Visual C++ version

#### Start of system configuration section. ####

CC = cl

# Normal flags
## /D "_CRT_SECURE_NO_WARNING" :: suppress "unsafe function" compiler warning
## /wd4996 :: suppress POSIX function name deprecation warning #C4996
CFLAGS = /nologo /MT /W3 /EHsc /Ox /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /wd4996 /WX /c
LDFLAGS = /nologo /subsystem:console /incremental:no /machine:I386

# Debugging flags
#CFLAGS = /nologo /MTd /W3 /EHsc /Od /Gm /Zi /I "." /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /c
#LDFLAGS = /nologo /subsystem:console /incremental:yes /debug /machine:I386

LD = link
LIBS = user32.lib

#### End of system configuration section. ####

# This rule allows us to supply the necessary -D options
# in addition to whatever the user asks for.
.c.obj:
	$(CC) $(CFLAGS) $<

OBJ =   main.obj screen.obj brac.obj ch.obj charset.obj cmdbuf.obj command.obj \
	decode.obj edit.obj filename.obj forwback.obj help.obj ifile.obj \
	input.obj jump.obj line.obj linenum.obj lsystem.obj \
	mark.obj optfunc.obj option.obj opttbl.obj os.obj output.obj \
	position.obj prompt.obj search.obj signal.obj tags.obj \
	ttyin.obj version.obj regexp.obj

all: less.exe lesskey.exe

# This is really horrible, but the command line is too long for 
# MS-DOS if we try to link ${OBJ}.
less.exe: $(OBJ)
	-if EXIST "lesskey.obj" del lesskey.obj
	$(LD) $(LDFLAGS) *.obj $(LIBS) /out:$@

lesskey.exe: lesskey.obj version.obj
	$(LD) $(LDFLAGS) lesskey.obj version.obj $(LIBS) /out:$@

defines.h: defines.wn
	-del defines.h
	-copy defines.wn defines.h

$(OBJ): less.h defines.h funcs.h cmd.h

clean:
	-del *.obj 
	-del less.exe
	-del lesskey.exe



# Build with MSVC 2015

OBJS = kiseger.obj
LIBS = user32.lib shell32.lib gdi32.lib

kiseger.exe: $(OBJS)
	LINK /DEBUG /NOLOGO /INCREMENTAL:NO /OUT:kiseger.exe /PDB:kiseger1.pdb $(OBJS) $(LIBS)

{}.cpp{}.obj::
	CL /nologo /c /Od /favor:ATOM /EHa /GA /Fdkiseger.pdb /Zi $<

clean:
	del /s /q kiseger.exe *.pdb *.obj *.ilk *.tmp

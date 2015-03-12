# DYNINST_INCLUDE and DYNINST_LIB should be set to locations
# where Dyninst header and library files were installed, respectively
DYNINST_INCLUDE="C:/Program Files/Dyninst/include"
DYNINST_LIB="C:/Program Files/Dyninst/lib"
BOOST_ROOT="C:/boost/boost_1_53_0"
CC = cl

#-lsymLite -lsymtabAPI -ldynElf -ldynDwarf -linstructionAPI -lpcontrol \	
	
all: codeCoverage.exe libInst.dll

codeCoverage.exe: codeCoverage.obj
	link  -out:codeCoverage.exe codeCoverage.obj $(DYNINST_LIB)\dyninstAPI.lib \
	dbghelp.lib imagehlp.lib
libInst.obj: libInst.c
	$(CC)  -c libInst.c  -I$(DYNINST_INCLUDE) -I$(BOOST_ROOT)  

libInst.dll:libInst.obj
	link -dll -out:libInst.dll libInst.obj user32.lib
	
	
codeCoverage.obj: codeCoverage.cpp
	$(CC) -Zi -c codeCoverage.cpp $(CFLAGS) -I$(DYNINST_INCLUDE) -I$(BOOST_ROOT)


#ifndef _RELOC_REPAIR_H
#define _RELOC_REPAIR_H

#include <vector>
#include <algorithm>
#include <functional>
#include <Windows.h>
#include <assert.h>
#include <map>

using namespace std;

class RelocRepair
{


	typedef unsigned long		ulong32;
	typedef unsigned short		uword;
	typedef unsigned char		uchar;

	typedef vector <ulong32>	type_RelocsList;


#define align(x,y)				(((x)+(y)-1)&(~((y)-1)))
#define STATUS_OK				1
#define STATUS_FAILED			0

	
	public:
		RelocRepair();
		~RelocRepair();
		struct comparer
		{
		    public:
		    bool operator()(const std::string x, const std::string y)
		    {
		         return x.compare(y)<0;
		    }
		};

		int		OpenFileAndMap(char *file);
		int		CloseFileMap(void);
		int		ReadOriginalRelocs(void);
		int		RepairGeneratedCode(std::map<string, void *,comparer> iatEntries, std::map<string, void *,comparer> relocEntries);
				void    parseIAT(std::map<string,void*,comparer> &iatEntries);
		int		GenerateNewRelocs(type_RelocsList	 *NewRelocs);
		int		WriteRelocs(void);


		type_RelocsList			RelocsList;			// relocations (each ulong32 represents rva entry)


		uchar					*computed_relocs;
		ulong32					computed_relocs_size;
		ulong32					computed_relocs_align_size;

	private:
		PIMAGE_DOS_HEADER			pMZ;
		PIMAGE_NT_HEADERS			pPE;
		PIMAGE_SECTION_HEADER		pSH;;
		LOADED_IMAGE				LI;
		DWORD						imagebase;

		char						out_file[MAX_PATH];

};


#endif
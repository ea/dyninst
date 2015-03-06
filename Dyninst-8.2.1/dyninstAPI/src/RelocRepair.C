#include <vector>
#include <algorithm>
#include <functional>

#include <Windows.h>
#include <imagehlp.h>
#include <assert.h>

#include <map>

#include "RelocRepair.h"

RelocRepair::RelocRepair(){}
RelocRepair::~RelocRepair(){}



int		RelocRepair::OpenFileAndMap(char *file)
{

	_snprintf(this->out_file, sizeof(this->out_file) - 1, "%s", file);

	if (!MapAndLoad(file, NULL, &this->LI, FALSE, FALSE))
	{
		printf("! Error: Unable to load file, error = %d\n", GetLastError());
		return STATUS_FAILED;
	}
		
	this->pMZ			=	(PIMAGE_DOS_HEADER)this->LI.MappedAddress;
	this->pPE			=	(PIMAGE_NT_HEADERS)this->LI.FileHeader;
	this->pSH			=	(PIMAGE_SECTION_HEADER)((ulong32)this->LI.FileHeader +  sizeof(IMAGE_NT_HEADERS));


	return STATUS_OK;
}

int RelocRepair::CloseFileMap(void)
{
	if (this->LI.MappedAddress)
		UnMapAndLoad(&this->LI);
	return STATUS_OK;
}


//crawl through new code memry looking for absolute references to dummy addresses 
void findAbsoluteAddr(void *haystack, unsigned long haystack_len, unsigned long address,std::vector<void *> &points){
	char *ptr = (char *)haystack;
	unsigned int len = haystack_len;
	for(int i = 0; i < haystack_len-3; i++){
		if(memcmp(ptr+i,&address,sizeof(unsigned long)) == 0){
			points.push_back(ptr+i);
		}
	}
}



//make a list of entries in final iat
void RelocRepair::parseIAT(std::map<string,void*,comparer> &iatEntries){
	PIMAGE_SECTION_HEADER		pSHc = &this->pSH[this->pPE->FileHeader.NumberOfSections-1];

	PIMAGE_IMPORT_DESCRIPTOR idt = (PIMAGE_IMPORT_DESCRIPTOR)
                            ImageRvaToVa(pPE, (PVOID)LI.MappedAddress, this->pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress,0);
	while( ImageRvaToVa(pPE, (PVOID)LI.MappedAddress,idt->Name,0) != 0){
		PIMAGE_THUNK_DATA pthd =  (PIMAGE_THUNK_DATA)ImageRvaToVa(pPE, (PVOID)LI.MappedAddress,idt->FirstThunk,0);
		while(pthd->u1.Function){
			PIMAGE_IMPORT_BY_NAME  pStr = (PIMAGE_IMPORT_BY_NAME) ImageRvaToVa(pPE, (PVOID)LI.MappedAddress,  pthd->u1.AddressOfData,0);
			//printf("%s %s %x\n", ImageRvaToVa(pPE, (PVOID)LI.MappedAddress,idt->Name,0),(PIMAGE_IMPORT_BY_NAME)pStr->Name,idt->FirstThunk + this->pPE->OptionalHeader.ImageBase);
			iatEntries[(char *)(PIMAGE_IMPORT_BY_NAME)pStr->Name] = (void *)(idt->FirstThunk + this->pPE->OptionalHeader.ImageBase);
			pthd++;
		}
		idt++;
	}
}

// takes all dummy references in the newly generated code and substitutes them with actuall pointers to appropriate iat entries
int RelocRepair::RepairGeneratedCode(std::map<string, void *,comparer> iatEntries, std::map<string, void *,comparer> relocEntries){
	// find dyninst section
	PIMAGE_SECTION_HEADER		pSHc;
	pSHc					=	&this->pSH[this->pPE->FileHeader.NumberOfSections-1];
	printf("section name %s %x %d \n",pSHc->Name,pSHc->VirtualAddress,pSHc->SizeOfRawData);
	void *dynInstStart		=	ImageRvaToVa(pPE, (PVOID)LI.MappedAddress, pSHc->VirtualAddress, 0);
	std::vector<void *> points;

	// iterate over  each reloc entries in newly generated code
	for(std::map<string, void *,comparer>::iterator rit = relocEntries.begin(); rit != relocEntries.end(); ++rit){
		//printf("%s dummy @ %x\n", rit->first.c_str(),rit->second);
		// find where it is supposed to be referenced in the actuall executable code
		findAbsoluteAddr(dynInstStart,pSHc->SizeOfRawData,(unsigned long)rit->second,points);
		// then , replace the dummy address in those instructions with actual pointers into appropriate import
		for(std::vector<void *>::iterator it = points.begin(); it != points.end(); it++){
			//printf("\tTo patch at %x to %x\n", *it,iatEntries[rit->first]);
			//match by symbol name
			if(iatEntries[rit->first]){
				memcpy(*it,&iatEntries[rit->first],sizeof(unsigned long));
				// add it to reloc list so reloc entry gets added for new code too
				RelocsList.push_back( (unsigned long)*it- (unsigned long)LI.MappedAddress);
			}
		}
		points.clear();
	}

	return STATUS_OK;
}


int	RelocRepair::ReadOriginalRelocs(void)
{

	PIMAGE_BASE_RELOCATION			pRE;
	int								entries;
	ulong32							addr, rva;


	this->RelocsList.clear();

#define reloc_members_num(x)		((x - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD))


	rva		=	(ulong32)this->pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
	pRE		=	(PIMAGE_BASE_RELOCATION)ImageRvaToVa(pPE, (PVOID)LI.MappedAddress, rva, 0);

	if ((ulong32)pRE == (ulong32)this->pMZ)
	{
		printf(__FUNCTION__": error - no relocs found!\r\n");
		return STATUS_FAILED;
	}



	while (pRE->SizeOfBlock != 0)
	{
		WORD			relocType;
		PWORD			pEntry;
		entries			= reloc_members_num(pRE->SizeOfBlock);
		
		pEntry			= (PWORD)((DWORD)pRE + (DWORD)sizeof(IMAGE_BASE_RELOCATION));

		for (int i=0; i < entries; i++ )
		{
			addr		= (DWORD)((*pEntry & 0x0FFF) + pRE->VirtualAddress);
			relocType	= (*pEntry & 0xF000) >> 12;		

			if (relocType == IMAGE_REL_BASED_HIGHLOW)
			{
	
				printf(__FUNCTION__": *** Relocs entry at %08x\r\n", (ulong32)addr);

				// add this
				this->RelocsList.push_back(addr);
			}
			pEntry++;
		}
		
		pRE = (PIMAGE_BASE_RELOCATION)((DWORD)pRE + (DWORD)pRE->SizeOfBlock);
	}

	return STATUS_OK;
}


int		RelocRepair::GenerateNewRelocs(type_RelocsList	 *NewRelocs)
{

	this->computed_relocs_size	=	NULL;
	this->computed_relocs		=	NULL;

	if (this->RelocsList.size() == NULL)
	{
		printf(__FUNCTION__": nothing in relocs!\n");
		return STATUS_FAILED;
	}


	ulong32 max_r_size		=	this->RelocsList.size() * sizeof(ulong32) + 16;

	this->computed_relocs	=	new uchar[max_r_size];
	assert(computed_relocs);
	memset((void*)this->computed_relocs, 0, max_r_size);


	// ok we are ready to go
	sort(this->RelocsList.begin(), this->RelocsList.end());


	int						num_of_blocks	=	0;
	ulong32					base_rva		=	this->RelocsList.front() & 0xFFFFFFF0;
	PIMAGE_BASE_RELOCATION	reloc_header	=	(PIMAGE_BASE_RELOCATION)this->computed_relocs;
	reloc_header->VirtualAddress			=	base_rva;
	reloc_header->SizeOfBlock				=	0;	
	uword *entry							=	(uword*)(this->computed_relocs + sizeof(IMAGE_BASE_RELOCATION));

#define I_RELOC_MAX_ENTRIES			((0xFFFF - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD))

	for (int i = 0; i < this->RelocsList.size(); i++)
	{
		ulong32		entry_rva	=	this->RelocsList[i];
		uword		write_entry	=	0;

		// we need to put another entry
		if (((entry_rva - base_rva) > 0xFFF) || (num_of_blocks > I_RELOC_MAX_ENTRIES))
		{
			// block exceeded need to add new one
#define ABSOLUTE_LOC 0
			*entry				=	ABSOLUTE_LOC;
			entry++;

			base_rva			=	entry_rva & 0xFFFFF000;
			reloc_header->SizeOfBlock		= (ulong32)entry - (ulong32)reloc_header;
			reloc_header		=	(PIMAGE_BASE_RELOCATION)entry;
			reloc_header->VirtualAddress	= base_rva;
			num_of_blocks		=	0;
			entry				=	(uword*)((ulong32)reloc_header + sizeof(IMAGE_BASE_RELOCATION));
		}

#define IMAGE_REL_BASED_HIGHLOWX	0x3000
		
		entry[0]					=	(uword)(entry_rva - base_rva) | IMAGE_REL_BASED_HIGHLOWX;
		entry++;
		num_of_blocks++;	
	}


	entry[0]							=	ABSOLUTE_LOC;
	entry++;
	reloc_header->SizeOfBlock			=	(ulong32)entry - (ulong32)reloc_header;
	this->computed_relocs_size			=	(ulong32)entry	- (ulong32)this->computed_relocs;// + sizeof(IMAGE_BASE_RELOCATION);
	this->computed_relocs_align_size	=	align(this->computed_relocs_size + sizeof(IMAGE_BASE_RELOCATION), 16) - this->computed_relocs_size ;
	this->computed_relocs_size			+= 	this->computed_relocs_align_size;

	return STATUS_OK;
}


int		RelocRepair::WriteRelocs(void)
{
	ulong32						raw_sec_size, raw_offset, new_reloc_rva, expand_size;
	PIMAGE_SECTION_HEADER		pSHc;


	pSHc					=	&this->pSH[this->pPE->FileHeader.NumberOfSections-1];
	raw_sec_size			=	this->computed_relocs_align_size + pSHc->SizeOfRawData;
	raw_sec_size			=	align(raw_sec_size, this->pPE->OptionalHeader.FileAlignment);
	raw_offset				=	pSHc->PointerToRawData + pSHc->SizeOfRawData;

	new_reloc_rva			=	pSHc->VirtualAddress + pSHc->SizeOfRawData;

	this->pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress = new_reloc_rva;
	this->pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size = this->computed_relocs_size;



	expand_size				=	raw_sec_size - pSHc->SizeOfRawData;
	pSHc->SizeOfRawData		=	raw_sec_size;

	this->CloseFileMap();


	// dump relocs now
	uchar		*temp		=	new uchar[expand_size];
	memset(temp, 0, expand_size);
	memcpy(temp, this->computed_relocs, this->computed_relocs_size);


	HFILE hFile = _lopen(this->out_file, OF_READWRITE);
	if (hFile == HFILE_ERROR)
	{
		printf(__FUNCTION__": unable to open file \"%s\", error = 0x%08x\r\n", this->out_file, GetLastError());
		assert(hFile != HFILE_ERROR);

	}
	_llseek(hFile, raw_offset, SEEK_SET);
	_lwrite(hFile, (LPCCH)temp, expand_size);
	_lclose(hFile);
	
	
	free(this->computed_relocs);
	free(temp);





	return STATUS_OK;
}

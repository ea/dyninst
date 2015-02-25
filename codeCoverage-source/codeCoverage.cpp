
/*
 *  A simple code coverage tool using DyninstAPI
 *
 *  This tool uses DyninstAPI to instrument the functions and basic blocks in
 *  an executable and its shared libraries in order to record code coverage
 *  data when the executable is run. This code coverage data is output when the
 *  rewritten executable finishes running.
 *
 *  The intent of this tool is to demonstrate some capabilities of DyninstAPI;
 *  it should serve as a good stepping stone to building a more feature-rich
 *  code coverage tool on top of Dyninst.
 */
#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>
#include <Imagehlp.h>

#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
using namespace std;

// DyninstAPI includes
#include "BPatch.h"
#include "BPatch_binaryEdit.h"
#include "BPatch_flowGraph.h"
#include "BPatch_function.h"
#include "BPatch_point.h"

using namespace Dyninst;

// configuration options
char *inBinary = NULL;
char *outBinary = NULL;
bool includeSharedLib = false;
int printAll = 0;
bool bbCoverage = false;
int alphabetical = 0;



BPatch_function *findFuncByName (BPatch_image * appImage, char *funcName)
{
    /* fundFunctions returns a list of all functions with the name 'funcName' in the binary */
    BPatch_Vector < BPatch_function * >funcs;


    if (NULL == appImage->findFunction (funcName, funcs) || !funcs.size ()
            || NULL == funcs[0]) {
        cerr << "Failed to find " << funcName <<
            " function in the instrumentation library" << endl;
        return NULL;
    }
	cout<< "findFuncByName " <<  funcName << funcs[0]<< endl;	
	
    return funcs[0];
}

bool insertFuncEntry (BPatch_binaryEdit * appBin, BPatch_function * curFunc,
        char *funcName, BPatch_function * instIncFunc,
        int funcId)
{
    /* Find the instrumentation points */
    vector < BPatch_point * >*funcEntry = curFunc->findPoint (BPatch_entry);
    if (NULL == funcEntry) {
        cerr << "Failed to find entry for function " << funcName << endl;
        return false;
    }

    cout << "Inserting instrumention at function entry of " << funcName << endl;
    /* Create a vector of arguments to the function
     * incCoverage function takes the function name as argument */
    BPatch_Vector < BPatch_snippet * >instArgs;
    BPatch_constExpr id (funcId);
    instArgs.push_back (&id);
    BPatch_funcCallExpr instIncExpr (*instIncFunc, instArgs);

    /* Insert the snippet at function entry */
    BPatchSnippetHandle *handle =
        appBin->insertSnippet (instIncExpr, *funcEntry, BPatch_callBefore,
                BPatch_lastSnippet);
    if (!handle) {
        cerr << "Failed to insert instrumention at function entry of " << funcName
            << endl;
        return false;
    }
    return true;

}

// -----------------------------------------------------------------------------------------------------------------
int FixImports(char *file)
{
	PIMAGE_DOS_HEADER			pMZ;
	PIMAGE_NT_HEADERS			pPE;
	PIMAGE_SECTION_HEADER		pSH;
	LOADED_IMAGE				LI;
	DWORD						imagebase;

	PIMAGE_IMPORT_DESCRIPTOR		pIMP;
	PIMAGE_THUNK_DATA				thunk;
	PIMAGE_IMPORT_BY_NAME			iname;
	DWORD							rva, ImportRVA, first;
	int								impd_num;
	char							*name;


	if (!MapAndLoad(file, NULL, &LI, FALSE, FALSE))
	{
		printf("! Error: Unable to load file, error = %d\n",GetLastError());
		return 0;
	}


		
	pMZ			=	(PIMAGE_DOS_HEADER)LI.MappedAddress;
	pPE			=	(PIMAGE_NT_HEADERS)LI.FileHeader;
	pSH			=	(PIMAGE_SECTION_HEADER)((DWORD)LI.FileHeader +  sizeof(IMAGE_NT_HEADERS));


	ImportRVA	=		pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
	pSH			+=		pPE->FileHeader.NumberOfSections - 1;
	imagebase	=		pPE->OptionalHeader.ImageBase;


	// hack
	rva			=	pSH->VirtualAddress + (ImportRVA & 0xFFFF);


	pIMP		= (PIMAGE_IMPORT_DESCRIPTOR)ImageRvaToVa(pPE,(PVOID)LI.MappedAddress, rva, 0);
	first		= (pIMP->OriginalFirstThunk == 0 ? pIMP->FirstThunk:pIMP->OriginalFirstThunk);


	impd_num				=	0;
	DWORD		bad_rva		=	ImportRVA & ~0xFFFF;

#define fix_field(xfield) { if (xfield >= bad_rva) { xfield = pSH->VirtualAddress + (xfield & 0xFFFF);  } }

	while ((pIMP->OriginalFirstThunk != 0) || (pIMP->FirstThunk != 0))
	{
	
		fix_field(pIMP->FirstThunk);
		fix_field(pIMP->OriginalFirstThunk);
		fix_field(pIMP->Name);

		name	=	(char*)ImageRvaToVa(pPE,(PVOID)LI.MappedAddress, pIMP->Name, 0);
		first	=	(pIMP->OriginalFirstThunk == 0 ? pIMP->FirstThunk : pIMP->OriginalFirstThunk);
		thunk	=	(PIMAGE_THUNK_DATA)ImageRvaToVa(pPE,(PVOID)LI.MappedAddress, first, 0);

		fix_field(thunk->u1.Function);

		int b		=	0;
		while ((DWORD)thunk->u1.Function != 0)
		{
			DWORD	api_addr	=	(DWORD)(pIMP->FirstThunk + b + imagebase);
			if (((DWORD)thunk->u1.Function & IMAGE_ORDINAL_FLAG32) == IMAGE_ORDINAL_FLAG32)
			{
				/* import by ordinal */
				printf("    + [0x%.08x] -> %s!ORD%x\n", api_addr ,name, (WORD)thunk->u1.Ordinal);
				
			}
			else
			{
				/* import by name */
				iname = (PIMAGE_IMPORT_BY_NAME)ImageRvaToVa(pPE, (PVOID)LI.MappedAddress, thunk->u1.ForwarderString,0);
				printf("    + [0x%.08x] -> %s!%s\n", api_addr, name, iname->Name);

				
			}
			b += sizeof(DWORD);
			thunk++;
		}
		pIMP++;
		impd_num++;
	}


	fix_field(pPE->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	if (LI.MappedAddress)
		UnMapAndLoad(&LI);

	return 1;
}
// -----------------------------------------------------------------------------------------------------------------


int main (int argc, char *argv[])
{

    /* Every Dyninst mutator needs to declare one instance of BPatch */
    BPatch bpatch;

    /* Open the specified binary for binary rewriting. 
     * When the second parameter is set to true, all the library dependencies 
     * as well as the binary are opened */
	inBinary = argv[1];
	outBinary = argv[2];
    BPatch_binaryEdit *appBin = bpatch.openBinary (inBinary, true);
    if (appBin == NULL) {
        cerr << "Failed to open binary" << endl;
        return EXIT_FAILURE;
    }
	
    /* Open the instrumentation library.
     * loadLibrary loads the instrumentation library into the binary image and
     * adds it as a new dynamic dependency in the rewritten library */
    const char *instLibrary = "libInst.dll";
    if (!appBin->loadLibrary (instLibrary)) {
        cerr << "Failed to open instrumentation library" << endl;
        return EXIT_FAILURE;
    }

    BPatch_image *appImage = appBin->getImage ();
    BPatch_Vector < BPatch_function * >funcs1;
	appImage->findFunction ("incFuncCoverage", funcs1);
    BPatch_function *instIncFunc =funcs1[0];


    if (!instIncFunc) {
			
        return EXIT_FAILURE;
    }

    /* To instrument every function in the binary
     * --> iterate over all the modules in the binary 
     * --> iterate over all functions in each modules */

    vector < BPatch_module * >*modules = appImage->getModules ();
    vector < BPatch_module * >::iterator moduleIter;
    BPatch_module *defaultModule;

    BPatch_Vector < BPatch_snippet * >registerCalls;

    int bbIndex = 0;
    int funcIndex = 0;
    for (moduleIter = modules->begin (); moduleIter != modules->end ();
            ++moduleIter) {
        char moduleName[1024];
        (*moduleIter)->getName (moduleName, 1024);

        /* if includeSharedLib is not set, skip instrumenting dependent libraries */
        if ((*moduleIter)->isSharedLib ()) {
            if (!includeSharedLib) {
                cout << "Skipping library: " << moduleName << endl;
                continue;
            }
        }

        /* Every binary has one default module.
         * code coverage initialize and finalize functions should be called only once.
         * Hence call them from the default module */
        if (string (moduleName).find ("DEFAULT_MODULE") != string::npos) {
            defaultModule = (*moduleIter);
        }

        cout << "Instrumenting module: " << moduleName << endl;
        vector < BPatch_function * >*allFunctions =
            (*moduleIter)->getProcedures ();
        vector < BPatch_function * >::iterator funcIter;
		cout << "Iterating over all functions" << endl;
        /* Insert snippets at the entry of every function */
        for (funcIter = allFunctions->begin (); funcIter != allFunctions->end ();
                ++funcIter) {
            BPatch_function *curFunc = *funcIter;

            char funcName[1024];
            curFunc->getName (funcName, 1024);
						cout << funcName << endl;

			if(strcmp(funcName,"main")==0)
            insertFuncEntry (appBin, curFunc, funcName, instIncFunc, funcIndex);
        }
    }
    
	cout << "writting the binary!" << endl;
    if (!appBin->writeFile (outBinary)) {
        cerr << "Failed to write output file: " << outBinary << endl;
        return EXIT_FAILURE;
    }
	
	//FixImports(outBinary);
	cout << "wrote the binary!" << endl;

    return EXIT_SUCCESS;
}

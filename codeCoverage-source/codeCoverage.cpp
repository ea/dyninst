
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



int main (int argc, char *argv[])
{

    /* Every Dyninst mutator needs to declare one instance of BPatch */
    BPatch bpatch;

    /* Open the specified binary for binary rewriting. 
     * When the second parameter is set to true, all the library dependencies 
     * as well as the binary are opened */
	inBinary = argv[1];
	outBinary = argv[2];
    BPatch_binaryEdit *appBin = bpatch.openBinary (inBinary, false);
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

			  // if(funcIndex < 1000) {
			  // cout << "Dumb function " << funcName << endl;
			  // funcIndex++;
			  // continue;
              // }
			insertFuncEntry (appBin, curFunc, funcName, instIncFunc, funcIndex);
			funcIndex++;
		}
    }
    
	cout << "writting the binary!" << endl;
    if (!appBin->writeFile (outBinary)) {
        cerr << "Failed to write output file: " << outBinary << endl;
        return EXIT_FAILURE;
    }
	
	cout << "wrote the binary!" << endl;

    return EXIT_SUCCESS;
}

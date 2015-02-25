/*
 * The instrumentation library for the codeCoverage tool. Provides
 * functions for initialization, registering functions and basic
 * blocks for coverage tracking, and outputting the results.
 */

#include<stdio.h>

#define DllExport   __declspec( dllexport ) 


// Should be called on function entry 
DllExport void __stdcall incFuncCoverage(int id) {
	printf("Hmm %d\n",id);
}


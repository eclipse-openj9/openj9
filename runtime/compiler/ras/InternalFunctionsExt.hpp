/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef INTERNAL_FUNCTIONS_EXT_INCL
#define INTERNAL_FUNCTIONS_EXT_INCL

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "env/FilePointerDecl.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Symbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "ras/Debug.hpp"
#include "ras/InternalFunctions.hpp"

class TR_FrontEnd;
class TR_ResolvedMethod;
namespace TR { class Compilation; }
namespace TR { class Node; }

/*
 * Debugger Extensions
 */
typedef void* (* TR_Malloc_t)(uintptrj_t, void*);
typedef void  (* TR_Free_t)(void*);
typedef void  (* TR_Printf_t)(const char *s, ...);

class TR_InternalFunctionsExt : public TR_InternalFunctions
   {
public:

   void *operator new (size_t s, TR_Malloc_t dxMalloc) { return dxMalloc(s, NULL); }

   TR_InternalFunctionsExt(TR::Compilation *comp, TR_FrontEnd * fe, TR_Printf_t dxPrintf, TR_Malloc_t dxMalloc, TR_Free_t dxFree)
      : TR_InternalFunctions(fe, 0, 0, comp), _dxPrintf(dxPrintf), _dxMalloc(dxMalloc), _dxFree(dxFree), _memchk(false) { }

   /* all memory allocations will be redirected to dxMalloc() */
   virtual void * stackAlloc(size_t s) { return malloc(s); }
   virtual void * stackMark() { return 0; }
   virtual void   stackRelease(void *p) { return; }
   void free(void* p) { if (_memchk) _dxPrintf("jit->free: 0x%p\n", p); _dxFree(p); }

   /* output redirection */
   virtual void fprintf(TR::FILE *file, const char * format, ...);
   virtual void fflush(TR::FILE *file) { /* do nothing */ return; }

   /* RTTI */
   virtual bool inDebugExtension() { return true; }

   virtual const char * signature(TR_ResolvedMethod * method, TR_AllocationKind allocKind = heapAlloc) { return _debug->getName(method); }

   void setMemChk()   { _memchk = true; }
   void resetMemChk() { _memchk = false; }

   // need this in case we have a different debug session
   void reInitialize(TR_Debug * debug, TR::Compilation * comp, TR_FrontEnd * fe, TR_Memory * trMemory)
      { _debug = debug; _comp = comp; _fe = fe; _trMemory = trMemory; }

private:

   TR_Debug  *_debug;
   TR_Printf_t _dxPrintf;
   TR_Malloc_t _dxMalloc;
   TR_Free_t   _dxFree;
   bool        _memchk;
   };

#endif // INTERNAL_FUNCTIONS_EXT_INCL

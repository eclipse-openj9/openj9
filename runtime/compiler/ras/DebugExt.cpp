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

#ifndef DEBUGEXT_C
#define DEBUGEXT_C

#include "ras/DebugExt.hpp"

#include <algorithm>
#include <assert.h>
#include "j9.h"
#include "j9cfg.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Relocation.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CHTable.hpp"
#include "env/KnownObjectTable.hpp"
#include "env/IO.hpp"
#include "env/PersistentInfo.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "infra/Cfg.hpp"
#include "infra/MonitorTable.hpp"
#include "infra/Monitor.hpp"
#include "infra/RWMonitor.hpp"
#include "optimizer/PreExistence.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "ras/HashTable.hpp"
#include "ras/InternalFunctionsExt.hpp"
#include "ras/IgnoreLocale.hpp"
#include "ras/CFGChecker.hpp"
#include "ras/CFGCheckerExt.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/CodeCacheMemorySegment.hpp"
#include "runtime/DataCache.hpp"
#include "runtime/J9Runtime.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "env/j9method.h"
#include "env/CpuUtilization.hpp"
#include "env/VMJ9.h"
#include "env/CompilerEnv.hpp"

#if defined(TR_TARGET_S390)
#include "codegen/S390Register.hpp"
#endif

#define DEFAULT_INDENT_INCREMENT   2
#define IS_4_BYTE_ALIGNED(p) (!((uintptrj_t) p & 0x3))

static bool isGoodPtr(void* p)
   {
   return (p != 0 &&
           p > (void*) 1024 &&
           IS_4_BYTE_ALIGNED(p));
   }

void*
TR_DebugExt::operator new (size_t s, TR_Malloc_t dbgjit_Malloc)
   {
   return dbgjit_Malloc(s, NULL);
   }

bool
TR_DebugExt::dxReadMemory(void* remotePtr, void* localPtr, uintptrj_t size)
   {
   uintptrj_t bytesRead;
   assert(remotePtr != 0 && localPtr != 0 && size != 0);
   if (localPtr == remotePtr)
      {
      // shouldn't reach here in theory
      _dbgPrintf("\n*** JIT Warning: local and remote memory (0x%p) are the same!\n", localPtr);
      if (_memchk) assert(false);
      return true;   // try to be nice by not crashing the debugger
      }
   _dbgReadMemory((uintptrj_t) remotePtr, localPtr, size, &bytesRead);
   if (bytesRead != size)
      {
      _dbgPrintf("\n*** JIT Error: could not read memory at 0x%x for %zu bytes\n", remotePtr, size);  // FIXME:: should use dbgError()
      if (_memchk) assert(false);
      return false;
      }
   return true;
   }

bool
TR_DebugExt::dxReadField(void* classPtr, uintptrj_t fieldOffset, void* localPtr, uintptrj_t size)
   {
   uintptrj_t remoteAddr = ((uintptrj_t)classPtr) + fieldOffset;
   bool rc = dxReadMemory((void*)remoteAddr, localPtr, size);
   return rc;
   }

void*
TR_DebugExt::dxMalloc(uintptrj_t size, void *remotePtr, bool dontAddToMap)
   {
   if (size == 0) return 0;

   void *localPtr = _dbgMalloc(size, remotePtr);
   if (localPtr && !dontAddToMap)
      _toRemotePtrMap->add(localPtr, remotePtr);
   if (_memchk)
      {
      _dbgPrintf("   JIT: malloc pair (local=0x%p, remote=0x%p, size=%d)\n", localPtr, remotePtr, size);
      memset(localPtr, 0, size);
      }
   return localPtr;
   }

void*
TR_DebugExt::dxMallocAndRead(uintptrj_t size, void *remotePtr, bool dontAddToMap)
   {
   if (size == 0 || remotePtr == 0) return 0;
   void *localPtr = dxMalloc(size, remotePtr, dontAddToMap);
   if (dxReadMemory(remotePtr, localPtr, size))
      return localPtr;
   else
      return 0;
   }

void*
TR_DebugExt::dxMallocAndReadString(void* remotePtr, bool dontAddToMap)
   {
   char c;
   int sz = 0;
   uintptrj_t bytesRead = 0;
   if (remotePtr == 0) return 0;
   do {
      _dbgReadMemory(((uintptrj_t)remotePtr+sz), &c, 1, &bytesRead);
      sz++;
   } while (bytesRead == 1 && c != '\0');
   if (bytesRead != 1) sz--;
   if (sz == 0) return 0;
   return dxMallocAndRead((uintptrj_t)sz, remotePtr, dontAddToMap);
   }

void
TR_DebugExt::dxFree(void * localAddr)
   {
   if (!localAddr)
      return;

   TR_HashIndex hi;
   if (_toRemotePtrMap->locate(localAddr, hi))
      {
      if (_memchk) _dbgPrintf("   JIT: free pair (local=0x%p, remote=0x%p)\n", localAddr, _toRemotePtrMap->getData(hi));
      _toRemotePtrMap->remove(hi);
      }

   _dbgFree(localAddr);
   }

// This could be used to free up memory lost when we seg fault
// Need to do it properly though...
void
TR_DebugExt::dxFreeAll()
   {
   TR_HashTableEntry *table = _toRemotePtrMap->_table;
   TR_HashIndex hi;

   for (hi=0; hi<_toRemotePtrMap->_tableSize; hi++)
      if (table[hi].isValid())
         _dbgFree(table[hi].getKey());

   _toRemotePtrMap->removeAll();
   }

static void updateVFT(void *destObject, void *srcObject)
   {
   char **srcObjectPtr = (char **)srcObject;
   char **destObjectPtr = (char **)destObject;
   *destObjectPtr = *srcObjectPtr;
   }

void
TR_DebugExt::updateLocalPersistentMemoryFunctionPointers(J9JITConfig *localJitConfig, TR_PersistentMemory *localPersistentMemory)
   {
   if (localPersistentMemory)
      {
      TR::PersistentAllocator *temp = new (dxMalloc(sizeof(TR::PersistentAllocator), 0, true)) TR::PersistentAllocator((TR::PersistentAllocatorKit(64 * 1024, *localJitConfig->javaVM)));
      localPersistentMemory->_persistentAllocator = TR::ref(*temp);
      }
   }

/*
 * initialize the TR_DebugExt object with data exported from j9.dll
 */
TR_DebugExt::TR_DebugExt(
   TR_InternalFunctions * f,
   struct J9PortLibrary *dbgextPortLib,
   J9JavaVM *localVM,
   void (*dbgjit_Printf)(const char *s, ...),
   void (*dbgjit_ReadMemory)(uintptrj_t remoteAddr, void *localPtr, uintptrj_t size, uintptrj_t *bytesRead),
   void* (*dbgjit_Malloc)(uintptrj_t size, void *originalAddress),
   void (*dbgjit_Free)(void * addr),
   uintptrj_t (*dbgGetExpression)(const char* args)
   ) :
   TR_Debug(NULL),
   _jit(f),
   _localVM(localVM),
   _localJITConfig(localVM ? localVM->jitConfig : NULL),
   _dbgextPortLib(dbgextPortLib),
   _dbgPrintf(dbgjit_Printf),
   _dbgReadMemory(dbgjit_ReadMemory),
   _dbgMalloc(dbgjit_Malloc),
   _dbgFree(dbgjit_Free),
   _dbgGetExpression(dbgGetExpression),
   _debugSegmentProvider(1 << 20, dbgjit_Malloc, dbgjit_Free),
   _debugRegion(_debugSegmentProvider, TR::RawAllocator(::jitConfig->javaVM)),
   _isAOT(false),
   _structureValid(false)
   {

   _localJITConfig->javaVM = _localVM;
   if (_localJITConfig->javaVM)
      _localJITConfig->javaVM->portLibrary = _dbgextPortLib;

   _remoteCompiler = NULL;
   _localCompiler = NULL;
   _showTypeInfo = false;
   _memchk = false;

   _remoteCompInfo = FrontEnd2CompInfo(J9JITConfig2FrontEnd());
   if (_remoteCompInfo)
      _localCompInfo = (TR::CompilationInfo*) dxMallocAndRead(sizeof(TR::CompilationInfo), _remoteCompInfo, true);
   else
      _localCompInfo = NULL;

   _remotePersistentMemory =  J9JITConfig2PersistentMemory();
   if (_remotePersistentMemory)
      {
      _localPersistentMemory = (TR_PersistentMemory *)dxMallocAndRead(sizeof(TR_PersistentMemory), _remotePersistentMemory, true);
      updateLocalPersistentMemoryFunctionPointers(_localJITConfig, _localPersistentMemory);
      }
   else
      {
      _localPersistentMemory = NULL;
      }

   _remotePersistentInfo = PersistentMemory2PersistentInfo();
   if (_remotePersistentInfo)
      _localPersistentInfo = (TR::PersistentInfo *) dxMallocAndRead(sizeof(TR::PersistentInfo), _remotePersistentInfo, true);
   else
      _localPersistentInfo = NULL;

   TR::FilePointer * myStdout = (TR::FilePointer *) _dbgMalloc(sizeof(TR::FilePointer), 0);
   OMR::IO::Stdout = new (myStdout) TR::FilePointer( stdout );

   void *myMem = _dbgMalloc(sizeof(TR_Memory), 0);
   _debugExtTrMemory = new (myMem) TR_Memory(*_localPersistentMemory, _debugRegion);

   _toRemotePtrMap = new (_debugExtTrMemory) TR_HashTable(_debugExtTrMemory);
   }

bool
TR_DebugExt::compInfosPerThreadAvailable()
   {
   if (_localCompInfosPT == NULL)
      {
      _localCompInfosPT = (TR::CompilationInfoPerThread **) dxMalloc(sizeof(TR::CompilationInfoPerThread *) * MAX_TOTAL_COMP_THREADS, NULL);
      if (_localCompInfosPT)
         {
         for (size_t i = 0; i < MAX_TOTAL_COMP_THREADS; ++i)
            {
            _localCompInfosPT[i] = _localCompInfo->_arrayOfCompilationInfoPerThread[i] != NULL ?
               (TR::CompilationInfoPerThread *) dxMallocAndRead(sizeof(TR::CompilationInfoPerThread), _localCompInfo->_arrayOfCompilationInfoPerThread[i], true) :
               NULL;
            }
         }
      }
   return _localCompInfosPT == NULL;
   }

bool
TR_DebugExt::activeMethodsToBeCompiledAvailable()
   {
   if (!compInfosPerThreadAvailable()) return false;
   if (_localActiveMethodsToBeCompiled == NULL)
      {
      _localActiveMethodsToBeCompiled = (TR_MethodToBeCompiled **) dxMalloc(sizeof(TR_MethodToBeCompiled *) * MAX_TOTAL_COMP_THREADS, NULL);
      if (_localActiveMethodsToBeCompiled)
         {
         for (size_t i = 0; i < MAX_TOTAL_COMP_THREADS; ++i)
            {
            _localActiveMethodsToBeCompiled[i] = _localCompInfosPT[i] && _localCompInfosPT[i]->_methodBeingCompiled ?
               (TR_MethodToBeCompiled *) dxMallocAndRead(sizeof(TR_MethodToBeCompiled), _localCompInfosPT[i]->_methodBeingCompiled, true) :
               NULL;
            }
         }
      }
   return _localActiveMethodsToBeCompiled == NULL;
   }

bool
TR_DebugExt::isAOTCompileRequested(TR::Compilation * remoteCompilation)
   {
   if (!compInfosPerThreadAvailable()) return false;
   if (!activeMethodsToBeCompiledAvailable()) return false;

   for (size_t i = 0; i < MAX_TOTAL_COMP_THREADS; ++i)
      {
      if (
         _localCompInfosPT[i]
         && _localCompInfosPT[i]->_compiler == remoteCompilation
         && _localActiveMethodsToBeCompiled[i]
         && _localActiveMethodsToBeCompiled[i]->_useAotCompilation
         )
         {
         return true;
         }
      }

   return false;
   }

void
TR_DebugExt::allocateLocalCompiler(TR::Compilation *remoteCompiler)
   {
   // Need to free up the previously allocated compiler if it exists
   freeLocalCompiler();
   _remoteCompiler = remoteCompiler;

   if (_remoteCompiler == 0 || !IS_4_BYTE_ALIGNED(_remoteCompiler))
      _dbgPrintf("*** JIT Warning: Compilation object 0x%p is invalid\n", _remoteCompiler);
   else
      _localCompiler = (TR::Compilation*) dxMallocAndRead(sizeof(TR::Compilation), (void*)_remoteCompiler);

   if (_localCompiler)
      {
      _dbgPrintf("*** JIT Info: Compilation object 0x%p is now cached\n", _remoteCompiler);

      _localCompiler->_trMemory = _debugExtTrMemory;
      _isAOT = isAOTCompileRequested(remoteCompiler);
      allocateLocalFrontEnd();

      _localCompiler->_codeGenerator = (TR::CodeGenerator *) dxMallocAndRead(sizeof(TR::CodeGenerator), _localCompiler->_codeGenerator);
      _localCompiler->_options = (TR::Options *) dxMallocAndRead(sizeof(TR::Options), _localCompiler->_options);
      _localCompiler->_optimizer = (TR::Optimizer *) dxMallocAndRead(sizeof(TR::Optimizer), _localCompiler->_optimizer);

      _localCompiler->_symRefTab = (TR::SymbolReferenceTable *)dxMallocAndRead(sizeof(TR::SymbolReferenceTable), _localCompiler->_symRefTab);
      if (_localCompiler->_currentSymRefTab)
         _localCompiler->_currentSymRefTab = (TR::SymbolReferenceTable *)dxMallocAndRead(sizeof(TR::SymbolReferenceTable), _localCompiler->_currentSymRefTab);

      _localCompiler->_methodSymbol = (TR::ResolvedMethodSymbol*) dxMallocAndRead(sizeof(TR::ResolvedMethodSymbol), _localCompiler->_methodSymbol);
      if (_localCompiler->_optimizer)
         _localCompiler->_optimizer->_methodSymbol = _localCompiler->_methodSymbol;

      _localCompiler->_knownObjectTable = (TR::KnownObjectTable *)dxMallocAndRead(sizeof(TR::KnownObjectTable), _localCompiler->_knownObjectTable);
      if (_localCompiler->_knownObjectTable)
         {
         TR::KnownObjectTable temp(_localCompiler);
         updateVFT((void *)_localCompiler->_knownObjectTable, (void *)&temp);
         _localCompiler->_knownObjectTable->setFe(_localCompiler->_fe);
         _localCompiler->_knownObjectTable->setComp(_localCompiler);

         TR_Array<uintptrj_t*> &localReferences = ((J9::KnownObjectTable *)_localCompiler->_knownObjectTable)->_references;
         uintptrj_t **array = localReferences._array;
         uint32_t arraySize = localReferences.size();
         uintptrj_t **localArray = (uintptrj_t **) dxMallocAndRead(sizeof(array[0])*arraySize, array);
         localReferences._array = localArray;

         for (int32_t i = 0; i < arraySize; i++)
            {
            uintptrj_t *remotePointer = localArray[i];
            uintptrj_t *localPointer = (uintptrj_t *)dxMallocAndRead(sizeof(uintptrj_t), remotePointer);
            localArray[i] = localPointer;
            }
         }

      TR::CFG *cfg = Compilation2CFG();
      TR::CFG *localCfg = (TR::CFG*) dxMallocAndRead(sizeof(TR::CFG), cfg);

      _structureValid = localCfg->getStructure() != NULL;

      dxFree(localCfg);
      tlsSet(OMR::compilation, _localCompiler);

      // Update TR_Debug Fields
      _comp = _localCompiler;
      _fe = _localCompiler->_fe;
      }
   }

// This should only be called from allocateLocalCompiler and it should only be called once!
void
TR_DebugExt::allocateLocalFrontEnd()
   {
   TR_FrontEnd *remoteFE, *localFE;

   remoteFE = _localCompiler->_fe;

   if (remoteFE == 0)
      {
      _dbgPrintf("*** JIT Error: TR_FrontEnd object 0x%p is invalid\n", remoteFE);
      _localCompiler->_fe = NULL;
      }
   else
      {
      _dbgPrintf("*** JIT Info: TR_FrontEnd object 0x%p is now cached\n", remoteFE);

      // Set options::sharedcache to false so that we dont' try to persistent alloc the J9SharedCache object...
      bool sharedCache = TR::Options::sharedClassCache();
      TR::Options::setSharedClassCache(false);
      if (!_isAOT)
         {
          localFE = (TR_FrontEnd*) dxMallocAndRead(sizeof(TR_J9VM), (void*)remoteFE);
          TR_J9VMExt temp(_localJITConfig, _localCompInfo, NULL);
          updateVFT((void *)localFE, (void *)&temp);
         }
#if defined(J9VM_OPT_SHARED_CLASSES) && defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
      else
         {
         localFE = (TR_FrontEnd*) dxMallocAndRead(sizeof(TR_J9SharedCacheVM), (void*)remoteFE);
         TR_J9SharedCacheVMExt temp(_localJITConfig, _localCompInfo, NULL);
         updateVFT((void *)localFE, (void *)&temp);
         }
#endif
      TR::Options::setSharedClassCache(sharedCache);

      _localCompiler->_fe = localFE;
      ((TR_J9VMBase *)_localCompiler->_fe)->_jitConfig = _localJITConfig;
      }
   }

void
TR_DebugExt::freeLocalCompiler()
   {
   if (_localCompiler)
      {
      if (_localCompiler->_codeGenerator)
         dxFree(_localCompiler->_codeGenerator);
      if (_localCompiler->_options)
         dxFree(_localCompiler->_options);
      if (_localCompiler->_optimizer)
         dxFree(_localCompiler->_optimizer);
      if (_localCompiler->_symRefTab)
         dxFree(_localCompiler->_symRefTab);
      if (_localCompiler->_currentSymRefTab)
         dxFree(_localCompiler->_currentSymRefTab);
      if (_localCompiler->_methodSymbol)
         dxFree(_localCompiler->_methodSymbol);
       if (_localCompiler->_knownObjectTable)
          {
          TR_Array<uintptrj_t*> &localReferences = ((J9::KnownObjectTable *)_localCompiler->_knownObjectTable)->_references;
          uintptrj_t **localArray = localReferences._array;
          uint32_t arraySize = localReferences.size();
          if (localArray)
             {
             for (int32_t i = 0; i < arraySize; i++)
                dxFree(localArray[i]);
             dxFree(localArray);
             }
          dxFree(_localCompiler->_knownObjectTable);
          }
      freeLocalFrontEnd();
      dxFree(_localCompiler);
      _comp = NULL;
      _fe = NULL;
      _localCompiler = NULL;
      tlsSet(OMR::compilation, NULL);
      }
   }

void
TR_DebugExt::freeLocalFrontEnd()
   {
   if (_localCompiler->_fe)
      dxFree(_localCompiler->_fe);
   }

bool
TR_DebugExt::initializeDebug(TR::Compilation *remoteCompiler)
   {
   bool ok = true;
   if (remoteCompiler && remoteCompiler != _remoteCompiler)
      {
      allocateLocalCompiler(remoteCompiler);

      if (_localCompiler && _localCompiler->_fe && _localCompiler->_trMemory)
         {
         /* initialize _debug, _comp and _fe of TR_InternalFunctions */
         ((TR_InternalFunctionsExt*)_jit)->reInitialize(this, _localCompiler, (TR_FrontEnd*) _localCompiler->_fe, _localCompiler->_trMemory);
         }
      else
         {
         ok = false;
         }
      }

   return ok;
   }

void
TR_DebugExt::dxAllocateLocalBlockInternals(TR::Node *localNode)
   {
   if (localNode->hasBlock())
      {
      TR::Block *localBlk = localNode->getBlock();

      // Set Entry
      if (localNode->getOpCodeValue() == TR::BBStart && localBlk->getEntry())
         {
         TR::TreeTop *localEntry = (TR::TreeTop*) dxMallocAndRead(sizeof(TR::TreeTop), (void*)localBlk->getEntry());
         localBlk->setEntry(localEntry);
         if (localEntry->getPrevTreeTop())
            {
            // Allocates a Tree Top, its Node, and the Node's _unionPropertyA
            TR::TreeTop *localPrevTreeTop = dxAllocateLocalTreeTop(localEntry->getPrevTreeTop());
            localEntry->setPrevTreeTop(localPrevTreeTop);
            }

         // Set Structure
         if (_structureValid && localBlk->getStructureOf())
            {
            TR_BlockStructure *blockStructure = localBlk->getStructureOf();
            TR_BlockStructure *localBlockStructure = (TR_BlockStructure *) dxMallocAndRead(sizeof(TR_BlockStructure), blockStructure);
            localBlk->setStructureOf(localBlockStructure);
            }
         else
            {
            localBlk->setStructureOf(NULL);
            }

         // Set Catch Block Extension
         if (localBlk->isCatchBlock())
            {
            TR::Block::TR_CatchBlockExtension *catchBlockExtension = localBlk->getCatchBlockExtension();
            TR::Block::TR_CatchBlockExtension *localCatchBlockExtension = (TR::Block::TR_CatchBlockExtension *)dxMallocAndRead(sizeof(TR::Block::TR_CatchBlockExtension),
                                                                                                                               catchBlockExtension);
            localBlk->setCatchBlockExtension(localCatchBlockExtension);
            }
         }

      // Set Exit
      if (localNode->getOpCodeValue() == TR::BBEnd && localBlk->getExit())
         {
         TR::TreeTop *localExit = (TR::TreeTop*) dxMallocAndRead(sizeof(TR::TreeTop), (void*)localBlk->getExit());
         localBlk->setExit(localExit);
         if (localExit->getNextTreeTop())
            {
            // Allocates a Tree Top, its Node, and the Node's _unionPropertyA
            TR::TreeTop *localNextTreeTop = dxAllocateLocalTreeTop(localExit->getNextTreeTop());
            localExit->setNextTreeTop(localNextTreeTop);
            }
         }
      }
   }

void
TR_DebugExt::dxFreeLocalBlockInternals(TR::Node *localNode)
   {
   if (localNode->hasBlock())
      {
      TR::Block *localBlk = localNode->getBlock();
      TR::TreeTop *localExit = localBlk->getExit();
      TR::TreeTop *localEntry = localBlk->getEntry();

      if (localNode->getOpCodeValue() == TR::BBEnd && localExit)
         {
         if (localExit->getNextTreeTop())
           dxFreeLocalTreeTop(localExit->getNextTreeTop());
         dxFree(localExit);
         }
      if (localNode->getOpCodeValue() == TR::BBStart && localEntry)
         {
         TR_BlockStructure *localBlockStructure = localBlk->getStructureOf();
         if (localBlockStructure)
           dxFree(localBlockStructure);

         if (localEntry->getPrevTreeTop())
           dxFreeLocalTreeTop(localEntry->getPrevTreeTop());
         dxFree(localEntry);
         }
      }
   }

void
TR_DebugExt::dxAllocateSymRefInternals(TR::SymbolReference *localSymRef, bool complete)
   {
   TR::Symbol * sym = localSymRef->getSymbol();
   TR::Symbol *tempSym = (TR::Symbol*) dxMallocAndRead(sizeof(TR::Symbol), sym);
   TR::Symbol *localSym = NULL;

   switch (tempSym->getKind())
      {
      case TR::Symbol::IsAutomatic:
         localSym = (TR::Symbol*) dxMallocAndRead(sizeof(TR::AutomaticSymbol), sym);
         break;
      case TR::Symbol::IsParameter:
         localSym = (TR::Symbol*) dxMallocAndRead(sizeof(TR::ParameterSymbol), sym);
         break;
      case TR::Symbol::IsStatic:
         localSym = (TR::Symbol*) dxMallocAndRead(sizeof(TR::StaticSymbol), sym);
         break;
      case TR::Symbol::IsResolvedMethod:
         {
         localSym = (TR::Symbol*) dxMallocAndRead(sizeof(TR::ResolvedMethodSymbol), sym);
         if (complete)
            {
            TR_J9MethodBase *localMethod = (TR_J9MethodBase *) dxMallocAndRead(sizeof(TR_J9MethodBase), (void*)localSym->getMethodSymbol()->getMethod());
            localSym->getMethodSymbol()->setMethod(localMethod);
            }
         }
         break;
      case TR::Symbol::IsMethod:
         {
         localSym = (TR::Symbol*) dxMallocAndRead(sizeof(TR::MethodSymbol), sym);
         if (complete)
            {
            TR_J9MethodBase *localMethod = (TR_J9MethodBase *) dxMallocAndRead(sizeof(TR_J9MethodBase), (void*)localSym->getMethodSymbol()->getMethod());
            localSym->getMethodSymbol()->setMethod(localMethod);
            }
         }
         break;
      case TR::Symbol::IsShadow:
         localSym = (TR::Symbol*) dxMallocAndRead(sizeof(TR::Symbol), sym);
         break;
      case TR::Symbol::IsMethodMetaData:
         {
         localSym = (TR::Symbol*) dxMallocAndRead(sizeof(TR::RegisterMappedSymbol), sym);
         char *localName = (char *) dxMallocAndReadString((void*)(localSym->getMethodMetaDataSymbol()->_name));
         localSym->getMethodMetaDataSymbol()->_name = localName;
         }
         break;
      case TR::Symbol::IsLabel:
         localSym = (TR::Symbol*) dxMallocAndRead(sizeof(TR::LabelSymbol), sym);
         break;
      }
   dxFree(tempSym);
   localSymRef->setSymbol(localSym);

   if (localSym->isStatic())
      {
      uintptrj_t *remoteStaticAddress = (uintptrj_t *)localSym->castToStaticSymbol()->getStaticAddress();
      uintptrj_t *localStaticAddress = (uintptrj_t *)dxMallocAndRead(sizeof(uintptrj_t), remoteStaticAddress);
      localSym->castToStaticSymbol()->setStaticAddress((void *)localStaticAddress);
      }
   }

void
TR_DebugExt::dxFreeSymRefInternals(TR::Symbol *localSymbol, bool complete)
   {
   if (localSymbol)
      {
      if (complete)
         {
         if (localSymbol->getKind() == TR::Symbol::IsResolvedMethod)
            dxFree(localSymbol->getMethodSymbol()->getMethod());
         if (localSymbol->getKind() == TR::Symbol::IsMethod)
            dxFree(localSymbol->getMethodSymbol()->getMethod());
         }
      if (localSymbol->getKind() == TR::Symbol::IsMethodMetaData)
         dxFree((void *)localSymbol->getMethodMetaDataSymbol()->_name);
      if (localSymbol->isStatic() && localSymbol->castToStaticSymbol()->getStaticAddress())
         dxFree(localSymbol->castToStaticSymbol()->getStaticAddress());
      dxFree(localSymbol);
      }
   }

TR::Node *
TR_DebugExt::dxAllocateLocalNode(TR::Node *remoteNode, bool recursive, bool complete)
   {
   TR::Node *localNode = NULL;
   uint16_t numChildren = 0;

   dxReadField(remoteNode, offsetof(TR::Node, _numChildren), &numChildren, sizeof(uint16_t));
   localNode = (TR::Node*) dxMallocAndRead(sizeof(TR::Node), (void*)remoteNode);

   if (localNode)
      {
      if (localNode->getOpCode().hasSymbolReference())
         {
         localNode->_unionPropertyA._symbolReference = (TR::SymbolReference*) dxMallocAndRead(sizeof(TR::SymbolReference), localNode->_unionPropertyA._symbolReference);
         dxAllocateSymRefInternals(localNode->_unionPropertyA._symbolReference, complete);
         }
      else if (localNode->hasRegLoadStoreSymbolReference())
         {
         localNode->_unionPropertyA._regLoadStoreSymbolReference = (TR::SymbolReference*) dxMallocAndRead(sizeof(TR::SymbolReference), localNode->_unionPropertyA._regLoadStoreSymbolReference);
         dxAllocateSymRefInternals(localNode->_unionPropertyA._regLoadStoreSymbolReference, complete);
         }
      else if (localNode->hasBranchDestinationNode())
         localNode->_unionPropertyA._branchDestinationNode = (TR::TreeTop*) dxMallocAndRead(sizeof(TR::TreeTop), localNode->_unionPropertyA._branchDestinationNode);
      else if (localNode->hasBlock())
         localNode->_unionPropertyA._block = (TR::Block*) dxMallocAndRead(sizeof(TR::Block), localNode->_unionPropertyA._block);
      else if (localNode->hasPinningArrayPointer())
         localNode->_unionPropertyA._pinningArrayPointer = (TR::AutomaticSymbol*) dxMallocAndRead(sizeof(TR::AutomaticSymbol), localNode->_unionPropertyA._pinningArrayPointer);

#if defined(TR_TARGET_S390)
      if (localNode->getOpCode().canHaveStorageReferenceHint())
         localNode->_storageReferenceHint = (TR_StorageReference*) dxMallocAndRead(sizeof(TR_StorageReference), localNode->_storageReferenceHint);
#endif

      if (localNode->hasNodeExtension())
         {
         OMR::Node::NodeExtensionStore *nodeExtensionStore = &localNode->_unionBase._extension;
         uint16_t numElements = nodeExtensionStore->getNumElems();
         size_t sizeOfNodeExtension = sizeof(TR::NodeExtension) + ((numElements-NUM_DEFAULT_ELEMS) * sizeof(uintptr_t));

         TR::NodeExtension *remoteNodeExtension = nodeExtensionStore->getExtensionPtr();
         TR::NodeExtension *localNodeExtension = (TR::NodeExtension *) dxMallocAndRead(sizeOfNodeExtension, remoteNodeExtension);
         nodeExtensionStore->setExtensionPtr(localNodeExtension);
         }

      if (recursive && numChildren)
         {
         for (uint16_t i = 0; i < numChildren; i++)
            {
            TR::Node *localChildNode = dxAllocateLocalNode(localNode->getChild(i), true, complete);
            if (localChildNode)
               localNode->setChild(i, localChildNode);
            }
         }
      }

   return localNode;
   }

void
TR_DebugExt::dxFreeLocalNode(TR::Node *localNode, bool recursive, bool complete)
   {
   if (localNode)
      {
      if (recursive)
         {
         uint16_t numChildren = 0;
         for (uint16_t i = 0; i < numChildren; i++)
            dxFreeLocalNode(localNode->getChild(i), true, complete);
         }

      if (localNode->getOpCode().hasSymbolReference())
         {
         dxFreeSymRefInternals(localNode->_unionPropertyA._symbolReference->getSymbol(), complete);
         dxFree(localNode->_unionPropertyA._symbolReference);
         }
      else if (localNode->hasRegLoadStoreSymbolReference())
         {
         dxFreeSymRefInternals(localNode->_unionPropertyA._regLoadStoreSymbolReference->getSymbol(), complete);
         dxFree(localNode->_unionPropertyA._regLoadStoreSymbolReference);
         }
      else if (localNode->hasBranchDestinationNode())
         dxFree(localNode->_unionPropertyA._branchDestinationNode);
      else if (localNode->hasBlock())
         dxFree(localNode->_unionPropertyA._block);
      else if (localNode->hasPinningArrayPointer())
         dxFree(localNode->_unionPropertyA._pinningArrayPointer);

#if defined(TR_TARGET_S390)
      if (localNode->getOpCode().canHaveStorageReferenceHint())
         dxFree(localNode->_storageReferenceHint);
#endif

      if (localNode->hasNodeExtension())
         {
         OMR::Node::NodeExtensionStore *nodeExtensionStore = &localNode->_unionBase._extension;
         if (nodeExtensionStore->getExtensionPtr())
            dxFree(nodeExtensionStore->getExtensionPtr());
         }

      dxFree(localNode);
      }
   }

TR::TreeTop *
TR_DebugExt::dxAllocateLocalTreeTop(TR::TreeTop *remoteTreeTop, bool recursive)
   {
   TR::TreeTop *localTreeTop = (TR::TreeTop*) dxMallocAndRead(sizeof(TR::TreeTop), (void*)remoteTreeTop);
   TR::Node *localNode = dxAllocateLocalNode(localTreeTop->getNode(), recursive);
   localTreeTop->setNode(localNode);
   return localTreeTop;
   }

void
TR_DebugExt::dxFreeLocalTreeTop(TR::TreeTop *localTreeTop, bool recursive)
   {
   dxFreeLocalNode(localTreeTop->getNode(), recursive);
   dxFree(localTreeTop);
   }

TR_PersistentMethodInfo*
TR_DebugExt::Compilation2MethodInfo()
   {
   if (_localCompiler == 0 || _remoteCompiler == 0)
      return 0;

   TR::Recompilation *recompilationInfo = _localCompiler->getRecompilationInfo();
   TR_PersistentMethodInfo *methodInfo = NULL;
   TR::Recompilation *localRecompilationInfo = (TR::Recompilation*) dxMallocAndRead(sizeof(TR::Recompilation), (void*)recompilationInfo);
   if (localRecompilationInfo)
      {
      methodInfo = localRecompilationInfo->getMethodInfo();
      dxFree(localRecompilationInfo);
      }

   return methodInfo;
   }


TR_PersistentProfileInfo*
TR_DebugExt::Compilation2ProfileInfo()
   {
   if (_localCompiler == 0 || _remoteCompiler == 0)
      return 0;

   TR_PersistentMethodInfo *minfo = Compilation2MethodInfo();
   TR_PersistentProfileInfo *profileInfo = NULL;
   if (minfo)
      {
      TR_PersistentMethodInfo *localMInfo = (TR_PersistentMethodInfo*) dxMallocAndRead(sizeof(TR_PersistentMethodInfo), (void*)minfo);
      if (localMInfo)
         {
         profileInfo = localMInfo->getBestProfileInfo();
         dxFree(localMInfo);
         }
      }

   return profileInfo;
   }

/*
 * return (TR::PersistentInfo*)->getRuntimeAssumptionTable();
 */
TR_RuntimeAssumptionTable*
TR_DebugExt::PersistentInfo2RuntimeAssumptionTable()
   {
   TR_RuntimeAssumptionTable *runtimeAssumptionTable = NULL;
   if (_localPersistentInfo && _remotePersistentInfo)
      {
      runtimeAssumptionTable = (TR_RuntimeAssumptionTable*)
         ((char*)_remotePersistentInfo + ((char*)_localPersistentInfo->getRuntimeAssumptionTable() - (char*)_localPersistentInfo));
      _dbgPrintf("((TR::PersistentInfo*)0x%p)->getRuntimeAssumptionTable() = (TR_RuntimeAssumptionTable*)0x%p\n", _remotePersistentInfo, runtimeAssumptionTable);
      }

   return runtimeAssumptionTable;
   }

/*
 * return (TR::PersistentInfo*)->getPersistentCHTable()->getCHTable();
 */
TR_PersistentCHTable*
TR_DebugExt::PersistentInfo2PersistentCHTable()
   {
   TR_PersistentCHTable *persistentCHTable = NULL;
   if (_localPersistentInfo && _remotePersistentInfo)
      {
      persistentCHTable = _localPersistentInfo->getPersistentCHTable();
      _dbgPrintf("((TR::PersistentInfo*)0x%p)->getPersistentCHTable() = (TR_PersistentCHTable*)0x%p\n", _remotePersistentInfo, _localPersistentInfo->getPersistentCHTable());
      }

   return persistentCHTable;
   }

/*
 * return (TR::PersistentInfo*)->getPersistentCHTable()->getCHTable();
 */
TR_CHTable*
TR_DebugExt::PersistentInfo2CHTable()
   {
   return NULL;
   }

/*
 * return TR_PersistentMemory::getPersistentInfo()
 */
TR::PersistentInfo *
TR_DebugExt::PersistentMemory2PersistentInfo()
   {
   if (_localPersistentMemory == 0 || _remotePersistentMemory == 0)
      return NULL;

   TR::PersistentInfo * persistentInfo = _localPersistentMemory->getPersistentInfo();
   _dbgPrintf("TR_Memory::getPersistentInfo() = (TR::PersistentInfo*)0x%p\n", persistentInfo);
   return persistentInfo;
   }

/*
 * return TR_PersistentMemory*
 *        = (J9JITConfig*)->scratchSegment;
 */
TR_PersistentMemory*
TR_DebugExt::J9JITConfig2PersistentMemory()
   {
   if (_localJITConfig == 0)
      return 0;

   _dbgPrintf("(J9JITConfig*)->scratchSegment = (TR_PersistentMemory*)0x%p\n", _localJITConfig->scratchSegment);

   return (TR_PersistentMemory*) _localJITConfig->scratchSegment;
   }

/*
 * return (J9JITConfig*)->compilationInfo
 */
TR_FrontEnd*
TR_DebugExt::J9JITConfig2FrontEnd()
   {
   if (_localJITConfig == 0)
      return 0;

   _dbgPrintf("(J9JITConfig*)->compilationInfo = (TR_J9VMBase*)0x%p\n", _localJITConfig->compilationInfo);

   return (TR_FrontEnd*) _localJITConfig->compilationInfo;
   }

/*
 * return (TR_J9VMBase*)->_compInfo
 */
TR::CompilationInfo*
TR_DebugExt::FrontEnd2CompInfo(TR_FrontEnd * remoteFE)
   {
   if (remoteFE == 0)
      return 0;

   TR::CompilationInfo *remoteCompInfo;
   dxReadField(remoteFE, offsetof(TR_J9VMBase, _compInfo), &remoteCompInfo, sizeof(TR::CompilationInfo *));

   _dbgPrintf("((TR_J9VMBase*)0x%p)->compInfo = (TR::CompilationInfo*)0x%p\n", remoteFE, remoteCompInfo);

   return remoteCompInfo;
   }

/*
 * return (TR::CompilationInfo*)->_arrayOfCompilationInfoPerThread
 */
TR::CompilationInfoPerThread **
TR_DebugExt::CompInfo2ArrayOfCompilationInfoPerThread(uint8_t *numThreads)
   {
   if (_remoteCompInfo == 0)
      {
      *numThreads = 0;
      return 0;
      }

   return _localCompInfo->_arrayOfCompilationInfoPerThread;
   }

/*
 * return (TR::Compilation*)->getOptimizer()
 */
TR::Optimizer*
TR_DebugExt::Compilation2Optimizer()
   {
   if (_remoteCompiler == 0)
      return 0;

   TR::Optimizer *optimizer;
   // Need to use the _remoteCompiler here because _localCompiler->_optimizer has been overwritten
   // with a local object
    dxReadField(_remoteCompiler, offsetof(TR::Compilation, _optimizer), &optimizer, sizeof(TR::Optimizer *));
   _dbgPrintf("((TR::Compilation*)0x%p)->_optimizer = (TR::Optimizer*)0x%p\n", _remoteCompiler, optimizer);
   return optimizer;
   }

/*
 * return (TR::Optimizer*)->getMethodSymbol()
 */
TR::ResolvedMethodSymbol*
TR_DebugExt::Optimizer2ResolvedMethodSymbol(TR::Optimizer *remoteOptimizer)
   {
   if (remoteOptimizer == 0)
      return 0;

   TR::ResolvedMethodSymbol *methodSymbol;
   dxReadField(remoteOptimizer, offsetof(TR::Optimizer, _methodSymbol), &methodSymbol, sizeof(TR::ResolvedMethodSymbol*));
   _dbgPrintf("((TR::Optimizer*)0x%p)->_methodSymbol = (TR::ResolvedMethodSymbol*)0x%p\n", remoteOptimizer, methodSymbol);
   return methodSymbol;
   }

/*
 * return (TR::Compilation*)->getMethodSymbol()
 */
TR::ResolvedMethodSymbol*
TR_DebugExt::Compilation2ResolvedMethodSymbol()
   {
   if (_remoteCompiler == 0)
      return 0;

   // Need to use the _remoteCompiler here because _localCompiler->_methodSymbol has been overwritten
   // with a local object
   TR::ResolvedMethodSymbol *methodSymbol;
   dxReadField(_remoteCompiler, offsetof(TR::Compilation, _methodSymbol), &methodSymbol, sizeof(TR::ResolvedMethodSymbol*));
   _dbgPrintf("((TR::Compilation*)0x%p)->_methodSymbol = (TR::ResolvedMethodSymbol*)0x%p\n", _remoteCompiler, methodSymbol);
   return methodSymbol;
   }

/*
 * (TR::Compilation*)->getMethodSymbol()->getFlowGraph()
 */
TR::CFG*
TR_DebugExt::Compilation2CFG()
   {
   TR::CFG *cfg = NULL;
   TR::ResolvedMethodSymbol *methodSymbol = Compilation2ResolvedMethodSymbol();
   if (methodSymbol)
      {
      TR::ResolvedMethodSymbol *localMethodSymbol = (TR::ResolvedMethodSymbol*) dxMallocAndRead(sizeof(TR::ResolvedMethodSymbol), methodSymbol);
      cfg = localMethodSymbol->getFlowGraph();
      dxFree(localMethodSymbol);
      _dbgPrintf("((TR::ResolvedMethodSymbol*)0x%p)->getFlowGraph() = (TR::CFG*)0x%p\n", methodSymbol, cfg);
      }
   return cfg;
   }

TR::CompilationInfoPerThread **
TR_DebugExt::dxMallocAndGetArrayOfCompilationInfoPerThread(uint8_t numThreads, void* addr) // Helper function, remember to use dxFree after
   {
   return static_cast<TR::CompilationInfoPerThread **>(dxMallocAndRead(sizeof(TR::CompilationInfoPerThread *) * numThreads, addr));
   }

/*
 * print debugger extension usage
 */
void
TR_DebugExt::dxPrintUsage()
   {
   _dbgPrintf("Usage:\n");
   _dbgPrintf("\t!trprint compilationinfo [TR::CompilationInfo address]        // print compilationInfo object\n");
   _dbgPrintf("\t!trprint compilationinfoperthreadbase <TR::CompilationInfoPerThreadBase address>\n");
   _dbgPrintf("\t!trprint arrayofcompilationinfoperthread <TR::CompilationInfoPerThread*[] address>\n");
   _dbgPrintf("\t!trprint compilationinfoperthread <TR::CompilationInfoPerThread address>\n");
   _dbgPrintf("\t!trprint compilation <TR::Compilation address>            // print compilation object\n");
   _dbgPrintf("\t!trprint methodtobecompiled <TR_MethodToBeCompiled address>\n");
   _dbgPrintf("\t!trprint optimizationplan <TR_OptimizationPlan address>\n");
   _dbgPrintf("\t!trprint trmemory <TR_Memory address>                // show jit Memory object\n");
   _dbgPrintf("\t!trprint persistentmemory <TR_PersistentMemory address> // show jit PersistentMemory\n");
   _dbgPrintf("\t!trprint persistentinfo // print TR::PersistentInfo object\n");
   _dbgPrintf("\t!trprint persistentchtable [TR_PersistentCHTable address]           // dump PersistentCHTable\n");
   _dbgPrintf("\t!trprint jittedbodyinfo <TR_PersistentJittedBodyInfo address>     // dump PersistentJittedBodyInfo\n");
   _dbgPrintf("\t!trprint findmethodfrompc <address> [limit]          //dump method metadata\n");
   _dbgPrintf("\t!trprint j9method <address>                          //dump J9Method and J9ROMMethod information\n");
   _dbgPrintf("\t!trprint runtimeassumptiontable [TR_RuntimeAssumptionTable address] // dump RuntimeAssumptionTable\n");
   _dbgPrintf("\t!trprint runtimeassumptionarray <OMR::RuntimeAssumption*[] address> start end  // dump RuntimeAssumption array\n");
   _dbgPrintf("\t!trprint runtimeassumption <OMR::RuntimeAssumption* address>                       // dump RuntimeAssumption\n");

   _dbgPrintf("\n");

   _dbgPrintf("\t*** The following should ONLY be invoked after invoking !trprint compilation in order to cache the compilation object.\n");
   _dbgPrintf("\t*** Additionally, make sure that the addresses passed in are associated with said compilation object.\n");
   _dbgPrintf("\t*** This is necessary because the TR_Debug object assumes a comp object (among many others) exists\n");
   _dbgPrintf("\t\t!trprint compilationil [TR::Compilation address]          // dump all ILs of the method in compilation object; will update cached TR::Compilation object\n");
   _dbgPrintf("\t\t!trprint inlinedcalls                                     // dump all inlined callsites in the method\n");
   _dbgPrintf("\t\t!trprint nodeil <TR::Node address>                        // print node as an IL tree\n");
   _dbgPrintf("\t\t!trprint node <TR::Node address>                          // print the node info as a list\n");
   _dbgPrintf("\t\t!trprint persistentmethodinfo [TR_PersistentMethodInfo address]     // dump PersistentMethodInfo\n");
   _dbgPrintf("\t\t!trprint persistentprofileinfo [TR_PersistentProfileInfo address]   // dump PersistentProfileInfo\n");

   _dbgPrintf("\n");

   _dbgPrintf("\tThese are broken or unverified:\n");
   _dbgPrintf("\t!trprint codecachelist                               // dump List of code caches\n");
   _dbgPrintf("\t!trprint cfg [TR::CFG address]                        // dump cfg\n");
   _dbgPrintf("\t!trprint compilationtracingbuffer\n");
   _dbgPrintf("\t!trprint blockil <TR::Block address> [all=1|0]          // print all ILs in a basic block / bbs that follows\n");
   _dbgPrintf("\t!trprint blockcfg <TR::Block address>                   // print block as cfg\n");
   _dbgPrintf("\t!trprint chtable [TR_CHTable address]                // dump transient CHTable\n");
   _dbgPrintf("\t!trprint codecache <TR::CodeCache address>           // dump TR::CodeCache struct\n");
   _dbgPrintf("\t!trprint freecodecacheblocklist <TR::CodeCache address>    // dump list of free blocks in this code cache\n");
   _dbgPrintf("\t!trprint freecodecacheblock <OMR::CodeCacheFreeCacheBlock address>   // dump OMR::CodeCacheFreeCacheBlock struct\n");
   _dbgPrintf("\t!trprint datacache <TR_DataCache address> // dump TR_DataCache contents\n");
   _dbgPrintf("\t!trprint datacachemanager <TR_DataCacheManager address>   // dump TR_DataCacheManager contents\n");
   _dbgPrintf("\t!trprint datacachesizebucket <TR_DataCacheManager::SizeBucket address>   // dump TR_DataCacheManager::SizeBucket contents\n");
   _dbgPrintf("\t!trprint datacacheallocation <TR_DataCacheManager::Allocation address>   // dump TR_DataCacheManager::Allocation contents\n");
   _dbgPrintf("\t!trprint datacachesizebucketlistelement <TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement address>   // dump TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement contents\n");
   _dbgPrintf("\t!trprint datacacheallocationlistelement <TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement address>   // dump TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement contents\n");
   _dbgPrintf("\t!trprint j9monitortable <TR_J9MonitorTable address>  // print TR_J9MonitorTable object\n");
   _dbgPrintf("\t!trprint typecast-info [1|0]                           // produce ILs with typecast info (on/off)\n");
   _dbgPrintf("\t!trprint verifytrees [TR::Compilation address]        // verify IR tress\n");
   _dbgPrintf("\t!trprint verifyblocks [TR::Compilation address]       // verify blocks\n");
   _dbgPrintf("\t!trprint verifycfg [TR::CFG address]                  // verify cfg\n");
   _dbgPrintf("\t!trprint aotinfo [AOT data address]                    // print AOT information at [address] (see !shrc to get AOT data address)\n");
   _dbgPrintf("\t!trprint memchk [1|0]                                  // turn on/off verbose output for memory allocation\n");
   _dbgPrintf("\t!trprint debug                                         // let another debugger to attach to current process for debugging\n");
   _dbgPrintf("\n");

   _dbgPrintf("\t*** PS: Sometimes when trprint segfaults, restarting fixes it...no idea why...\n");
   }

/*
 * main entry for debugger extension print
 */
void
TR_DebugExt::dxTrPrint(const char* name1, void* addr2, uintptrj_t argCount, const char *args)
   {
   /*
    * Printing the default help page
    * Also prevent the uninitialized args to get processed
    */
   if (argCount == 0)
      {
      dxPrintUsage();
      return;
      }

   /*
    * Re-process arguments here
    * The delimiter is space or comma (maintain compatibility with j9)
    * The algorithm is not bullet proof, but should be sufficient for us
    */
   #define ARGSIZE 200
   #define MAX_ARGS 5
   char  myArgs[ARGSIZE];   // copy of args
   char  *argv[MAX_ARGS];   // pointers to arguments stored in myArgs
   int32_t argc = 0;
   const char *argp1 = args;
   char *argp2 = myArgs;
   while (*argp2++ = *argp1++);
   for (argp2 = myArgs; *argp2; )
      {
      argv[argc++] = argp2;
      while (*argp2 && *argp2 != ' ' && *argp2 != ',') argp2++;
      while (*argp2 && (*argp2 == ' ' || *argp2 == ',')) *argp2++ = '\0';   // advanced to next arg while replacing delimiter to null char
      }

   /*
    * For historically reason, use className as 1st argument,
    * addr as 2nd argument and argCount as argument count.
    */
   char *className = argv[0];
   void *addr = 0;
   if (argc >= 2) addr = (void*) _dbgGetExpression(argv[1]);
   argCount = argc;

   /* simple syntax check */
   if (argCount > MAX_ARGS || className == 0)
      {
      _dbgPrintf("*** JIT Error: wrong argument counts!\n");
      dxPrintUsage();
      return;
      }
   #undef ARGSIZE
   #undef MAX_ARGS

   bool ok = true;

   /*
    * processing cmd line
    * - cmds are in small letters
    * - the first few key characters are compared
    * - the last addr used is cached
    * - automatically finding the class address from j9jitconfig if 0 is used in addr
    */
   if (stricmp_ignore_locale(className, "nodeil") == 0 && argCount == 2) // Node IL
      {
      seenNode *seenNodes = NULL;
      dxPrintNodeIL((TR::Node*) addr, &seenNodes);
      freeSeenNodes(&seenNodes);
      }
   else if (stricmp_ignore_locale(className, "node") == 0 && argCount == 2) // Node
      {
      dxPrintNode((TR::Node*) addr);
      }
   else if (stricmp_ignore_locale(className, "blockil") == 0 && argCount <= 3) // Block IL
      {
      seenNode *seenNodes = NULL;
      int32_t numBlocks = (argCount >= 3 ? (int32_t) _dbgGetExpression(argv[2]) : 1);
      dxPrintBlockIL((TR::Block *) addr, &seenNodes, numBlocks);
      freeSeenNodes(&seenNodes);
      }
   else if (stricmp_ignore_locale(className, "blockcfg") == 0 && argCount == 2) // Block
      {
      dxPrintBlockCFG((TR::Block *) addr);
      }
   else if (stricmp_ignore_locale(className, "compilationil") == 0 // Compilation ILs
            && (ok = initializeDebug((TR::Compilation*)addr)))
      {
      dxPrintCompilationIL();
      }
   else if (stricmp_ignore_locale(className, "inlinedcalls") == 0) // Method ILs
      {
      if (addr == 0)
         addr = Compilation2ResolvedMethodSymbol();
      dxPrintInlinedCallSites((TR::ResolvedMethodSymbol*)addr);
      }
   else if (stricmp_ignore_locale(className, "cfg") == 0) // CFG
      {
      if (addr == 0)
         addr = Compilation2CFG();
      dxPrintCFG((TR::CFG*) addr);
      }
   else if (stricmp_ignore_locale(className, "trmemory") == 0) // MemoryHeader
      {
      if (argCount == 2 && addr != NULL)
         dxPrintTRMemory((TR_Memory*) addr);
      else
         _dbgPrintf("*** JIT Warning: Must provide an address for trMemory\n");
      }
   else if (stricmp_ignore_locale(className, "persistentmemory") == 0) // MemoryHeader
      {
      dxPrintPersistentMemory((TR_PersistentMemory*) addr);
      }
   else if (stricmp_ignore_locale(className, "chtable") == 0) // CHTable
      {
      if (addr == 0)
         addr = PersistentInfo2CHTable();
      dxPrintCHTable((TR_CHTable*) addr);
      }
   else if (stricmp_ignore_locale(className, "compilation") == 0 // Compilation
            && (ok = initializeDebug((TR::Compilation*) addr)))
      {
      if (argCount == 2 && addr != NULL)
         dxPrintCompilation();
      else
         _dbgPrintf("*** JIT Warning: Must provide an address for TR::Compilation\n");
      }
   else if (stricmp_ignore_locale(className, "jittedbodyinfo") == 0)
      {
      if (argCount == 2 && addr!=0)
         dxPrintPersistentJittedBodyInfo((TR_PersistentJittedBodyInfo *)addr);
      else
         _dbgPrintf("*** JIT Warning: Must provide an address for TR_PersistentJittedBodyInfo\n");
      }
   else if (stricmp_ignore_locale(className, "compilationinfo") == 0) // CompilationInfo
      {
      dxPrintCompilationInfo((TR::CompilationInfo *)addr);
      }
   else if (stricmp_ignore_locale(className, "compilationinfoperthreadbase") == 0) // CompilationInfoPerThreadBase
      {
      if (argCount == 2 && addr!=0)
         dxPrintCompilationInfoPerThreadBase((TR::CompilationInfoPerThreadBase *)addr);
      else
         _dbgPrintf("*** JIT Warning: Must provide an address for compilationInfoPerThreadBase\n");
      }
   else if (stricmp_ignore_locale(className, "arrayofcompilationinfoperthread") == 0)
      {
      TR::CompilationInfoPerThread ** arrayOfCompInfoPT = NULL;
      uint8_t numThreads = MAX_TOTAL_COMP_THREADS;
      bool allocated = false;

      if (argCount == 2 && addr != 0)
         {
         arrayOfCompInfoPT = dxMallocAndGetArrayOfCompilationInfoPerThread(numThreads, addr);
         allocated = true;
         }
      else
         arrayOfCompInfoPT = CompInfo2ArrayOfCompilationInfoPerThread(&numThreads);

      // print the compInfoPT objects
      // NOTE: dxPrintCompilationInfoPerThread checks for them being null
      if (arrayOfCompInfoPT)
         {
         for (uint8_t i = 0; i < numThreads; i++)
            dxPrintCompilationInfoPerThread(arrayOfCompInfoPT[i]);

         if (allocated) dxFree(arrayOfCompInfoPT);
         }
      }
   else if (stricmp_ignore_locale(className, "compilationinfoperthread") == 0 && addr != NULL)
      {
         dxPrintCompilationInfoPerThread((TR::CompilationInfoPerThread *)addr);
      }
   else if (stricmp_ignore_locale(className, "methodtobecompiled") == 0)
      {
      if (argCount == 2 && addr!=0)
         dxPrintMethodToBeCompiled((TR_MethodToBeCompiled *)addr);
      else
         _dbgPrintf("*** JIT Warning: Must provide an address for methodtobecompiled\n");
      }
   else if (stricmp_ignore_locale(className, "compilationtracingbuffer") == 0)
      {
      dxPrintCompilationTracingBuffer();
      }
   else if (stricmp_ignore_locale(className, "j9monitortable") == 0)
      {
      if (argCount == 2 && addr!=0)
         dxPrintJ9MonitorTable((TR::MonitorTable *)addr);
      else
         _dbgPrintf("*** JIT Warning: Must provide an address for j9monitortable\n");
      }
   else if (stricmp_ignore_locale(className, "persistentinfo") == 0)
      {
      dxPrintPersistentInfo((TR::PersistentInfo *)addr);
      }
   else if (stricmp_ignore_locale(className, "persistentchtable") == 0) // PersistentCHTable
      {
      if (addr == 0)
         addr = PersistentInfo2PersistentCHTable();
      dxPrintPersistentCHTable((TR_PersistentCHTable*) addr);
      }
   else if (stricmp_ignore_locale(className, "codecachelist") == 0) // PersistentCHTable
      {
      dxPrintListOfCodeCaches();
      }
   else if (stricmp_ignore_locale(className, "codecache") == 0) // PersistentCHTable
      {
      dxPrintCodeCacheInfo((TR::CodeCache*) addr);
      }
   else if (stricmp_ignore_locale(className, "freecodecacheblocklist") == 0) // PersistentCHTable
      {
      dxPrintFreeCodeCacheBlockList((TR::CodeCache*) addr);
      }
   else if (stricmp_ignore_locale(className, "freecodecacheblock") == 0) // PersistentCHTable
      {
      dxPrintFreeCodeCacheBlock((OMR::CodeCacheFreeCacheBlock*) addr);
      }
   else if (stricmp_ignore_locale(className, "datacachemanager") == 0)
      {
      dxPrintDataCacheManager((TR_DataCacheManager*) addr);
      }
   else if (stricmp_ignore_locale(className, "datacachesizebucket") == 0)
      {
      dxPrintDataCacheSizeBucket( static_cast<void *>(addr) ); // TR_DataCacheManager::SizeBucket *
      }
   else if (stricmp_ignore_locale(className, "datacacheallocation") == 0)
      {
      dxPrintDataCacheAllocation( static_cast<void *>(addr) ); // TR_DataCacheManager::Allocation *
      }
   else if (stricmp_ignore_locale(className, "datacacheallocationlistelement") == 0)
      {
      dxPrintDataCacheAllocationListElement( static_cast<void *>(addr) ); // TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement *
      }
   else if (stricmp_ignore_locale(className, "datacachesizebucketlistelement") == 0)
      {
      dxPrintDataCacheSizeBucketListElement( static_cast<void *>(addr) ); // TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement *
      }
   else if (stricmp_ignore_locale(className, "persistentmethodinfo") == 0) // PersistentMethodInfo
      {
      if (addr == 0)
         addr = Compilation2MethodInfo();
      dxPrintPersistentMethodInfo((TR_PersistentMethodInfo*) addr);
      }
   else if (stricmp_ignore_locale(className, "persistentprofileinfo") == 0) // PersistentProfileInfo
      {
      if (addr == 0)
         addr = Compilation2ProfileInfo();
      dxPrintPersistentProfileInfo((TR_PersistentProfileInfo*) addr);
      }
   else if (stricmp_ignore_locale(className, "runtimeassumptiontable") == 0) // RuntimeAssumptionTable
      {
      if (addr == 0)
         addr = PersistentInfo2RuntimeAssumptionTable();
      dxPrintRuntimeAssumptionTable((TR_RuntimeAssumptionTable*) addr);
      }
   else if (stricmp_ignore_locale(className, "runtimeassumptionarray") == 0 && argCount >= 2) // RuntimeAssumption array
      {
      int32_t start = 0;
      int32_t end = 0;
      if (argCount > 2)
         start = _dbgGetExpression(argv[2]);
      if (argCount > 3)
         end = _dbgGetExpression(argv[3]);
      dxPrintRuntimeAssumptionArray((OMR::RuntimeAssumption**) addr, start, end);
      }
   else if (stricmp_ignore_locale(className, "runtimeassumption") == 0 && argCount == 2) // RuntimeAssumption array
      {
      dxPrintRuntimeAssumption((OMR::RuntimeAssumption*) addr);
      }
   else if (stricmp_ignore_locale(className, "runtimeassumptionlist") == 0 && argCount == 2) // RuntimeAssumptionList
      {
      dxPrintRuntimeAssumptionList((OMR::RuntimeAssumption*) addr);
      }
   else if (stricmp_ignore_locale(className, "runtimeassumptionlistfrommetadata") == 0 && argCount == 2) // RuntimeAssumptionList
      {
      dxPrintRuntimeAssumptionListFromMetadata((J9JITExceptionTable*) addr);
      }
   else if (stricmp_ignore_locale(className, "verifytree") == 0) // verifytrees
      {
      dxVerifyTrees();
      }
   else if (stricmp_ignore_locale(className, "verifyblock") == 0) // verifyblocks
      {
      dxVerifyBlocks();
      }
   else if (stricmp_ignore_locale(className, "verifycfg") == 0) // verifycfg
      {
      if (addr == 0)
         addr = Compilation2CFG();
      dxVerifyCFG((TR::CFG*) addr);
      }
   else if (stricmp_ignore_locale(className, "typecast") == 0) // typecast-info
      {
      if (argCount == 1) _showTypeInfo = true;
      else _showTypeInfo = addr ? true : false;
      _dbgPrintf("   JIT: Typecast Info is set to %s\n", _showTypeInfo ? "TRUE" : "FALSE");
      }
   else if (stricmp_ignore_locale(className, "findmethodfrompc") == 0 && addr != NULL) // method metadata
      {
      if (argCount > 2)
         {
         int32_t limitSize = (int32_t) _dbgGetExpression(argv[2]);
         dxPrintMethodName((char *) addr, limitSize);
         }
      else
         {
         dxPrintMethodName((char *) addr);
         }
      }
   else if (stricmp_ignore_locale(className, "memchk") == 0) // memchk
      {
      if (argCount == 1) _memchk = true;
      else _memchk = addr ? true : false;
      _dbgPrintf("   JIT: memchk mode is set to %s\n", _memchk ? "TRUE" : "FALSE");
      if (_memchk)
         ((TR_InternalFunctionsExt*)_jit)->setMemChk();
      else
         ((TR_InternalFunctionsExt*)_jit)->resetMemChk();
      }
   else if ((stricmp_ignore_locale(className, "debug") == 0) && argCount == 1) // debug
      {
      _dbgPrintf("   JIT: please use another debugger to attach to the current process for debugging...\n");
      assert(false);  // breakpoint for nested debugging
      }
   else if (stricmp_ignore_locale(className, "aotinfo") == 0 && addr != NULL)  // aotinfo
      {
      dxPrintAOTinfo(addr);
      }
   else if (stricmp_ignore_locale(className, "j9method") == 0 && addr != NULL) // j9method
      {
      dxPrintJ9RamAndRomMethod((J9Method *) addr);
      }
   else if (stricmp_ignore_locale(className, "optimizationplan") == 0 && addr != NULL) // optimizationplan
      {
      dxPrintOptimizationPlan((TR_OptimizationPlan *) addr);
      }
   else if (stricmp_ignore_locale(className, "datacache") == 0 && addr != NULL) // datacache
      {
      dxPrintDataCache((TR_DataCache *) addr);
      }
   else
      {
      if (!ok)
         _dbgPrintf("*** JIT Error: failed to initialize debug!\n");
      else if (argCount > 0)
         _dbgPrintf("*** JIT Error: unrecognized command / incorrect arguments!\n");
      }

   }

/*
 * Overridden version of TR_Debug::getName().  It simply prints type string and address
 */
const char *
TR_DebugExt::dxGetName(const char* s, void* p)
   {
   #define BUFSIZE 5
   static char buf[BUFSIZE][100];
   static int32_t bi = 0;

   if (bi==BUFSIZE)
      bi = 0;

   TR_HashIndex hi = 0;
   if (p && _toRemotePtrMap->locate(p, hi))
      p = _toRemotePtrMap->getData(hi);
   if (_showTypeInfo)
      sprintf(buf[bi], "%s %p", s, p);
   else
      sprintf(buf[bi], "%p", p);
   return (const char *) buf[bi++];
   #undef BUFSIZE
   }

const char *
TR_DebugExt::getName(const char* s, int32_t len)
   {
   #define BUFSIZE 5
   #define MAX_STRING_SIZE 255
   static char buf[BUFSIZE][MAX_STRING_SIZE+1];
   static int32_t bi = 0;   // buf[bi] will be returned
   if (bi==BUFSIZE)
      bi = 0;

   if (s == 0 || len == 0)
      return dxGetName("(char*)", (void*) s);
   int32_t i = 0;
   char c = 1;
   if (len < 0)
      {
      while (c)
         {
         if (!dxReadField((void*)s, i, &c, sizeof(char)))
            return dxGetName("(char*)", (void*) s);
         if (i == MAX_STRING_SIZE)
            break;
         i++;
         }
      }
   else
      i = len < MAX_STRING_SIZE ? len : MAX_STRING_SIZE;
   dxReadMemory((void*)s, (void*)buf[bi], i);
   buf[bi][i] = 0;
   return buf[bi++];   // won't hurt to add extra in some cases
   #undef BUFSIZE
   #undef MAX_STRING_SIZE
   }

const char*
TR_DebugExt::getName(const char* s)
   {
   return getName(s, -1);
   }

/*
 * Printing Compilation / IR Trees
 */
void
TR_DebugExt::printDestination(TR::FILE *pOutFile, TR::TreeTop *treeTop)
   {
   TR_PrettyPrinterString output(this);
   printDestination(treeTop, output);
   trfprintf(pOutFile, "%s", output.getStr());
   _comp->incrNodeOpCodeLength( output.getLength() );
   }

void
TR_DebugExt::printDestination(TR::TreeTop *treeTop, TR_PrettyPrinterString& output)
   {
   //TR::TreeTop *localTreeTop = (TR::TreeTop*) dxMallocAndRead(sizeof(TR::TreeTop), treeTop);
   TR::TreeTop *localTreeTop = treeTop;   // _block / _branchDestination is already copied locally in printNodeInfo
   if (localTreeTop == 0)
      {
      _dbgPrintf("\n*** JIT Warning: malformed node with _branchDestination!\n");
      return;
      }
   TR::Node *node = localTreeTop->getNode();
   TR::Node *localNode = (TR::Node*) dxMallocAndRead(sizeof(TR::Node), node);
   localTreeTop->setNode(localNode);
   TR::Block *block = localNode->getBlock(true);
   TR::Block *localBlk = (TR::Block *) dxMallocAndRead(sizeof(TR::Block), block);
   localNode->setBlock(localBlk);

   TR_BlockStructure *blockStructure;
   TR_BlockStructure *localBlockStructure = NULL;

   if (_structureValid && localNode->getOpCodeValue() == TR::BBStart)
      {
      blockStructure = localBlk->getStructureOf();
      localBlockStructure = (TR_BlockStructure *) dxMallocAndRead(sizeof(TR_BlockStructure), blockStructure);
      localBlk->setStructureOf(localBlockStructure);
      }
   else
      {
      localBlk->setStructureOf(NULL);
      }

   TR_Debug::printDestination(localTreeTop, output);

   if (localNode->getOpCodeValue() == TR::BBStart)
      {
      dxFree(localBlockStructure);
      // localBlk->setStructureOf(blockStructure);
      }

   //localNode->setBlock(block);
   dxFree(localBlk);
   localTreeTop->setNode(node);
   dxFree(localNode);
   //dxFree(localTreeTop);
   }

void
TR_DebugExt::print(TR::FILE *pOutFile, TR::SymbolReference *symref)
   {
   TR_PrettyPrinterString output(this);
   TR_Debug::print(symref, output);
   trfprintf(pOutFile, "%s", output.getStr());
   }

/* the TR::Node parameter refers to a local node */
void
TR_DebugExt::printNodeInfo(TR::FILE *pOutFile, TR::Node *node)
   {
   TR_PrettyPrinterString output(this);
   TR_Debug::printNodeInfo(node, output, false);
   trfprintf(pOutFile, "%s", output.getStr());
   _comp->incrNodeOpCodeLength( output.getLength() );
   }

void
TR_DebugExt::printInlinedCallSites(TR::FILE *pOutFile, TR::ResolvedMethodSymbol * methodSymbol)
   {
   TR_Array<TR::Compilation::TR_InlinedCallSiteInfo> &localInlinedCallSites = _localCompiler->_inlinedCallSites;
   TR::Compilation::TR_InlinedCallSiteInfo *array = localInlinedCallSites._array;
   uint32_t arraySize = localInlinedCallSites.size();
   TR::Compilation::TR_InlinedCallSiteInfo *localArray = (TR::Compilation::TR_InlinedCallSiteInfo*) dxMallocAndRead(sizeof(array[0])*arraySize, array);
   localInlinedCallSites._array = localArray;

   trfprintf(pOutFile, "\nCall Stack Info\n");
   trfprintf(pOutFile, "CalleeIndex CallerIndex ByteCodeIndex CalleeMethod\n");

   for (int32_t i = 0; i < localInlinedCallSites.size(); ++i)
         {
         TR_InlinedCallSite & ics = localInlinedCallSites.element(i).site();
         TR_OpaqueMethodBlock *mb = _isAOT ? (TR_OpaqueMethodBlock*) ((TR_AOTMethodInfo *)ics._methodInfo)->resolvedMethod->getNonPersistentIdentifier() : ics._vmMethodInfo;

         trfprintf(pOutFile, "    %4d       %4d       %5d       %s !trprint j9method %p\n", i, ics._byteCodeInfo.getCallerIndex(),
                       ics._byteCodeInfo.getByteCodeIndex(), getMethodName(mb), mb);
         }

   dxFree(localArray);
   localInlinedCallSites._array = array;
   }

/*
 * Print all ILs in the given block
 */
void
TR_DebugExt::dxPrintBlockIL(TR::Block *block, seenNode **seenNodes, int32_t numBlocks)
   {
   assert(numBlocks > 0);
   if (block == 0 || !IS_4_BYTE_ALIGNED(block))
      {
      _dbgPrintf("*** JIT Error: TR::Block value 0x%p is invalid\n", block);
      return;
      }
   TR::Block * localBlock = (TR::Block *) dxMallocAndRead(sizeof(TR::Block), block);
   TR::TreeTop * firstTreeTop = localBlock->getEntry();
   TR::TreeTop * lastTreeTop = localBlock->getExit();
   _dbgPrintf("((TR::Block *)0x%p)->getEntry() = (TR::TreeTop*) 0x%p\n", block, firstTreeTop);
   _dbgPrintf("((TR::Block *)0x%p)->getExit() = (TR::TreeTop*) 0x%p\n", block, lastTreeTop);

   /*
    * print all tree tops within the block
    */
   TR_Debug::printTopLegend(TR::IO::Stdout);
   TR::TreeTop *nextTreeTop = firstTreeTop;
   int32_t numBlocksVisited = 1;
   while (nextTreeTop)
      {
      TR::TreeTop *localTreeTop = (TR::TreeTop*) dxMallocAndRead(sizeof(TR::TreeTop), (void*)nextTreeTop);
      TR::Node *node = localTreeTop->getNode();
      dxPrintNodeIL(node, seenNodes);
      if (lastTreeTop == 0)
         {
         TR::Node *localNode = (TR::Node*) dxMallocAndRead(sizeof(TR::Node), (void*)node);
         TR::Block *localBlock2 = (TR::Block *) dxMallocAndRead(sizeof(TR::Block), localNode->getBlock(true));
         lastTreeTop = localBlock2->getExit();
         dxFree(localNode);
         dxFree(localBlock2);
         }
      if (nextTreeTop != lastTreeTop)
         nextTreeTop = localTreeTop->getNextTreeTop();
      else if (numBlocksVisited == numBlocks)
         nextTreeTop = 0;
      else
         {
         nextTreeTop = localTreeTop->getNextTreeTop();
         lastTreeTop = 0;
         numBlocksVisited++;
         }
      dxFree(localTreeTop);
      /* prevent infinite recursion caused by wrong input */
      if (!IS_4_BYTE_ALIGNED(nextTreeTop))
         {
         _dbgPrintf("*** JIT Error: invalid TR::TreeTop found: nextTreeTop = 0x%p\n", nextTreeTop);
         break;
         }
      }

   TR_Debug::printBottomLegend(TR::IO::Stdout);

   dxFree(localBlock);
   }

/**
 *free nodes list that were seen while printing il (used to detect commoning
 **/
void
TR_DebugExt::freeSeenNodes(seenNode **seenNodes)
   {
   seenNode *s = *seenNodes;
   seenNode *temp;
   while(s)
      {
      temp = s;
      s = s->next;
      dxFree(temp);
      }
   }

/***
 * print Node in tree form
 * simplify from TR_Debug::print(TR::FILE *pOutFile, TR::Node * node, uint32_t indentation, bool printChildren)
 */
void
TR_DebugExt::dxPrintNodeIL(TR::Node *node, seenNode **seenNodes, int32_t indentation)
   {


   if (node == 0 || !IS_4_BYTE_ALIGNED(node))
      {
      _dbgPrintf("*** JIT Error: TR::Node value 0x%p is invalid\n", node);
      return;
      }

   TR::Node *localNode = dxAllocateLocalNode(node);
   TR::Node *localRecursiveNode = dxAllocateLocalNode(node, true);
   // Has to be done here, leads to infinite recursion if done inside dxAllocateLocalNode
   dxAllocateLocalBlockInternals(localRecursiveNode);

   TR_Debug::printBasicPreNodeInfoAndIndent(TR::IO::Stdout, localRecursiveNode, indentation);

   //find node in seenNodes list
   seenNode *temp = *seenNodes;
   while(temp && (temp->node != node))
      temp = temp->next;

   TR_PrettyPrinterString output(this);

   if(temp && temp->node == node)
      {
      output.append("==>");
      if (localRecursiveNode->getOpCode().isLoadConst())
         printNodeInfo(TR::IO::Stdout, localRecursiveNode);
      else
         output.append("%s", TR_Debug::getName(localRecursiveNode->getOpCode()));
      output.append(" at [%p]", node);
      _dbgPrintf("%s", output.getStr());

      _comp->incrNodeOpCodeLength( output.getLength() );

      TR_Debug::printBasicPostNodeInfo(TR::IO::Stdout, localRecursiveNode, indentation);
      _dbgPrintf( "\n");
      }
   else
      {
      printNodeInfo(TR::IO::Stdout, localRecursiveNode);
      printNodeFlags(TR::IO::Stdout, localRecursiveNode);
      //add the node to the list of seen nodes.
      //Note:Another option is to keep the nodes available and if a map exists,
      // we know to common - might take also long and requires more space.

      seenNode *sNode = (seenNode *)dxMalloc(sizeof(seenNode),NULL);
      sNode->node = node;
      sNode->next = *seenNodes;
      *seenNodes = sNode;
      TR_Debug::printBasicPostNodeInfo(TR::IO::Stdout, localRecursiveNode, indentation);
      _dbgPrintf("\n");

      for (int32_t i=0; i<localNode->getNumChildren(); i++)
         {
         TR::Node *child = localNode->getChild(i);
         /* prevent infinite recursion caused by wrong input */
         if (!IS_4_BYTE_ALIGNED(child))
            {
            _dbgPrintf("*** JIT Error: invalid TR::Node 0x%p found: %dth child of 0x%p\n", child, i, node);
            break;
            }
         dxPrintNodeIL(child, seenNodes, indentation+DEFAULT_INDENT_INCREMENT);
         }
      }

   dxFreeLocalBlockInternals(localRecursiveNode);
   dxFreeLocalNode(localRecursiveNode, true);
   dxFreeLocalNode(localNode);
   }


void
TR_DebugExt::dxPrintMethodIL()
   {
   seenNode *seenNodes = NULL;
   /*
    * proceed to get treetop from resolvedMethodSymbol
    */
   TR::TreeTop *firstTreeTop = _localCompiler->_methodSymbol->getFirstTreeTop();

   /*
    * print all tree tops within method
    */
   printInlinedCallSites(TR::IO::Stdout, NULL);
   TR_Debug::printTopLegend(TR::IO::Stdout);
   TR::TreeTop *nextTreeTop = firstTreeTop;
   while (nextTreeTop)
      {
      TR::TreeTop *localTreeTop = (TR::TreeTop*) dxMallocAndRead(sizeof(TR::TreeTop), (void*)nextTreeTop);
      dxPrintNodeIL(localTreeTop->getNode(),&seenNodes);
      nextTreeTop = localTreeTop->getNextTreeTop();
      dxFree(localTreeTop);
      /* prevent infinite recursion caused by wrong input */
      if (!IS_4_BYTE_ALIGNED(nextTreeTop))
         {
         _dbgPrintf("*** JIT Error: invalid TR::TreeTop found: nextTreeTop = 0x%p\n", nextTreeTop);
         break;
         }
      }
   TR_Debug::printBottomLegend(TR::IO::Stdout);

   freeSeenNodes(&seenNodes);
   }

void
TR_DebugExt::dxPrintInlinedCallSites(TR::ResolvedMethodSymbol *methodSymbol)
   {
   printInlinedCallSites(TR::IO::Stdout, methodSymbol);
   }

void
TR_DebugExt::dxPrintCompilationIL()
   {
   if (_remoteCompiler == 0 || _localCompiler == 0)
      {
      _dbgPrintf("*** JIT Error: TR::Compilation is NULL\n");
      return;
      }

   _dbgPrintf("\nThis method is %s\n", _localCompiler->getHotnessName(_localCompiler->getMethodHotness()));
   dxPrintMethodIL();
   }

/*
 * Verifying IR Trees
 */
void
TR_DebugExt::dxVerifyTrees()
   {
   TR::ResolvedMethodSymbol *remoteMethodSymbol = Compilation2ResolvedMethodSymbol();
   verifyTrees(remoteMethodSymbol);
   }

void
TR_DebugExt::verifyTrees(TR::ResolvedMethodSymbol *s)
   {
   _dbgPrintf("Not implemented\n");
   }


/*
 * Verifying Blocks
 */
void
TR_DebugExt::dxVerifyBlocks()
   {
   TR::ResolvedMethodSymbol *remoteMethodSymbol = Compilation2ResolvedMethodSymbol();
   verifyBlocks(remoteMethodSymbol);
   }

void
TR_DebugExt::verifyBlocks(TR::ResolvedMethodSymbol *s)
   {
   _dbgPrintf("Not implemented\n");
   }

/*
 * Verifying CFG
 */
void
TR_DebugExt::dxVerifyCFG(TR::CFG *cfg)
   {
   TR::CFG *localCfg = newCFG(cfg);
   TR_CFGCheckerExt *checker = new ((TR_Malloc_t) _dbgMalloc) TR_CFGCheckerExt(localCfg, TR::IO::Stdout);
   checker->check();
   free(checker);
   freeCFG(localCfg);
   }

/*
 * Printing StructureSubgraph
 */
void
TR_DebugExt::print(TR::FILE *pOutFile, TR_Structure * structure, uint32_t indentation)
   {
   /*
    * we are interested in printing RegionStructure only
    */
#ifdef TRPRINT_STRUCTURE_SUBGRAPH
   print(pOutFile, (TR_RegionStructure*) structure, 0); // force downcast to avoid virtual call asRegion()
#else
   _dbgPrintf("   <StructureSubGraph is not printed>\n");
#endif
   }

#ifdef TRPRINT_STRUCTURE_SUBGRAPH
void
TR_DebugExt::print(TR::FILE *pOutFile, TR_RegionStructure * regionStructure, uint32_t indentation)
   {
   TR_RegionStructure *localRegionStructure = (TR_RegionStructure*) dxMallocAndRead(sizeof(TR_RegionStructure), regionStructure);
   TR_RegionStructure *versionedLoop = localRegionStructure->getVersionedLoop();
   if (versionedLoop)
      localRegionStructure->setVersionedLoop(( TR_RegionStructure *) dxMallocAndRead(sizeof(TR_RegionStructure), versionedLoop));
   TR_StructureSubGraphNode *entryNode = localRegionStructure->getEntry();
   TR_StructureSubGraphNode *localEntryNode = (TR_StructureSubGraphNode *) dxMallocAndRead(sizeof(TR_StructureSubGraphNode), entryNode);
   TR_Structure *structure = localEntryNode->getStructure();
   TR_Structure *localStructure = (TR_Structure *) dxMallocAndRead(sizeof(TR_Structure), structure);
   localEntryNode->setStructure(localStructure);
   localRegionStructure->setEntry(localEntryNode);

   TR_Debug::print(pOutFile, localRegionStructure, indentation);

   dxFree(localStructure);
   dxFree(localEntryNode);
   if (versionedLoop)
      dxFree(localRegionStructure->getVersionedLoop());
   dxFree(localRegionStructure);
   }

/*
 * Override TR_Debug::printSubGraph
 */
void
TR_DebugExt::printSubGraph(TR::FILE *pOutFile, TR_RegionStructure * regionStructure, uint32_t indentation)
   {
   trfprintf(pOutFile, "%*s printSubGraph not printed!\n", indentation, " ");
   }

/*
 * Override TR_Debug::print()
 */
void
TR_DebugExt::print(TR::FILE *pOutFile, TR_InductionVariable * inductionVariable, uint32_t indentation)
   {
   trfprintf(pOutFile, "%*s Induction Variable Info not printed!\n", indentation, " ");
   return;
   }
#endif

void
TR_DebugExt::print(TR::FILE *pOutFile, TR::Block * block, uint32_t indentation)
   {
   TR::TreeTop *tt = block->getEntry();
   if (tt)
      block->setEntry((TR::TreeTop*) dxMallocAndRead(sizeof(TR::TreeTop), tt));

   TR_Debug::print(pOutFile, block, indentation);

   if (tt)
      {
      dxFree(block->getEntry());
      block->setEntry(tt);
      }
   }

void
TR_DebugExt::print(TR::FILE *pOutFile, TR::CFGNode * cfgNode, uint32_t indentation)
   {
   print(pOutFile, (TR::Block *)cfgNode, 0); // force downcast to avoid virtual call asBlock()
   }

TR::CFG*
TR_DebugExt::newCFG(TR::CFG *cfg)
   {
   TR::CFG *localCfg = (TR::CFG*) dxMallocAndRead(sizeof(TR::CFG), cfg);

   TR_LinkHead1<TR::CFGNode> &localNodes = localCfg->getNodes();
   TR::CFGNode *firstNode = localNodes.getFirst();
   int32_t cfgNodeSize = std::max(sizeof(TR::Block), sizeof(TR_StructureSubGraphNode));  // explore all sub-classes possibilities
   TR::CFGNode *localFirstNode = firstNode ? (TR::CFGNode *) dxMallocAndRead(cfgNodeSize, firstNode) : 0;
   localNodes.setFirst(localFirstNode);

   TR::CFGNode *localNode = localFirstNode;
   while (localNode && localNode->_next)
      {
      TR::CFGNode *nextNode = localNode->_next;
      TR::CFGNode *localNextNode = (TR::CFGNode *) dxMallocAndRead(cfgNodeSize, nextNode);

      while (!localNextNode->isValid())
         {
         TR::CFGNode * temp = localNextNode;
         nextNode = localNextNode->_next;
         localNextNode = (TR::CFGNode *) dxMallocAndRead(cfgNodeSize, nextNode);
         dxFree(temp);
         }

      localNode->setNext(localNextNode);
      localNode = localNextNode;
      }

   return localCfg;
   }

void
TR_DebugExt::freeCFG(TR::CFG *localCfg)
   {
   TR::CFGNode *localFirstNode = localCfg->getNodes().getFirst();
   if (localFirstNode)
      {
      TR::CFGNode *localNode = localFirstNode;
      while (localNode->getNext())
         {
         TR::CFGNode *localNextNode = localNode->getNext();
         dxFree(localNode);
         localNode = localNextNode;
         }
      dxFree(localNode);
      }

   dxFree(localCfg);
   }

/*
 * modified from TR_Debug::print(TR::FILE *pOutFile, TR::CFG * cfg)
 */
void
TR_DebugExt::dxPrintCFG(TR::CFG *cfg)
   {
   // This does not work and will crash
   // There are too many virtual functions involved in printing out the CFG
   // and it is hard to determine what the right call should be.
   _dbgPrintf("Not implemented\n");
   return;

   if (cfg == 0)
      {
      _dbgPrintf("*** JIT Error: cfg is NULL\n");
      return;
      }

   TR::CFG *localCfg = newCFG(cfg);
   //TR_Debug::print(TR::IO::Stdout, localCfg);

   int32_t     numNodes = 0;
   int32_t     index;
   TR::CFGNode *node;

   // If the depth-first numbering for the blocks has already been done, use this
   // numbering to order the blocks. Otherwise use a reasonable ordering.
   //
   for (node = localCfg->getFirstNode(); node; node = node->getNext())
      {
      index = node->getNumber();
      if (index < 0)
         numNodes++;
      else if (index >= numNodes)
         numNodes = index+1;
      }

   TR::CFGNode **array;
   array = (TR::CFGNode**)(((TR_InternalFunctionsExt *)_jit)->stackAlloc(numNodes*sizeof(TR::CFGNode*)));
   memset(array, 0, numNodes*sizeof(TR::CFGNode*));
   index = numNodes;

   for (node = localCfg->getFirstNode(); node; node = node->getNext())
      {
      int32_t nodeNum = node->getNumber();
      array[nodeNum>=0?nodeNum:--index] = node;
      }

   trfprintf(TR::IO::Stdout, "\n<cfg>\n");

   for (index = 0; index < numNodes; index++)
      if (array[index] != NULL)
         TR_Debug::print(TR::IO::Stdout, array[index], 6);

   if (localCfg->getStructure())
      {
      trfprintf(TR::IO::Stdout, "<structure>\n");
      TR_Debug::print(TR::IO::Stdout, localCfg->getStructure(), 6);
      trfprintf(TR::IO::Stdout, "</structure>");
      }
   trfprintf(TR::IO::Stdout, "\n</cfg>\n");

   ((TR_InternalFunctionsExt *)_jit)->free(array);
   freeCFG(localCfg);
   }

/*
 * Printing a single block
 */
void
TR_DebugExt::dxPrintBlockCFG(TR::Block *block)
   {
   if (block == 0)
      {
      _dbgPrintf("*** JIT Error: block is NULL\n");
      return;
      }
   TR::Block *localBlock = (TR::Block *) dxMallocAndRead(sizeof(TR::Block), block);
   print(TR::IO::Stdout, localBlock, 0);   // call TR_DebugExt's version
   dxFree(localBlock);
   }

void
TR_DebugExt::dxPrintPersistentJittedBodyInfo(TR_PersistentJittedBodyInfo *bodyInfo)
   {
   if (bodyInfo == 0)
      {
      _dbgPrintf("*** JIT Error: PersistentJittedBodyInfo is NULL\n");
      return;
      }

   TR_PersistentJittedBodyInfo * localBodyInfo = (TR_PersistentJittedBodyInfo *) dxMallocAndRead(sizeof(TR_PersistentJittedBodyInfo), bodyInfo);
   if (localBodyInfo)
      {
      _dbgPrintf("TR_PersistentJittedBodyInfo at 0x%p\n", bodyInfo);
      _dbgPrintf("\tint32_t                   _counter = %d\n", localBodyInfo->_counter);
      _dbgPrintf("\tTR_PersistentMethodInfo * _methodInfo = !trprint persistentmethodinfo 0x%p\n", localBodyInfo->_methodInfo);
 #ifdef FASTWALK
      _dbgPrintf("\tvoid                    * _mapTable = 0x%p\n", localBodyInfo->_mapTable);
 #endif // def FASTWALK
      _dbgPrintf("\tint32_t                   _startCount = %d\n", localBodyInfo->_startCount);
      _dbgPrintf("\tuint16_t                  _hotStartCountDelta = %u\n", localBodyInfo->_hotStartCountDelta);
      _dbgPrintf("\tuint16_t                  _oldStartCountDelta = %u\n", localBodyInfo->_oldStartCountDelta);
      _dbgPrintf("\tflags16_t                 _flags = 0x%04x\n", localBodyInfo->_flags.getValue());
      _dbgPrintf("\tint8_t                    _sampleIntervalCount = %d\n", localBodyInfo->_sampleIntervalCount);
      _dbgPrintf("\tuint8_t                   _aggressiveRecompilationChances = %d\n", localBodyInfo->_aggressiveRecompilationChances);
      _dbgPrintf("\tTR_Hotness                _hotness = %d (%s)\n", localBodyInfo->_hotness, localBodyInfo->_hotness == -1 ? "unknown" : comp()->getHotnessName(localBodyInfo->_hotness));
      _dbgPrintf("\tuint8_t                   _numScorchingIntervals = %d\n", localBodyInfo->_numScorchingIntervals);
      _dbgPrintf("\tbool                      _isInvalidated = %d\n", localBodyInfo->_isInvalidated);
      _dbgPrintf("\tDetails of flags:\n");
      _dbgPrintf("\t\tHasLoops                  =%d\n", localBodyInfo->getHasLoops());
      _dbgPrintf("\t\tUsesPreexistence          =%d\n", localBodyInfo->getUsesPreexistence());
      _dbgPrintf("\t\tDisableSampling           =%d\n", localBodyInfo->getDisableSampling());
      _dbgPrintf("\t\tIsProfilingBody           =%d\n", localBodyInfo->getIsProfilingBody());
      _dbgPrintf("\t\tIsAotedBody               =%d\n", localBodyInfo->getIsAotedBody());
      _dbgPrintf("\t\tSamplingRecomp            =%d\n", localBodyInfo->getSamplingRecomp());
      _dbgPrintf("\t\tIsPushedForRecompilation  =%d\n", localBodyInfo->getIsPushedForRecompilation());
      _dbgPrintf("\t\tFastHotRecompilation      =%d\n", localBodyInfo->getFastHotRecompilation());
      _dbgPrintf("\t\tFastScorchingRecompilation=%d\n", localBodyInfo->getFastScorchingRecompilation());
      _dbgPrintf("\t\tUsesGCR                   =%d\n", localBodyInfo->getUsesGCR());
      dxFree(localBodyInfo);
      }
   else
      {
      _dbgPrintf("*** JIT Error: Cannot read memory at 0x%p\n", bodyInfo);
      }
   }

void*
TR_DebugExt::dxGetJ9MethodFromMethodToBeCompiled(TR_MethodToBeCompiled *methodToBeCompiled)
   {
   TR_MethodToBeCompiled *localMethodBeingCompiled = (TR_MethodToBeCompiled *) dxMallocAndRead(sizeof(TR_MethodToBeCompiled), methodToBeCompiled);

   if (localMethodBeingCompiled)
      {
      TR::IlGeneratorMethodDetails & localDetails = localMethodBeingCompiled->getMethodDetails();
      J9Method *method = localDetails.getMethod();
      dxFree (localMethodBeingCompiled);
      return (void *) method;
      }

   dxFree (localMethodBeingCompiled);
   return NULL;
   }

void
TR_DebugExt::dxPrintMethodsBeingCompiled(TR::CompilationInfo *remoteCompInfo)
   {
   if (remoteCompInfo == 0)
      {
      _dbgPrintf("*** JIT Error: compInfo is NULL\n");
      return;
      }

   TR::CompilationInfoPerThread ** arrayOfCompInfoPT = NULL;
   uint8_t numThreads = MAX_TOTAL_COMP_THREADS;

   arrayOfCompInfoPT = dxMallocAndGetArrayOfCompilationInfoPerThread(numThreads, remoteCompInfo->_arrayOfCompilationInfoPerThread);

   for (uint8_t i = 0; i < numThreads; i++)
      {
      if (arrayOfCompInfoPT[i])
         {
         TR::CompilationInfoPerThread *localCompInfoPT = (TR::CompilationInfoPerThread *) dxMallocAndRead(sizeof(TR::CompilationInfoPerThread), arrayOfCompInfoPT[i]);

         struct J9Method *ramMethod = (struct J9Method *) dxGetJ9MethodFromMethodToBeCompiled(localCompInfoPT->_methodBeingCompiled);

         if (ramMethod)
            {
            _dbgPrintf("Currently compiling: !trprint j9method 0x%p\n", ramMethod);
            _dbgPrintf("\tAssociated TR_MethodToBeCompiled: !trprint methodtobecompiled 0x%p\n", localCompInfoPT->_methodBeingCompiled);
            _dbgPrintf("\tAssociated TR::CompilationInfoPerThread: !trprint compilationinfoperthread 0x%p\n\n", arrayOfCompInfoPT[i]);
            }

         if (localCompInfoPT) dxFree(localCompInfoPT);
         }
      }
   if (arrayOfCompInfoPT) dxFree(arrayOfCompInfoPT);
   }

void TR_DebugExt::dxPrintCompilationInfo(TR::CompilationInfo *remoteCompInfo)
   {
   TR::CompilationInfo *localCompInfo = NULL;

   if (remoteCompInfo == NULL)
      remoteCompInfo = _remoteCompInfo;

   if (remoteCompInfo != _remoteCompInfo)
      localCompInfo = (TR::CompilationInfo*) dxMallocAndRead(sizeof(TR::CompilationInfo), remoteCompInfo);
   else
      localCompInfo = _localCompInfo;

   if (localCompInfo)
      {
      _dbgPrintf("TR::CompilationInfoPerThread * const * _arrayOfCompilationInfoPerThread = !trprint arrayofcompilationinfoperthread 0x%p\n", remoteCompInfo->_arrayOfCompilationInfoPerThread);
      _dbgPrintf("TR::CompilationInfoPerThreadBase *     _compInfoForCompOnAppThread      = !trprint compilationinfoperthreadbase 0x%p\n",localCompInfo->_compInfoForCompOnAppThread);
      _dbgPrintf("TR_MethodToBeCompiled *               _methodQueue                     = !trprint methodtobecompiled 0x%p\n", localCompInfo->_methodQueue);
      _dbgPrintf("TR_MethodToBeCompiled *               _methodPool                      = !trprint methodtobecompiled 0x%p\n", localCompInfo->_methodPool);
      _dbgPrintf("J9JITConfig *                         _jitConfig                       = 0x%p\n", localCompInfo->_jitConfig);
      _dbgPrintf("TR_PersistentMemory *                 _persistentMemory                = !trprint persistentmemory 0x%p\n", localCompInfo->_persistentMemory);
      _dbgPrintf("TR::Monitor *                        _compilationMonitor              = 0x%p\n", localCompInfo->_compilationMonitor);
      _dbgPrintf("J9::RWMonitor *                   _classUnloadMonitor              = 0x%p\n", localCompInfo->_classUnloadMonitor);
      _dbgPrintf("TR::Monitor *                        _logMonitor                      = 0x%p\n", localCompInfo->_logMonitor);
      _dbgPrintf("TR::Monitor *                        _schedulingMonitor               = 0x%p\n", localCompInfo->_schedulingMonitor);
   #if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
      _dbgPrintf("TR::Monitor *                        _dltMonitor                      = 0x%p\n", localCompInfo->_dltMonitor);
      _dbgPrintf("struct DLT_record *                   _freeDLTRecord                   = 0x%p\n", localCompInfo->_freeDLTRecord);
      _dbgPrintf("struct DLT_record *                   _dltHash[DLT_HASHSIZE]           = 0x%p\n", localCompInfo->_dltHash);
   #endif
      _dbgPrintf("TR::Monitor *                        _vlogMonitor                     = 0x%p\n", localCompInfo->_vlogMonitor);
      _dbgPrintf("TR::Monitor *                        _rtlogMonitor                    = 0x%p\n", localCompInfo->_rtlogMonitor);
      _dbgPrintf("TR::Monitor *                        _applicationThreadMonitor        = 0x%p\n", localCompInfo->_applicationThreadMonitor);
      _dbgPrintf("TR::MonitorTable *                   _j9MonitorTable                  = 0x%p\n", localCompInfo->_j9MonitorTable);
      _dbgPrintf("TR_LinkHead0<TR_ClassHolder>          _classesToCompileList            = 0x%p\n", &(localCompInfo->_classesToCompileList));
      _dbgPrintf("CpuUtilization *                      _cpuUtil                         = 0x%p\n", localCompInfo->_cpuUtil);
      _dbgPrintf("J9VMThread *                          _samplerThread                   = 0x%p\n", localCompInfo->_samplerThread);
      _dbgPrintf("int32_t                               _methodPoolSize                  = %d\n", localCompInfo->_methodPoolSize);
      _dbgPrintf("IDATA                                 _numSyncCompilations             = %d\n", localCompInfo->_numSyncCompilations);
      _dbgPrintf("IDATA                                 _numAsyncCompilations            = %d\n", localCompInfo->_numAsyncCompilations);
      _dbgPrintf("int32_t                               _numCompThreadsActive            = %d\n", localCompInfo->_numCompThreadsActive);
      _dbgPrintf("int32_t                               _numCompThreadsJobless           = %d\n", localCompInfo->_numCompThreadsJobless);
      _dbgPrintf("int32_t                               _numCompThreadsCompilingHotterMethods= %d\n", localCompInfo->_numCompThreadsCompilingHotterMethods);
      _dbgPrintf("int32_t                               _numQueuedMethods                = %d\n", localCompInfo->_numQueuedMethods);
      _dbgPrintf("int32_t                               _numQueuedFirstTimeCompilations  = %d\n", localCompInfo->_numQueuedFirstTimeCompilations);
      _dbgPrintf("int32_t                               _queueWeight                     = %d\n", localCompInfo->_queueWeight);

      _dbgPrintf("TR_MethodToBeCompiled *               _firstLPQentry                   = !trprint methodtobecompiled 0x%p\n", localCompInfo->_lowPriorityCompilationScheduler._firstLPQentry);
      _dbgPrintf("TR_MethodToBeCompiled *               _lastLPQentry                    = !trprint methodtobecompiled 0x%p\n", localCompInfo->_lowPriorityCompilationScheduler._lastLPQentry);
      _dbgPrintf("int32_t                               _sizeLPQ                         = %d\n", localCompInfo->_lowPriorityCompilationScheduler._sizeLPQ);
      _dbgPrintf("int32_t                               _LPQWeight                       = %d\n", localCompInfo->_lowPriorityCompilationScheduler._LPQWeight);
      _dbgPrintf("int32_t                               _spine                           = 0x%p\n", localCompInfo->_lowPriorityCompilationScheduler._spine);
      _dbgPrintf("uint32_t                              _STAT_compReqQueuedByIProfiler   = %u\n", localCompInfo->_lowPriorityCompilationScheduler._STAT_compReqQueuedByIProfiler);
      _dbgPrintf("uint32_t                              _STAT_conflict                   = %u\n", localCompInfo->_lowPriorityCompilationScheduler._STAT_conflict);
      _dbgPrintf("uint32_t                              _STAT_staleScrubbed              = %u\n", localCompInfo->_lowPriorityCompilationScheduler._STAT_staleScrubbed);
      _dbgPrintf("uint32_t                              _STAT_bypass                     = %u\n", localCompInfo->_lowPriorityCompilationScheduler._STAT_bypass);
      _dbgPrintf("uint32_t                              _STAT_compReqQueuedByJIT         = %u\n", localCompInfo->_lowPriorityCompilationScheduler._STAT_compReqQueuedByJIT);
      _dbgPrintf("uint32_t                              _STAT_LPQcompFromIprofiler       = %u\n", localCompInfo->_lowPriorityCompilationScheduler._STAT_LPQcompFromIprofiler);
      _dbgPrintf("uint32_t                              _STAT_LPQcompFromInterpreter     = %u\n", localCompInfo->_lowPriorityCompilationScheduler._STAT_LPQcompFromInterpreter);
      _dbgPrintf("uint32_t                              _STAT_LPQcompUpgrade             = %u\n", localCompInfo->_lowPriorityCompilationScheduler._STAT_LPQcompUpgrade);
      _dbgPrintf("uint32_t                              _STAT_compReqQueuedByInterpreter = %u\n", localCompInfo->_lowPriorityCompilationScheduler._STAT_compReqQueuedByInterpreter);
      _dbgPrintf("uint32_t                              _STAT_numFailedToEnqueueInLPQ    = %u\n", localCompInfo->_lowPriorityCompilationScheduler._STAT_numFailedToEnqueueInLPQ);
      _dbgPrintf("int32_t                               _idleThreshold                   = %d\n", localCompInfo->_idleThreshold);
      _dbgPrintf("int32_t                               _compilationBudget               = %d\n", localCompInfo->_compilationBudget);
      _dbgPrintf("int32_t                               _samplerState                    = %d\n", localCompInfo->_samplerState);
      _dbgPrintf("int32_t                               _samplingThreadLifetimeState     = %d\n", localCompInfo->_samplingThreadLifetimeState);
      _dbgPrintf("int32_t                               _numMethodsFoundInSharedCache    = %d\n", localCompInfo->_numMethodsFoundInSharedCache);
      _dbgPrintf("int32_t                               _numInvRequestsInCompQueue       = %d\n", localCompInfo->_numInvRequestsInCompQueue);
      _dbgPrintf("int32_t                               _compThreadIndex                 = %d\n", localCompInfo->_compThreadIndex);
      _dbgPrintf("int32_t                               _numDiagnosticThreads            = %d\n", localCompInfo->_numDiagnosticThreads);
      _dbgPrintf("int32_t                               _numSeriousFailures              = %d\n", localCompInfo->_numSeriousFailures);
      _dbgPrintf("int32_t[numHotnessLevels]             _statsOptLevels                  = 0x%p\n", localCompInfo->_statsOptLevels);
      _dbgPrintf("TR_StatsEvents<compilationMaxError>   statCompErrors                   = 0x%p\n", &(localCompInfo->statCompErrors));
      _dbgPrintf("uint32_t                              _statNumAotedMethods             = %u\n", localCompInfo->_statNumAotedMethods);
      _dbgPrintf("uint32_t                              _statNumMethodsFromSharedCache   = %u\n", localCompInfo->_statNumMethodsFromSharedCache);
      _dbgPrintf("uint32_t                              _statNumAotedMethodsRecompiled   = %u\n", localCompInfo->_statNumAotedMethodsRecompiled);
      _dbgPrintf("uint32_t                              _statNumAotedMethodsRecompiled   = %u\n", localCompInfo->_statNumAotedMethodsRecompiled);
      _dbgPrintf("uint32_t                              _statNumJNIMethodsCompiled       = %u\n", localCompInfo->_statNumJNIMethodsCompiled);
      _dbgPrintf("uint32_t                              _statNumPriorityChanges          = %u\n", localCompInfo->_statNumPriorityChanges);
      _dbgPrintf("uint32_t                              _statNumYields                   = %u\n", localCompInfo->_statNumYields);
      _dbgPrintf("uint32_t                              _statNumUpgradeInterpretedMethod = %u\n", localCompInfo->_statNumUpgradeInterpretedMethod);
      _dbgPrintf("uint32_t                              _statNumDowngradeInterpretedMethod = %u\n", localCompInfo->_statNumDowngradeInterpretedMethod);
      _dbgPrintf("uint32_t                              _statNumUpgradeJittedMethod      = %u\n", localCompInfo->_statNumUpgradeJittedMethod);
      _dbgPrintf("uint32_t                              _statNumQueuePromotions          = %u\n", localCompInfo->_statNumQueuePromotions);
      _dbgPrintf("uint32_t                              _statTotalAotQueryTime           = %u\n", localCompInfo->_statTotalAotQueryTime);
      _dbgPrintf("uint32_t                              _numberBytesReadInaccessible     = %u\n", localCompInfo->_numberBytesReadInaccessible);
      _dbgPrintf("uint32_t                              _numberBytesWriteInaccessible    = %u\n", localCompInfo->_numberBytesWriteInaccessible);
      _dbgPrintf("uint64_t                              _lastReqStartTime                = %llu\n", localCompInfo->_lastReqStartTime);
      _dbgPrintf("uint64_t                              _lastCompilationsShouldBeInterruptedTime= %llu\n", localCompInfo->_lastCompilationsShouldBeInterruptedTime);
      _dbgPrintf("bool                                  _compBudgetSupport               = %s\n", localCompInfo->_compBudgetSupport ? "true" : "false");
      _dbgPrintf("bool                                  _rampDownMCT                     = %s\n", localCompInfo->_rampDownMCT ? "true" : "false");
      _dbgPrintf("flags32_t                             _flags                           = %u\n", localCompInfo->_flags.getValue());
   #ifdef DEBUG
      _dbgPrintf("bool                                  _traceCompiling                  = %s\n", localCompInfo->_traceCompiling ? "true" : "false");
   #endif

      dxPrintMethodsBeingCompiled(remoteCompInfo);
      }
   else
      {
      _dbgPrintf("*** JIT Error: compinfo is NULL\n");
      }


   if (remoteCompInfo && remoteCompInfo != _remoteCompInfo && localCompInfo)
      dxFree(localCompInfo);
   }

void TR_DebugExt::dxPrintCompilationInfoPerThreadBase(TR::CompilationInfoPerThreadBase *remoteCompInfoPTB)
   {
   if (remoteCompInfoPTB == 0)
      {
      _dbgPrintf("*** JIT Error: remoteCompInfoPTB is NULL\n");
      return;
      }

   TR::CompilationInfoPerThreadBase *localCompInfoPTB = (TR::CompilationInfoPerThreadBase *)dxMallocAndRead(sizeof(TR::CompilationInfoPerThreadBase), remoteCompInfoPTB);

   _dbgPrintf("\n\tcompilationinfoperthreadbase at 0x%p\n", remoteCompInfoPTB);
   _dbgPrintf("\tTR::CompilationInfo *               _compInfo = !trprint compilationinfo 0x%p\n",&localCompInfoPTB->_compInfo);
   _dbgPrintf("\tJ9JITConfig *                      _jitConfig = 0x%p\n",localCompInfoPTB->_jitConfig);
   _dbgPrintf("\tTR_MethodToBeCompiled *            _methodBeingCompiled = !trprint methodtobecompiled 0x%p\n",localCompInfoPTB->_methodBeingCompiled);
   _dbgPrintf("\tTR::Compilation *                  _compiler = !trprint compilation 0x%p\n",localCompInfoPTB->_compiler);
   _dbgPrintf("TR_MethodMetaData *                  _metadata = 0x%p\n",localCompInfoPTB->_metadata);
   _dbgPrintf("\tvolatile CompilationThreadState    _compInfo = 0x%d\n",localCompInfoPTB->_compilationThreadState);
   _dbgPrintf("\tTR_DataCache *                     _reservedDataCache = !trprint datacache 0x%p\n",localCompInfoPTB->_reservedDataCache);
   _dbgPrintf("\tint32_t                            _compThreadId = 0x%d\n",localCompInfoPTB->_compThreadId);
   _dbgPrintf("\tbool                               _compilationShouldBeInterrupted = 0x%d\n",localCompInfoPTB->_compilationShouldBeInterrupted);

   dxFree(localCompInfoPTB);
   }

void TR_DebugExt::dxPrintCompilationInfoPerThread(TR::CompilationInfoPerThread *remoteCompInfoPT)
   {
   if (remoteCompInfoPT == 0)
      {
      _dbgPrintf("*** JIT Error: compInfoPT is NULL\n");
      return;
      }

   TR::CompilationInfoPerThread *localCompInfoPT = (TR::CompilationInfoPerThread *) dxMallocAndRead(sizeof(TR::CompilationInfoPerThread), remoteCompInfoPT);

   _dbgPrintf("\n\tcompilationInfoPerThread at 0x%p\n", remoteCompInfoPT);
   _dbgPrintf("\tTR::CompilationInfo *       _compInfo = !trprint compilationinfo 0x%p\n", &localCompInfoPT->_compInfo);
   _dbgPrintf("\tTR_MethodToBeCompiled *    _methodBeingCompiled = !trprint methodtobecompiled 0x%p\n",localCompInfoPT->_methodBeingCompiled);
   _dbgPrintf("\tJ9JITConfig *              _jitConfig = 0x%p\n",localCompInfoPT->_jitConfig);
   _dbgPrintf("\tTR::Compilation *          _compiler = !trprint compilation 0x%p\n",localCompInfoPT->_compiler);
   _dbgPrintf("\tTR_MethodMetaData *        _metadata = 0x%p\n",localCompInfoPT->_metadata);
   _dbgPrintf("\tCompilationThreadState     _compilationThreadState = 0x%d\n",localCompInfoPT->_compilationThreadState);
   _dbgPrintf("\tTR_DataCache *             _reservedDataCache = !trprint datacache 0x%p\n",localCompInfoPT->_reservedDataCache);
   _dbgPrintf("\tint32_t                    _compThreadId = 0x%d\n",localCompInfoPT->_compThreadId);
   _dbgPrintf("\tbool                       _compilationShouldBeInterrupted = 0x%d\n",localCompInfoPT->_compilationShouldBeInterrupted);
   _dbgPrintf("\tj9thread_t                 _osThread = 0x%x\n",localCompInfoPT->_osThread);
   _dbgPrintf("\tJ9VMThread *               _compilationThread = 0x%p\n",localCompInfoPT->_compilationThread);
   _dbgPrintf("\tint32_t                    _compThreadPriority = 0x%p\n",localCompInfoPT->_compThreadPriority);
   _dbgPrintf("\tTR::Monitor *             _compThreadMonitor = 0x%p\n",localCompInfoPT->_compThreadMonitor);

   dxFree(localCompInfoPT);
   }

void TR_DebugExt::dxPrintMethodToBeCompiled(TR_MethodToBeCompiled *remoteCompEntry)
   {
 if (remoteCompEntry == 0)
      {
      _dbgPrintf("*** JIT Error: compEntry is NULL\n");
      return;
      }

    TR_MethodToBeCompiled *localCompEntry = (TR_MethodToBeCompiled *) dxMallocAndRead(sizeof(TR_MethodToBeCompiled), remoteCompEntry);

   _dbgPrintf("\n\tTR_MethodToBeCompiled at 0x%p\n", remoteCompEntry);
   _dbgPrintf("\tTR_MethodToBeCompiled *       _next = !trprint methodtobecompiled 0x%p\n", localCompEntry->_next);
   _dbgPrintf("\tvoid *                        _oldStartPC = 0x%p\n",localCompEntry->_oldStartPC);
   _dbgPrintf("\tvoid *                        _newStartPC = 0x%p\n",localCompEntry->_newStartPC);
   _dbgPrintf("\tTR::Monitor *                  _monitor = 0x%p\n",localCompEntry->_monitor);
   _dbgPrintf("\tchar *                        _monitorName = 0x%p\n",localCompEntry->_monitorName);
   _dbgPrintf("\tTR_OptimizationPlan *         _optimizationPlan = !trprint optimizationplan 0x%p\n",localCompEntry->_optimizationPlan);
   _dbgPrintf("\tuint64_t                      _entryTime = %llu\n",localCompEntry->_entryTime);
   _dbgPrintf("\tTR::CompilationInfoPerThread * _compInfoPT = 0x%p\n",localCompEntry->_compInfoPT);
   _dbgPrintf("\tuint16_t                      _priority = 0x%x\n",localCompEntry->_priority);
   _dbgPrintf("\tint16_t                       _numThreadsWaiting = %d\n",localCompEntry->_numThreadsWaiting);
   _dbgPrintf("\tint8_t                        _compilationAttemptsLeft = %d\n",localCompEntry->_compilationAttemptsLeft);
   _dbgPrintf("\tint8_t                        _compErrCode = 0x%x\n",localCompEntry->_compErrCode);
   _dbgPrintf("\tTR_YesNoMaybe                 _methodIsInSharedCache = %d\n",localCompEntry->_methodIsInSharedCache);
   _dbgPrintf("\tbool                          _unloadedMethod = %d\n",localCompEntry->_unloadedMethod);
   _dbgPrintf("\tbool                          _useAotCompilation = %d\n",localCompEntry->_useAotCompilation);
   _dbgPrintf("\tbool                          _doNotUseAotCodeFromSharedCache = %d\n",localCompEntry->_doNotUseAotCodeFromSharedCache);
   _dbgPrintf("\tbool                          _tryCompilingAgain = %d\n",localCompEntry->_tryCompilingAgain);
   _dbgPrintf("\tbool                          _async = %d\n",localCompEntry->_async);
   _dbgPrintf("\tbool                          _reqFromSecondaryQueue = %d\n",localCompEntry->_reqFromSecondaryQueue);
   _dbgPrintf("\tbool                          _reqFromJProfilingQueue = %d\n", localCompEntry->_reqFromJProfilingQueue);
   _dbgPrintf("\tbool                          _changedFromAsyncToSync = %d\n",localCompEntry->_changedFromAsyncToSync);
   _dbgPrintf("\tbool                          _entryShouldBeDeallocated = %d\n",localCompEntry->_entryShouldBeDeallocated);
   _dbgPrintf("\tint16_t                       _index = %d\n",localCompEntry->_index);
   _dbgPrintf("\tbool                          _freeTag = %d\n",localCompEntry->_freeTag);
   _dbgPrintf("\tuint8_t                       _weight = %u\n",localCompEntry->_weight);
   _dbgPrintf("\tbool                          _hasIncrementedNumCompThreadsCompilingHotterMethods = %d\n\n",localCompEntry->_hasIncrementedNumCompThreadsCompilingHotterMethods);

   struct J9Method *ramMethod = (struct J9Method *)dxGetJ9MethodFromMethodToBeCompiled(remoteCompEntry);
   if (ramMethod)
      _dbgPrintf("\tAssociated J9Method = !trprint j9method 0x%p\n", ramMethod);

   dxFree(localCompEntry);
   }

 /*
  * Print the content of the compilation tracing buffer
  */
void
TR_DebugExt::dxPrintCompilationTracingBuffer()
   {
   // First find compilation info to extract the address of the tracing buffer
   if (_remoteCompInfo && _localCompInfo)
      {
      _dbgPrintf("*** JIT Info: compilationInfo=0x%p\n", _remoteCompInfo);
      TR::CompilationTracingFacility *localCompTracingFacility = &_localCompInfo->_compilationTracingFacility;
      _dbgPrintf("*** JIT Info: compilationTracingFacility struct at 0x%p\n", &_remoteCompInfo->_compilationTracingFacility);

      TR_CompilationTracingEntry *tracingBuffer = localCompTracingFacility->getEntry(0);
      _dbgPrintf("*** JIT Info: compilationTracingFacility buffer at 0x%p\n", tracingBuffer);
      int32_t crtIndex = localCompTracingFacility->getIndex();
      _dbgPrintf("*** JIT Info: compilationTracingFacility index = %d\n", crtIndex);
      int32_t size = localCompTracingFacility->getSize();
      if (localCompTracingFacility->isInitialized())
         {
         // copy the buffer locally
         TR_CompilationTracingEntry *localBuffer = (TR_CompilationTracingEntry*) dxMallocAndRead(sizeof(TR_CompilationTracingEntry)*size, tracingBuffer);
         for (int32_t i=0; i < size; i++) // visit all elements
            {
            TR_CompilationTracingEntry *entry = localBuffer+crtIndex;
            const char *name = (entry->_operation < OP_LastValidOperation) ? TR::CompilationInfo::OperationNames[entry->_operation] : "INVALID";
            _dbgPrintf("Index=%d J9VMThread=0x%p operation=%s otherData=%u\n",
                       crtIndex, localCompTracingFacility->expandJ9VMThreadId(entry->_J9VMThreadId),
                       name, entry->_otherData);
            crtIndex = localCompTracingFacility->getNextIndex(crtIndex);
            }
         dxFree(localBuffer);
         }
       }
    else
       {
       _dbgPrintf("\n*** JIT Warning: compilationInfo found to be NULL\n");
       }
    }

/*
 * Printing TR::PersistentInfo class
 */
void
TR_DebugExt::dxPrintPersistentInfo(TR::PersistentInfo *remotePersistentInfo)
   {
   if (remotePersistentInfo == NULL)
      remotePersistentInfo = _remotePersistentInfo;

   TR::PersistentInfo * localPersistentInfo = (TR::PersistentInfo *) dxMallocAndRead(sizeof(TR::PersistentInfo), remotePersistentInfo);

   _dbgPrintf("TR::PersistentInfo at (TR::PersistentInfo *)0x%p\n", remotePersistentInfo);
   _dbgPrintf("\tint32_t                _countForRecompile = %d\n", localPersistentInfo->_countForRecompile);
   _dbgPrintf("\tTR_PersistentMemory *  _persistentMemory = !trprint persistentmemory 0x%p\n", localPersistentInfo->_persistentMemory);
   _dbgPrintf("\tTR_PersistentCHTable * _persistentCHTable = !trprint persistentchtable 0x%p\n", localPersistentInfo->_persistentCHTable);
   _dbgPrintf("\tTR_OpaqueClassBlock ** _visitedSuperClasses = 0x%p\n", localPersistentInfo->_visitedSuperClasses);
   _dbgPrintf("\tint32_t                _numVisitedSuperClasses = %d\n", localPersistentInfo->_numVisitedSuperClasses);
   _dbgPrintf("\tbool                   _tooManySuperClasses = %d\n", localPersistentInfo->_tooManySuperClasses);
   _dbgPrintf("\tTableOfConstants     * _persistentTOC = 0x%p\n", localPersistentInfo->_persistentTOC);
   _dbgPrintf("\tint32_t                _numUnloadedClasses = %d\n", localPersistentInfo->_numUnloadedClasses);
   _dbgPrintf("\tTR_AddressSet *        _unloadedClassAddresses = 0x%p\n", localPersistentInfo->_unloadedClassAddresses);
   _dbgPrintf("\tTR_AddressSet *        _unloadedMethodAddresses = 0x%p\n", localPersistentInfo->_unloadedMethodAddresses);
   _dbgPrintf("\tint32_t                _numLoadedClasses = %d\n", localPersistentInfo->_numLoadedClasses);
   _dbgPrintf("\tint32_t                _classLoadingPhaseGracePeriod = %d\n", localPersistentInfo->_classLoadingPhaseGracePeriod);
   _dbgPrintf("\tbool                   _classLoadingPhase = %d\n", localPersistentInfo->_classLoadingPhase);
   _dbgPrintf("\tuint64_t               _startTime = %llu\n", localPersistentInfo->_startTime);
   _dbgPrintf("\tuint64_t               _elapsedTime = %llu\n", localPersistentInfo->_elapsedTime);
   _dbgPrintf("\tuint64_t               _timeGCwillBlockOnClassUnloadMonitorWasSet = %llu\n", localPersistentInfo->_timeGCwillBlockOnClassUnloadMonitorWasSet);
   _dbgPrintf("\tint32_t                _globalClassUnloadID = %d\n", localPersistentInfo->_globalClassUnloadID);
   _dbgPrintf("\tuint32_t               _loadFactor = %u\n", localPersistentInfo->_loadFactor);
   _dbgPrintf("\tbool                   _GCwillBlockOnClassUnloadMonitor = %d\n", localPersistentInfo->_GCwillBlockOnClassUnloadMonitor);
   _dbgPrintf("\tbool                   _externalStartupEndedSignal = %d\n", localPersistentInfo->_externalStartupEndedSignal);
   _dbgPrintf("\tbool                   _disableFurtherCompilation = %d\n", localPersistentInfo->_disableFurtherCompilation);
   _dbgPrintf("\tuint_8                 _jitState = %u\n", localPersistentInfo->_jitState);
   _dbgPrintf("\tuint32_t               _jitTotalSampleCount = %u\n", localPersistentInfo->_jitTotalSampleCount);
   _dbgPrintf("\tuint64_t               _lastTimeSamplerThreadEnteredIdle = %llu\n", localPersistentInfo->_lastTimeSamplerThreadEnteredIdle);
   _dbgPrintf("\tuint64_t               _lastTimeSamplerThreadEnteredDeepIdle = %llu\n", localPersistentInfo->_lastTimeSamplerThreadEnteredDeepIdle);
   _dbgPrintf("\tuint64_t               _lastTimeSamplerThreadWasSuspended = %llu\n", localPersistentInfo->_lastTimeSamplerThreadWasSuspended);
   _dbgPrintf("\tuint64_t               _lastTimeThreadsWereActive = %llu\n", localPersistentInfo->_lastTimeThreadsWereActive);

   dxFree(localPersistentInfo);
   }

/*
 * Printing TR_J9MonitorTable class
 */
void
TR_DebugExt::dxPrintJ9MonitorTable(TR::MonitorTable *remoteMonTable)
   {
   if (remoteMonTable == 0)
      {
      _dbgPrintf("*** JIT Error: TR::MonitorTable is NULL\n");
      return;
      }

   TR::MonitorTable * localMonTable = (TR::MonitorTable *) dxMallocAndRead(sizeof(TR::MonitorTable), remoteMonTable);

   _dbgPrintf("\tJ9MonitorTable at 0x%p\n", remoteMonTable);
   _dbgPrintf("\tTR::Monitor * _tableMonitor = 0x%p\n", &(remoteMonTable->_tableMonitor));
   _dbgPrintf("\tTR::Monitor * _j9MemoryAllocMonitor = 0x%p\n", &(remoteMonTable->_j9MemoryAllocMonitor));
   _dbgPrintf("\tTR::Monitor * _j9ScratchMemoryPoolMonitor = 0x%p\n", &(remoteMonTable->_j9ScratchMemoryPoolMonitor));
   _dbgPrintf("\tTR::Monitor * _classUnloadMonitor = 0x%p\n", &(remoteMonTable->_classUnloadMonitor));
   _dbgPrintf("\tTR::Monitor * _classTableMutex = 0x%p\n", &(remoteMonTable->_classTableMutex));
   _dbgPrintf("\tTR::Monitor * _iprofilerPersistenceMonitor = 0x%p\n", &(remoteMonTable->_iprofilerPersistenceMonitor));
   _dbgPrintf("\tHolders of classUnloadMonitor at address 0x%p\n", remoteMonTable->_classUnloadMonitorHolders);

   dxFree(localMonTable);
   }
/*
 * Printing compilation struct
 */
void
TR_DebugExt::dxPrintCompilation()
   {
   if (_remoteCompiler == 0 || _localCompiler == 0)
      {
      _dbgPrintf("*** JIT Error: compilation is NULL\n");
      return;
      }

   // Need to do this because some of _localCompiler 's field have been overwritten
   TR::Compilation* localCompiler = (TR::Compilation*) dxMallocAndRead(sizeof(TR::Compilation), (void*)_remoteCompiler);

   _dbgPrintf("\tcompilation at 0x%p\n",_remoteCompiler);
   _dbgPrintf("\tconst char * _signature = 0x%p\n",localCompiler->_signature);
   _dbgPrintf("\tTR_ResolvedMethod *_method = 0x%p\n",localCompiler->_method);
   _dbgPrintf("\tTR_FrontEnd *_fe = 0x%p\n",localCompiler->_fe);
   _dbgPrintf("\tTR_Memory *_trMemory = !trprint trmemory 0x%p\n",localCompiler->_trMemory);
   _dbgPrintf("\tTR::ResolvedMethodSymbol *_methodSymbol = 0x%p\n",localCompiler->_methodSymbol);
   _dbgPrintf("\tTR::CodeGenerator *_codeGenerator = 0x%p\n",localCompiler->_codeGenerator);
   _dbgPrintf("\tTR_J9ByteCodeIlGenerator *_ilGenerator = 0x%p\n",localCompiler->_ilGenerator);
   _dbgPrintf("\tTR::Optimizer *_optimizer = 0x%p\n",localCompiler->_optimizer);
   _dbgPrintf("\tTR_Debug *_debug = 0x%p\n",localCompiler->_debug);
   _dbgPrintf("\tTR::SymbolReferenceTable *_currentSymRefTab = 0x%p\n",localCompiler->_currentSymRefTab);
   _dbgPrintf("\tTR::Recompilation *_recompilationInfo = 0x%p\n",localCompiler->_recompilationInfo);
   _dbgPrintf("\tTR_OpaqueClassBlock *_cachedClassPointers[OBJECT_CLASS_POINTER]     = 0x%p\n",localCompiler->_cachedClassPointers[J9::Compilation::OBJECT_CLASS_POINTER]);
   _dbgPrintf("\tTR_OpaqueClassBlock *_cachedClassPointers[RUNNABLE_CLASS_POINTER]   = 0x%p\n",localCompiler->_cachedClassPointers[J9::Compilation::RUNNABLE_CLASS_POINTER]);
   _dbgPrintf("\tTR_OpaqueClassBlock *_cachedClassPointers[STRING_CLASS_POINTER]     = 0x%p\n",localCompiler->_cachedClassPointers[J9::Compilation::STRING_CLASS_POINTER]);
   _dbgPrintf("\tTR_OpaqueClassBlock *_cachedClassPointers[SYSTEM_CLASS_POINTER]     = 0x%p\n",localCompiler->_cachedClassPointers[J9::Compilation::SYSTEM_CLASS_POINTER]);
   _dbgPrintf("\tTR_OpaqueClassBlock *_cachedClassPointers[REFERENCE_CLASS_POINTER]  = 0x%p\n",localCompiler->_cachedClassPointers[J9::Compilation::REFERENCE_CLASS_POINTER]);
   _dbgPrintf("\tTR_OpaqueClassBlock *_cachedClassPointers[JITHELPERS_CLASS_POINTER] = 0x%p\n",localCompiler->_cachedClassPointers[J9::Compilation::JITHELPERS_CLASS_POINTER]);

   _dbgPrintf("\tTR_OptimizationPlan *_optimizationPlan = !trprint optimizationplan 0x%p\n",localCompiler->_optimizationPlan);
   _dbgPrintf("\tTR_Array<TR::ResolvedMethodSymbol*> _methodSymbols = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_methodSymbols) - (char*)localCompiler) );
   _dbgPrintf("\tTR_Array<TR::SymbolReference*> _resolvedMethodSymbolReferences = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_resolvedMethodSymbolReferences) - (char*)localCompiler) );
   _dbgPrintf("\tTR_Array<TR_InlinedCallSite> _inlinedCallSites = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_inlinedCallSites) - (char*)localCompiler) );

   _dbgPrintf("\tTR_Stack<int32_t> _inlinedCallStack = 0x%p\n",(char *)_remoteCompiler +((char*)&(localCompiler->_inlinedCallStack) - (char*)localCompiler));
   _dbgPrintf("\tTR_Stack<TR_PrexArgInfo *> _inlinedCallArgInfoStack = 0x%p\n",(char *)_remoteCompiler + ((char *)&(localCompiler->_inlinedCallArgInfoStack) - (char *)localCompiler));
   _dbgPrintf("\tList<TR_DevirtualizedCallInfo> _devirtualizedCalls = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_devirtualizedCalls) - (char*)localCompiler) );
   _dbgPrintf("\tList<TR_VirtualGuard> _virtualGuards = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_virtualGuards) - (char*)localCompiler) );
   _dbgPrintf("\tList<TR_VirtualGuardSite> _sideEffectGuardPatchSites = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_sideEffectGuardPatchSites) - (char*)localCompiler) );
   _dbgPrintf("\tTR_LinkHead<TR_ClassLoadCheck> _classesThatShouldNotBeLoaded = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_classesThatShouldNotBeLoaded) - (char*)localCompiler) );
   _dbgPrintf("\tTR_LinkHead<TR_ClassExtendCheck> _classesThatShouldNotBeNewlyExtended = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_classesThatShouldNotBeNewlyExtended) - (char*)localCompiler) );
   _dbgPrintf("\tList<TR::Instruction> _staticPICSites = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_staticPICSites) - (char*)localCompiler) );
   _dbgPrintf("\tList<TR::Instruction> _staticMethodPICSites = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_staticMethodPICSites) - (char*)localCompiler) );
   _dbgPrintf("\tList<TR_Snippet> _snippetsToBePatchedOnClassUnload = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_snippetsToBePatchedOnClassUnload) - (char*)localCompiler) );
   _dbgPrintf("\tList<TR_Snippet> _methodSnippetsToBePatchedOnClassUnload = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_methodSnippetsToBePatchedOnClassUnload) - (char*)localCompiler) );
   _dbgPrintf("\t&(TR::SymbolReferenceTable _symRefTab) = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_symRefTab) - (char*)localCompiler) );
   _dbgPrintf("\tTR::Options *_options = 0x%p\n",localCompiler->_options);
   _dbgPrintf("\tuint32_t _returnInfo = %d\n",localCompiler->_returnInfo);
   _dbgPrintf("\tflags32_t _flags = 0x%x\n",localCompiler->_flags.getValue());
   _dbgPrintf("\tvcount_t _visitCount = %d\n",localCompiler->_visitCount);
   _dbgPrintf("\tnCount _nodeCount = %d\n",localCompiler->_nodeCount);
   _dbgPrintf("\tuint16_t _maxInlineDepth = %d\n",localCompiler->_maxInlineDepth);
   _dbgPrintf("\tint16_t _optIndex = %d\n",localCompiler->_currentOptIndex);
   _dbgPrintf("\tbool _needsClassLookahead = %s\n",localCompiler->_needsClassLookahead?"TRUE":"FALSE");
   _dbgPrintf("\tbool _usesPreexistence = %s\n",localCompiler->_usesPreexistence?"TRUE":"FALSE");
   _dbgPrintf("\tbool _loopVersionedWrtAsyncChecks = %s\n",localCompiler->_loopVersionedWrtAsyncChecks?"TRUE":"FALSE");
   _dbgPrintf("\tbool _commitedCallSiteInfo = %s\n",localCompiler->_commitedCallSiteInfo?"TRUE":"FALSE");
   _dbgPrintf("\tint32_t _errorCode = 0x%x\n",localCompiler->_errorCode);
   _dbgPrintf("\tTR_Stack<TR_PeekingArgInfo *> _peekingArgInfo = 0x%p\n",(char *)_remoteCompiler + ((char*)&(localCompiler->_peekingArgInfo) - (char*)localCompiler));
   _dbgPrintf("\tTR::SymbolReferenceTable *_peekingSymRefTab = 0x%p\n",localCompiler->_peekingSymRefTab);
   _dbgPrintf("\tPhaseTimingSummary _phaseTimer = 0x%p\n", (char*)_remoteCompiler + offsetof(TR::Compilation, _phaseTimer));
   _dbgPrintf("\tTR::PhaseMemSummary _phaseMemProfiler = 0x%p\n", (char*)_remoteCompiler + offsetof(TR::Compilation, _phaseMemProfiler));
   _dbgPrintf("\tTR_ValueProfileInfoManager *_vpInfoManager = 0x%p\n",localCompiler->_vpInfoManager);

   _dbgPrintf("\tList<TR_ExternalValueProfileInfo> _methodVPInfoList = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_externalVPInfoList) - (char*)localCompiler) );
   _dbgPrintf("\tList<TR_Pair<TR_ByteCodeInfo, TR::Node> > _checkcastNullChkInfo = 0x%p\n",(char*)_remoteCompiler +((char*)&(localCompiler->_checkcastNullChkInfo) - (char*)localCompiler) );

   _dbgPrintf("\tTR_CHTable * _transientCHTable = !trprint chtable 0x%p\n",localCompiler->_transientCHTable);
   _dbgPrintf("\tvoid * _aotMethodDataStart = %p\n",localCompiler->_aotMethodDataStart);
   _dbgPrintf("\tvoid * _relocatableMethodCodeStart = %p\n",localCompiler->_relocatableMethodCodeStart);
   _dbgPrintf("\tint32_t _compThreadID = %d\n",localCompiler->_compThreadID);
   _dbgPrintf("\tbool _failCHtableCommitFlag = %s\n",localCompiler->_failCHtableCommitFlag?"TRUE":"FALSE");
   _dbgPrintf("\tsize_t _scratchSpaceLimit = %llu\n", static_cast<unsigned long long>(localCompiler->_scratchSpaceLimit));


   dxFree(localCompiler);
   }

void
TR_DebugExt::dxPrintOptimizationPlan(TR_OptimizationPlan * remoteOptimizationPlan)
   {
   if (remoteOptimizationPlan == 0)
      {
      _dbgPrintf("*** JIT Error: optimizationPlan is NULL\n");
      return;
      }

   TR_OptimizationPlan * localOptimizationPlan = (TR_OptimizationPlan *) dxMallocAndRead(sizeof(TR_OptimizationPlan), remoteOptimizationPlan);
   _dbgPrintf("TR_OptimizationPlan at (TR_OptimizationPlan *)0x%p\n", remoteOptimizationPlan);
   _dbgPrintf("TR_OptimizationPlan *     _next = !trprint optimizationplan 0x%p\n", localOptimizationPlan->_next);
   _dbgPrintf("TR_Hotness                _optLevel = 0x%p\n", localOptimizationPlan->_optLevel);
   _dbgPrintf("flags32_t                 _flags = 0x%p\n", localOptimizationPlan->_flags.getValue());
   _dbgPrintf("int32_t                   _perceivedCPUUtil = 0x%x\n", localOptimizationPlan->_perceivedCPUUtil);
   _dbgPrintf("static unsigned long      _totalNumAllocatedPlans = 0x%lx\n", localOptimizationPlan->_totalNumAllocatedPlans);
   _dbgPrintf("static unsigned long      _poolSize = 0x%lx\n", localOptimizationPlan->_poolSize);
   _dbgPrintf("static unsigned long      _numAllocOp = 0x%lx\n", localOptimizationPlan->_numAllocOp);
   _dbgPrintf("static unsigned long      _numFreeOp = 0x%lx\n", localOptimizationPlan->_numFreeOp);

   dxFree(localOptimizationPlan);
   }

void
TR_DebugExt::dxPrintDataCache(TR_DataCache * remoteDataCache)
   {
   if (remoteDataCache == 0)
      {
      _dbgPrintf("*** JIT Error: dataCache is NULL\n");
      return;
      }

   TR_DataCache * localDataCache = (TR_DataCache *) dxMallocAndRead(sizeof(TR_DataCache), remoteDataCache);
   _dbgPrintf("TR_DataCache at (TR_DataCache *)0x%p\n", remoteDataCache);
   _dbgPrintf("TR_DataCache *        _next = !trprint datacache 0x%p\n", localDataCache->_next);
   _dbgPrintf("J9MemorySegment *     _segment = 0x%p\n", localDataCache->_segment);
   _dbgPrintf("J9VMThread *          _vmThread = 0x%p\n", localDataCache->_vmThread);
   _dbgPrintf("uint8_t *             _status = 0x%x\n", localDataCache->_status);

   dxFree(localDataCache);
   }

void
TR_DebugExt::dxPrintPersistentMemory(TR_PersistentMemory *remotePersistentMemory)
   {
   if (remotePersistentMemory == NULL)
      remotePersistentMemory = _remotePersistentMemory;

   // Need to do this because _localPersistentMemory's fields can be overwritten
   TR_PersistentMemory * localPersistentMemory = (TR_PersistentMemory *) dxMallocAndRead(sizeof(TR_PersistentMemory), remotePersistentMemory);

   _dbgPrintf("TR_PersistentMemory at (TR_PersistentMemory *)0x%p\n", remotePersistentMemory);
   _dbgPrintf("\tTR::PersistentInfo * _persistentInfo = !trprint persistentinfo 0x%p\n", &(remotePersistentMemory->_persistentInfo));

   dxFree(localPersistentMemory);
   }

/*
 * Printing jit global memHdr
 */
void
TR_DebugExt::dxPrintTRMemory(TR_Memory *remoteTrMemory)
   {
   if (remoteTrMemory == 0)
      {
      _dbgPrintf("*** JIT Error: trMemory is NULL\n");
      return;
      }
   TR_Memory * localMemory = (TR_Memory *) dxMallocAndRead(sizeof(TR_Memory), remoteTrMemory);

   _dbgPrintf("TR_Memory at (TR_Memory *)0x%p\n", remoteTrMemory);
   _dbgPrintf("\tTR_PersistentMemory *_trPersistentMemory = !trprint persistentmemory 0x%p\n", localMemory->_trPersistentMemory);
   _dbgPrintf("\tTR::Compilation *_comp = !trprint compilation 0x%p\n", localMemory->_comp);

   dxFree(localMemory);
   }

/*
 * Printing CHTable
 */
void
TR_DebugExt::dump(TR::FILE *pOutFile, TR_CHTable *chtable)
   {
   TR_CHTable *localCHTable = (TR_CHTable *) dxMallocAndRead(sizeof(TR_CHTable), chtable);
   TR_Array<TR_OpaqueClassBlock*> *localClasses = (TR_Array<TR_OpaqueClassBlock*> *) dxMallocAndRead(sizeof(TR_Array<TR_OpaqueClassBlock*>), localCHTable->_classes);
   TR_Array<TR_ResolvedMethod*> *localPreXMethods = (TR_Array<TR_ResolvedMethod*> *) dxMallocAndRead(sizeof(TR_Array<TR_ResolvedMethod*>), localCHTable->_preXMethods);
   localCHTable->_classes = localClasses;
   localCHTable->_preXMethods = localPreXMethods;

   /* TR_Array need a different copy constructor in debugger extension */
   if (localClasses)
      localClasses->_array = (TR_OpaqueClassBlock**) newArray (sizeof(TR_OpaqueClassBlock*), localClasses->_internalSize , localClasses->_array);
   if (localPreXMethods)
      localPreXMethods->_array = (TR_ResolvedMethod**) newArray (sizeof(TR_ResolvedMethod*), localPreXMethods->_internalSize , localPreXMethods->_array);

   TR_Debug::dump(pOutFile, localCHTable);

   if (localPreXMethods)
      freeArray(localPreXMethods->_array);
   if (localClasses)
      freeArray(localClasses->_array);
   dxFree(localPreXMethods);
   dxFree(localClasses);
   dxFree(localCHTable);
   }

void
TR_DebugExt::dxPrintCHTable(TR_CHTable *chtable)
   {
   if (chtable == 0)
      {
      _dbgPrintf("chtable is NULL\n");
      return;
      }
   _dbgPrintf("Printing chtable 0x%p ...\n", chtable);

   TR_CHTable *localCHTable = (TR_CHTable *) dxMallocAndRead(sizeof(TR_CHTable), chtable);
   _dbgPrintf("((TR_CHTable*)0x%p)->_classes = TR_Array<TR_ResolvedMethod*>* 0x%p\n", chtable, localCHTable->_classes);
   _dbgPrintf("((TR_CHTable*)0x%p)->_preXMethods = TR_Array<TR_OpaqueClassBlock*>* 0x%p\n", chtable, localCHTable->_preXMethods);
   dxFree(localCHTable);

   /*
    * try to reuse TR_Debug::dump() for printing classes and preXMethods
    */

   dump(TR::IO::Stdout, chtable);

   _dbgPrintf("Finish printing chtable\n");
   }


void
TR_DebugExt::dxPrintPersistentProfileInfo(TR_PersistentProfileInfo *pinfo)
   {
   if (pinfo == 0)
      {
      _dbgPrintf("PersistentProfileInfo is NULL\n");
      return;
      }
   TR_PersistentProfileInfo *localPInfo = (TR_PersistentProfileInfo*) dxMallocAndRead(sizeof(TR_PersistentProfileInfo), pinfo);

   _dbgPrintf("PersistentProfileInfo = 0x%p\n", pinfo);
   _dbgPrintf("  ->_callSiteInfo = (TR_CallSiteInfo*)0x%p\n", localPInfo->getCallSiteInfo());
   _dbgPrintf("  ->_catchBlockProfileInfo = (TR_CatchBlockProfileInfo*)0x%p\n", localPInfo->getCatchBlockProfileInfo());
   _dbgPrintf("  ->_blockFrequencyInfo = (TR_BlockFrequencyInfo*)0x%p\n", localPInfo->getBlockFrequencyInfo());
   _dbgPrintf("  ->_valueProfileInfo = (TR_ValueProfileInfo*)0x%p\n", (char*)pinfo+((char*)localPInfo->getValueProfileInfo()-(char*)localPInfo),PROFILING_INVOCATION_COUNT);
   _dbgPrintf("  ->_profilingFrequency = (int32_t)0x%p[%d]\n", (char*)pinfo+((char*)localPInfo->getProfilingFrequencyArray()-(char*)localPInfo),PROFILING_INVOCATION_COUNT);
   _dbgPrintf("  ->_profilingCount = (int32_t)0x%p[%d]\n", (char*)localPInfo+((char*)localPInfo->getProfilingCountArray()-(char*)localPInfo));
   _dbgPrintf("  ->_maxCount = (int32_t)0x%p\n", localPInfo->getMaxCount());

   dxFree(localPInfo);
   }


void
TR_DebugExt::dxPrintCodeCacheInfo(TR::CodeCache *cacheInfo)
   {
   if (cacheInfo == 0)
      {
      _dbgPrintf("TR::CodeCache is NULL\n");
      return;
      }
   TR::CodeCache *localCacheInfo = (TR::CodeCache*) dxMallocAndRead(sizeof(TR::CodeCache), cacheInfo);
   _dbgPrintf("TR::CodeCache = 0x%p\n", cacheInfo);
   _dbgPrintf("  ->warmCodeAlloc = (U_8*)0x%p\n", localCacheInfo->getWarmCodeAlloc());
   _dbgPrintf("  ->coldCodeAlloc = (U_8*)0x%p\n", localCacheInfo->getColdCodeAlloc());
   _dbgPrintf("  ->segment = (TR::CodeCacheMemorySegment*)0x%p\n", localCacheInfo->_segment);
   _dbgPrintf("  ->helperBase = (U_8*)0x%p\n", localCacheInfo->_helperBase);
   _dbgPrintf("  ->helperTop = (U_8*)0x%p\n", localCacheInfo->_helperTop);
   _dbgPrintf("  ->tempTrampolineBase = (U_8*)0x%p\n", localCacheInfo->_tempTrampolineBase);
   _dbgPrintf("  ->tempTrampolineTop = (U_8*)0x%p\n", localCacheInfo->_tempTrampolineTop);
   _dbgPrintf("  ->CCPreLoadedCodeBase = (U_8*)0x%p\n", localCacheInfo->_CCPreLoadedCodeBase);
   _dbgPrintf("  ->tempTrampolineNext = (U_8*)0x%p\n", localCacheInfo->_tempTrampolineNext);
   _dbgPrintf("  ->trampolineAllocationMark = (U_8*)0x%p\n", localCacheInfo->_trampolineAllocationMark);
   _dbgPrintf("  ->trampolineReservationMark = (U_8*)0x%p\n", localCacheInfo->_trampolineReservationMark);
   _dbgPrintf("  ->trampolineBase = (U_8*)0x%p\n", localCacheInfo->_trampolineBase);
   _dbgPrintf("  ->resolvedMethodHT = (OMR::CodeCacheHashTable*)0x%p\n", localCacheInfo->_resolvedMethodHT);
   _dbgPrintf("  ->unresolvedMethodHT = (OMR::CodeCacheHashTable*)0x%p\n", localCacheInfo->_unresolvedMethodHT);
   _dbgPrintf("  ->hashEntrySlab = (OMR::CodeCacheHashEntrySlab*)0x%p\n", localCacheInfo->hashEntrySlab());
   _dbgPrintf("  ->hashEntryFreeList = (OMR::CodeCacheHashEntry*)0x%p\n", localCacheInfo->hashEntryFreeList());
   _dbgPrintf("  ->tempTrampolinesMax = (U_32)%u\n", localCacheInfo->_tempTrampolinesMax);
   _dbgPrintf("  ->flags = (U_32)0x%x\n", localCacheInfo->_flags);
   _dbgPrintf("  ->trampolineSyncList = (OMR::CodeCacheTempTrampolineSyncBlock*)0x%p\n", localCacheInfo->trampolineSyncList());
   _dbgPrintf("  ->freeBlockList = (OMR::CodeCacheFreeCacheBlock*)0x%p\n", localCacheInfo->freeBlockList());
   _dbgPrintf("  ->mutex = (TR::Monitor*)0x%p\n", localCacheInfo->_mutex);
#if defined(TR_TARGET_X86)
   _dbgPrintf("  ->prefetchCodeSnippetAddress = (uintptrj_t)0x%p\n", localCacheInfo->_CCPreLoadedCode[TR_AllocPrefetch]);
   _dbgPrintf("  ->noZeroPrefetchCodeSnippetAddress = (uintptrj_t)0x%p\n", localCacheInfo->_CCPreLoadedCode[TR_NonZeroAllocPrefetch]);
#endif
   _dbgPrintf("  ->next = (TR::CodeCache*)0x%p\n", localCacheInfo->_next);
   _dbgPrintf("  ->reserved = (bool)%d\n", localCacheInfo->_reserved);
   _dbgPrintf("  ->almostFull = (TR_YesNoMaybe)%d\n", localCacheInfo->_almostFull);
   _dbgPrintf("  ->_reservingCompThreadID = (int32_t)%d\n", localCacheInfo->_reservingCompThreadID);
   _dbgPrintf("  ->_sizeOfLargestFreeColdBlock = (int32_t)%d\n", localCacheInfo->_sizeOfLargestFreeColdBlock);
   _dbgPrintf("  ->_sizeOfLargestFreeWarmBlock = (int32_t)%d\n", localCacheInfo->_sizeOfLargestFreeWarmBlock);
   dxFree(localCacheInfo);
   }

OMR::CodeCacheFreeCacheBlock*
TR_DebugExt::dxPrintFreeCodeCacheBlock(OMR::CodeCacheFreeCacheBlock *blockPtr)
   {
   OMR::CodeCacheFreeCacheBlock *next;
   if (blockPtr == 0)
      {
      _dbgPrintf("OMR::CodeCacheFreeCacheBlock is NULL\n");
      return NULL;
      }
   OMR::CodeCacheFreeCacheBlock *localBlockInfo = (OMR::CodeCacheFreeCacheBlock*) dxMallocAndRead(sizeof(OMR::CodeCacheFreeCacheBlock), blockPtr);
   _dbgPrintf("OMR::CodeCacheFreeCacheBlock = 0x%p\n", blockPtr);
   _dbgPrintf("  ->size = (UDATA)%u\n", (uint32_t)(localBlockInfo->_size));
   _dbgPrintf("  ->next = (OMR::CodeCacheFreeCacheBlock*)0x%p\n", localBlockInfo->_next);
   next = localBlockInfo->_next;
   dxFree(localBlockInfo);
   return next;
   }

void
TR_DebugExt::dxPrintFreeCodeCacheBlockList(TR::CodeCache *cacheInfo)
   {
   if (cacheInfo == 0)
      {
      _dbgPrintf("TR::CodeCache is NULL\n");
      return;
      }
   TR::CodeCache *localCacheInfo = (TR::CodeCache*) dxMallocAndRead(sizeof(TR::CodeCache), cacheInfo);
   _dbgPrintf("  List of free block starting at:(OMR::CodeCacheFreeCacheBlock*)0x%p\n", localCacheInfo->freeBlockList());
   OMR::CodeCacheFreeCacheBlock *freeBlock = localCacheInfo->freeBlockList();
   while (freeBlock)
      {
      freeBlock = dxPrintFreeCodeCacheBlock(freeBlock);
      }
   dxFree(localCacheInfo);
   }

void
TR_DebugExt::dxPrintListOfCodeCaches()
   {
   if (!_remotePersistentMemory)
      return;

   TR::CodeCacheManager * manager = NULL;

   /**
    * The CodeCacheManager will need to be materialized from the cached codeCacheManager field of
    * the TR_JitPrivateConfig struct.  However, a local TR_JitPrivateConfig struct must first be
    * materialized when the local JitConfig struct and local VM is materialized when the remote
    * VM is still available.  The remote VM is not presently available to this function otherwise
    * it could be done here.
    *
    * Until this is done, this function will simply return and not report anything.
    */
   if (!manager)
      return;

   _dbgPrintf("TR::CodeCacheManager = 0x%p  List of code caches:\n", manager);
   TR::CodeCacheManager *localManager = (TR::CodeCacheManager*) dxMallocAndRead(sizeof(TR::CodeCacheManager), manager);
   TR::CodeCache *codeCache = localManager->getFirstCodeCache();

   if (codeCache)
      {
      while (codeCache)
         {
         _dbgPrintf("   TR::CodeCache = 0x%p\n", codeCache);
         TR::CodeCache *nextCodeCache;
         dxReadField(codeCache, offsetof(TR::CodeCache, _next), &nextCodeCache, sizeof(TR::CodeCache*));
         codeCache = nextCodeCache;
         }
      }
   else
      {
      _dbgPrintf("    No code cache\n");
      }

   dxFree(localManager);
   }

void
TR_DebugExt::dxPrintDataCacheManager(TR_DataCacheManager *manager)
   {
   if (manager == 0)
      {
      _dbgPrintf("DataCacheManager is NULL\n");
      return;
      }
   TR_DataCacheManager *localManager = static_cast<TR_DataCacheManager *>(dxMallocAndRead(sizeof(TR_DataCacheManager), manager));
   _dbgPrintf("TR_DataCacheManager @ 0x%p\n", manager);
   _dbgPrintf("  ->_activeDataCacheList = (TR_DataCache *) 0x%p\n", localManager->_activeDataCacheList);
   _dbgPrintf("  ->_almostFullDataCacheList = (TR_DataCache *) 0x%p\n",localManager->_almostFullDataCacheList);
   _dbgPrintf("  ->_cachesInPool = (TR_DataCache *) 0x%p\n", localManager->_cachesInPool);
   _dbgPrintf("  ->_numAllocatedCaches = (int32_t) %d\n",localManager->_numAllocatedCaches);
   _dbgPrintf("  ->_totalSegmentMemoryAllocated = (uint32_t) %u\n",localManager->_totalSegmentMemoryAllocated);
   _dbgPrintf("  ->_flags = 0x%x\n",localManager->_flags);
   _dbgPrintf("  ->_jitConfig = (J9JITConfig *) 0x%p\n",localManager->_jitConfig);
   _dbgPrintf("  ->_quantumSize = (uint32_t) %u\n",localManager->_quantumSize);
   _dbgPrintf("  ->_minQuanta = (uint32_t) %u\n",localManager->_minQuanta);
   _dbgPrintf("  ->_newImplementation = (bool) %s\n", localManager->_newImplementation ? "true" : "false");
   _dbgPrintf("  ->_worstFit = (bool) %s\n", localManager->_worstFit ? "true" : "false");
   _dbgPrintf("  ->_sizeList  = TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>\n");
   _dbgPrintf("  ->_sizeList._sentinel = TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement\n");
   _dbgPrintf("  ->_sizeList._sentinel._prev = (TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement *) 0x%p\n", localManager->_sizeList._sentinel._prev);
   _dbgPrintf("  ->_sizeList._sentinel._next = (TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement *) 0x%p\n", localManager->_sizeList._sentinel._next);
   _dbgPrintf("  ->_sizeList._sentinel._contents = (TR_DataCacheManager::SizeBucket *) 0x%p\n", localManager->_sizeList._sentinel._contents);
   _dbgPrintf("  ->_mutex = (TR::Monitor *) 0x%p\n", localManager->_mutex);
   dxFree(localManager);
   }

void
TR_DebugExt::dxPrintDataCacheSizeBucket(void *sizeBucket) // declared void * as forward declaration not possible in header
   {
   if (sizeBucket == 0)
      {
      _dbgPrintf("SizeBucket is NULL\n");
      return;
      }
   TR_DataCacheManager::SizeBucket *localSizeBucket = static_cast<TR_DataCacheManager::SizeBucket *>(dxMallocAndRead(sizeof(TR_DataCacheManager::SizeBucket), sizeBucket));
   _dbgPrintf("TR_DataCacheManager::SizeBucket @ 0x%p\n", sizeBucket);
   _dbgPrintf("  ->_listElement = InPlaceList<SizeBucket>::ListElement\n");
   _dbgPrintf("  ->_listElement._prev = (TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement *)0x%p\n", localSizeBucket->_listElement._prev);
   _dbgPrintf("  ->_listElement._next = (TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement *)0x%p\n", localSizeBucket->_listElement._next);
   _dbgPrintf("  ->_listElement._contents = (TR_DataCacheManager::SizeBucket *) 0x%p\n", localSizeBucket->_listElement._contents);
   _dbgPrintf("  ->_size = (U_32) %u\n", localSizeBucket->_size);
   _dbgPrintf("  ->_allocations = TR_DataCacheManager::InPlaceList<Allocation>\n");
   _dbgPrintf("  ->_allocations._sentinel = TR_DataCacheManager::InPlaceList<Allocation>::ListElement\n");
   _dbgPrintf("  ->_allocations._sentinel._prev = (TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement *) 0x%p\n", localSizeBucket->_allocations._sentinel._prev);
   _dbgPrintf("  ->_allocations._sentinel._next = (TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement *) 0x%p\n", localSizeBucket->_allocations._sentinel._next);
   _dbgPrintf("  ->_allocations._sentinel._contents = (TR_DataCacheManager::Allocation *) 0x%p\n", localSizeBucket->_allocations._sentinel._contents);
   dxFree(localSizeBucket);
   }


void
TR_DebugExt::dxPrintDataCacheAllocation(void *allocation) // declared void * as forward declaration not possible in header
   {
   if (allocation == 0)
      {
      _dbgPrintf("Allocation is NULL\n");
      return;
      }
   TR_DataCacheManager::Allocation *localAllocation = static_cast<TR_DataCacheManager::Allocation *>(dxMallocAndRead(sizeof(TR_DataCacheManager::Allocation), allocation));
   _dbgPrintf("TR_DataCacheManager::Allocation @ 0x%p\n", allocation);
   _dbgPrintf("  ->_header = J9JITDataCacheHeader\n");
   _dbgPrintf("  ->_header.size = (uint32_t) %u\n", localAllocation->_header.size);
   _dbgPrintf("  ->_header.type = (uint32_t) %x\n", localAllocation->_header.type);
   _dbgPrintf("  ->_listElement = TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement\n");
   _dbgPrintf("  ->_listElement._prev = (TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement *) 0x%p\n", localAllocation->_listElement._prev);
   _dbgPrintf("  ->_listElement._next = (TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement *) 0x%p\n", localAllocation->_listElement._next);
   _dbgPrintf("  ->_listElement._contents = (TR_DataCacheManager::Allocation *) 0x%p\n", localAllocation->_listElement._contents);
   dxFree(localAllocation);
   }

void
TR_DebugExt::dxPrintDataCacheAllocationListElement(void *listElement) // declared void * as forward declaration not possible in header
   {
   if (listElement == 0)
      {
      _dbgPrintf("List Element is NULL\n");
      return;
      }
   TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement *localListElement = static_cast<TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement *>(dxMallocAndRead(sizeof(TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement), listElement));
   _dbgPrintf("TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement @ 0x%p\n", listElement);
   _dbgPrintf("  ->_prev = (TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement *) 0x%p\n", localListElement->_prev);
   _dbgPrintf("  ->_next = (TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement *) 0x%p\n", localListElement->_next);
   _dbgPrintf("  ->_contents = (TR_DataCacheManager::Allocation *) 0x%p\n", localListElement->_contents);
   dxFree(localListElement);
   }

void
TR_DebugExt::dxPrintDataCacheSizeBucketListElement(void *listElement) // declared void * as forward declaration not possible in header
   {
   if (listElement == 0)
      {
      _dbgPrintf("List Element is NULL\n");
      return;
      }
   TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement *localListElement = static_cast<TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement *>(dxMallocAndRead(sizeof(TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement), listElement));
   _dbgPrintf("TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement @ 0x%p\n", listElement);
   _dbgPrintf("  ->_prev = (TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement *) 0x%p\n", localListElement->_prev);
   _dbgPrintf("  ->_next = (TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement *) 0x%p\n", localListElement->_next);
   _dbgPrintf("  ->_contents = (TR_DataCacheManager::SizeBucket *) 0x%p\n", localListElement->_contents);
   dxFree(localListElement);
   }

void
TR_DebugExt::dxPrintPersistentMethodInfo(TR_PersistentMethodInfo *minfo)
   {
   if (minfo == 0)
      {
      _dbgPrintf("PersistentMethodInfo is NULL\n");
      return;
      }
   TR_PersistentMethodInfo *localMInfo = (TR_PersistentMethodInfo*) dxMallocAndRead(sizeof(TR_PersistentMethodInfo), minfo);
   _dbgPrintf("PersistentMethodInfo = 0x%p\n", minfo);
   _dbgPrintf("  ->_methodInfo = (TR_OpaqueMethodBlock*)0x%p\n", localMInfo->getMethodInfo());
   _dbgPrintf("  ->_flags = 0x%x\n", localMInfo->_flags.getValue());
   _dbgPrintf("  ->_nextHotness = (TR_Hotness)0x%p\n", localMInfo->getNextCompileLevel());
   _dbgPrintf("  ->_profileInfo = (TR_PersistentProfileInfo*)0x%p\n", localMInfo->getBestProfileInfo());
   _dbgPrintf("  ->_cpoSampleCounter = (int32_t)%d\n", localMInfo->cpoGetCounter());

   dxFree(localMInfo);
   }


void
TR_DebugExt::dxPrintPersistentCHTable(TR_PersistentCHTable *ptable)
   {
   if (ptable == 0)
      {
      _dbgPrintf("PersistentCHTable is NULL\n");
      return;
      }

   TR_PersistentCHTable *localPTable = (TR_PersistentCHTable*) dxMallocAndRead(sizeof(TR_PersistentCHTable), ptable);

   dxFree(localPTable);
   }

void
TR_DebugExt::dxPrintRuntimeAssumption(OMR::RuntimeAssumption *ra)
   {
   if (ra == 0)
      {
      _dbgPrintf("RuntimeAssumption is NULL\n");
      return;
      }
   OMR::RuntimeAssumption *localRuntimeAssumption = (OMR::RuntimeAssumption*) dxMallocAndRead(sizeof(OMR::RuntimeAssumption), ra);
   _dbgPrintf("((OMR::RuntimeAssumption*)0x%p)->_key=0x%x, ", ra, localRuntimeAssumption->_key);
   _dbgPrintf(" ->_next= !trprint runtimeassumption 0x%p\n", localRuntimeAssumption->getNext());
   _dbgPrintf(" ->_nextAssumptionForSameJittedBody= !trprint runtimeassumption 0x%p\n", localRuntimeAssumption->getNextAssumptionForSameJittedBodyEvenIfDead());
   dxFree(localRuntimeAssumption);
   }
void
TR_DebugExt::dxPrintRuntimeAssumptionList(OMR::RuntimeAssumption *ra)
   {
   if (ra == 0)
      {
      _dbgPrintf("First RA is NULL\n");
      return;
      }
   OMR::RuntimeAssumption *localRuntimeAssumption = (OMR::RuntimeAssumption*) dxMallocAndRead(sizeof(OMR::RuntimeAssumption), ra);
   _dbgPrintf("((OMR::RuntimeAssumption*)0x%p)->_key=0x%x, ", ra, localRuntimeAssumption->_key);
   _dbgPrintf("((OMR::RuntimeAssumption*)0x%p)->isMarkedForDetach()=%d, ", ra, localRuntimeAssumption->isMarkedForDetach());
   _dbgPrintf(" ->_next= !trprint runtimeassumption 0x%p\n", localRuntimeAssumption->getNextEvenIfDead());
   _dbgPrintf(" ->_nextAssumptionForSameJittedBody= !trprint runtimeassumption 0x%p\n", localRuntimeAssumption->getNextAssumptionForSameJittedBodyEvenIfDead());
   OMR::RuntimeAssumption *nextRuntimeAssumption = localRuntimeAssumption->getNextAssumptionForSameJittedBodyEvenIfDead();
   dxFree(localRuntimeAssumption);

   while (nextRuntimeAssumption != ra)
      {
      OMR::RuntimeAssumption *localRuntimeAssumption = (OMR::RuntimeAssumption*) dxMallocAndRead(sizeof(OMR::RuntimeAssumption), nextRuntimeAssumption);
      _dbgPrintf("((OMR::RuntimeAssumption*)0x%p)->_key=0x%x, ", nextRuntimeAssumption, localRuntimeAssumption->_key);
      _dbgPrintf("((OMR::RuntimeAssumption*)0x%p)->isMarkedForDetach()=%d, ", nextRuntimeAssumption, localRuntimeAssumption->isMarkedForDetach());
      _dbgPrintf(" ->_next= !trprint runtimeassumption 0x%p\n", localRuntimeAssumption->getNextEvenIfDead());
      _dbgPrintf(" ->_nextAssumptionForSameJittedBody= !trprint runtimeassumption 0x%p\n", localRuntimeAssumption->getNextAssumptionForSameJittedBodyEvenIfDead());
      nextRuntimeAssumption = localRuntimeAssumption->getNextAssumptionForSameJittedBodyEvenIfDead();
      dxFree(localRuntimeAssumption);
      }
   _dbgPrintf("Finish printing runtimeassumptionlist\n");
   }

void
TR_DebugExt::dxPrintRuntimeAssumptionListFromMetadata(J9JITExceptionTable *methodMetaData)
   {
   J9JITExceptionTable *localMetadata = (J9JITExceptionTable *) dxMallocAndRead(sizeof(J9JITExceptionTable), methodMetaData);
   //dxPrintRuntimeAssumptionList(localMetadata->bodyInfo) // TODO: change this when we settle for a location
   dxFree(localMetadata);
   }

void
TR_DebugExt::dxPrintRuntimeAssumptionArray(OMR::RuntimeAssumption **raa, int32_t start, int32_t end)
   {
   if (raa == 0)
      {
      _dbgPrintf("RuntimeAssumptionArray is NULL\n");
      return;
      }

   //if (end >= ASSUMPTIONTABLE_SIZE)
   //   _dbgPrintf("*** JIT Warning: index %d exceeds the array boundary (size=%d)\n", end, ASSUMPTIONTABLE_SIZE);

   bool dontMap = true;
   OMR::RuntimeAssumption **localRuntimeAssumptionArray = (OMR::RuntimeAssumption**) dxMallocAndRead(sizeof(OMR::RuntimeAssumption*) * (end+1), raa);
   OMR::RuntimeAssumption *localRuntimeAssumption = (OMR::RuntimeAssumption*) dxMalloc(sizeof(OMR::RuntimeAssumption), 0, dontMap);

   _dbgPrintf("Printing _key for non-zero entry in (OMR::RuntimeAssumption*)[%d..%d]:\n", start, end);
   int32_t i;
   for (i = start; i <= end; ++i)
      {
      if (localRuntimeAssumptionArray[i])
         {
         _dbgPrintf("(OMR::RuntimeAssumption*)[%d]= !trprint runtimeassumption 0x%p, ", i, localRuntimeAssumptionArray[i]);
          dxReadMemory((void*)localRuntimeAssumptionArray[i], (void*)localRuntimeAssumption, sizeof(OMR::RuntimeAssumption));
         _dbgPrintf("->_key=0x%x, ", localRuntimeAssumption->_key);
         _dbgPrintf("->_next= !trprint runtimeassumption 0x%p\n", localRuntimeAssumption->getNext());
         }
      }
   _dbgPrintf("Finish printing runtimeassumptionarray\n");

   dxFree(localRuntimeAssumption);
   dxFree(localRuntimeAssumptionArray);
   }

void
TR_DebugExt::dxPrintRuntimeAssumptionTable(TR_RuntimeAssumptionTable *rtable)
   {
   if (rtable == 0)
      {
      _dbgPrintf("RuntimeAssumptionTable is NULL\n");
      return;
      }
   TR_RuntimeAssumptionTable *localRTable = (TR_RuntimeAssumptionTable*) dxMallocAndRead(sizeof(TR_RuntimeAssumptionTable), rtable);
   for (int i=0; i < LastAssumptionKind; i++)
      {
      _dbgPrintf("&(((TR_RuntimeAssumptionTable*)0x%p)->%s)[%u]= !trprint runtimeassumptionarray 0x%p 0 %u\n",
         rtable, runtimeAssumptionKindNames[i], (uint32_t)localRTable->_tables[i]._spineArraySize,
         (char*)rtable + ((char*)(&(localRTable->_tables[i]._htSpineArray)) - (char*)localRTable),
         (uint32_t)localRTable->_tables[i]._spineArraySize-1);
      }
   dxFree(localRTable);
   }


void
TR_DebugExt::dxPrintJ9RamAndRomMethod(struct J9Method * ramMethod)
   {
   if (ramMethod == 0)
      {
      _dbgPrintf("J9Method is NULL\n");
      return;
      }

   struct J9Method *localRamMethod = (J9Method *) dxMallocAndRead(sizeof(J9Method), ramMethod);
   struct J9ROMMethod *localROMMethod = (J9ROMMethod *)dxMallocAndRead(sizeof(J9ROMMethod), J9_ROM_METHOD_FROM_RAM_METHOD(localRamMethod));

   _dbgPrintf("J9Method at (J9Method *) 0x%p\n", ramMethod);
   _dbgPrintf("\t%-50s0x%p\n", "U8 * bytecodes =", localRamMethod->bytecodes);
   _dbgPrintf("\t%-50s0x%p\n", "struct J9ConstantPool * constantPool =", localRamMethod->constantPool);
   _dbgPrintf("\t%-50s0x%p\n", "void * methodRunAddress =", localRamMethod->methodRunAddress);
   _dbgPrintf("\t%-50s0x%p\n\n", "void * extra =", localRamMethod->extra);

   _dbgPrintf("J9ROMMethod at (J9ROMMethod *) 0x%p\n", J9_ROM_METHOD_FROM_RAM_METHOD(localRamMethod));
   _dbgPrintf("\t%-50s0x%p\n", "struct J9ROMNameAndSignature nameAndSignature =", localROMMethod->nameAndSignature);
   _dbgPrintf("\t%-50s0x%p\n", "U32 modifiers =", localROMMethod->modifiers);
   _dbgPrintf("\t%-50s0x%p\n", "U16 maxStack =", localROMMethod->maxStack);
   _dbgPrintf("\t%-50s0x%p\n", "U16 bytecodeSizeLow =", localROMMethod->bytecodeSizeLow);
   _dbgPrintf("\t%-50s0x%p\n", "U8 bytecodeSizeHigh =", localROMMethod->bytecodeSizeHigh);
   _dbgPrintf("\t%-50s0x%p\n", "U8 argCount =", localROMMethod->argCount);
   _dbgPrintf("\t%-50s0x%p\n\n", "U16 tempCount =", localROMMethod->tempCount);

   if (_J9ROMMETHOD_J9MODIFIER_IS_SET(localROMMethod,J9AccMethodHasMethodHandleInvokes))
      _dbgPrintf("Method is JSR292\n");
   else
      _dbgPrintf("Method is not JSR292\n");

   dxFree(localRamMethod);
   dxFree(localROMMethod);
   }


void
TR_DebugExt::dxPrintMethodName(char *p, int32_t searchLimit)
   {
#ifdef J9VM_RAS_EYECATCHERS
   OMR::CodeCacheMethodHeader *localCodeCacheMethodHeader = dxGet_CodeCacheMethodHeader(p, searchLimit);
   J9JITExceptionTable *remoteMetadata = localCodeCacheMethodHeader->_metaData;
   if (localCodeCacheMethodHeader == NULL || remoteMetadata == NULL)
      {
      _dbgPrintf("JIT Error: could not read meta data\n");
      return;
      }
   J9JITExceptionTable *localMetadata = (J9JITExceptionTable *) dxMallocAndRead(sizeof(J9JITExceptionTable), remoteMetadata);

   struct J9UTF8 *localClassNameStruct = (J9UTF8 *) dxMallocAndRead(sizeof(J9UTF8), localMetadata->className);
   char *className = (char *) dxMallocAndRead(J9UTF8_LENGTH(localClassNameStruct) + 1, (void *) J9UTF8_DATA(localMetadata->className));
   className[J9UTF8_LENGTH(localClassNameStruct)] = 0;

   struct J9UTF8 *localMethodNameStruct = (J9UTF8 *) dxMallocAndRead(sizeof(J9UTF8), localMetadata->methodName);
   char *methodName = (char *) dxMallocAndRead(J9UTF8_LENGTH(localMethodNameStruct) + 1, (void *) J9UTF8_DATA(localMetadata->methodName));
   methodName[J9UTF8_LENGTH(localMethodNameStruct)] = 0;

   struct J9UTF8 *localMethodSignatureStruct = (J9UTF8 *) dxMallocAndRead(sizeof(J9UTF8), localMetadata->methodSignature);
   char *methodSignature = (char *) dxMallocAndRead(J9UTF8_LENGTH(localMethodSignatureStruct) + 1, (void *) J9UTF8_DATA(localMetadata->methodSignature));
   methodSignature[J9UTF8_LENGTH(localMethodSignatureStruct)] = 0;

   int32_t hotness = -1;
   bool invalidated = false;
   TR_PersistentJittedBodyInfo *localBodyInfo = (TR_PersistentJittedBodyInfo *) dxMalloc(sizeof(TR_PersistentJittedBodyInfo), localMetadata->bodyInfo);
   if (localMetadata->bodyInfo == NULL)
      {
      uint32_t *pflags = (uint32_t *) dxMalloc(sizeof(uint32_t), (char *) localMetadata->startPC - sizeof(uint32_t));
      dxReadMemory((char *) localMetadata->startPC - sizeof(uint32_t), pflags, sizeof(uint32_t));
      uint32_t flags = *pflags;
      char **ptr;
      if (flags & (METHOD_SAMPLING_RECOMPILATION | METHOD_COUNTING_RECOMPILATION))
         {
         if (TR::Compiler->target.is64Bit())
            {
            ptr = (char **) dxMallocAndRead(sizeof(char *), (char *)localMetadata->startPC - 12);
            }
         else
            {
            ptr = (char **) dxMallocAndRead(sizeof(char *), (char *)localMetadata->startPC - 8);
            }
         if (ptr && *ptr)
            {
            dxReadMemory(*ptr, localBodyInfo, sizeof(TR_PersistentJittedBodyInfo));
            if (localBodyInfo != NULL)
               {
               hotness = localBodyInfo->getHotness();
               invalidated = localBodyInfo->getIsInvalidated();
               }
            }

         if (ptr != NULL)
            dxFree(ptr);
         }
      if (pflags != NULL)
         dxFree(pflags);
      }
   else
      {
      dxReadMemory(localMetadata->bodyInfo, localBodyInfo, sizeof(TR_PersistentJittedBodyInfo));
      if (localBodyInfo)
         {
         hotness = localBodyInfo->getHotness();
         invalidated = localBodyInfo->getIsInvalidated();
         }
      }
   // If we can read hotness from bodyInfo (because it does not exist) read it from exception table
   if (hotness == -1)
      hotness = localMetadata->hotness;

   TR_LinkageInfo *linkageInfo = (TR_LinkageInfo *) dxMallocAndRead(sizeof(TR_LinkageInfo), (void*)((char *)localMetadata->startPC - 4) );

   _dbgPrintf("\n\nMethod:\t%s.%s%s\n\n", className, methodName, methodSignature);
   dxPrintJ9RamAndRomMethod(localMetadata->ramMethod);
   _dbgPrintf("Method Hotness:\t%i = %s\n\n", hotness, hotness == -1 ? "unknown" : comp()->getHotnessName((TR_Hotness) hotness));


   _dbgPrintf("Linkage Info (_word = 0x%p)\n",linkageInfo->_word);
   if(linkageInfo->isCountingMethodBody())
      _dbgPrintf("\tIs a Counting Method Body\n");
   if(linkageInfo->isSamplingMethodBody())
      _dbgPrintf("\tIs a Sampling Method Body\n");
   if(linkageInfo->isRecompMethodBody())
      _dbgPrintf("\tIs a Recomp Method Body\n");
   if (invalidated)
      _dbgPrintf("\tHas Been Invalidated\n");
   if(linkageInfo->hasBeenRecompiled())
      _dbgPrintf("\tHas Been Recompiled\n");
   if(linkageInfo->hasFailedRecompilation())
      _dbgPrintf("\tHas Failed Recompilation\n");
   if(linkageInfo->recompilationAttempted())
      _dbgPrintf("\tRecompilation Attempted\n");
   if(linkageInfo->isBeingCompiled())
      _dbgPrintf("\tIs Being Compiled\n");

   _dbgPrintf("\n");
   TR_Debug::printJ9JITExceptionTableDetails(localMetadata, remoteMetadata);


   if (localCodeCacheMethodHeader != NULL) dxFree(localCodeCacheMethodHeader);
   if (localMetadata != NULL)              dxFree(localMetadata);
   if (localClassNameStruct != NULL)       dxFree(localClassNameStruct);
   if (localMethodNameStruct != NULL)      dxFree(localMethodNameStruct);
   if (localMethodSignatureStruct != NULL) dxFree(localMethodSignatureStruct);
   if (className != NULL)                  dxFree(className);
   if (methodName != NULL)                 dxFree(methodName);
   if (methodSignature != NULL)            dxFree(methodSignature);
   if (localBodyInfo != NULL)              dxFree(localBodyInfo);
   if (linkageInfo != NULL)                dxFree(linkageInfo);
#else // ifdef J9VM_RAS_EYECATCHERS
   _dbgPrintf("dxPrintMethodName not implemented\n");
#endif
   }



/*
 * Printing Node struct
 */
void
TR_DebugExt::dxPrintNode(TR::Node *remoteNode)
   {
   _dbgPrintf("\tNode at 0x%p\n",remoteNode);

   TR::Node *localNode = (TR::Node*) dxMallocAndRead(sizeof(TR::Node), (void*)remoteNode);
   TR::Node *fullyLocalNode = dxAllocateLocalNode(remoteNode, true, true);

   _dbgPrintf("\tunion\n");
   _dbgPrintf("\t{\n");
   _dbgPrintf("\t\tTR::SymbolReference *_symbolReference = 0x%p\n",localNode->_unionPropertyA._symbolReference);
   _dbgPrintf("\t\tTR::TreeTop *_branchDestinationNode = 0x%p\n",localNode->_unionPropertyA._branchDestinationNode);
   _dbgPrintf("\t\tTR::Block *_block = 0x%p\n",localNode->_unionPropertyA._block);
   _dbgPrintf("\t\tint32_t _arrayStride = %d\n",localNode->_unionPropertyA._arrayStride);
   _dbgPrintf("\t\tTR::AutomaticSymbol *_pinningArrayPointer = 0x%p\n",localNode->_unionPropertyA._pinningArrayPointer);
   _dbgPrintf("\t}\n");


   _dbgPrintf("\tunion\n");
   _dbgPrintf("\t{\n");
   _dbgPrintf("\t\tuint16_t  _useDefIndex = %d\n", fullyLocalNode->getUseDefIndex());
   if (fullyLocalNode->getOpCodeValue() != TR::BBStart)
      _dbgPrintf("\t\tTR_Register *_register = 0x%p\n",fullyLocalNode->getRegister());
   _dbgPrintf("\t}\n");

   _dbgPrintf("\t&(TR_ByteCodeInfo _byteCodeInfo) = 0x%p\n",(char*)remoteNode +((char*)&(localNode->_byteCodeInfo) - (char*)localNode) );

   _dbgPrintf("\tunion\n");
   _dbgPrintf("\t{\n");
   _dbgPrintf("\t\tnCount_t _globalIndex = %d\n",localNode->getGlobalIndex());
   _dbgPrintf("\t}\n");


   _dbgPrintf("\trcount_t _referenceCount = %d\n",fullyLocalNode->getReferenceCount());
   _dbgPrintf("\tvcount_t _visitCount = %d\n",fullyLocalNode->getVisitCount());
   _dbgPrintf("\tuint16_t _numChildren = %d\n",localNode->_numChildren);
   _dbgPrintf("\tTR::ILOpCode _opCode = %s[%d]\n",TR_Debug::getName(localNode->_opCode),localNode->_opCode._opCode);
   _dbgPrintf("\tflags16_t _flags = 0x%x\n", localNode->_flags.getValue());

   _dbgPrintf("\tunion\n");
   _dbgPrintf("\t{\n");
   _dbgPrintf("\t\tint64_t _constValue = %d\n",localNode->_unionBase._constValue);
   _dbgPrintf("\t\tfloat _fpConstValue = %d\n", localNode->_unionBase._fpConstValue);
   _dbgPrintf("\t\tfloat _dpConstValue = %d\n", localNode->_unionBase._dpConstValue);
   _dbgPrintf("\t\tsize_t _offset = %d\n",localNode->_unionBase._unionedWithChildren._offset);
   _dbgPrintf("\t\t&(RelocationInfo _relocationInfo) = 0x%p\n",(char*)remoteNode +((char*)&(localNode->_unionBase._unionedWithChildren._relocationInfo) - (char*)localNode) );
   _dbgPrintf("\t\t&(GlobalRegisterInfo _globalRegisterInfo) = 0x%p\n",(char*)remoteNode +((char*)&(localNode->_unionBase._unionedWithChildren._globalRegisterInfo) - (char*)localNode) );
   _dbgPrintf("\t\t&(CaseInfo _caseInfo) = 0x%p\n",(char*)remoteNode +((char*)&(localNode->_unionBase._unionedWithChildren._caseInfo) - (char*)localNode) );
   _dbgPrintf("\t\t&(MonitorInfo _monitorInfo) = 0x%p\n",(char*)remoteNode +((char*)&(localNode->_unionBase._unionedWithChildren._monitorInfo) - (char*)localNode) );
   _dbgPrintf("\t\t&(TR::Node *_children[NUM_DEFAULT_CHILDREN]) = 0x%p\n",(char*)remoteNode +((char*)&(localNode->_unionBase._children) - (char*)localNode) );
   _dbgPrintf("\t}\n");

   TR_PrettyPrinterString output(this);
   nodePrintAllFlags(fullyLocalNode, output);
   _dbgPrintf("\tNode Flags: %s\n", output.getStr());

   dxFreeLocalNode(fullyLocalNode, true, true);
   dxFree(localNode);
   }

typedef struct TR_RelocationHeader
   {
   uintptr_t size;
   } TR_RelocationHeader;

typedef struct TR_RelocationRecordHeader
   {
   uint16_t size;
   uint8_t  type;
   uint8_t  flags;
   } TR_RelocationRecordHeader;

typedef struct TR_RelocationRecordHeaderPadded
   {
   uint16_t size;
   uint8_t  type;
   uint8_t  flags;
#if defined(TR_HOST_64BIT)
   uint32_t padding; /* 4 bytes of automatic padding */
#endif
   } TR_RelocationRecordHeaderPadded;

typedef struct TR_RelocationRecordWithOffset
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t offset;
   } TR_RelocationRecordWithOffset;

typedef TR_RelocationRecordWithOffset TR_RelocationRecordJavaVMFieldOffset;
typedef TR_RelocationRecordWithOffset TR_RelocationRecordAbsoluteMethodAddressFromOffset;
typedef TR_RelocationRecordWithOffset TR_RelocationRecordGlobalValue;

typedef struct TR_RelocationRecordHelperAddress
   {
   uint16_t size;
   uint8_t  type;
   uint8_t  flags;
   uint32_t helperID;
   } TR_RelocationRecordHelperAddress;

typedef struct TR_RelocationRecordConstantPool
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t inlinedSiteIndex;
   uintptr_t constantPool;
   } TR_RelocationRecordConstantPool;

typedef struct TR_RelocationRecordGeneric
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t inlinedSiteIndex;
   uintptr_t constantPool;
   } TR_RelocationRecordGeneric;

typedef struct TR_RelocationRecordThunksJXE
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t constantPool;
   uintptr_t thunkAddress;
   } TR_RelocationRecordThunksJXE;

typedef struct TR_RelocationRecordPicTrampolines
   {
   TR_RelocationRecordHeader hdr;
   uint32_t numTrampolines;
   } TR_RelocationRecordPicTrampolines;

typedef struct TR_RelocationRecordMethodEnterExitTracing
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t destinationAddress;
   } TR_RelocationRecordMethodEnterExitCheck;

typedef struct TR_RelocationRecordValidateMethod
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t inlinedSiteIndex;
   uintptr_t constantPool;
   uintptr_t cpIndex;
   uintptr_t destinationAddress;
   uintptr_t romClassOffsetInSharedCache;
   } TR_RelocationRecordValidateMethod;

typedef struct TR_RelocationRecordProfiledClassGuard
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t inlinedSiteIndex;
   uintptr_t constantPool;
   uintptr_t cpIndex;
   uintptr_t destinationAddress;
   uintptr_t romClassOffsetInSharedCache;
   uintptr_t classChainIdentifyingClassLoader;
   } TR_RelocationRecordProfiledClassGuard;

typedef struct TR_RelocationRecordProfiledMethodGuard
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t inlinedSiteIndex;
   uintptr_t constantPool;
   uintptr_t cpIndex;
   uintptr_t destinationAddress;
   uintptr_t romClassOffsetInSharedCache;
   uintptr_t classChainIdentifyingClassLoader;
   uintptr_t vtableSlot;
   } TR_RelocationRecordProfiledMethodGuard;

typedef struct TR_RelocationRecordClassAlloc
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t inlinedSiteIndex;
   uintptr_t constantPool;
   uintptr_t cpIndex;
   uintptr_t branchOffset;
   uintptr_t allocationSize;
   } TR_RelocationRecordClassAlloc;

typedef struct TR_RelocationRecordReferenceArray
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t inlinedSiteIndex;
   uintptr_t constantPool;
   uintptr_t cpIndex;
   uintptr_t branchOffset;
   } TR_RelocationRecordReferenceArray;

typedef struct TR_RelocationRecordValidateField
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t inlinedSiteIndex;
   uintptr_t constantPool;
   uintptr_t cpIndex;
   uintptr_t classChainOffsetInSharedCache;
   } TR_RelocationRecordValidateField;

typedef struct TR_RelocationRecordStaticField
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t inlinedSiteIndex;
   uintptr_t constantPool;
   uintptr_t cpIndex;
   uintptr_t offset;
   } TR_RelocationRecordStaticField;


typedef struct TR_RelocationClassAddress
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t inlinedSiteIndex;
   uintptr_t constantPool;
   uintptr_t cpIndex;
   } TR_RelocationClassAddress;

typedef struct TR_RelocationRecordMethodCallAddress
   {
   TR_RelocationRecordHeaderPadded hdr;
   uintptr_t methodAddress;
   } TR_RelocationRecordMethodCallAddress;

typedef TR_RelocationRecordMethodEnterExitCheck TR_RelocationRecordUnresolvedAddressMaterializationHCR;

typedef TR_RelocationRecordConstantPool TR_RelocationRecordThunks;
typedef TR_RelocationRecordConstantPool TR_RelocationRecordTrampolines;
typedef TR_RelocationClassAddress TR_RelocationRecordJ2IThunk;


void
TR_DebugExt::dxPrintAOTinfo(void *addr)
   {
   _dbgPrintf("\tAOT information stored at 0x%p\n",addr);

   J9JITDataCacheHeader *dataCacheHeader = (J9JITDataCacheHeader *) dxMallocAndRead(sizeof(J9JITDataCacheHeader), addr);
   U_32 size = dataCacheHeader->size;
   TR_AOTMethodHeader *aotMethodHeaderEntry = (TR_AOTMethodHeader *) dxMallocAndRead(size-sizeof(J9JITDataCacheHeader), (U_8*)addr + sizeof(J9JITDataCacheHeader));
   if (aotMethodHeaderEntry->offsetToRelocationDataItems == 0)
      {
      _dbgPrintf("No relocation records found\n");
      return;
      }

   // Print info from the meta data
   J9JITDataCacheHeader *exceptionTableCacheEntry = (J9JITDataCacheHeader *)dxMallocAndRead(sizeof(J9JITDataCacheHeader) + sizeof(J9JITExceptionTable), (U_8 *)addr + aotMethodHeaderEntry->offsetToExceptionTable);
   J9JITExceptionTable *exceptionTable = (J9JITExceptionTable *) (exceptionTableCacheEntry + 1);

   _dbgPrintf("%-20s", "startPC");
   _dbgPrintf("%-20s", "endPC");
   _dbgPrintf("%-10s", "size");
   _dbgPrintf("%-14s", "gcStackAtlas");
   _dbgPrintf("%-20s\n", "bodyInfo");

   _dbgPrintf("%-16p    ", exceptionTable->startPC);
   _dbgPrintf("%-16p    ", exceptionTable->endPC);
   _dbgPrintf("%-10x", exceptionTable->size);
   _dbgPrintf("%-14x", exceptionTable->gcStackAtlas);
   _dbgPrintf("%-16p\n", exceptionTable->bodyInfo);

   _dbgPrintf("%-20s", "CodeStart");
   _dbgPrintf("%-20s", "DataStart");
   _dbgPrintf("%-10s", "CodeSize");
   _dbgPrintf("%-10s", "DataSize");
   _dbgPrintf("%-20s\n", "inlinedCalls");

   _dbgPrintf("%-16p    ",aotMethodHeaderEntry->compileMethodCodeStartPC);
   _dbgPrintf("%-16p    ",aotMethodHeaderEntry->compileMethodDataStartPC);
   _dbgPrintf("%-10x",aotMethodHeaderEntry->compileMethodCodeSize);
   _dbgPrintf("%-10x",aotMethodHeaderEntry->compileMethodDataSize);
   _dbgPrintf("%-16p\n", exceptionTable->inlinedCalls);


   TR_RelocationHeader *header = (TR_RelocationHeader *) dxMallocAndRead(sizeof(TR_RelocationHeader), (U_8 *)addr + aotMethodHeaderEntry->offsetToRelocationDataItems);
   TR_RelocationRecordHeader *firstRecord = (TR_RelocationRecordHeader *) dxMallocAndRead(header->size - sizeof(TR_RelocationHeader), (U_8 *)addr + aotMethodHeaderEntry->offsetToRelocationDataItems+sizeof(TR_RelocationHeader));
   TR_RelocationRecordHeader *endRecord = (TR_RelocationRecordHeader *) ((U_8 *) firstRecord + header->size - sizeof(TR_RelocationHeader));
   TR_RelocationRecordHeader *reloRecord = firstRecord;

   _dbgPrintf("Size: %x, Header: %p, firstRecord: %p, End Record: %p, sizeof reloheader: %x\t\n", header->size, header, firstRecord, endRecord, sizeof(TR_RelocationHeader));

   while (reloRecord < endRecord)
      {
      U_8 recordType = reloRecord->type;
      U_16 recordSize = reloRecord->size;
      U_8* ptr = (U_8*) reloRecord;
      U_8 *recordEnd = ptr + recordSize;

      _dbgPrintf("%20s\t", TR::ExternalRelocation::getName((TR_ExternalRelocationTargetKind)(recordType & TR_ExternalRelocationTargetKindMask)));

      switch(recordType & TR_ExternalRelocationTargetKindMask)
         {
         case TR_HelperAddress:
         case TR_AbsoluteHelperAddress:
            {
            TR_RelocationRecordHelperAddress *record = (TR_RelocationRecordHelperAddress *) reloRecord;
            _dbgPrintf("0x%-16x", record->helperID);
            ptr = (U_8*) (record+1);
            }
            break;
         case TR_AbsoluteMethodAddress:
         case TR_AbsoluteMethodAddressOrderedPair:
#if defined(TR_HOST_32BIT)
         case TR_ArrayCopyHelper:
         case TR_ArrayCopyToc:
#endif
         case TR_BodyInfoAddress:
         case TR_RamMethod:
         case TR_RamMethodSequence:
         case TR_RamMethodSequenceReg:
         case TR_BodyInfoAddressLoad:
            _dbgPrintf("No additional fields");
            ptr = (U_8*) (((UDATA*)reloRecord)+1);
            break;
         case TR_DataAddress:
            {
            TR_RelocationRecordStaticField *record = (TR_RelocationRecordStaticField *) reloRecord;
            _dbgPrintf("0x%-16x  0x%-16x  0x%-16x  0x%-16x", record->inlinedSiteIndex, record->constantPool, record->cpIndex, record->offset);
            ptr = (U_8*) (record+1);
            }
            break;
         case TR_MethodObject:
         case TR_Thunks:
         case TR_Trampolines:
         case TR_ConstantPool:
         case TR_ConstantPoolOrderedPair:
            {
            TR_RelocationRecordConstantPool *record = (TR_RelocationRecordConstantPool *) reloRecord;
            _dbgPrintf("0x%-16x  0x%-16x", record->inlinedSiteIndex, record->constantPool);
            ptr = (U_8*) (record+1);
            }
         break;
         case TR_FixedSequenceAddress:
         case TR_FixedSequenceAddress2:
#if defined(TR_HOST_64BIT)
         case TR_ArrayCopyHelper:
         case TR_ArrayCopyToc:
#endif
         case TR_VerifyRefArrayForAlloc:
         case TR_GlobalValue:
            {
            TR_RelocationRecordWithOffset *record = (TR_RelocationRecordWithOffset *) reloRecord;
            _dbgPrintf("0x%-16x", record->offset);
            ptr = (U_8*) (record+1);
            }
         break;
         case TR_PicTrampolines:
            {
            TR_RelocationRecordPicTrampolines *record = (TR_RelocationRecordPicTrampolines *) reloRecord;
            _dbgPrintf("0x%-16x", record->numTrampolines);
            ptr = (U_8*) (record+1);
            }
         break;
         case TR_CheckMethodEnter:
         case TR_CheckMethodExit:
            {
            TR_RelocationRecordMethodEnterExitTracing *record = (TR_RelocationRecordMethodEnterExitTracing *) reloRecord;
            _dbgPrintf("0x%-16x", record->destinationAddress);
            ptr = (U_8*) (record+1);
            }
         break;
         case TR_VerifyClassObjectForAlloc:
            {
            TR_RelocationRecordClassAlloc *record = (TR_RelocationRecordClassAlloc *) reloRecord;
            _dbgPrintf("0x%-16x  0x%-16x  0x%-16x", record->inlinedSiteIndex, record->constantPool, record->cpIndex, record->branchOffset, record->allocationSize);
            ptr = (U_8*) (record+1);
            }
         break;
         case TR_InlinedStaticMethodWithNopGuard:
         case TR_InlinedSpecialMethodWithNopGuard:
         case TR_InlinedVirtualMethodWithNopGuard:
         case TR_InlinedInterfaceMethodWithNopGuard:
         case TR_InlinedHCRMethod:
            {
            TR_RelocationRecordValidateMethod *record = (TR_RelocationRecordValidateMethod *) reloRecord;
            _dbgPrintf("0x%-16x  0x%-16x  0x%-16x", record->inlinedSiteIndex, record->constantPool, record->cpIndex, record->destinationAddress, record->romClassOffsetInSharedCache);
            ptr = (U_8*) (record+1);
            }
         break;
         case TR_ValidateInstanceField:
         case TR_ValidateStaticField:
         case TR_ValidateClass:
            {
            TR_RelocationRecordValidateField *record = (TR_RelocationRecordValidateField *) reloRecord;
            _dbgPrintf("0x%-16x  0x%-16x  0x%-16x  0x%-16x", record->inlinedSiteIndex, record->constantPool, record->cpIndex, record->classChainOffsetInSharedCache);
            ptr = (U_8*) (record+1);
            }
         break;
         case TR_ValidateArbitraryClass:
            {
            TR_RelocationClassAddress *record = (TR_RelocationClassAddress *) reloRecord;
            // faked out: these are actually the chain identifying loader and the class chain
            _dbgPrintf("0x%-16x  0x%-16x", record->inlinedSiteIndex, record->constantPool);
            }
         case TR_ClassAddress:
            {
            TR_RelocationClassAddress *record = (TR_RelocationClassAddress *) reloRecord;
            _dbgPrintf("0x%-16x  0x%-16x", record->inlinedSiteIndex, record->constantPool);
            ptr = (U_8*) (record+1);
            }
         break;
         case TR_MethodCallAddress:
            {
            TR_RelocationRecordMethodCallAddress *record = (TR_RelocationRecordMethodCallAddress *)reloRecord;
            _dbgPrintf("0x%-16x", record->methodAddress);
            ptr = (U_8*) (record+1);
            }
         break;
         case TR_RelativeMethodAddress:
            // unsupported
         default:
            _dbgPrintf("Unrecognized relocation record\n");
         }

      if (reloRecord->type & RELOCATION_TYPE_WIDE_OFFSET)
         {
         /* Offsets from startPC are 4 bytes */
         U_32 * reloOffset, i;
         for (i = 0, reloOffset = (U_32 *) ptr; reloOffset < (U_32 *)recordEnd; i++, reloOffset++)
            {
            if (i % 10 == 0)
               _dbgPrintf("\n\t\t\t");
            _dbgPrintf("0x%04x ", *reloOffset);
            }
         reloRecord = (TR_RelocationRecordHeader *) reloOffset;
         }
      else
         {
         /* Offsets from startPC are 2 bytes */
         U_16 * reloOffset, i;
         for (i = 0, reloOffset = (U_16 *) ptr; reloOffset < (U_16*)recordEnd; i++, reloOffset++)
            {
            if (i % 10 == 0)
               _dbgPrintf("\n\t\t\t");
            _dbgPrintf("0x%04x ", *reloOffset);
            }
         reloRecord = (TR_RelocationRecordHeader *) reloOffset;
         }
      _dbgPrintf("\n");
      }

   dxFree(dataCacheHeader);
   dxFree(aotMethodHeaderEntry);
   dxFree(exceptionTableCacheEntry);
   dxFree(header);
   dxFree(firstRecord);
   }


/*
 * The correct way of doing this is to ask the CodeCacheManager for the eyecatcher.
 */
#undef CODECACHE_WARM_EYE_CATCHER_DEPRECATED_DO_NOT_USE
#undef CODECACHE_COLD_EYE_CATCHER_DEPRECATED_DO_NOT_USE

#if defined(J9ZOS390)   // EBCDIC
#define CODECACHE_WARM_EYE_CATCHER_DEPRECATED_DO_NOT_USE {'\xD1','\xC9','\xE3','\xE6'}
#define CODECACHE_COLD_EYE_CATCHER_DEPRECATED_DO_NOT_USE {'\xD1','\xC9','\xE3','\xC3'}
#else                   // ASCII
#define CODECACHE_WARM_EYE_CATCHER_DEPRECATED_DO_NOT_USE {'J','I','T','W'}
#define CODECACHE_COLD_EYE_CATCHER_DEPRECATED_DO_NOT_USE {'J','I','T','C'}
#endif


OMR::CodeCacheMethodHeader *
TR_DebugExt::dxGet_CodeCacheMethodHeader(char *p, int32_t searchLimit)
   {

   char warmEyeCatcher[4] = CODECACHE_WARM_EYE_CATCHER_DEPRECATED_DO_NOT_USE;
   char coldEyeCatcher[4] = CODECACHE_COLD_EYE_CATCHER_DEPRECATED_DO_NOT_USE;

   searchLimit *= 1024; //convert to bytes
   p = p - (((UDATA) p) % 4);   //check if param address is multiple of 4 bytes, if not, shift

   OMR::CodeCacheMethodHeader *localCodeCacheMethodHeader = NULL;
   char *potentialEyeCatcher = NULL;

   int32_t sizeSearched = 0;
   int32_t size = sizeof(localCodeCacheMethodHeader->_eyeCatcher);  //size of eyecatcher and therefore the search interval as well

   int32_t codeCacheMethodHeaderSize = sizeof(OMR::CodeCacheMethodHeader);
   while (potentialEyeCatcher == NULL || (strncmp(potentialEyeCatcher, warmEyeCatcher, size) != 0 && strncmp(potentialEyeCatcher, coldEyeCatcher, size) != 0))
      {
      if (localCodeCacheMethodHeader != NULL)
         dxFree(localCodeCacheMethodHeader);
      if (sizeSearched >= searchLimit)
         {
         _dbgPrintf("dxPrintMethodMetadata - could not find eyecatcher within search limit size of %i\n", sizeSearched);
         return NULL;
         }

      localCodeCacheMethodHeader = (OMR::CodeCacheMethodHeader *) dxMalloc(codeCacheMethodHeaderSize, (OMR::CodeCacheMethodHeader *) p);
      bool isAllBytesRead = dxReadMemory(p, localCodeCacheMethodHeader, codeCacheMethodHeaderSize);
      if (isAllBytesRead)
         {
         potentialEyeCatcher = localCodeCacheMethodHeader->_eyeCatcher;
         }
      sizeSearched += size;
      p = p - size;
      }
   _dbgPrintf("Eye Catcher found after %i bytes: \t[0x%x]:\t%s\n", sizeSearched, &((OMR::CodeCacheMethodHeader *)(p + size))->_eyeCatcher, strncmp(potentialEyeCatcher, warmEyeCatcher, size) == 0 ? "JITW" : "JITC");
   return localCodeCacheMethodHeader;
   }

const char *
TR_DebugExt::dxGetSignature(J9UTF8 *className, J9UTF8 *name, J9UTF8 *signature)
   {
   J9UTF8 * localClassName = (struct J9UTF8 *) dxMallocAndRead(sizeof(J9UTF8), (void*)className);
   J9UTF8 * localName = (J9UTF8 *) dxMallocAndRead(sizeof(J9UTF8), (void*)name);
   J9UTF8 * localSignature = (J9UTF8 *) dxMallocAndRead(sizeof(J9UTF8), (void*)signature);

   int32_t classNameLen = J9UTF8_LENGTH(localClassName);
   int32_t nameLen = J9UTF8_LENGTH(localName);
   int32_t sigLen = J9UTF8_LENGTH(localSignature);
   J9UTF8 * localClassName2 = (J9UTF8 *) dxMallocAndRead(classNameLen+2, (void*)className);
   J9UTF8 * localName2 = (J9UTF8 *) dxMallocAndRead(nameLen+2, (void*)name);
   J9UTF8 * localSignature2 = (J9UTF8 *) dxMallocAndRead(sigLen+2, (void*)signature);

   int32_t len = classNameLen +  nameLen + sigLen + 3 ;

   char * s = (char *)dxMalloc(len,NULL);

   sprintf(s, "%.*s.%.*s%.*s", classNameLen,J9UTF8_DATA(localClassName2), nameLen, J9UTF8_DATA(localName2), sigLen, J9UTF8_DATA(localSignature2));
   dxFree(localClassName);
   dxFree(localName);
   dxFree(localSignature);
   dxFree(localClassName2);
   dxFree(localName2);
   dxFree(localSignature2);
   return s;
   }


const char *
TR_DebugExt::getMethodName(TR::SymbolReference * symRef)
   {
   TR_Method * method = symRef->getSymbol()->castToMethodSymbol()->getMethod();
   TR_J9MethodBase *localMethod = (TR_J9MethodBase *) dxMallocAndRead(sizeof(TR_J9MethodBase), (void*)method);
   //TR_J9MethodBase *localMethod = (TR_J9MethodBase *)symRef->getSymbol()->castToMethodSymbol()->getMethod();

   const char *s = dxGetSignature(localMethod->_className, localMethod->_name, localMethod->_signature);
   dxFree(localMethod);
   return s;
   }

const char *
TR_DebugExt::getMethodName(TR_OpaqueMethodBlock *mb)
   {
   struct J9Method *localRamMethod = (J9Method *) dxMallocAndRead(sizeof(J9Method), (J9Method*)mb);
   if (!localRamMethod)
      return NULL;

   struct J9ConstantPool *localCP = (J9ConstantPool *) dxMallocAndRead(sizeof(J9ConstantPool), localRamMethod->constantPool);
   if (!localCP)
      return NULL;

   struct J9Class *localRamClass = (J9Class *) dxMallocAndRead(sizeof(J9Class), localCP->ramClass);
   if (!localRamClass)
      return NULL;

   struct J9ROMClass *localRomClass = (J9ROMClass *) dxMallocAndRead(sizeof(J9ROMClass), localRamClass->romClass);
   if (!localRomClass)
      return NULL;

   intptrj_t localClassNameAddr = (intptrj_t)((intptrj_t)(localRamClass->romClass) + offsetof(J9ROMClass, className));
   struct J9UTF8 *localClassName = (struct J9UTF8 *)(localClassNameAddr + (intptrj_t)localRomClass->className);

   intptrj_t romMethod = (intptrj_t)J9_ROM_METHOD_FROM_RAM_METHOD(localRamMethod);
   intptrj_t localMethodNameSigAddr = (intptrj_t)(((J9ROMMethod *)romMethod) + offsetof(J9ROMMethod, nameAndSignature));

   struct J9ROMNameAndSignature *localMethodNameAndSig = (J9ROMNameAndSignature *) dxMallocAndRead(sizeof(J9ROMNameAndSignature), (J9ROMNameAndSignature *)localMethodNameSigAddr);
   if (!localMethodNameAndSig)
      return NULL;

   intptrj_t localMethodNameAddr = (intptrj_t)(localMethodNameSigAddr + localMethodNameAndSig->name + offsetof(J9ROMNameAndSignature, name));
   intptrj_t localMethodSigAddr  = (intptrj_t)(localMethodNameSigAddr + localMethodNameAndSig->signature + offsetof(J9ROMNameAndSignature, signature));
   struct J9UTF8 *localMethodName = (struct J9UTF8 *)(localMethodNameAddr);
   struct J9UTF8 *localMethodSig =  (struct J9UTF8 *)(localMethodSigAddr);


   const char *s = dxGetSignature(localClassName, localMethodName, localMethodSig);
   return s;
   }


void
TR_InternalFunctionsExt::fprintf(TR::FILE *file, const char *format, ...)
   {
   va_list args;
   va_start(args, format);
   char buf[1024];
   vsprintf(buf, format, args);
   _dxPrintf(buf);
   va_end(args);
   }

#endif // DEBUGEXT_C


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

#ifndef DEBUGEXT_INCL
#define DEBUGEXT_INCL

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/jittypes.h"
#include "infra/Assert.hpp"
#include "ras/HashTable.hpp"
#include "optimizer/Optimizations.hpp"
#include "ras/Debug.hpp"
#include "ras/InternalFunctionsExt.hpp"
#include "env/VMJ9.h"
#include "ras/DebugExtSegmentProvider.hpp"
#include "env/Region.hpp"

class TR_CHTable;
namespace TR { class CompilationInfo; }
namespace TR { class CompilationInfoPerThread; }
namespace TR { class CompilationInfoPerThreadBase; }
class TR_DataCache;
class TR_DataCacheManager;
class TR_FrontEnd;
class TR_InductionVariable;
class TR_InternalFunctions;
class TR_Memory;
class TR_MemorySegmentHeader;
class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;
class TR_OptimizationPlan;
class TR_PersistentCHTable;
class TR_PersistentJittedBodyInfo;
class TR_PersistentMemory;
class TR_PersistentMethodInfo;
class TR_PersistentProfileInfo;
class TR_RegionStructure;
class TR_ResolvedMethod;
namespace OMR { class RuntimeAssumption; }
class TR_RuntimeAssumptionTable;
class TR_Structure;
namespace TR { class MonitorTable; }
namespace OMR { struct CodeCacheFreeCacheBlock; }
namespace OMR { struct CodeCacheMethodHeader; }
namespace TR { class Block; }
namespace TR { class CFGEdge; }
namespace TR { class CFGNode; }
namespace TR { class CodeCache; }
namespace TR { class Node; }
namespace TR { class Optimizer; }
namespace TR { class PersistentInfo; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }
struct J9JITConfig;
struct J9JITExceptionTable;
struct J9JavaVM;
struct J9UTF8;
struct TR_MethodToBeCompiled;

#define TRPRINT_STRUCTURE_SUBGRAPH 1

struct FieldDescriptor
   {
   char *typeStr;    // "type"
   char *fieldStr;   // "field"
   int32_t  len;         // usually is sizeof(type)
   int32_t  offset;      // usually is offsetof(TR_Class, field)
   char typeId;      // one of b(ool),i(nteger),u(nsigned int),c(har),p(ointer),s(tring),a(ggregrate)
   char *commandStr; // The trprint command (eg: !tprint compilationinfo)
   };


class TR_J9VMExt : public TR_J9VM
   {
public:
   TR_J9VMExt(J9JITConfig * jitConfig, TR::CompilationInfo * compInfo, J9VMThread * vmContext)
      : TR_J9VM(jitConfig, compInfo, vmContext)
      {}

   virtual bool acquireVMAccessIfNeeded() { return true; }
   virtual void releaseVMAccessIfNeeded(bool haveAcquiredVMAccess) { return; }
   virtual bool haveAccess() { return true; }
   virtual bool haveAccess(TR::Compilation *comp) { return true; }
   };

#if defined(J9VM_OPT_SHARED_CLASSES) && defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
class TR_J9SharedCacheVMExt : public TR_J9SharedCacheVM
   {
public:
   TR_J9SharedCacheVMExt(J9JITConfig * jitConfig, TR::CompilationInfo * compInfo, J9VMThread * vmContext)
      : TR_J9SharedCacheVM(jitConfig, compInfo, vmContext)
      {}

   virtual bool acquireVMAccessIfNeeded() { return true; }
   virtual void releaseVMAccessIfNeeded(bool haveAcquiredVMAccess) { return; }
   virtual bool haveAccess() { return true; }
   virtual bool haveAccess(TR::Compilation *comp) { return true; }
   };
#endif

/***
 * debugger extensions class
 */
/* for describing the fields in TR class so that debugger extension can print them */
class TR_DebugExt : public TR_Debug
   {
public:
   void *operator new (size_t s, TR_Malloc_t dbgjit_Malloc);
   TR_DebugExt(
      TR_InternalFunctions * f,
      struct J9PortLibrary *dbgextPortLib,
      J9JavaVM *localVM,
      void (*dbgjit_Printf)(const char *s, ...),
      void (*dbgjit_ReadMemory)(uintptrj_t remoteAddr, void *localPtr, uintptrj_t size, uintptrj_t *bytesRead),
      void* (*dbgjit_Malloc)(uintptrj_t size, void *originalAddress),
      void (*dbgjit_Free)(void * addr),
      uintptrj_t (*dbgGetExpression)(const char* args)
      );
   /* entry points need virtual keywords */
   virtual void dxTrPrint(const char* name1, void* addr2, uintptrj_t argCount, const char* args);
   virtual void   setFile(TR::FILE *f) {_file = f;}
   // override these for TR_Debug
   virtual bool inDebugExtension() { return true; }
   virtual struct J9PortLibrary * getPortLib() { TR_ASSERT(_dbgextPortLib, "_dbgextPortLib should be initialized by dbgjit_TrInitialize()."); return _dbgextPortLib; }
   virtual const char * getName(const char *s, int32_t len);
   virtual const char * getName(const char *p);
   virtual const char * getName(TR::LabelSymbol *p) { return dxGetName("(TR::LabelSymbol*)", (void*) p); }
   virtual const char * getName(TR::SymbolReference *p) { return dxGetName("(TR::SymbolReference*)", (void*) p); }
   virtual const char * getName(TR::Node *p) { return dxGetName(0, (void*) p); }
   virtual const char * getName(TR::Symbol *p) { return dxGetName("(TR::Symbol*)", (void*) p); }
   virtual const char * getName(TR_Structure *p) { return dxGetName("(TR_Structure*)", (void*) p); }
   virtual const char * getName(TR::CFGNode *p) { return dxGetName("(TR::CFGNode*)", (void*) p); }
   virtual const char * getName(TR_ResolvedMethod *p) { return dxGetName("(TR_ResolvedMethod*)", (void*) p); }
   virtual const char * getName(TR_OpaqueClassBlock *p) { return dxGetName("(J9Class*)", (void*) p); }
   virtual void print(TR::FILE *, TR::SymbolReference *);
   virtual void print(TR::FILE *, TR::CFGNode *, uint32_t);
   virtual void print(TR::FILE *, TR::Block * block, uint32_t indentation);
   virtual void print(TR::FILE *, TR_Structure * structure, uint32_t indentation);
#ifdef TRPRINT_STRUCTURE_SUBGRAPH
   virtual void print(TR::FILE *, TR_RegionStructure * regionStructure, uint32_t indentation);
   virtual void printSubGraph(TR::FILE *, TR_RegionStructure * regionStructure, uint32_t indentation);
   virtual void print(TR::FILE *, TR_InductionVariable * inductionVariable, uint32_t indentation);
#endif
   virtual void printDestination(TR::FILE *, TR::TreeTop *);
   virtual void printDestination(TR::TreeTop *, TR_PrettyPrinterString&);
   virtual void printNodeInfo(TR::FILE *, TR::Node *);
   virtual void verifyTrees (TR::ResolvedMethodSymbol *s);
   virtual void verifyBlocks(TR::ResolvedMethodSymbol *s);
   virtual void dump(TR::FILE *, TR_CHTable *);
   virtual void printInlinedCallSites(TR::FILE *, TR::ResolvedMethodSymbol *);
   virtual const char * getMethodName(TR::SymbolReference *);
   const char * getMethodName(TR_OpaqueMethodBlock *);

private:
   struct seenNode;
   void updateLocalPersistentMemoryFunctionPointers(J9JITConfig *localJitConfig, TR_PersistentMemory *localPersistentMemory);
   /* jit extension cmd starts from these */
   void dxPrintUsage();
   void dxPrintPersistentJittedBodyInfo(TR_PersistentJittedBodyInfo *bodyInfo);
   void dxPrintMethodsBeingCompiled(TR::CompilationInfo *remoteCompInfo);
   TR::CompilationInfoPerThread ** dxMallocAndGetArrayOfCompilationInfoPerThread(uint8_t numThreads, void* addr);
   void dxPrintCompilationInfo(TR::CompilationInfo *remoteCompInfo);
   void dxPrintCompilationInfoPerThreadBase(TR::CompilationInfoPerThreadBase *remoteCompInfoPTB);
   void dxPrintCompilationInfoPerThread(TR::CompilationInfoPerThread *remoteCompInfoPT);
   void dxPrintMethodToBeCompiled(TR_MethodToBeCompiled *remoteCompEntry);
   void dxPrintCompilationTracingBuffer();
   void dxPrintOptimizationPlan(TR_OptimizationPlan *remoteOptimizationPlan);
   void dxPrintDataCache(TR_DataCache *remoteDataCache);
   void dxPrintPersistentInfo(TR::PersistentInfo *remotePersistentInfo);
   void dxPrintJ9MonitorTable(TR::MonitorTable *remoteMonTable);
   void dxPrintCompilation();
   void dxPrintTRMemory(TR_Memory*remoteTrMemory);
   void dxPrintPersistentMemory(TR_PersistentMemory *remotePersistentMemory);
   void dxPrintBlockIL(TR::Block *p, seenNode **seenNodes, int32_t numBlocks);
   void dxPrintBlockCFG(TR::Block *p);
   void dxPrintNodeIL(TR::Node *p, seenNode **seenNodes, int32_t indentation = 0);
   void dxPrintNode(TR::Node *p);
   void dxPrintMethodIL();
   void dxPrintCompilationIL();
   void dxPrintCFG(TR::CFG *p);
   void dxPrintCHTable(TR_CHTable *p);
   void dxPrintPersistentCHTable(TR_PersistentCHTable *p);
   void dxPrintCodeCacheInfo(TR::CodeCache *cacheInfo);
   OMR::CodeCacheFreeCacheBlock* dxPrintFreeCodeCacheBlock(OMR::CodeCacheFreeCacheBlock *blockPtr);
   void dxPrintFreeCodeCacheBlockList(TR::CodeCache *cacheInfo);
   void dxPrintListOfCodeCaches(void);
   void dxPrintDataCacheManager(TR_DataCacheManager *p);
   void dxPrintDataCacheSizeBucket(void *p); // Can't use TR_DataCacheManager::SizeBucket without full definition of TR_DataCacheManager.
   void dxPrintDataCacheAllocation(void *p); // Same thing but for TR_DataCacheManager::Allocation
   void dxPrintDataCacheAllocationListElement(void *p); // TR_DataCacheManager::InPlaceList<TR_DataCacheManager::Allocation>::ListElement
   void dxPrintDataCacheSizeBucketListElement(void *p); // TR_DataCacheManager::InPlaceList<TR_DataCacheManager::SizeBucket>::ListElement
   void dxPrintPersistentMethodInfo(TR_PersistentMethodInfo *p);
   void dxPrintPersistentProfileInfo(TR_PersistentProfileInfo *p);
   void dxPrintRuntimeAssumptionTable(TR_RuntimeAssumptionTable *p);
   void dxPrintRuntimeAssumptionArray(OMR::RuntimeAssumption **a, int32_t start, int32_t end);
   void dxPrintRuntimeAssumption(OMR::RuntimeAssumption *p);
   void dxPrintRuntimeAssumptionList(OMR::RuntimeAssumption *ra);
   void dxPrintRuntimeAssumptionListFromMetadata(J9JITExceptionTable *methodMetaData);
   void dxVerifyTrees();
   void dxVerifyBlocks();
   void dxVerifyCFG(TR::CFG *p);
   void* dxGetJ9MethodFromMethodToBeCompiled(TR_MethodToBeCompiled *methodToBeCompiled);
   void dxPrintJ9RamAndRomMethod(struct J9Method * ramMethod);
   void dxPrintMethodName(char *p, int32_t searchLimit = 32);
   void dxPrintInlinedCallSites(TR::ResolvedMethodSymbol *p);
   void dxPrintAOTinfo(void *addr);

   /*
    * helpers for jit extensions
    */
   bool         initializeDebug(TR::Compilation *remoteCompiler = NULL);
   void         allocateLocalCompiler(TR::Compilation *remoteCompiler);
   void         allocateLocalFrontEnd();
   void         freeLocalCompiler();
   void         freeLocalFrontEnd();
   bool         compInfosPerThreadAvailable();
   bool         activeMethodsToBeCompiledAvailable();
   bool         isAOTCompileRequested(TR::Compilation *);
   void         dxAllocateLocalBlockInternals(TR::Node *localNode);
   void         dxFreeLocalBlockInternals(TR::Node *localNode);
   void         dxAllocateSymRefInternals(TR::SymbolReference *localSymRef, bool complete);
   void         dxFreeSymRefInternals(TR::Symbol *localSymbol, bool complete);
   TR::Node*    dxAllocateLocalNode(TR::Node *remoteNode, bool recursive = false, bool complete = false);
   void         dxFreeLocalNode(TR::Node *localNode, bool recursive = false, bool complete = false);
   TR::TreeTop* dxAllocateLocalTreeTop(TR::TreeTop *remoteTreeTop, bool recursive = false);
   void         dxFreeLocalTreeTop(TR::TreeTop *localTreeTop, bool recursive = false);

   const char * dxGetName(const char *s, void *p);
   const char * dxGetSignature(J9UTF8 *className, J9UTF8 *name, J9UTF8 *signature);
   TR::CFG*      newCFG(TR::CFG *cfg);
   void         freeCFG(TR::CFG *localCfg);
   void*        newArray(uint32_t sizeofType, uint32_t size, void *array) { return dxMallocAndRead(sizeofType * size, array); }
   void         freeArray(void *array) { if (array) dxFree(array); }
   OMR::CodeCacheMethodHeader *dxGet_CodeCacheMethodHeader(char *p, int32_t searchLimit = 32);

   // helpers for searching and computing the jit's variables
   TR_PersistentMethodInfo*             Compilation2MethodInfo();
   TR_PersistentProfileInfo*            Compilation2ProfileInfo();
   TR_RuntimeAssumptionTable*           PersistentInfo2RuntimeAssumptionTable();
   TR_PersistentCHTable*                PersistentInfo2PersistentCHTable();
   TR_CHTable*                          PersistentInfo2CHTable();
   TR_PersistentMemory*                 J9JITConfig2PersistentMemory();
   TR::PersistentInfo *                PersistentMemory2PersistentInfo();
   TR_FrontEnd*                         J9JITConfig2FrontEnd();
   TR::CompilationInfo*                  FrontEnd2CompInfo(TR_FrontEnd * remoteFE);
   TR::CompilationInfoPerThread**        CompInfo2ArrayOfCompilationInfoPerThread(uint8_t *numThreads);
   TR::Optimizer*                       Compilation2Optimizer();
   TR::ResolvedMethodSymbol*           Optimizer2ResolvedMethodSymbol(TR::Optimizer *remoteOptimizer);
   TR::ResolvedMethodSymbol*           Compilation2ResolvedMethodSymbol();
   TR::CFG*                             Compilation2CFG();

   // for memory management
   bool  dxReadMemory(void* remotePtr, void* localPtr, uintptrj_t size);
   bool  dxReadField(void* classPtr, uintptrj_t fieldOffset, void* localPtr, uintptrj_t size);
   void* dxMalloc(uintptrj_t size, void *remotePtr, bool dontAddToMap = false);
   void* dxMallocAndRead(uintptrj_t size, void *remotePtr, bool dontAddToMap = false);
   void* dxMallocAndReadString(void *remotePtr, bool dontAddToMap = false);
   void  dxFree(void * localPtr);
   void  dxFreeAll();
   void  dxPrintMemory(void *p);

   struct seenNode
      {
      TR::Node *node;
      seenNode *next;
      };

   void freeSeenNodes(seenNode **seenNode);

   TR_InternalFunctions *_jit;

   /* j9's exports are stored here  */
   J9JavaVM            *_localVM;
   J9JITConfig         *_localJITConfig;
   struct J9PortLibrary *_dbgextPortLib;
   void  (*_dbgPrintf)(const char *s, ...);
   void  (*_dbgReadMemory)(uintptrj_t remoteAddr, void *localPtr, uintptrj_t size, uintptrj_t *bytesRead);
   void* (*_dbgMalloc)(uintptrj_t size, void *originalAddress);
   void  (*_dbgFree)(void *addr);
   uintptrj_t (*_dbgGetExpression)(const char* args);

   J9::DebugSegmentProvider _debugSegmentProvider;
   TR::Region _debugRegion;

   TR_HashTable *_toRemotePtrMap;
   TR_Memory *_debugExtTrMemory;

   /* Cache these since there are only one of these */
   TR::CompilationInfo *  _localCompInfo;
   TR::CompilationInfo *  _remoteCompInfo;
   TR::CompilationInfoPerThread ** _localCompInfosPT;
   TR_MethodToBeCompiled ** _localActiveMethodsToBeCompiled;

   TR_PersistentMemory * _localPersistentMemory;
   TR_PersistentMemory * _remotePersistentMemory;
   TR::PersistentInfo * _remotePersistentInfo;
   TR::PersistentInfo * _localPersistentInfo;

   /* memorize the last used option / setting */
   TR::Compilation *      _remoteCompiler;
   TR::Compilation *      _localCompiler;

   bool                  _showTypeInfo;
   bool                  _memchk;
   bool                 _isAOT;
   bool                  _structureValid;
   };

#endif

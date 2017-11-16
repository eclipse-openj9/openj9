/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef J9_PROFILER_INCL
#define J9_PROFILER_INCL

#include <stddef.h>                           // for NULL
#include <stdint.h>                           // for int32_t, uint32_t, etc
#include "codegen/FrontEnd.hpp"               // for TR_FrontEnd
#include "compile/Compilation.hpp"            // for Compilation
#include "env/TRMemory.hpp"                   // for TR_Memory, etc
#include "env/jittypes.h"                     // for uintptrj_t
#include "il/DataTypes.hpp"                   // for DataTypes
#include "il/Node.hpp"                        // for vcount_t
#include "infra/Flags.hpp"                    // for flags32_t
#include "infra/Link.hpp"                     // for TR_Link
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager
#include "env/VMJ9.h"
#include "infra/TRlist.hpp"

#define PROFILING_INVOCATION_COUNT (2)

class TR_ValueProfiler;
class TR_AbstractInfo;
class TR_BitVector;
class TR_BlockFrequencyInfo;
class TR_CallSiteInfo;
class TR_CatchBlockProfileInfo;
class TR_ExternalProfiler;
class TR_HWProfiler;
class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;
namespace TR { class Recompilation; }
class TR_ResolvedMethod;
class TR_Structure;
class TR_ValueProfileInfo;
namespace TR { class Block; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }
class TR_PersistentMethodInfo;
struct TR_ByteCodeInfo;
struct TR_InlinedCallSite;
template <typename ListKind> class List;


/**
 * Types of value profiling information.
 *
 * \TODO: All types of value info should likely end up in here.
 */
enum TR_ValueInfoType
   {
   Any,
   BigDecimal,
   String,
   NotBigDecimalOrString,
   };


//////////////////////////
// PersistentProfileInfos
//////////////////////////

class TR_PersistentProfileInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_PersistentProfileInfo(int32_t frequency, int32_t count)
      : _maxCount(count)
       ,_catchBlockProfileInfo(NULL),
        _blockFrequencyInfo(NULL),
        _valueProfileInfo(NULL),
        _callSiteInfo(NULL)

      {
      for (int i=0; i < PROFILING_INVOCATION_COUNT; i++)
         {
         _profilingFrequency[i] = frequency;
         _profilingCount[i] = (count/PROFILING_INVOCATION_COUNT);
         }
      }

   ~TR_PersistentProfileInfo();

   static TR_PersistentProfileInfo * get(TR::Compilation *comp);
   static TR_PersistentProfileInfo * get(TR_ResolvedMethod * vmMethod);

   int32_t &getProfilingFrequency() { return _profilingFrequency[0]; }
   int32_t *getProfilingFrequencyArray() { return _profilingFrequency; }
   void    setProfilingFrequency(int32_t f) {for (int i=0; i<PROFILING_INVOCATION_COUNT; i++) _profilingFrequency[i] = f;}
   void    setProfilingFrequency(int32_t *f) {for (int i=0; i<PROFILING_INVOCATION_COUNT; i++) _profilingFrequency[i] = f[i];}
   int32_t &getProfilingCount()     { return _profilingCount[0]; }
   int32_t *getProfilingCountArray(){ return _profilingCount; }
   void    setProfilingCount(int32_t c)
      {
      for (int i=0; i < PROFILING_INVOCATION_COUNT; i++)
         _profilingCount[i] = c/PROFILING_INVOCATION_COUNT;
      _maxCount = c;
      }
   void    setProfilingCount(int32_t *c) {for (int i=0; i<PROFILING_INVOCATION_COUNT; i++) _profilingCount[i] = c[i];}
   int32_t getMaxCount() { return _maxCount;}


   TR_CatchBlockProfileInfo * getCatchBlockProfileInfo()                               {return _catchBlockProfileInfo;}
   void                       setCatchBlockProfileInfo(TR_CatchBlockProfileInfo *info) {_catchBlockProfileInfo = info;}

   TR_BlockFrequencyInfo * getBlockFrequencyInfo()                            {return _blockFrequencyInfo;}
   void                    setBlockFrequencyInfo(TR_BlockFrequencyInfo *info) {_blockFrequencyInfo = info;}

   TR_ValueProfileInfo   * getValueProfileInfo()                            {return _valueProfileInfo;}
   void                    setValueProfileInfo(TR_ValueProfileInfo *info) {_valueProfileInfo = info;}

   TR_CallSiteInfo       * getCallSiteInfo()                      {return _callSiteInfo;}
   void                    setCallSiteInfo(TR_CallSiteInfo *info) {_callSiteInfo = info;}

   // This function will set CatchBlockProfileInfo, BlockFrequencyInfo, and ValueProfileInfo
   // to NULL
   void                    clearInfo() {setCatchBlockProfileInfo(NULL);
                                        setBlockFrequencyInfo(NULL);
                                        setValueProfileInfo(NULL);}

   #if defined(DEBUG)
      void dumpInfo(TR_FrontEnd *, TR::FILE *, const char *);
   #else
      void dumpInfo(TR_FrontEnd *, TR::FILE *, const char *) { }
   #endif

   private:

   TR_CallSiteInfo          *_callSiteInfo;
   TR_CatchBlockProfileInfo *_catchBlockProfileInfo;
   TR_BlockFrequencyInfo    *_blockFrequencyInfo;
   TR_ValueProfileInfo      *_valueProfileInfo;

   // NOTE: if the element size of the following two arrays changes
   // one have to adjust the code in createProfiledMethod() in ProfilerGenerator.cpp
   // Currently the code assumes that the element size is 4 bytes (sizeof(int32_t))
   int32_t _profilingFrequency[PROFILING_INVOCATION_COUNT];
   int32_t _profilingCount[PROFILING_INVOCATION_COUNT];

   int32_t _maxCount;
   };


///////////////////////////
// TR_RecompilationModifier
///////////////////////////

class TR_RecompilationModifier : public TR::Optimization
   {
   public:

   TR_RecompilationModifier(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_RecompilationModifier(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   TR::Recompilation *_recompilation;
   };


///////////////////////////
// Recompilation Profilers
///////////////////////////

class TR_RecompilationProfiler : public TR_Link<TR_RecompilationProfiler>
   {
   public:

   TR_RecompilationProfiler(TR::Compilation * c, TR::Recompilation * r, bool initialComp = false)
      : _compilation(c), _recompilation(r), _trMemory(c->trMemory()) { }

   virtual void modifyTrees() { }
   virtual void removeTrees() { }

   bool getHasModifiedTrees()       { return _flags.testAny(treesModified); }
   void setHasModifiedTrees(bool b) { _flags.set(treesModified, b); }

   bool getInitialCompilation()        { return _flags.testAny(initialCompilation); }
   void setInitialiCompilation(bool b) { _flags.set(initialCompilation, b); }

   virtual TR_ValueProfiler * asValueProfiler() {return NULL;}

   protected:

   TR::Compilation   * _compilation;
   TR::Recompilation * _recompilation;
   TR_Memory        * _trMemory;

   TR::Compilation *          comp()                        { return _compilation; }

   TR_Memory *               trMemory()                    { return _trMemory; }
   TR_StackMemory            trStackMemory()               { return _trMemory; }
   TR_HeapMemory             trHeapMemory()                { return _trMemory; }
   TR_PersistentMemory *     trPersistentMemory()          { return _trMemory->trPersistentMemory(); }

   enum
      {
      treesModified            = 0x00000001,
      initialCompilation       = 0x00000002,
      dummyLastEnum
      };
   flags32_t          _flags;
   };

class TR_LocalRecompilationCounters : public TR_RecompilationProfiler
   {
   public:

   TR_LocalRecompilationCounters(TR::Compilation  * c, TR::Recompilation * r)
      : TR_RecompilationProfiler(c, r) { }

   virtual void modifyTrees();
   virtual void removeTrees();
   };

class TR_GlobalRecompilationCounters : public TR_LocalRecompilationCounters
   {
   public:

   TR_GlobalRecompilationCounters(TR::Compilation * c, TR::Recompilation * r)
      : TR_LocalRecompilationCounters(c, r) { }

   virtual void modifyTrees();

   private:

   void examineStructure(TR_Structure *, TR_BitVector &);
   };

class TR_CatchBlockProfiler : public TR_RecompilationProfiler
   {
   public:

   TR_CatchBlockProfiler(TR::Compilation  * c, TR::Recompilation * r, bool initialCompilation = false);

   virtual void modifyTrees();
   virtual void removeTrees();

   private:

   TR_CatchBlockProfileInfo * findOrCreateProfileInfo();

   TR_CatchBlockProfileInfo * _profileInfo;
   TR::SymbolReference * _catchCounterSymRef;
   TR::SymbolReference * _throwCounterSymRef;
   };

class TR_BlockFrequencyProfiler : public TR_RecompilationProfiler
   {
   public:

   TR_BlockFrequencyProfiler(TR::Compilation  * c, TR::Recompilation * r);

   virtual void modifyTrees();
   };

class TR_ValueProfiler : public TR_RecompilationProfiler
   {
   private:

   TR_ValueProfileInfo * findOrCreateValueProfileInfo();
   TR_ValueProfileInfo * _valueProfileInfo;
   TR_OpaqueClassBlock * _bdClass;
   TR_OpaqueClassBlock * _stringClass;

   public:

   TR_ValueProfiler(TR::Compilation  * c, TR::Recompilation * r, bool initialCompilation = false)
       : TR_RecompilationProfiler(c, r, initialCompilation),
         _valueProfileInfo(NULL), _bdClass(NULL), _stringClass(NULL)
      {
      findOrCreateValueProfileInfo();
      }

   void setValueProfileInfo(TR_ValueProfileInfo *valueProfileInfo)
      {
      _valueProfileInfo = valueProfileInfo;
      }

   virtual TR_ValueProfiler * asValueProfiler() {return this;}
   virtual void modifyTrees();
   void visitNode(TR::Node *, TR::TreeTop *, vcount_t);
   void addProfilingTrees(TR::Node *, TR::TreeTop *, TR_AbstractInfo *valueInfo = NULL, bool commonNode = false, int32_t numExpandedValues = 0, bool decrmentRecompilationCounter = false, bool doBigDecimalProfiling = false, bool doStringProfiling = false);
   static TR_AbstractInfo *getProfiledValueInfo(TR::Node *, TR::Compilation*, TR_ValueInfoType type = NotBigDecimalOrString);
   };




/**
 * The ValueProfileInfoManager serves as a facade for multiple sources of value
 * profiling information.
 *
 */
class TR_ValueProfileInfoManager
   {
   public:
   TR_ALLOC(TR_Memory::ValueProfileInfoManager)

   /**
    * Source selection types.
    */
   enum
      {
      allProfileInfoKinds              = 0x00000001, ///< Search all profile info types
      justJITProfileInfo               = 0x00000002, ///< Only return JIT Profile info
      justInterpreterProfileInfo       = 0x00000003  ///< Only return IProfiler info
      };

   TR_ValueProfileInfoManager(TR::Compilation *comp);

   /**
    * Return TR_AbstractInfo that can be downcast to a particular kind of value profiling info.
    *
    * @param node               The node for which value info is desired.
    * @param comp               Compilation
    * @param profileInfoKind    Source selection.
    * @param type               Acceptable return record types.
    */
   TR_AbstractInfo *getValueInfo(TR::Node *node,
                                 TR::Compilation *comp ,
                                 uint32_t profileInfoKind = allProfileInfoKinds,
                                 TR_ValueInfoType type = NotBigDecimalOrString);
   /**
    * Return TR_AbstractInfo that can be downcast to a particular kind of value profiling info.
    *
    * @param bcInfo             The ByteCodeInfo for which info is desired.
    * @param comp               Compilation
    * @param profileInfoKind    Source selection.
    * @param type               Acceptable return record types.
    * @param useHWProfile       Use HW profiling info.
    *
    */
   TR_AbstractInfo *getValueInfo(TR_ByteCodeInfo & bcInfo,
                                 TR::Compilation *comp,
                                 uint32_t profileInfoKind = allProfileInfoKinds,
                                 TR_ValueInfoType type = NotBigDecimalOrString);

   void             updateCallGraphProfilingCount(TR::Block *block, TR_OpaqueMethodBlock *method, int32_t byteCodeIndex, TR::Compilation *comp);
   bool             isColdCall(TR_OpaqueMethodBlock *method, int32_t byteCodeIndex, TR::Compilation *comp);
   bool             isColdCall(TR_OpaqueMethodBlock *calleeMethod, TR_OpaqueMethodBlock *method, int32_t byteCodeIndex, TR::Compilation *comp);
   bool             isColdCall(TR::Node *node, TR::Compilation *comp);
   bool             isWarmCallGraphCall(TR::Node *node, TR_OpaqueMethodBlock *method, TR::Compilation *comp);
   bool             isWarmCall(TR::Node *node, TR::Compilation *comp);
   bool             isCallGraphProfilingEnabled(TR::Compilation *comp);
   int32_t          getCallGraphProfilingCount(TR::Node *node, TR_OpaqueMethodBlock *method, TR::Compilation *comp);
   int32_t          getCallGraphProfilingCount(TR::Node *node, TR::Compilation *comp);
   int32_t          getCallGraphProfilingCount(TR_OpaqueMethodBlock *method, int32_t byteCodeIndex, TR::Compilation *comp);
   int32_t          getCallGraphProfilingCount(TR_OpaqueMethodBlock *calleeMethod, TR_OpaqueMethodBlock *method, int32_t byteCodeIndex, TR::Compilation *comp);
   float            getAdjustedInliningWeight(TR::Node *node, int32_t weight, TR::Compilation *comp);
   bool             isHotCall(TR::Node *node, TR::Compilation *comp);

   static inline TR_ValueProfileInfoManager * get(TR::Compilation *comp);

   void setJitValueProfileInfo(TR_ValueProfileInfo *v) { _jitValueProfileInfo = v; }

private:
   TR_ValueProfileInfo   *_jitValueProfileInfo;
   TR_BlockFrequencyInfo *_jitBlockFrequencyInfo;
   TR_OpaqueMethodBlock  *_cachedJ9Method;
   bool                   _isCountZero;
   };


TR_ValueProfileInfoManager *TR_ValueProfileInfoManager::get(TR::Compilation *comp)
   {
   if (!comp->getValueProfileInfoManager())
      {
      comp->setValueProfileInfoManager( new (comp->trHeapMemory()) TR_ValueProfileInfoManager (comp) );
      }
   return comp->getValueProfileInfoManager();
   }


class TR_BranchProfileInfoManager
   {
   public:
   TR_ALLOC(TR_Memory::BranchProfileInfoManager)
   TR_BranchProfileInfoManager(TR::Compilation *comp)
      {
      _iProfiler = (TR_ExternalProfiler *)comp->fej9()->getIProfiler();
      }

   TR_ExternalProfiler     *_iProfiler;

   static inline TR_BranchProfileInfoManager * get(TR::Compilation *comp);
   float getCallFactor(int32_t callSiteIndex, TR::Compilation *comp);
   void getBranchCounters(TR::Node *node, TR::TreeTop *block, int32_t *branchToCount, int32_t *fallThroughCount, TR::Compilation *comp);
   };


TR_BranchProfileInfoManager *TR_BranchProfileInfoManager::get(TR::Compilation *comp)
   {
   if (!comp->getBranchProfileInfoManager())
      {
      comp->setBranchProfileInfoManager( new (comp->trHeapMemory()) TR_BranchProfileInfoManager (comp) );
      }
   return comp->getBranchProfileInfoManager();
   }


class TR_MethodBranchProfileInfo
   {
public:
   TR_ALLOC(TR_Memory::MethodBranchProfileInfo)

   TR_MethodBranchProfileInfo (uint32_t callSiteIndex, TR::Compilation *comp)
      : _callSiteIndex(callSiteIndex), _factor(-1.0f), _initialBlockFrequency(0)
      {
      }

   static TR_MethodBranchProfileInfo * addMethodBranchProfileInfo (uint32_t callSiteIndex, TR::Compilation *comp);
   static TR_MethodBranchProfileInfo * getMethodBranchProfileInfo(uint32_t callSiteIndex, TR::Compilation *comp);
   static void resetMethodBranchProfileInfos(int32_t oldMaxFrequency, int32_t oldMaxEdgeFrequency, TR::Compilation *comp);

   uint32_t getCallSiteIndex() { return _callSiteIndex; }
   float    getCallFactor() { return _factor; }
   uint32_t getInitialBlockFrequency() { return _initialBlockFrequency; }

   void     setCallFactor(float factor) { _factor = factor; }
   void     setInitialBlockFrequency(uint32_t initialFrequency) { _initialBlockFrequency =  initialFrequency; }

public:
   int32_t  _oldMaxFrequency;
   int32_t  _oldMaxEdgeFrequency;

private:
   uint32_t _callSiteIndex;
   float    _factor;
   uint32_t _initialBlockFrequency;
   };


class TR_MethodValueProfileInfo
   {
   public:
   TR_ALLOC(TR_Memory::MethodValueProfileInfo)

   TR_MethodValueProfileInfo (TR_OpaqueMethodBlock *vmMethod, TR_ValueProfileInfo *vpInfo, TR::Compilation *comp)
      : _method(vmMethod), _vpInfo(vpInfo)
      {
      }

   static void addValueProfileInfo (TR_OpaqueMethodBlock *method, TR_ValueProfileInfo *vpInfo, TR::Compilation *comp);
   static TR_ValueProfileInfo *getValueProfileInfo(TR_OpaqueMethodBlock *method, TR::Compilation *comp);

   static void addHWValueProfileInfo (TR_OpaqueMethodBlock *method, TR_ValueProfileInfo *vpInfo, TR::Compilation *comp);
   static TR_ValueProfileInfo *getHWValueProfileInfo(TR_OpaqueMethodBlock *method, TR::Compilation *comp);

   TR_ValueProfileInfo *getValueProfileInfo() { return _vpInfo; }
   TR_OpaqueMethodBlock *getPersistentIdentifier() { return _method; }

   private:
   static TR_ValueProfileInfo *getValueProfileInfo(TR_OpaqueMethodBlock *method, TR::list<TR_MethodValueProfileInfo*> &infos, TR::Compilation *comp);
   TR_OpaqueMethodBlock *_method;
   TR_ValueProfileInfo *_vpInfo;
   };


class TR_ValueProfileInfo
   {
public:

   TR_ALLOC(TR_Memory::ValueProfileInfo)

   TR_ValueProfileInfo();

   TR_ValueProfileInfo(
         TR_AbstractInfo *values,
         TR_AbstractInfo *iProfilerValues,
         int32_t numValues,
         int32_t numiProfilerValues,
         TR_CallSiteInfo *callSites
        ,TR_ExternalProfiler *profiler
         ) :
      _values(values),
      _iProfilerValues(iProfilerValues),
      _numValues(numValues),
      _numiProfilerValues(numiProfilerValues),
      _callSites(callSites)
     ,_profiler(profiler)
      { }

   ~TR_ValueProfileInfo();

   static inline TR_ValueProfileInfo * get(TR_PersistentProfileInfo * profileInfo);
   static inline TR_ValueProfileInfo * get(TR_ResolvedMethod * vmMethod);
   static inline TR_ValueProfileInfo * get(TR::Compilation *comp);

   #ifdef DEBUG
      void dumpInfo(TR_FrontEnd *, TR::FILE *);
   #endif

   TR_AbstractInfo *getValueInfo(TR::Node *node, TR::Compilation *, TR_ValueInfoType type = NotBigDecimalOrString);
   TR_AbstractInfo *getValueInfo(TR_ByteCodeInfo & bcInfo, TR::Compilation *, TR_ValueInfoType type = NotBigDecimalOrString);
   TR_AbstractInfo *getValueInfoFromExternalProfiler(TR_ByteCodeInfo & bcInfo, TR::Compilation *);
   TR_AbstractInfo *createAndInitializeValueInfo(TR::Node *node, bool, TR::Compilation *, TR_AllocationKind allocKind, uintptrj_t initialValue, TR_ValueInfoType type = NotBigDecimalOrString);
   TR_AbstractInfo *createAndInitializeValueInfo(TR_ByteCodeInfo &bcInfo, TR::DataType dataType, bool, TR::Compilation *, TR_AllocationKind allocKind, uintptrj_t initialValue, uint32_t frequency = 0, bool externalProfilerValue = false, TR_ValueInfoType type = NotBigDecimalOrString, bool hwProfilerValue = false);
   TR_AbstractInfo *getOrCreateValueInfo(TR::Node *node, bool, TR::Compilation *, TR_ValueInfoType type = NotBigDecimalOrString);
   TR_AbstractInfo *getOrCreateValueInfo(TR::Node *node, bool warmCompilePICCallSite, TR_AllocationKind allocKind = persistentAlloc, uintptrj_t initialValue = 0xdeadf00d, TR::Compilation *comp = NULL, TR_ValueInfoType type = NotBigDecimalOrString);
   TR_AbstractInfo *getOrCreateValueInfo(TR_ByteCodeInfo &bcInfo, TR::DataType dataType, bool warmCompilePICCallSite, TR_AllocationKind allocKind = persistentAlloc, uintptrj_t initialValue = 0xdeadf00d, TR::Compilation *comp = NULL, TR_ValueInfoType type = NotBigDecimalOrString);

   void    setCallSiteInfo (TR_CallSiteInfo *callSiteInfo) {_callSites = callSiteInfo;}
   void    setExternalProfiler (TR_ExternalProfiler *externalProfiler) {_profiler = externalProfiler;}

   TR_AbstractInfo *getValues() { return _values;};  // for WCode
   void setValues(TR_AbstractInfo *v) {_values = v;} // for replay
private:

   TR_AbstractInfo *_values;
   TR_AbstractInfo *_iProfilerValues;
   TR_AbstractInfo *_hwProfilerValues;
   int32_t          _numValues;
   int32_t          _numiProfilerValues;
   TR_CallSiteInfo *_callSites;
   TR_ExternalProfiler *_profiler;
   };


TR_ValueProfileInfo * TR_ValueProfileInfo::get(TR_PersistentProfileInfo * profileInfo)
   {
   if (profileInfo && profileInfo->getValueProfileInfo())
      return profileInfo->getValueProfileInfo();

   return NULL;
   }

TR_ValueProfileInfo * TR_ValueProfileInfo::get(TR_ResolvedMethod * vmMethod)
   {
   return get(TR_PersistentProfileInfo::get(vmMethod));
   }

TR_ValueProfileInfo * TR_ValueProfileInfo::get(TR::Compilation *comp)
   {
   return get(TR_PersistentProfileInfo::get(comp));
   }


class TR_BlockFrequencyInfo
   {
   public:

   TR_ALLOC(TR_Memory::BlockFrequencyInfo)

   TR_BlockFrequencyInfo(TR::Compilation *comp, TR_AllocationKind allocKind);

   TR_BlockFrequencyInfo(TR_CallSiteInfo *callSiteInfo, int32_t numBlocks, TR_ByteCodeInfo *blocks, int32_t *frequencies);

   ~TR_BlockFrequencyInfo();

   static inline TR_BlockFrequencyInfo * get(TR_PersistentProfileInfo * profileInfo);
   static inline TR_BlockFrequencyInfo * get(TR_ResolvedMethod * vmMethod);
   static inline TR_BlockFrequencyInfo * get(TR::Compilation *comp);

   void   *getFrequencyForBlock(int32_t blockNum) {return &_frequencies[blockNum];}
   int32_t getFrequencyInfo(TR::Block *block, TR::Compilation *comp);
   int32_t getFrequencyInfo(TR_ByteCodeInfo &bci, TR::Compilation *comp, bool normalizeForCallers = true, bool trace = true);
   void    setCounterDerivationInfo(TR_BitVector **counterDerivationInfo) { _counterDerivationInfo = counterDerivationInfo; }
   void    setEntryBlockNumber(int32_t number) { _entryBlockNumber = number; }
   bool    isJProfilingData() { return _counterDerivationInfo != NULL; }
   static int32_t *getEnableJProfilingRecompilation() { return &_enableJProfilingRecompilation; }
   static void    enableJProfilingRecompilation() { _enableJProfilingRecompilation = -1; }

   #ifdef DEBUG
      void dumpInfo(TR_FrontEnd *, TR::FILE *);
   #endif

   int32_t getCallCount();
   int32_t getMaxRawCount(int32_t callerIndex);
   int32_t getMaxRawCount();

   private:
   int32_t getRawCount(TR::ResolvedMethodSymbol *resolvedMethod, TR_ByteCodeInfo &bci, TR_CallSiteInfo *callSiteInfo, int64_t maxCount, TR::Compilation *comp);
   int32_t getRawCount(TR_ByteCodeInfo &bci, TR_CallSiteInfo *callSiteInfo, int64_t maxCount, TR::Compilation *comp);

   TR_CallSiteInfo * _callSiteInfo;
   int32_t const _numBlocks;
   TR_ByteCodeInfo * const _blocks;
   int32_t         * const _frequencies;

   // counterDeriviationInfo is used by JProfiling to store which counters to add and subtract to derrive
   // the frequency of a basic block. A NULL entry represents no counters, a low tagged entry represents
   // a single block which can be obtained by right shifiting the entry by 1, otherwise it will be a pointer
   // to a TR_BitVector holding the counters to add together
   TR_BitVector    ** _counterDerivationInfo;
   int32_t         _entryBlockNumber;
   static int32_t  _enableJProfilingRecompilation;
   };


TR_BlockFrequencyInfo * TR_BlockFrequencyInfo::get(TR_PersistentProfileInfo * profileInfo)
   {
   return profileInfo ? profileInfo->getBlockFrequencyInfo() : 0;
   }
TR_BlockFrequencyInfo * TR_BlockFrequencyInfo::get(TR_ResolvedMethod * vmMethod)
   {
   return get(TR_PersistentProfileInfo::get(vmMethod));
   }
TR_BlockFrequencyInfo * TR_BlockFrequencyInfo::get(TR::Compilation *comp)
   {
   return get(TR_PersistentProfileInfo::get(comp));
   }


class TR_CatchBlockProfileInfo
   {
   public:

   TR_ALLOC(TR_Memory::CatchBlockProfileInfo)

   TR_CatchBlockProfileInfo() : _catchCounter(0), _throwCounter(0) { }

   static inline TR_CatchBlockProfileInfo * get(TR_PersistentProfileInfo * profileInfo);
   static inline TR_CatchBlockProfileInfo * get(TR_ResolvedMethod * vmMethod);
   static inline TR_CatchBlockProfileInfo * get(TR::Compilation *comp);

   static const uint32_t EDOThreshold;

   uint32_t & getCatchCounter() { return _catchCounter; }
   uint32_t & getThrowCounter() { return _throwCounter; }

   #ifdef DEBUG
      void dumpInfo(TR_FrontEnd *, TR::FILE *, const char *);
   #endif

   private:

   uint32_t _catchCounter;
   uint32_t _throwCounter;
   };


TR_CatchBlockProfileInfo * TR_CatchBlockProfileInfo::get(TR_PersistentProfileInfo * profileInfo)
   {
   return profileInfo ? profileInfo->getCatchBlockProfileInfo() : 0;
   }
TR_CatchBlockProfileInfo * TR_CatchBlockProfileInfo::get(TR_ResolvedMethod * vmMethod)
   {
   return get(TR_PersistentProfileInfo::get(vmMethod));
   }
TR_CatchBlockProfileInfo * TR_CatchBlockProfileInfo::get(TR::Compilation *comp)
   {
   return get(TR_PersistentProfileInfo::get(comp));
   }



class TR_CallSiteInfo
   {
   public:

   TR_ALLOC(TR_Memory::CallSiteInfo)

   TR_CallSiteInfo(TR::Compilation * comp, TR_AllocationKind allocKind);
   ~TR_CallSiteInfo();

   static inline TR_CallSiteInfo * get(TR_PersistentProfileInfo * profileInfo);
   static inline TR_CallSiteInfo * get(TR_ResolvedMethod * vmMethod);
   static inline TR_CallSiteInfo * get(TR::Compilation *comp);

   bool computeEffectiveCallerIndex(TR::Compilation *comp, TR::list<std::pair<TR_OpaqueMethodBlock *, TR_ByteCodeInfo> > &callStack, int32_t &effectiveCallerIndex);
   bool hasSameBytecodeInfo(TR_ByteCodeInfo & persistentByteCodeInfo, TR_ByteCodeInfo & currentByteCodeInfo, TR::Compilation *comp);
   int32_t hasSamePartialBytecodeInfo(TR_ByteCodeInfo & persistentBytecodeInfo, TR_ByteCodeInfo & currentBytecodeInfo, TR::Compilation *comp);
   size_t getNumCallSites() { return _numCallSites;}
   TR_OpaqueMethodBlock *inlinedMethod(TR_ByteCodeInfo & persistentByteCodeInfo, TR::Compilation *comp);

   #ifdef DEBUG
      void dumpInfo(TR_FrontEnd *, TR::FILE *);
   #endif
   TR_InlinedCallSite *getCallSites() { return _callSites; } // for WCode

   private:

   size_t const _numCallSites;
   TR_InlinedCallSite * const _callSites;
   TR_AllocationKind const _allocKind;
   };


TR_CallSiteInfo * TR_CallSiteInfo::get(TR_PersistentProfileInfo * profileInfo)
   {
   return profileInfo ? profileInfo->getCallSiteInfo() : 0;
   }

TR_CallSiteInfo * TR_CallSiteInfo::get(TR_ResolvedMethod * vmMethod)
   {
   return get(TR_PersistentProfileInfo::get(vmMethod));
   }

TR_CallSiteInfo * TR_CallSiteInfo::get(TR::Compilation *comp)
   {
   return get(TR_PersistentProfileInfo::get(comp));
   }

#endif

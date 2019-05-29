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

#ifndef IPROFILER_HPP
#define IPROFILER_HPP

/**
 * \page Iprofiling Interpreter Profiling 
 * 
 * The Interpreter Profiler, or IProfiler, is a mechanism created to extract 
 * knowledge from the interpreter to feed into our first compilation. 
 *
 * Structurally, the IProfiler consists of three main components: 
 *
 * - The Interpreter / Virtual Machine which creates the data. The creation
 *   of data is not free however, and so this data collection should be 
 *   possible to disable. 
 * - The IProfiler component consumes the raw data from the VM, parsing it 
 *   into a form consumable in the JIT compiler. 
 * - The JIT compiler, which consumes the data produced by the IProfiler 
 *   and may also query the IProfiler for specific data. 
 *   
 * Below is a rough diagram of the IProfiler system. 
 * 
 * 
 *             Control            Queries          
 *          +-----------+       +-------- --+      
 *          |           |       |           |      
 *          |           |       |           |      
 *     +----v-+       +-+-------v-+        ++-----+
 *     |      |       |           |        |      |
 *     | VM   |       | IProfiler |        | JIT  |
 *     |      |       |           |        |      |
 *     +----+-+       +-^-------+-+        +^-----+
 *          |           |       |           |      
 *          +-----------+       +-----------+      
 *            Raw Data            Processed       
 *                                  Data          
 *
 *
 * For further details see "Experiences in Designing a Robust and Scalable
 * Interpreter Profiling Framework" by Ian Gartley, Marius Pirvu, Vijay
 * Sundaresan and Nikola Grecvski, published in *Code Generation and Optimization (CGO)*, 2013. 
 */

#include "j9.h"
#include "j9cfg.h"
#include "env/jittypes.h"
#include "infra/Link.hpp"
#include "runtime/ExternalProfiler.hpp"

#undef EXPERIMENTAL_IPROFILER

class TR_BlockFrequencyInfo;
namespace TR { class CompilationInfo; }
class TR_FrontEnd;
class TR_IPBCDataPointer;
class TR_IPBCDataCallGraph;
class TR_IPBCDataFourBytes;
class TR_IPBCDataEightWords;
class TR_IPBCDataAllocation;
class TR_IPByteVector;
class TR_J9ByteCodeIterator;
class TR_ExternalValueProfileInfo;
class TR_OpaqueMethodBlock;
namespace TR { class PersistentInfo; }
namespace TR { class Monitor; }
struct J9Class;
struct J9PortLibrary;

#if defined (_MSC_VER)
extern "C" __declspec(dllimport) void __stdcall DebugBreak();
#if _MSC_VER < 1600
#error "Testarossa requires atleast MSVC10 aka Win SDK 7.1 aka Visual Studio 2010"
#endif
#endif

struct TR_IPHashedCallSite  // TODO: is this needed?
   {
   //why do we even need this anymore?
   TR_PERSISTENT_ALLOC(TR_Memory::IPHashedCallSite)
   void * operator new (size_t size) throw();
   //
   TR_IPHashedCallSite () : _method(NULL), _offset(0) {};
   TR_IPHashedCallSite (J9Method* method, uint32_t offset) : _method(method), _offset(offset) {};

   J9Method* _method;
   uint32_t _offset;
   };


#define SWITCH_DATA_COUNT 4
#define NUM_CS_SLOTS 3
class CallSiteProfileInfo
   {
public:
   uint16_t _weight[NUM_CS_SLOTS];
   uint16_t _residueWeight:15;
   uint16_t _tooBigToBeInlined:1;

   void initialize()
         {
         for (int i = 0; i < NUM_CS_SLOTS; i++)
            {
            _clazz[i]=0;
            _weight[i]=0;
            }
         _residueWeight=0;
         _tooBigToBeInlined=0;
         }

   uintptrj_t getClazz(int index);
   void setClazz(int index, uintptrj_t clazzPtr);

private:
#if defined(OMR_GC_COMPRESSED_POINTERS) //compressed references
   uint32_t _clazz[NUM_CS_SLOTS]; // store them in 32bits
#else
   uintptrj_t _clazz[NUM_CS_SLOTS]; // store them in either 64 or 32 bits
#endif //OMR_GC_COMPRESSED_POINTERS
   };

#define TR_IPBCD_FOUR_BYTES  1
#define TR_IPBCD_EIGHT_WORDS 2
#define TR_IPBCD_CALL_GRAPH  3


// We rely on the following structures having the same first 4 fields
typedef struct TR_IPBCDataStorageHeader
   {
   uint32_t pc;
   uint32_t left:8;
   uint32_t right:16;
   uint32_t ID:8;
   } TR_IPBCDataStorageHeader;

typedef struct TR_IPBCDataFourBytesStorage
   {
   TR_IPBCDataStorageHeader header;
   uint32_t data;
   } TR_IPBCDataFourBytesStorage;

typedef struct TR_IPBCDataEightWordsStorage
   {
   TR_IPBCDataStorageHeader header;
   uint64_t data[SWITCH_DATA_COUNT];
   } TR_IPBCDataEightWordsStorage;

typedef struct TR_IPBCDataCallGraphStorage
   {
   TR_IPBCDataStorageHeader header;
   CallSiteProfileInfo _csInfo;
   } TR_IPBCDataCallGraphStorage;

enum TR_EntryStatusInfo
   {
   IPBC_ENTRY_CANNOT_PERSIST = 0,
   IPBC_ENTRY_CAN_PERSIST,
   IPBC_ENTRY_PERSIST_LOCK,
   IPBC_ENTRY_PERSIST_NOTINSCC,
   IPBC_ENTRY_PERSIST_UNLOADED
   };


#define TR_IPBC_PERSISTENT_ENTRY_READ  0x1 // used to check if the persistent entry has been read, if so we want to avoid the overhead introduced by calculating sample counts

// Hash table for bytecodes
class TR_IPBytecodeHashTableEntry
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::IProfiler)
   static void* alignedPersistentAlloc(size_t size);
   TR_IPBytecodeHashTableEntry(uintptrj_t pc) : _next(NULL), _pc(pc), _lastSeenClassUnloadID(-1), _entryFlags(0), _persistFlags(IPBC_ENTRY_CAN_PERSIST_FLAG) {}
    
   uintptrj_t getPC() const { return _pc; }
   TR_IPBytecodeHashTableEntry * getNext() const { return _next; }
   void setNext(TR_IPBytecodeHashTableEntry *n) { _next = n; }
   int32_t getLastSeenClassUnloadID() const { return _lastSeenClassUnloadID; }
   void setLastSeenClassUnloadID(int32_t v) { _lastSeenClassUnloadID = v; }
   virtual uintptrj_t getData(TR::Compilation *comp = NULL) = 0;
   virtual uint32_t* getDataReference() { return NULL; }
   virtual int32_t setData(uintptrj_t value, uint32_t freq = 1) = 0;
   virtual bool isInvalid() = 0;
   virtual void setInvalid() = 0;
   virtual bool isCompact() = 0;
   virtual TR_IPBCDataPointer        *asIPBCDataPointer()        { return NULL; }
   virtual TR_IPBCDataFourBytes      *asIPBCDataFourBytes()      { return NULL; }
   virtual TR_IPBCDataEightWords     *asIPBCDataEightWords()     { return NULL; }
   virtual TR_IPBCDataCallGraph      *asIPBCDataCallGraph()      { return NULL; }
   virtual TR_IPBCDataAllocation     *asIPBCDataAllocation()     { return NULL; }
   // returns the number of bytes the equivalent storage structure needs
   virtual uint32_t                   getBytesFootprint() = 0; 

   virtual uint32_t canBePersisted(uintptrj_t cacheStartAddress, uintptrj_t cacheSize, TR::PersistentInfo *info) { return IPBC_ENTRY_CAN_PERSIST; }
   virtual void createPersistentCopy(uintptrj_t cacheStartAddress, uintptrj_t cacheSize, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info)  = 0;
   virtual void loadFromPersistentCopy(TR_IPBCDataStorageHeader * storage, TR::Compilation *comp, uintptrj_t cacheStartAddress) {}
   virtual void copyFromEntry(TR_IPBytecodeHashTableEntry * originalEntry, TR::Compilation *comp) {}
   void clearEntryFlags(){ _entryFlags = 0;};
   void setPersistentEntryRead(){ _entryFlags |= TR_IPBC_PERSISTENT_ENTRY_READ;};
   bool isPersistentEntryRead(){ return (_entryFlags & TR_IPBC_PERSISTENT_ENTRY_READ) != 0;};

   bool getCanPersistEntryFlag() const { return (_persistFlags & IPBC_ENTRY_CAN_PERSIST_FLAG) != 0; }
   void setDoNotPersist() { _persistFlags &= ~IPBC_ENTRY_CAN_PERSIST_FLAG; }
   bool isLockedEntry() const { return (_persistFlags & IPBC_ENTRY_PERSIST_LOCK_FLAG) != 0; }
   void setLockedEntry() { _persistFlags |= IPBC_ENTRY_PERSIST_LOCK_FLAG; }
   void resetLockedEntry() { _persistFlags &= ~IPBC_ENTRY_PERSIST_LOCK_FLAG; }

protected:
   TR_IPBytecodeHashTableEntry *_next;
   uintptrj_t _pc;
   int32_t    _lastSeenClassUnloadID;

   enum TR_PersistenceFlags
      {
      IPBC_ENTRY_CAN_PERSIST_FLAG = 0x01,
      IPBC_ENTRY_PERSIST_LOCK_FLAG = 0x02,
      };

   uint8_t _entryFlags;
   uint8_t _persistFlags;
   }; // class TR_IPBytecodeHashTableEntry

class TR_IPMethodData
   {
   public:
   TR_PERSISTENT_ALLOC(TR_Memory::IProfiler)
   TR_IPMethodData() : _method(0),_pcIndex(0),_weight(0) {}
   TR_OpaqueMethodBlock *getMethod() { return _method; }
   void setMethod (TR_OpaqueMethodBlock *meth) { _method = meth; }
   uint32_t getWeight() { return _weight; }
   void     incWeight() { ++_weight; }
   uint32_t getPCIndex() { return _pcIndex; }
   void     setPCIndex(uint32_t i) { _pcIndex = i; }

   TR_IPMethodData* next;

   private:
   TR_OpaqueMethodBlock *_method;
   uint32_t _pcIndex;
   uint32_t _weight;
   };

struct TR_DummyBucket
   {
   uint32_t getWeight() const { return _weight; }
   void     incWeight() { ++_weight; }

   uint32_t _weight;
   };



struct TR_IPMethodHashTableEntry
   {
   static const int32_t MAX_IPMETHOD_CALLERS = 20;
   public:
   TR_PERSISTENT_ALLOC(TR_Memory::IProfiler)
   void * operator new (size_t size) throw();

   TR_IPMethodHashTableEntry *_next;   // for chaining in the hashtable
   TR_OpaqueMethodBlock      *_method; // callee
   TR_IPMethodData            _caller; // link list of callers and their weights. Capped at MAX_IPMETHOD_CALLERS
   TR_DummyBucket             _otherBucket;
    
   void add(TR_OpaqueMethodBlock *caller, TR_OpaqueMethodBlock *callee, uint32_t pcIndex);
   };

class TR_IPBCDataFourBytes : public TR_IPBytecodeHashTableEntry
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::IPBCDataFourBytes)
   TR_IPBCDataFourBytes(uintptrj_t pc) : TR_IPBytecodeHashTableEntry(pc), data(0) {} 
   void * operator new (size_t size) throw();
   void * operator new (size_t size, void * placement) {return placement;}
   static const uint32_t IPROFILING_INVALID = ~0;
   virtual uintptrj_t getData(TR::Compilation *comp = NULL) { return (uint32_t)data; }
   virtual uint32_t* getDataReference() { static uint32_t data_copy = (uint32_t)data; return &data_copy; }

   virtual int32_t setData(uintptrj_t value, uint32_t freq = 1) { data = (uint32_t)value; return 0;}
   virtual bool isCompact() { return true; }
   virtual bool isInvalid() { if (data == IPROFILING_INVALID) return true; return false; }
   virtual void setInvalid() { data = IPROFILING_INVALID; }
   virtual TR_IPBCDataFourBytes  *asIPBCDataFourBytes() { return this; }
   virtual uint32_t getBytesFootprint() {return sizeof (TR_IPBCDataFourBytesStorage);}

   virtual void createPersistentCopy(uintptrj_t cacheStartAddress, uintptrj_t cacheSize, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info);
   virtual void loadFromPersistentCopy(TR_IPBCDataStorageHeader * storage, TR::Compilation *comp, uintptrj_t cacheStartAddress);
   int16_t getSumBranchCount();
   virtual void copyFromEntry(TR_IPBytecodeHashTableEntry * originalEntry, TR::Compilation *comp);
private:
   uint32_t data;
   };

class TR_IPBCDataAllocation : public TR_IPBytecodeHashTableEntry
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::IPBCDataAllocation)
   TR_IPBCDataAllocation(uintptrj_t pc) : TR_IPBytecodeHashTableEntry(pc), clazz(0), method(0), data(0) {}  
   void * operator new (size_t size) throw();
   static const uint32_t IPROFILING_INVALID = ~0;
   virtual uintptrj_t getData(TR::Compilation *comp = NULL) { return (uint32_t)data; }
   virtual uint32_t* getDataReference() { return &data; }
   virtual int32_t setData(uintptrj_t value, uint32_t freq = 1) { data = (uint32_t)value; return 0;}
   virtual bool isCompact() { return true; }
   virtual bool isInvalid() { if (data == IPROFILING_INVALID) return true; return false; }
   virtual void setInvalid() { data = IPROFILING_INVALID; }
   virtual TR_IPBCDataAllocation  *asIPBCDataAllocation() { return this; }

   uintptrj_t getClass() { return (uintptrj_t)clazz; }
   void setClass(uintptrj_t c) { clazz = c; }
   uintptrj_t getMethod() { return (uintptrj_t)method; }
   void setMethod(uintptrj_t m) { method = m; }
private:
   uintptrj_t clazz;
   uintptrj_t method;
   uint32_t data;
   };

#define SWITCH_DATA_COUNT 4
class TR_IPBCDataEightWords : public TR_IPBytecodeHashTableEntry
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::IPBCDataEightWords)
   TR_IPBCDataEightWords(uintptrj_t pc) : TR_IPBytecodeHashTableEntry(pc)
      {
      for (int i = 0; i < SWITCH_DATA_COUNT; i++)
         data[i] = 0;
      };
   void * operator new (size_t size) throw();
   void * operator new (size_t size, void * placement) {return placement;}
   static const uint64_t IPROFILING_INVALID = ~0;
   virtual uintptrj_t getData(TR::Compilation *comp = NULL) { /*TR_ASSERT(0, "Don't call me, I'm empty"); */return 0;}
   virtual int32_t setData(uintptrj_t value, uint32_t freq = 1) { /*TR_ASSERT(0, "Don't call me, I'm empty");*/ return 0;}
   uint64_t* getDataPointer() { return data; }
   virtual bool isCompact() { return false; }
   virtual bool isInvalid() { if (data[0] == IPROFILING_INVALID) return true; return false; }
   virtual void setInvalid() { data[0] = IPROFILING_INVALID; }
   virtual TR_IPBCDataEightWords  *asIPBCDataEightWords() { return this; }
   virtual uint32_t getBytesFootprint() {return sizeof(TR_IPBCDataEightWordsStorage);}

   virtual void createPersistentCopy(uintptrj_t cacheStartAddress, uintptrj_t cacheSize, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info);
   virtual void loadFromPersistentCopy(TR_IPBCDataStorageHeader * storage, TR::Compilation *comp, uintptrj_t cacheStartAddress);
   virtual int32_t getSumSwitchCount();
   virtual void copyFromEntry(TR_IPBytecodeHashTableEntry * originalEntry, TR::Compilation *comp);

private:
   uint64_t data[SWITCH_DATA_COUNT];
   };



class TR_IPBCDataCallGraph : public TR_IPBytecodeHashTableEntry
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::IPBCDataCallGraph)
   TR_IPBCDataCallGraph (uintptrj_t pc) : TR_IPBytecodeHashTableEntry(pc)
      {
      _csInfo.initialize();
      }
   void * operator new (size_t size) throw();
   void * operator new (size_t size, void * placement) {return placement;}

#if defined(OMR_GC_COMPRESSED_POINTERS) //compressed references
   static const uint32_t IPROFILING_INVALID = ~0; //only take up the bottom 32, class compression issue
#else
   static const uintptrj_t IPROFILING_INVALID = ~0;
#endif //OMR_GC_COMPRESSED_POINTERS

   virtual uintptrj_t getData(TR::Compilation *comp = NULL);
   virtual CallSiteProfileInfo* getCGData() { return &_csInfo; } // overloaded
   virtual int32_t setData(uintptrj_t v, uint32_t freq = 1);
   virtual uint32_t* getDataReference() { return NULL; }
   virtual bool isCompact() { return false; }
   virtual TR_IPBCDataCallGraph *asIPBCDataCallGraph() { return this; }
   int32_t getSumCount(TR::Compilation *comp);
   int32_t getSumCount(TR::Compilation *comp, bool);
   int32_t getEdgeWeight(TR_OpaqueClassBlock *clazz, TR::Compilation *comp);
   void updateEdgeWeight(TR_OpaqueClassBlock *clazz, int32_t weight);
   void printWeights(TR::Compilation *comp);

   void setWarmCallGraphTooBig(bool set=true) { _csInfo._tooBigToBeInlined = (set) ? 1 : 0; }
   bool isWarmCallGraphTooBig() { return (_csInfo._tooBigToBeInlined == 1); }

   virtual bool isInvalid() { if (_csInfo.getClazz(0) == IPROFILING_INVALID) return true; return false; }
   virtual void setInvalid() { _csInfo.setClazz(0, IPROFILING_INVALID); }
   virtual uint32_t getBytesFootprint() {return sizeof (TR_IPBCDataCallGraphStorage);}

   virtual uint32_t canBePersisted(uintptrj_t cacheStartAddress, uintptrj_t cacheSize, TR::PersistentInfo *info);
   virtual void createPersistentCopy(uintptrj_t cacheStartAddress, uintptrj_t cacheSize, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info);
   virtual void loadFromPersistentCopy(TR_IPBCDataStorageHeader * storage, TR::Compilation *comp, uintptrj_t cacheStartAddress);
   virtual void copyFromEntry(TR_IPBytecodeHashTableEntry * originalEntry, TR::Compilation *comp);

   bool lockEntry();
   void releaseEntry();
   bool isLocked();

private:
   CallSiteProfileInfo _csInfo;
   };

class IProfilerBuffer : public TR_Link0<IProfilerBuffer>
   {
   public:
   U_8* getBuffer() {return _buffer;}
   void setBuffer(U_8* buffer) { _buffer = buffer; }
   UDATA getSize() {return _size;}
   void setSize(UDATA size) {_size = size;}
   bool isValid() const { return !_isInvalidated; }
   void setIsInvalidated(bool b) { _isInvalidated = b; }
   private:
   U_8 *_buffer;
   UDATA _size;
   volatile bool _isInvalidated;
   };

class TR_ReadSampleRequestsStats
   {
friend class TR_ReadSampleRequestsHistory;
private:
   uint32_t _totalReadSampleRequests; // no locks used to update this
   uint32_t _failedReadSampleRequests;
   };

class TR_ReadSampleRequestsHistory
   {
   TR_PERSISTENT_ALLOC(TR_Memory::IProfiler);
   #define SAMPLE_CUTOFF 120     // this must be correlated with HISTORY_BUFFER_SIZE
                                  // the bigger the HISTORY_BUFFER_SIZE the bigger SAMPLE_CUTOFF should be
public:
   bool init(int32_t historyBufferSize);
   void incTotalReadSampleRequests()  { _history[_crtIndex]._totalReadSampleRequests++; }
   void incFailedReadSampleRequests() { _history[_crtIndex]._failedReadSampleRequests++; }
   uint32_t getTotalReadSampleRequests() const  { return _history[_crtIndex]._totalReadSampleRequests; }
   uint32_t getFailedReadSampleRequests() const { return _history[_crtIndex]._failedReadSampleRequests; }
   uint32_t getReadSampleFailureRate() const;
   void advanceEpoch(); // performed by sampling thread only
   uint32_t numSamplesInHistoryBuffer() const
      {
      return _history[_crtIndex]._totalReadSampleRequests - _history[nextIndex()]._totalReadSampleRequests;
      }
private:
   int32_t nextIndex() const { return (_crtIndex + 1) % _historyBufferSize; }
private:
   int32_t _historyBufferSize;
   int32_t _crtIndex;
   TR_ReadSampleRequestsStats *_history; // My circular buffer
   };

class TR_IProfiler : public TR_ExternalProfiler
   {
public:

   TR_PERSISTENT_ALLOC(TR_Memory::IProfiler);
   static TR_IProfiler *allocate (J9JITConfig *);
   static uint32_t getProfilerMemoryFootprint();

   uintptrj_t getReceiverClassFromCGProfilingData(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp);

   TR_IProfiler (J9JITConfig *);
   void * operator new (size_t) throw();
   void shutdown();
   void outputStats();
   void dumpIPBCDataCallGraph(J9VMThread* currentThread);
   void resetProfiler();
   void startIProfilerThread(J9JavaVM *javaVM);
   void deallocateIProfilerBuffers();
   void stopIProfilerThread();
   void invalidateProfilingBuffers(); // called for class unloading
   bool isIProfilingEnabled() const { return _isIProfilingEnabled; }
   void incrementNumRequests() { _numRequests++; }
   // this is registered as the BufferFullEvent handler
   UDATA parseBuffer(J9VMThread * vmThread, const U_8* dataStart, UDATA size, bool verboseReparse=false);

   void printAllocationReport(); //Called by HookedByTheJIT

   bool isWarmCallGraphTooBig(TR_OpaqueMethodBlock *method, int32_t bcIndex, TR::Compilation *comp);
   void setWarmCallGraphTooBig(TR_OpaqueMethodBlock *method, int32_t bcIndex, TR::Compilation *comp, bool set = true);

   // This method is used to search the hash table first, then the shared cache. Added to support RI data >> public.
   TR_IPBytecodeHashTableEntry *profilingSampleRI (uintptrj_t pc, uintptrj_t data, bool addIt, uint32_t freq);   
   
   /* 
   leave the TR_ResolvedMethodSymbol argument for debugging purpose when called from Ilgen
   */
   void persistIprofileInfo(TR::ResolvedMethodSymbol *methodSymbol, TR_ResolvedMethod *method, TR::Compilation *comp);

   void checkMethodHashTable();

   int32_t getMaxCallCount();

   //j9method.cpp
   void getFaninInfo(TR_OpaqueMethodBlock *calleeMethod, uint32_t *count, uint32_t *weight, uint32_t *otherBucketWeight = NULL);
   bool getCallerWeight(TR_OpaqueMethodBlock *calleeMethod, TR_OpaqueMethodBlock *callerMethod , uint32_t *weight, uint32_t pcIndex = ~0, TR::Compilation *comp = 0);

   //VMJ9.cpp
   int32_t getCallCount(TR_OpaqueMethodBlock *calleeMethod, TR_OpaqueMethodBlock *method, int32_t bcIndex, TR::Compilation *);
   int32_t getCallCount(TR_OpaqueMethodBlock *method, int32_t bcIndex, TR::Compilation *);
   int32_t getCallCount(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp);
   void    setCallCount(TR_OpaqueMethodBlock *method, int32_t bcIndex, int32_t count, TR::Compilation *);
   void    setCallCount(TR_ByteCodeInfo &bcInfo, int32_t count, TR::Compilation *comp);

   int32_t getCGEdgeWeight (TR::Node *callerNode, TR_OpaqueMethodBlock *callee, TR::Compilation *comp);
   bool    isCallGraphProfilingEnabled();

   virtual TR_AbstractInfo *createIProfilingValueInfo( TR_ByteCodeInfo &bcInfo, TR::Compilation *comp);
   virtual TR_ExternalValueProfileInfo * getValueProfileInfo(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp);
   uint32_t *getAllocationProfilingDataPointer(TR_ByteCodeInfo &bcInfo, TR_OpaqueClassBlock *clazz, TR_OpaqueMethodBlock *method, TR::Compilation *comp);
   uint32_t *getGlobalAllocationDataPointer(bool isAOT);
   TR_ExternalProfiler * canProduceBlockFrequencyInfo(TR::Compilation& comp);
   void setupEntriesInHashTable(TR_IProfiler *ip);

   TR_IPMethodHashTableEntry *findOrCreateMethodEntry(J9Method *, J9Method *, bool addIt, uint32_t pcIndex =  ~0);
   uint32_t releaseAllEntries();
   uint32_t countEntries();
   void advanceEpochForHistoryBuffer() { _readSampleRequestsHistory->advanceEpoch(); }
   uint32_t getReadSampleFailureRate() const { return _readSampleRequestsHistory->getReadSampleFailureRate(); }
   uint32_t getTotalReadSampleRequests() const { return _readSampleRequestsHistory->getTotalReadSampleRequests(); }
   uint32_t getFailedReadSampleRequests() const { return _readSampleRequestsHistory->getFailedReadSampleRequests(); }
   uint32_t numSamplesInHistoryBuffer() const { return _readSampleRequestsHistory->numSamplesInHistoryBuffer(); }



public:
   J9VMThread* getIProfilerThread() { return _iprofilerThread; }
   void setIProfilerThread(J9VMThread* thread) { _iprofilerThread = thread; }
   j9thread_t getIProfilerOSThread() { return _iprofilerOSThread; }
   TR::Monitor* getIProfilerMonitor() { return _iprofilerMonitor; }
   bool processProfilingBuffer(J9VMThread *vmThread, const U_8* dataStart, UDATA size);
   void setAttachAttempted(bool b) { _iprofilerThreadAttachAttempted = b; }
   void processWorkingQueue();
   bool getAttachAttempted() const { return _iprofilerThreadAttachAttempted; }
   IProfilerBuffer *getCrtProfilingBuffer() const { return _crtProfilingBuffer; }
   void setCrtProfilingBuffer(IProfilerBuffer *b) { _crtProfilingBuffer = b; }
   void setIProfilerThreadExitFlag() { _iprofilerThreadExitFlag = 1; }
   void jitProfileParseBuffer(J9VMThread *vmThread);
   uint32_t getIProfilerThreadExitFlag() { return _iprofilerThreadExitFlag; }
   bool postIprofilingBufferToWorkingQueue(J9VMThread * vmThread, const U_8* dataStart, UDATA size);
   // this is wrapper of registered version, for the helper function, from JitRunTime

private:
#ifdef J9VM_INTERP_PROFILING_BYTECODES
   U_8 *getProfilingBufferCursor(J9VMThread *vmThread) const { return vmThread->profilingBufferCursor; }
   void setProfilingBufferCursor(J9VMThread *vmThread, U_8* p) { vmThread->profilingBufferCursor = p; }
   U_8 *getProfilingBufferEnd(J9VMThread *vmThread) const { return vmThread->profilingBufferEnd; }
   void setProfilingBufferEnd(J9VMThread *vmThread, U_8* p) { vmThread->profilingBufferEnd = p; }
#else
   U_8 *getProfilingBufferCursor(J9VMThread *vmThread) const { return NULL; }
   void setProfilingBufferCursor(J9VMThread *vmThread, U_8* p) {}
   U_8 *getProfilingBufferEnd(J9VMThread *vmThread) const { return NULL; }
   void setProfilingBufferEnd(J9VMThread *vmThread, U_8* p) {}
#endif
   bool needTriggerCallContextCollection(U_8 *pc, J9Method *caller, J9Method *callee);
   uintptrj_t getProfilingData(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp);
   virtual void setBlockAndEdgeFrequencies( TR::CFG *cfg, TR::Compilation *comp);
   virtual bool hasSameBytecodeInfo(TR_ByteCodeInfo & persistentByteCodeInfo, TR_ByteCodeInfo & currentByteCodeInfo, TR::Compilation *comp);
   virtual void getBranchCounters (TR::Node *node, TR::TreeTop *fallThroughTree, int32_t *taken, int32_t *notTaken, TR::Compilation *comp);
   int32_t getMaxCount(bool isAOT);
   virtual int32_t getSwitchCountForValue (TR::Node *node, int32_t value, TR::Compilation *comp);
   virtual int32_t getSumSwitchCount (TR::Node *node, TR::Compilation *comp);
   virtual int32_t getFlatSwitchProfileCounts (TR::Node *node, TR::Compilation *comp);
   virtual bool isSwitchProfileFlat (TR::Node *node, TR::Compilation *comp);
   int32_t getSamplingCount( TR_IPBytecodeHashTableEntry *entry, TR::Compilation *comp);
   // for replay
   void copyDataFromEntry(TR_IPBytecodeHashTableEntry *oldEntry, TR_IPBytecodeHashTableEntry *newEntry, TR_IProfiler *ip);

   TR_IPBCDataStorageHeader *getJ9SharedDataDescriptorForMethod(J9SharedDataDescriptor * descriptor, unsigned char * buffer, uint32_t length, TR_OpaqueMethodBlock * method, TR::Compilation *comp);

   static int32_t bcHash (uintptrj_t);
   static int32_t allocHash (uintptrj_t);

   static int32_t methodHash(uintptrj_t pc);
//   static int32_t pcHash(uintptrj_t pc);

   bool isCompact(U_8 byteCode);
   bool isSwitch(U_8 byteCode);
   bool isNewOpCode(U_8 byteCode);

   bool acquireHashTableWriteLock(bool forceFullLock);
   void releaseHashTableWriteLock();

   TR_IPBytecodeHashTableEntry *searchForSample(uintptrj_t pc, int32_t bucket);
   TR_IPBCDataStorageHeader *searchForPersistentSample(TR_IPBCDataStorageHeader  *root, uintptrj_t pc);
   TR_IPBCDataAllocation *searchForAllocSample(uintptrj_t pc, int32_t bucket);

   // This method is used to search the hash table first, then the shared cache
   TR_IPBytecodeHashTableEntry *profilingSample (uintptrj_t pc, uintptrj_t data, bool addIt, bool isRIData = false, uint32_t freq  = 1);
   // This method is used to search only the hash table
   TR_IPBytecodeHashTableEntry *profilingSample (TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex,
                                                 TR::Compilation *comp, uintptrj_t data = 0xDEADF00D, bool addIt = false);
   TR_IPBytecodeHashTableEntry *profilingSample1 (uintptrj_t pc, uintptrj_t data, bool addIt = false);
   TR_IPBytecodeHashTableEntry * persistentProfilingSample (TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation *comp, bool *methodProfileExistsInSCC);
   TR_IPBCDataStorageHeader * persistentProfilingSample (TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation *comp, bool *methodProfileExistsInSCC, uintptrj_t *cacheOffset, TR_IPBCDataStorageHeader *store);

   TR_IPBCDataAllocation *profilingAllocSample (uintptrj_t pc, uintptrj_t data, bool addIt);
   TR_IPBytecodeHashTableEntry *findOrCreateEntry (int32_t bucket, uintptrj_t pc, bool addIt);
   TR_IPBCDataAllocation *findOrCreateAllocEntry (int32_t bucket, uintptrj_t pc, bool addIt);
   uintptrj_t getProfilingData(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation *);
   uintptrj_t getProfilingData(TR::Node *node, TR::Compilation *comp);
   TR_OpaqueMethodBlock * getMethodFromNode(TR::Node *node, TR::Compilation *comp);
   uintptrj_t getSearchPC (TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation *);
   bool addSampleData(TR_IPBytecodeHashTableEntry *entry, uintptrj_t data, bool isRIData = false, uint32_t freq = 1);
   TR_AbstractInfo *createIProfilingValueInfo( TR::Node *node, TR::Compilation *comp);

   TR_IPMethodHashTableEntry *searchForMethodSample(TR_OpaqueMethodBlock *omb, int32_t bucket);

   bool branchHasSameDirection(TR::ILOpCodes nodeOpCode, TR::Node *node, TR::Compilation *comp);
   bool branchHasOppositeDirection(TR::ILOpCodes nodeOpCode, TR::Node *node, TR::Compilation *comp);
   uint8_t getBytecodeOpCode(TR::Node *node, TR::Compilation *comp);
   bool invalidateEntryIfInconsistent(TR_IPBytecodeHashTableEntry *entry);
   bool isNewOpCode(uintptrj_t pc);
   bool isSwitch(uintptrj_t pc);
   J9Class *getInterfaceClass(J9Method *aMethod, TR::Compilation *comp);
   int32_t getOrSetSwitchData(TR_IPBCDataEightWords *entry, uint32_t data, bool isSet, bool isLookup);
   TR_IPBytecodeHashTableEntry *getProfilingEntry(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation *);

   TR_IPBCDataCallGraph* getCGProfilingData(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp);
   TR_IPBCDataCallGraph* getCGProfilingData(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation *comp);

   uintptrj_t createBalancedBST(uintptrj_t *pcEntries, int32_t low, int32_t high, uintptrj_t memChunk,
                                TR::Compilation *comp, uintptrj_t cacheStartAddress, uintptrj_t cacheSize);
   uint32_t walkILTreeForEntries(uintptrj_t *pcEntries, uint32_t &numEntries, TR_J9ByteCodeIterator *bcIterator, TR_OpaqueMethodBlock *method, TR::Compilation *comp,
                                 uintptrj_t cacheOffset, uintptrj_t cacheSize, vcount_t visitCount, int32_t callerIndex, TR_BitVector *BCvisit, bool &abort);


   // data members
   J9PortLibrary                  *_portLib;
   bool                            _isIProfilingEnabled; // set to TRUE in constructor; set to FALSE in shutdown()
   TR_J9VMBase                    *_vm;
   TR::CompilationInfo *            _compInfo;
   TR::Monitor *_hashTableMonitor;
   uint32_t                        _lightHashTableMonitor;

   // value profiling
   TR_OpaqueMethodBlock           *_valueProfileMethod;

   // bytecode hashtable
   TR_IPBytecodeHashTableEntry   **_bcHashTable;

#if defined(EXPERIMENTAL_IPROFILER)
   // bytecode hashtable
   TR_IPBCDataAllocation         **_allocHashTable;
#endif

   // block frequency
   int32_t                         _maxCount;
   // giving out profiling information for inlined calls
   bool                            _allowedToGiveInlinedInformation;
   int32_t                         _classLoadTimeStampGap;
   // call-graph profiling
   bool                            _enableCGProfiling;
   uint32_t                        _globalAllocationCount;
   int32_t                         _maxCallFrequency;
   j9thread_t                      _iprofilerOSThread;
   J9VMThread                     *_iprofilerThread;
   TR_LinkHead0<IProfilerBuffer>   _freeBufferList;
   TR_LinkHead0<IProfilerBuffer>   _workingBufferList;
   IProfilerBuffer                *_workingBufferTail;
   IProfilerBuffer                *_crtProfilingBuffer; // profiling buffer being processes by iprofiling thread
   TR::Monitor                    *_iprofilerMonitor;
   volatile int32_t                _numOutstandingBuffers;
   uint64_t                        _numRequests;
   uint64_t                        _numRequestsSkipped;
   uint64_t                        _numRequestsHandedToIProfilerThread;
   volatile uint32_t               _iprofilerThreadExitFlag;
   volatile bool                   _iprofilerThreadAttachAttempted;
   uint64_t                        _iprofilerNumRecords; // info stats only

   TR_IPMethodHashTableEntry       **_methodHashTable;

   uint32_t                        _iprofilerBufferSize;
   TR_ReadSampleRequestsHistory   *_readSampleRequestsHistory;


   public:
   static int32_t                  _STATS_noProfilingInfo;
   static int32_t                  _STATS_doesNotWantToGiveProfilingInfo;
   static int32_t                  _STATS_weakProfilingRatio;
   static int32_t                  _STATS_cannotGetClassInfo;
   static int32_t                  _STATS_timestampHasExpired;
   static int32_t                  _STATS_abortedPersistence;
   static int32_t                  _STATS_methodPersisted;
   static int32_t                  _STATS_methodNotPersisted_SCCfull;
   static int32_t                  _STATS_methodNotPersisted_classNotInSCC;
   static int32_t                  _STATS_methodNotPersisted_delayed;
   static int32_t                  _STATS_methodNotPersisted_other;
   static int32_t                  _STATS_methodNotPersisted_alreadyStored;
   static int32_t                  _STATS_methodNotPersisted_noEntries;
   static int32_t                  _STATS_persistError;
   static int32_t                  _STATS_methodPersistenceAttempts;
   static int32_t                  _STATS_entriesPersisted;
   static int32_t                  _STATS_entriesNotPersisted_NotInSCC;
   static int32_t                  _STATS_entriesNotPersisted_Unloaded;
   static int32_t                  _STATS_entriesNotPersisted_NoInfo;
   static int32_t                  _STATS_entriesNotPersisted_Other;

   static int32_t                  _STATS_persistedIPReadFail;
   static int32_t                  _STATS_persistedIPReadHadBadData;
   static int32_t                  _STATS_persistedIPReadSuccess;
   static int32_t                  _STATS_persistedAndCurrentIPDataDiffer;
   static int32_t                  _STATS_persistedAndCurrentIPDataMatch;

   static int32_t                  _STATS_currentIPReadFail;
   static int32_t                  _STATS_currentIPReadSuccess;
   static int32_t                  _STATS_currentIPReadHadBadData;

   static int32_t                  _STATS_IPEntryRead;
   static int32_t                  _STATS_IPEntryChoosePersistent;
   };
#endif

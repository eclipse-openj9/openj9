#ifndef JAAS_IPROFILER_HPP
#define JAAS_IPROFILER_HPP

#include "runtime/J9Profiler.hpp"
#include "runtime/IProfiler.hpp"

namespace JAAS
{
class J9ClientStream;
}

struct TR_ContiguousIPMethodData
   {
   TR_OpaqueMethodBlock *_method;
   uint32_t _pcIndex;
   uint32_t _weight;
   };

struct TR_ContiguousIPMethodHashTableEntry
   {
   static TR_ContiguousIPMethodHashTableEntry serialize(TR_IPMethodHashTableEntry *entry);

   TR_OpaqueMethodBlock *_method; // callee
   TR_ContiguousIPMethodData _callers[TR_IPMethodHashTableEntry::MAX_IPMETHOD_CALLERS]; // array of callers and their weights. null _method means EOL
   TR_DummyBucket _otherBucket;
   };

class TR_JaasIProfiler : public TR_IProfiler
   {
public:

   TR_PERSISTENT_ALLOC(TR_Memory::IProfiler);
   static TR_JaasIProfiler * allocate(J9JITConfig *jitConfig);
   TR_JaasIProfiler(J9JITConfig *);

   // Data accessors, overridden for JaaS
   //
   virtual TR_IPMethodHashTableEntry *searchForMethodSample(TR_OpaqueMethodBlock *omb, int32_t bucket) override;

   // This method is used to search only the hash table
   virtual TR_IPBytecodeHashTableEntry *profilingSample (uintptrj_t pc, uintptrj_t data, bool addIt, bool isRIData = false, uint32_t freq  = 1) override;
   // This method is used to search the hash table first, then the shared cache
   virtual TR_IPBytecodeHashTableEntry *profilingSample (TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex,
                                                         TR::Compilation *comp, uintptrj_t data = 0xDEADF00D, bool addIt = false) override;

   virtual int32_t getMaxCallCount();

   TR_IPBytecodeHashTableEntry* ipBytecodeHashTableEntryFactory(TR_IPBCDataStorageHeader *storage, uintptrj_t pc, TR_Memory* mem, TR_AllocationKind allocKind);
   void printStats();

private:
   void validateCachedIPEntry(TR_IPBytecodeHashTableEntry *entry, TR_IPBCDataStorageHeader *clientData, uintptrj_t methodStart, bool isMethodBeingCompiled, TR_OpaqueMethodBlock *method);
   bool _useCaching;
   // Statistics
   uint32_t _statsIProfilerInfoFromCache;  // IP cache answered the query
   uint32_t _statsIProfilerInfoMsgToClient; // queries sent to client
   uint32_t _statsIProfilerInfoReqNotCacheable; // info returned from client should not be cached
   uint32_t _statsIProfilerInfoIsEmpty; // client has no IP info for indicated PC
   uint32_t _statsIProfilerInfoCachingFailures;
   };

class TR_JaasClientIProfiler : public TR_IProfiler
   {
   public:

      TR_PERSISTENT_ALLOC(TR_Memory::IProfiler);
      static TR_JaasClientIProfiler * allocate(J9JITConfig *jitConfig);
      TR_JaasClientIProfiler(J9JITConfig *);

    
      uint32_t walkILTreeForIProfilingEntries(uintptrj_t *pcEntries, uint32_t &numEntries, TR_J9ByteCodeIterator *bcIterator,
                                              TR_OpaqueMethodBlock *method, TR_BitVector *BCvisit, bool &abort);
      uintptr_t serializeIProfilerMethodEntries(uintptrj_t *pcEntries, uint32_t numEntries, uintptr_t memChunk, uintptrj_t methodStartAddress);
      void serializeAndSendIProfileInfoForMethod(TR_OpaqueMethodBlock*method, TR::Compilation *comp, JAAS::J9ClientStream *client);
   };

#endif

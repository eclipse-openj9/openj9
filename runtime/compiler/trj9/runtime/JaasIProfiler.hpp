#ifndef JAAS_IPROFILER_HPP
#define JAAS_IPROFILER_HPP

#include "runtime/J9Profiler.hpp"
#include "runtime/IProfiler.hpp"

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
   };

#endif

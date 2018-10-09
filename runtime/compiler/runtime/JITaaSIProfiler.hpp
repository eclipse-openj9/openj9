/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef JITaaS_IPROFILER_HPP
#define JITaaS_IPROFILER_HPP

#include "runtime/J9Profiler.hpp"
#include "runtime/IProfiler.hpp"

namespace JITaaS
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

class TR_JITaaSIProfiler : public TR_IProfiler
   {
public:

   TR_PERSISTENT_ALLOC(TR_Memory::IProfiler);
   static TR_JITaaSIProfiler * allocate(J9JITConfig *jitConfig);
   TR_JITaaSIProfiler(J9JITConfig *);

   // Data accessors, overridden for JITaaS
   //
   virtual TR_IPMethodHashTableEntry *searchForMethodSample(TR_OpaqueMethodBlock *omb, int32_t bucket) override;

   // This method is used to search only the hash table
   virtual TR_IPBytecodeHashTableEntry *profilingSample (uintptrj_t pc, uintptrj_t data, bool addIt, bool isRIData = false, uint32_t freq  = 1) override;
   // This method is used to search the hash table first, then the shared cache
   virtual TR_IPBytecodeHashTableEntry *profilingSample (TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex,
                                                         TR::Compilation *comp, uintptrj_t data = 0xDEADF00D, bool addIt = false) override;

   virtual int32_t getMaxCallCount() override;
   virtual void setCallCount(TR_OpaqueMethodBlock *method, int32_t bcIndex, int32_t count, TR::Compilation *) override;

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

class TR_JITaaSClientIProfiler : public TR_IProfiler
   {
   public:

      TR_PERSISTENT_ALLOC(TR_Memory::IProfiler);
      static TR_JITaaSClientIProfiler * allocate(J9JITConfig *jitConfig);
      TR_JITaaSClientIProfiler(J9JITConfig *);
      // NOTE: since the JITaaS client can act as a regular JVM compiling methods 
      // locally, we must not change the behavior of functions used in TR_IProfiler
      // Thus, any virtual function here must call the corresponding method in
      // the base class. It may be better not to override any methods though
    
      uint32_t walkILTreeForIProfilingEntries(uintptrj_t *pcEntries, uint32_t &numEntries, TR_J9ByteCodeIterator *bcIterator,
                                              TR_OpaqueMethodBlock *method, TR_BitVector *BCvisit, bool &abort);
      uintptr_t serializeIProfilerMethodEntries(uintptrj_t *pcEntries, uint32_t numEntries, uintptr_t memChunk, uintptrj_t methodStartAddress);
      void serializeAndSendIProfileInfoForMethod(TR_OpaqueMethodBlock*method, TR::Compilation *comp, JITaaS::J9ClientStream *client);
   };

#endif

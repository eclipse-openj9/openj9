#include "runtime/JaasIProfiler.hpp"
#include "control/CompilationRuntime.hpp"

TR_JaasIProfiler *
TR_JaasIProfiler::allocate(J9JITConfig *jitConfig)
   {
   TR_JaasIProfiler * profiler = new (PERSISTENT_NEW) TR_JaasIProfiler(jitConfig);
   return profiler;
   }

TR_JaasIProfiler::TR_JaasIProfiler(J9JITConfig *jitConfig)
   : TR_IProfiler(jitConfig)
   {
   }

TR_IPMethodHashTableEntry *
deserializeMethodEntry(TR_ContiguousIPMethodHashTableEntry *serialEntry)
   {
   // TODO when caching don't use heap mem
   TR_IPMethodHashTableEntry *entry = (TR_IPMethodHashTableEntry *)TR::comp()->trMemory()->allocateHeapMemory(sizeof(TR_IPMethodHashTableEntry));
   if (entry)
      {
      memset(entry, 0, sizeof(TR_IPMethodHashTableEntry));
      entry->_next = nullptr; // TODO: set this if caching locally
      entry->_method = serialEntry->_method;
      entry->_otherBucket = serialEntry->_otherBucket;

      TR_IPMethodData *caller = &entry->_caller;
      for (size_t i = 0; i < TR_IPMethodHashTableEntry::MAX_IPMETHOD_CALLERS; i++)
         {
         auto &serialCaller = serialEntry->_callers[i];
         if (serialCaller._method == nullptr)
            break;

         if (i != 0)
            {
            // TODO when caching don't use heap mem
            TR_IPMethodData* newCaller = (TR_IPMethodData*)TR::comp()->trMemory()->allocateHeapMemory(sizeof(TR_IPMethodData));
            if (!newCaller)
               break; // couldn't alloc, end table early
            caller->next = newCaller;
            caller = newCaller;
            }

         caller->setMethod(serialCaller._method);
         caller->setPCIndex(serialCaller._pcIndex);
         caller->setWeight(serialCaller._weight);
         caller->next = nullptr;
         }
      }
   return entry;
   }

TR_ContiguousIPMethodHashTableEntry
TR_ContiguousIPMethodHashTableEntry::serialize(TR_IPMethodHashTableEntry *entry)
   {
   TR_ContiguousIPMethodHashTableEntry serialEntry;
   memset(&serialEntry, 0, sizeof(TR_ContiguousIPMethodHashTableEntry));
   serialEntry._method = entry->_method;
   serialEntry._otherBucket = entry->_otherBucket;

   size_t callerIdx = 0;
   for (TR_IPMethodData *caller = &entry->_caller; caller; caller = caller->next)
      {
      auto &serialCaller = serialEntry._callers[callerIdx];
      serialCaller._method = caller->getMethod();
      serialCaller._pcIndex = caller->getPCIndex();
      serialCaller._weight = caller->getWeight();
      callerIdx++;
      }
   return serialEntry;
   }

TR_IPMethodHashTableEntry *
TR_JaasIProfiler::searchForMethodSample(TR_OpaqueMethodBlock *omb, int32_t bucket)
   {
   auto stream = TR::CompilationInfo::getStream();
   if (!stream)
      {
      fprintf(stderr, "searchForMethodSample no stream\n");
      return nullptr;
      }
   stream->write(JAAS::J9ServerMessageType::IProfiler_searchForMethodSample, omb, bucket);
   const std::string &entryStr = std::get<0>(stream->read<std::string>());
   if (entryStr.empty())
      {
      fprintf(stderr, "searchForMethodSample no sample for %p %p\n", omb, bucket);
      return nullptr;
      }
   fprintf(stderr, "searchForMethodSample found sample for %p %p\n", omb, bucket);
   const auto serialEntry = (TR_ContiguousIPMethodHashTableEntry*) &entryStr[0];
   return deserializeMethodEntry(serialEntry);
   }


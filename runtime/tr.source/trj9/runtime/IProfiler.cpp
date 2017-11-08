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

#if SOLARIS || AIXPPC || LINUX
#include <strings.h>
#define J9OS_STRNCMP strncasecmp
#else
#define J9OS_STRNCMP strncmp
#endif
#define J9_EXTERNAL_TO_VM

#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include "bcnames.h"
#include "jilconsts.h"
#include "j9cp.h"
#include "j9cfg.h"
#include "rommeth.h"
#include "vmaccess.h"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/PersistentInfo.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Monitor.hpp"
#include "infra/MonitorTable.hpp"
#include "infra/SimpleRegex.hpp"
#include "runtime/J9VMAccess.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/Runtime.hpp"
#include "trj9/control/CompilationRuntime.hpp"
#include "trj9/env/J9JitMemory.hpp"
#include "trj9/env/VMJ9.h"
#include "trj9/env/j9method.h"
#include "trj9/env/ut_j9jit.h"
#include "trj9/ilgen/J9ByteCode.hpp"
#include "trj9/ilgen/J9ByteCodeIterator.hpp"
#include "trj9/runtime/IProfiler.hpp"
#include "runtime/J9Profiler.hpp"

#define BC_HASH_TABLE_SIZE  34501 // 131071// 34501
#undef  IPROFILER_CONTENDED_LOCKING
#define ALLOC_HASH_TABLE_SIZE 1201
#define TEST_verbose 0
#define TEST_callsite 0
#define TEST_statistics 0
#define TEST_inlining 0
#define TEST_disableCSI 0
#undef PERSISTENCE_VERBOSE

#define IPMETHOD_HASH_TABLE_SIZE 12007
#define MAX_THREE(X,Y,Z) ((X>Y)?((X>Z)?X:Z):((Y>Z)?Y:Z))


//#define TRACE_CSI_SIMILARITY
#undef TRACE_CSI_SIMILARITY
//#define TRACE_CSI_PARTIAL
#undef TRACE_CSI_PARTIAL
//#define TRACE_CSI_MERGE
#undef TRACE_CSI_MERGE
//#define TRACE_CSI_INLINER_MATCHING
#undef TRACE_CSI_INLINER_MATCHING

//these are hard limits for allocating onstack arrays for CSI
const int CONTEXT_SLACK = 5; //in case if we went over limit due to non-thread-safe code
const int HARD_MAX_NUM_CONTEXTS_LIMIT = 1000;
const int MAX_NUM_CONTEXTS = HARD_MAX_NUM_CONTEXTS_LIMIT + CONTEXT_SLACK;
const uint32_t INVALID_BYTECODE_INDEX = ~0;

extern "C" int32_t getCount(J9ROMMethod *romMethod, TR::Options *optionsJIT, TR::Options *optionsAOT);


static J9PortLibrary *staticPortLib = NULL;
static uint32_t memoryConsumed = 0;



static
void printHashedCallSite ( TR_IPHashedCallSite * hcs, ::FILE* fout = stderr, void* tag = NULL) {

   J9UTF8 * nameUTF8;
   J9UTF8 * signatureUTF8;
   J9UTF8 * methodClazzUTRF8;

   getClassNameSignatureFromMethod(hcs->_method, methodClazzUTRF8, nameUTF8, signatureUTF8);
   fprintf(fout, "%p %.*s.%.*s%.*s %p %d\n", tag,
      J9UTF8_LENGTH(methodClazzUTRF8), J9UTF8_DATA(methodClazzUTRF8),
      J9UTF8_LENGTH(nameUTF8), J9UTF8_DATA(nameUTF8),
      J9UTF8_LENGTH(signatureUTF8), J9UTF8_DATA(signatureUTF8),
      hcs->_method,
      hcs->_offset);
   fflush(fout);

}





static bool matchesRegularExpression (TR::Compilation* comp) {

   static char* cRegex = feGetEnv ("TR_printIfRegex");
   if (cRegex && comp->getOptions())
      {
      static TR::SimpleRegex * regex = TR::SimpleRegex::create(cRegex);
      if (TR::SimpleRegex::match(regex, comp->signature(), false))
         {
         return true;
         }
      }

   return false;
}


//__declspec( thread ) int tlsdata;

int32_t TR_IProfiler::_STATS_noProfilingInfo                = 0;
int32_t TR_IProfiler::_STATS_weakProfilingRatio             = 0;
int32_t TR_IProfiler::_STATS_doesNotWantToGiveProfilingInfo = 0;
int32_t TR_IProfiler::_STATS_cannotGetClassInfo             = 0;
int32_t TR_IProfiler::_STATS_timestampHasExpired            = 0;
int32_t TR_IProfiler::_STATS_abortedPersistence             = 0;
int32_t TR_IProfiler::_STATS_methodPersisted                = 0;
int32_t TR_IProfiler::_STATS_persistError                   = 0;
int32_t TR_IProfiler::_STATS_methodPersistenceAttempts      = 0;
int32_t TR_IProfiler::_STATS_methodNotPersisted_SCCfull     = 0;
int32_t TR_IProfiler::_STATS_methodNotPersisted_classNotInSCC=0;
int32_t TR_IProfiler::_STATS_methodNotPersisted_delayed     = 0;
int32_t TR_IProfiler::_STATS_methodNotPersisted_other       = 0;
int32_t TR_IProfiler::_STATS_methodNotPersisted_alreadyStored=0;
int32_t TR_IProfiler::_STATS_methodNotPersisted_noEntries   = 0;
int32_t TR_IProfiler::_STATS_entriesPersisted               = 0;
int32_t TR_IProfiler::_STATS_entriesNotPersisted_NotInSCC   = 0;
int32_t TR_IProfiler::_STATS_entriesNotPersisted_Unloaded   = 0;
int32_t TR_IProfiler::_STATS_entriesNotPersisted_NoInfo     = 0;
int32_t TR_IProfiler::_STATS_entriesNotPersisted_Other      = 0;
int32_t TR_IProfiler::_STATS_persistedIPReadFail =0;
int32_t TR_IProfiler::_STATS_persistedIPReadHadBadData =0;
int32_t TR_IProfiler::_STATS_persistedIPReadSuccess =0;
int32_t TR_IProfiler::_STATS_persistedAndCurrentIPDataDiffer =0;
int32_t TR_IProfiler::_STATS_persistedAndCurrentIPDataMatch =0;

int32_t TR_IProfiler::_STATS_currentIPReadFail =0;
int32_t TR_IProfiler::_STATS_currentIPReadSuccess =0;
int32_t TR_IProfiler::_STATS_currentIPReadHadBadData =0;
int32_t TR_IProfiler::_STATS_IPEntryRead = 0;
int32_t TR_IProfiler::_STATS_IPEntryChoosePersistent = 0;




bool TR_ReadSampleRequestsHistory::init(int32_t historyBufferSize)
   {
   _crtIndex = 0;
   _historyBufferSize = historyBufferSize;
   _history =  (TR_ReadSampleRequestsStats *) jitPersistentAlloc(historyBufferSize * sizeof(TR_ReadSampleRequestsStats));
   if (_history)
      {
      memset(_history, 0, historyBufferSize*sizeof(TR_ReadSampleRequestsStats));
      return true;
      }
   return false;
   }



uint32_t TR_ReadSampleRequestsHistory::getReadSampleFailureRate() const
   {
   int32_t oldestIndex = nextIndex();
   uint32_t readSamplesDiff = _history[_crtIndex]._totalReadSampleRequests - _history[oldestIndex]._totalReadSampleRequests;
   if (readSamplesDiff > SAMPLE_CUTOFF) // prevent division by 0 and also false alarms when the number of samples is too small
      {
      uint32_t failedReadSamplesDiff = _history[_crtIndex]._failedReadSampleRequests - _history[oldestIndex]._failedReadSampleRequests;
      return failedReadSamplesDiff*100/readSamplesDiff;
      }
   else
      {
      return 0; // Pretend everything is fine
      }
   }

void TR_ReadSampleRequestsHistory::advanceEpoch() // performed by sampling thread only
   {
   // compute the index of the new buffer
   int32_t newIndex = nextIndex();
   // propagate values from current buffer into the new one
   _history[newIndex]._totalReadSampleRequests = _history[_crtIndex]._totalReadSampleRequests;
   _history[newIndex]._failedReadSampleRequests = _history[_crtIndex]._failedReadSampleRequests;
   // update value of current buffer
   _crtIndex = newIndex;
   }



uintptrj_t
TR_IProfiler::createBalancedBST(uintptrj_t *pcEntries, int32_t low, int32_t high, uintptrj_t memChunk,
                                TR::Compilation *comp, uintptrj_t cacheStartAddress)
   {
   if (high < low)
      return NULL;

   TR_IPBCDataStorageHeader * storage = (TR_IPBCDataStorageHeader *) memChunk;
   int32_t middle = (high+low)/2;
   TR_IPBytecodeHashTableEntry *entry = profilingSample (pcEntries[middle], 0, false);
   uint32_t bytes = entry->getBytesFootprint();
   entry->createPersistentCopy(cacheStartAddress, storage, _compInfo->getPersistentInfo());

   uintptrj_t leftChild = createBalancedBST(pcEntries, low, middle-1,
                                            memChunk + bytes, comp, cacheStartAddress);

   if (leftChild)
      {
      TR_ASSERT(bytes < 1 << 8, "Error storing iprofile information: left child too far away"); // current size of left child
      storage->left = bytes;
      }

   uintptrj_t rightChild = createBalancedBST(pcEntries, middle+1, high,
                                             memChunk + bytes + leftChild, comp, cacheStartAddress);
   if (rightChild)
      {
      TR_ASSERT(bytes + leftChild < 1 << 16, "Error storing iprofile information: right child too far away"); // current size of right child
      storage->right = bytes+leftChild;
      }

   return bytes + leftChild + rightChild;
   }

uint32_t
TR_IProfiler::walkILTreeForEntries(uintptrj_t *pcEntries, uint32_t &numEntries, TR_J9ByteCodeIterator *bcIterator, TR_OpaqueMethodBlock *method, TR::Compilation *comp,
                                   uintptrj_t cacheOffset, uintptrj_t cacheSize, vcount_t visitCount, int32_t callerIndex, TR_BitVector *BCvisit, bool &abort)
   {
   abort = false;
   uint32_t bytesFootprint = 0;
   TR_J9ByteCode bc = bcIterator->first(), nextBC;
   for (; bc != J9BCunknown; bc = bcIterator->next())
      {
      uint32_t bci = bcIterator->bcIndex();

      if (bci < TR::Compiler->mtd.bytecodeSize(method) && !BCvisit->isSet(bci))
         {
         uintptrj_t thisPC = getSearchPC (method, bci, comp);
         TR_IPBytecodeHashTableEntry *entry = profilingSample (thisPC, 0, false);
         BCvisit->set(bci);

         if (entry && !invalidateEntryIfInconsistent(entry))
            {
            int32_t i;
            // now I check if it can be persisted, lock it, and add it to my list.
            uint32_t canPersist = entry->canBePersisted(cacheOffset, cacheSize, _compInfo->getPersistentInfo());
            if (canPersist == IPBC_ENTRY_CAN_PERSIST)
               {
               bytesFootprint += entry->getBytesFootprint();
               // doing insertion sort as we go.
               for (i = numEntries; i > 0 && pcEntries[i-1] > thisPC; i--)
                  {
                  pcEntries[i] = pcEntries[i-1];
                  }
               pcEntries[i] = thisPC;
               numEntries++;
               }
            else // cannot persist
               {
               switch (canPersist) {
                  case IPBC_ENTRY_PERSIST_LOCK:
                     // that means the entry is locked by another thread. going to abort the
                     // storage of iprofiler information for this method
                     {
                     // In some corener cases of invoke interface, we may come across the same entry
                     // twice under 2 different bytecodes. In that case, the other entry has been
                     // locked by this thread and is in the list of entries, so don't abort.
                     bool found = false;
                     int32_t a1 = 0, a2 = numEntries-1;
                     while (a2 >= a1 && !found)
                        {
                        i = (a1+a2)/2;
                        if (pcEntries[i] == thisPC)
                           found = true;
                        else if (pcEntries[i] < thisPC)
                           a1 = i+1;
                        else
                           a2 = i-1;
                        }
                     if (!found)
                        {
                        abort = true;
                        return 0;
                        }
                     }
                     break;
                  case IPBC_ENTRY_PERSIST_NOTINSCC:
                     _STATS_entriesNotPersisted_NotInSCC++;
                     break;
                  case IPBC_ENTRY_PERSIST_UNLOADED:
                     _STATS_entriesNotPersisted_Unloaded++;
                     break;
                  default:
                     _STATS_entriesNotPersisted_Other++;
                  }
               }
            }
         else
            {
            _STATS_entriesNotPersisted_NoInfo++;
            }
         }
      else
         {
         TR_ASSERT(bci < TR::Compiler->mtd.bytecodeSize(method), "bytecode can't be greater then method size");
         }
      }

   return bytesFootprint;
   }


void
TR_IProfiler::persistIprofileInfo(TR::ResolvedMethodSymbol *resolvedMethodSymbol, TR_ResolvedMethod *resolvedMethod, TR::Compilation *comp)
   {
   static bool traceIProfiling = (debug("traceIProfiling") != NULL);
   static bool SCfull = false;

   if (resolvedMethod->isNative())
      return; // there nothing to store in natives

   _STATS_methodPersistenceAttempts++;

   // From here, down, stack memory allocations will expire / die when the method returns
   TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());

   TR_OpaqueMethodBlock* method = resolvedMethod->getPersistentIdentifier();
#ifdef PERSISTENCE_VERBOSE
   char methodSig[500];
   _vm->printTruncatedSignature(methodSig, 500, method);
   fprintf(stderr, "persistIprofileInfo for %s while compiling %s\n", methodSig, comp->signature());
#endif

   if (TR::Options::sharedClassCache()        // shared classes must be enabled
      && !comp->getOption(TR_DisablePersistIProfile) &&
      isIProfilingEnabled() &&
      (!SCfull || !TR::Options::getCmdLineOptions()->getOption(TR_DisableUpdateJITBytesSize)))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)_vm;
      J9SharedClassConfig * scConfig = _compInfo->getJITConfig()->javaVM->sharedClassConfig;
      J9SharedClassCacheDescriptor* desc = scConfig->cacheDescriptorList;
      uintptrj_t cacheOffset = (uintptrj_t) desc->cacheStartAddress;
      uintptrj_t cacheSize = (uintptrj_t) desc->cacheSizeBytes;

      uintptrj_t methodSize = (uintptrj_t ) TR::Compiler->mtd.bytecodeSize(method);
      uintptrj_t methodStart = (uintptrj_t ) TR::Compiler->mtd.bytecodeStart(method);

      uint32_t bytesFootprint = 0;
      uint32_t numEntries = 0;

      // Allocate memory for every possible node in this method
      //
      J9ROMMethod * romMethod = (J9ROMMethod *)J9_ROM_METHOD_FROM_RAM_METHOD((J9Method *)method);

      TR::Options * optionsJIT = TR::Options::getJITCmdLineOptions();
      TR::Options * optionsAOT = TR::Options::getAOTCmdLineOptions();

      int32_t count = getCount(romMethod, optionsJIT, optionsAOT);
      int32_t currentCount = resolvedMethod->getInvocationCount();


      bool doit = true;

      // can only persist profile info if the method is in the shared cache
      if (doit && _compInfo->reloRuntime()->isROMClassInSharedCaches((uintptrj_t)romMethod, _compInfo->getJITConfig()->javaVM))
        {
         TR_ASSERT(_compInfo->reloRuntime()->isROMClassInSharedCaches((uintptrj_t)methodStart, _compInfo->getJITConfig()->javaVM), "bytecodes not in shared cache");
         // check if there is already an entry
         unsigned char storeBuffer[1000];
         uint32_t bufferLength = 1000;
         J9SharedDataDescriptor descriptor;
         descriptor.address = storeBuffer;
         descriptor.length = bufferLength;
         descriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
         descriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
         J9VMThread *vmThread = ((TR_J9VM *)comp->fej9())->getCurrentVMThread();
         IDATA dataIsCorrupt;
         const U_8 *found = scConfig->findAttachedData(vmThread, romMethod, &descriptor, &dataIsCorrupt);
         if (!found)
            {
            if (traceIProfiling && resolvedMethodSymbol)
               comp->dumpMethodTrees("Pre Iprofiler Walk", resolvedMethodSymbol);

            if (comp->getOption(TR_DumpPersistedIProfilerMethodNamesAndCounts))
               {
               char methodSig[3000];
               fej9->printTruncatedSignature(methodSig, 3000, method);
               fprintf(stdout, "Persist: %s count %d  Compiling %s\n", methodSig, comp->signature() );
               }

            vcount_t visitCount = comp->incVisitCount();
            TR_BitVector *BCvisit = new (comp->trStackMemory()) TR_BitVector(TR::Compiler->mtd.bytecodeSize(method), comp->trMemory(), stackAlloc);
            bool abort=false;
            TR_J9ByteCodeIterator bci(0, static_cast<TR_ResolvedJ9Method *> (resolvedMethod), static_cast<TR_J9VMBase *> (comp->fej9()), comp);
            uintptrj_t * pcEntries = (uintptrj_t *) comp->trMemory()->allocateMemory(sizeof(uintptrj_t) * bci.maxByteCodeIndex(), stackAlloc);
            bytesFootprint += walkILTreeForEntries(pcEntries, numEntries, &bci, method, comp, cacheOffset, cacheSize,
                                                      visitCount, -1, BCvisit, abort);

            if (numEntries && !abort)
               {
               uint32_t bytesToPersist = 0;

               if (!SCfull)
                  {
#ifdef PERSISTENCE_VERBOSE
                  fprintf(stderr, "Entries found:");
                  for (int32_t i=0; i < numEntries; i++)
                     fprintf(stderr, " %p(bytecode=0x%x)", pcEntries[i], *((char*)pcEntries[i]));
                  fprintf(stderr, "\n");
#endif
                  void * memChunk = comp->trMemory()->allocateMemory(bytesFootprint, stackAlloc);
                  intptrj_t bytes = createBalancedBST(pcEntries, 0, numEntries-1, (uintptrj_t) memChunk, comp, cacheOffset);
                  TR_ASSERT(bytes == bytesFootprint, "BST doesn't match expected footprint");


                  // store in the shared cache
                  descriptor.address = (U_8 *) memChunk;
                  descriptor.length = bytesFootprint;
                  UDATA store = scConfig->storeAttachedData(vmThread, romMethod, &descriptor, 0);
                  if (store == 0)
                     {
                     _STATS_methodPersisted++;
                     _STATS_entriesPersisted += numEntries;
#ifdef PERSISTENCE_VERBOSE
                     fprintf(stderr, "\tPersisted %d entries\n", numEntries);
#endif
                     }
                  else if (store != J9SHR_RESOURCE_STORE_FULL)
                     {
                     _STATS_persistError++;
#ifdef PERSISTENCE_VERBOSE
                     fprintf(stderr, "\tNot Persisted: error\n");
#endif
                     }
                  else
                     {
                     SCfull = true;
                     _STATS_methodNotPersisted_SCCfull++;
                     bytesToPersist = bytesFootprint;
#ifdef PERSISTENCE_VERBOSE
                     fprintf(stderr, "\tNot Persisted: SCC full\n");
#endif
                     }
                  }
               else // SC Full
                  {
                  _STATS_methodNotPersisted_SCCfull++;
#ifdef PERSISTENCE_VERBOSE
                  fprintf(stderr, "\tNot Persisted: SCC full\n");
#endif
                  bytesToPersist = bytesFootprint;
                  }

               if (SCfull &&
                   bytesToPersist &&
                   !TR::Options::getCmdLineOptions()->getOption(TR_DisableUpdateJITBytesSize))
                  {
                  _compInfo->increaseUnstoredBytes(0, bytesToPersist);
                  }
               }
            else
               {
               if (abort)
                  {
                  _STATS_abortedPersistence++;
#ifdef PERSISTENCE_VERBOSE
                  fprintf(stderr, "\tNot Persisted: aborted\n");
#endif
                  }
               else if (numEntries == 0)
                  {
                  _STATS_methodNotPersisted_noEntries++;
#ifdef PERSISTENCE_VERBOSE
                  fprintf(stderr, "\tNot Persisted: no entries\n");
#endif
                  }
               }

            // release any entry that has been locked by us
            for (uint32_t i = 0; i < numEntries; i++)
               {
               TR_IPBCDataCallGraph *cgEntry = profilingSample (pcEntries[i], 0, false)->asIPBCDataCallGraph();
               if (cgEntry)
                  cgEntry->releaseEntry();
               }
            }
         else
            {
            _STATS_methodNotPersisted_alreadyStored++;
#ifdef PERSISTENCE_VERBOSE
            fprintf(stderr, "\tNot Persisted: already stored\n");
#endif
            }
         }
      else
         {
         if(!doit && comp->getOption(TR_DumpPersistedIProfilerMethodNamesAndCounts))
             {
             char methodSig[3000];
             fej9->printTruncatedSignature(methodSig, 3000, method);
             fprintf(stdout, "Delay Persist: %s count %d  Compiling %s\n", methodSig, comp->signature() );
             }

           if(doit)
              _STATS_methodNotPersisted_classNotInSCC++;
           else
              _STATS_methodNotPersisted_delayed++;

#ifdef PERSISTENCE_VERBOSE
         fprintf(stderr, "\tNot Persisted: class not in SCC\n");
#endif
         }
      }
   else
      {
      _STATS_methodNotPersisted_other++;
#ifdef PERSISTENCE_VERBOSE
       fprintf(stderr, "\tNot Persisted: other\n");
#endif
      }
   }

uint32_t
TR_IProfiler::getProfilerMemoryFootprint()
   {
   return memoryConsumed;
   }

void *
TR_IProfiler::operator new (size_t size) throw()
   {
   memoryConsumed += (int32_t)size;
   void *alloc = jitPersistentAlloc(size);
   return alloc;
   }

TR_IProfiler *
TR_IProfiler::allocate(J9JITConfig *jitConfig)
   {
   TR_IProfiler * profiler = new (PERSISTENT_NEW) TR_IProfiler(jitConfig);
   return profiler;
   }

TR_IProfiler::TR_IProfiler(J9JITConfig *jitConfig)
   : _isIProfilingEnabled(true),
     _valueProfileMethod(NULL), _maxCount(DEFAULT_PROFILING_COUNT), _lightHashTableMonitor(0), _allowedToGiveInlinedInformation(true),
     _globalAllocationCount (0), _maxCallFrequency(0), _iprofilerThread(0), _iprofilerOSThread(NULL),
     _workingBufferTail(NULL), _numOutstandingBuffers(0), _numRequests(1), _numRequestsSkipped(0),
     _numRequestsHandedToIProfilerThread(0), _iprofilerThreadExitFlag(0), _iprofilerMonitor(NULL),
     _crtProfilingBuffer(NULL), _iprofilerThreadAttachAttempted(false), _iprofilerNumRecords(0)
   {
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);

   _iprofilerBufferSize = (uint32_t)jitConfig->iprofilerBufferSize; //J9_PROFILING_BUFFER_SIZE;
   _portLib = jitConfig->javaVM->portLibrary;
   _vm = TR_J9VMBase::get(jitConfig, 0);
   staticPortLib = _portLib;
   _classLoadTimeStampGap = (int32_t)jitConfig->samplingFrequency * 30;

   _compInfo = TR::CompilationInfo::get(jitConfig);

   if (TR::Options::getCmdLineOptions()->getOption(TR_DisableInterpreterProfiling))
      _isIProfilingEnabled = false;

   // initialize the monitors
   _hashTableMonitor = TR::Monitor::create("JIT-InterpreterProfilingMonitor");

   // bytecode hashtable
   _bcHashTable = (TR_IPBytecodeHashTableEntry**)jitPersistentAlloc(BC_HASH_TABLE_SIZE*sizeof(TR_IPBytecodeHashTableEntry*));
   if (_bcHashTable != NULL)
      memset(_bcHashTable, 0, BC_HASH_TABLE_SIZE*sizeof(TR_IPBytecodeHashTableEntry*));
   else
      _isIProfilingEnabled = false;

#if defined(EXPERIMENTAL_IPROFILER)
   _allocHashTable = (TR_IPBCDataAllocation**)jitPersistentAlloc(ALLOC_HASH_TABLE_SIZE*sizeof(TR_IPBCDataAllocation*));
   if (_allocHashTable != NULL)
      memset(_allocHashTable, 0, ALLOC_HASH_TABLE_SIZE*sizeof(TR_IPBCDataAllocation*));
#endif
   _methodHashTable = (TR_IPMethodHashTableEntry **) jitPersistentAlloc(IPMETHOD_HASH_TABLE_SIZE * sizeof(TR_IPMethodHashTableEntry *));
   if (_methodHashTable != NULL)
      memset(_methodHashTable, 0, IPMETHOD_HASH_TABLE_SIZE * sizeof(TR_IPMethodHashTableEntry *));
   _readSampleRequestsHistory = (TR_ReadSampleRequestsHistory *) jitPersistentAlloc(sizeof (TR_ReadSampleRequestsHistory));
   if (!_readSampleRequestsHistory || !_readSampleRequestsHistory->init(TR::Options::_iprofilerFailHistorySize))
      {
      _isIProfilingEnabled = false;
      }
   }


void
TR_IProfiler::shutdown()
   {
   _isIProfilingEnabled = false; // This is the only instance where we disable the profiler

   }

static uint16_t cpIndexFromPC(uintptrj_t pc) { return *((uint16_t*)(pc + 1)); }
static U_32 vftOffsetFromPC (J9Method *method, uintptrj_t pc)
   {
   uint16_t cpIndex = cpIndexFromPC(pc);
   TR_ASSERT(cpIndex != (uint16_t)~0, "cpIndex shouldn't be -1");
   J9RAMConstantPoolItem *literals = (J9RAMConstantPoolItem *) J9_CP_FROM_METHOD(method);
   UDATA vTableSlot = ((J9RAMVirtualMethodRef *)literals)[cpIndex].methodIndexAndArgCount >> 8;
   TR_ASSERT(vTableSlot, "vTableSlot called for unresolved method");

   return (U_32) (J9JIT_INTERP_VTABLE_OFFSET - vTableSlot);
   }


bool
TR_IProfiler::isCallGraphProfilingEnabled()
   {
   // if less than 5% of the table is populated don't give out
   // call-graph profiling data to consumers. It's likely count=0
   // or initial startup so better to be safe than sorry.
   return _enableCGProfiling && !_compInfo->getPersistentInfo()->isClassLoadingPhase();
   }


inline int32_t
TR_IProfiler::bcHash(uintptrj_t pc)
   {
   return (int32_t)((pc & 0x7FFFFFFF) % BC_HASH_TABLE_SIZE);
   }

inline int32_t
TR_IProfiler::allocHash(uintptrj_t pc)
   {
   return (int32_t)((pc & 0x7FFFFFFF) % ALLOC_HASH_TABLE_SIZE);
   }

inline int32_t
TR_IProfiler::methodHash(uintptrj_t data)
   {
   return (int32_t)((data & 0x7FFFFFFF) % IPMETHOD_HASH_TABLE_SIZE);
   }

bool
TR_IProfiler::isCompact (U_8 byteCode)
   {
   switch (byteCode)
      {
         case JBifacmpeq:
         case JBifacmpne:
         case JBifeq:
         case JBifge:
         case JBifgt:
         case JBifle:
         case JBiflt:
         case JBifne:
         case JBificmpeq:
         case JBificmpne:
         case JBificmpge:
         case JBificmpgt:
         case JBificmple:
         case JBificmplt:
         case JBifnull:
         case JBifnonnull:
            return true;
         default:
            return false;
      }
   }

bool
TR_IProfiler::isSwitch (U_8 byteCode)
   {
   return ((byteCode == JBlookupswitch) ||
           (byteCode == JBtableswitch));
   }

bool isCallByteCode(U_8 byteCode)
   {
   switch(byteCode)
      {
      case JBinvokestatic:
      case JBinvokespecial:
      case JBinvokeinterface:
      case JBinvokeinterface2:
      case JBinvokevirtual:
        return true;
      default:
        return false;
      }
   }

bool isInterfaceBytecode(U_8 byteCode)
   {
   return (byteCode == JBinvokeinterface);
   }

bool isInterface2Bytecode(U_8 byteCode)
   {
   return (byteCode == JBinvokeinterface2);
   }

bool isCheckCastOrInstanceOf(U_8 byteCode)
   {
   return ((byteCode == JBinstanceof) ||
           (byteCode == JBcheckcast));
   }

bool isSpecialOrStatic(U_8 byteCode)
   {
   return ((byteCode == JBinvokestatic)
          || (byteCode == JBinvokespecial)
#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
          || (byteCode == JBinvokestaticsplit)
          || (byteCode == JBinvokespecialsplit)
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
          );
   }

static void
getSwitchSegmentDataAndCount(uint64_t segment, uint32_t *segmentData, uint32_t *segmentCount)
   {
   // each segment is 2 bytes long and contains
   // switch data   count
   // | 0000000 | 00000000 |
   *segmentData = (uint32_t)((segment >> 32) & 0xFFFFFFFF);
   *segmentCount = (uint32_t)(segment & 0xFFFFFFFF);
   }

static void
setSwitchSegmentData (uint64_t *segment, uint32_t data)
   {
   *segment &= CONSTANT64(0x00000000FFFFFFFF);
   *segment |= (((uint64_t)data) << 32);
   }

static void
setSwitchSegmentCount (uint64_t *segment, uint32_t count)
   {
   *segment &= CONSTANT64(0xFFFFFFFF00000000);
   *segment |= count;
   }

static int32_t nextSwitchValue(uintptrj_t & pc)
   {
   int32_t value = *((int32_t *)pc);
   pc += 4;
   return value;
   }


static void lookupSwitchIndexForValue(uintptrj_t startPC, int32_t value, int32_t & target, int32_t & index)
   {
   uintptrj_t pc = (startPC + 4) & ((uintptrj_t) -4) ;

   index = 0;
   target = nextSwitchValue(pc);
   int32_t tableSize = nextSwitchValue(pc);

   for (int32_t i = 0; i < tableSize; i++)
      {
      int32_t intMatch = nextSwitchValue(pc);

      if (value == intMatch)
         {
         index = i+1;
         target = nextSwitchValue(pc);
         return;
         }
      else
         {
         int32_t dummyTarget = nextSwitchValue(pc);
         }
      }
   }

static void tableSwitchIndexForValue(uintptrj_t startPC, int32_t value, int32_t & target, int32_t & index)
   {
   uintptrj_t pc = (startPC + 4) & ((uintptrj_t) -4) ;

   index = 0;
   target = nextSwitchValue(pc);
   int32_t low = nextSwitchValue(pc);
   int32_t high = nextSwitchValue(pc);

   if (value >= low && value <= high)
      {
      index = value - low + 1;
      pc += 4 * (value-low);
      target = nextSwitchValue(pc);
      }
   }

static int32_t lookupSwitchBytecodeToOffset (uintptrj_t startPC, int32_t index)
   {
   uintptrj_t pc = (startPC + 4) & ((uintptrj_t) -4) ;

   if (index > 0) pc += 8*index+4;

   return nextSwitchValue(pc);
   }

static int32_t tableSwitchBytecodeToOffset (uintptrj_t startPC, int32_t index)
   {
   uintptrj_t pc = (startPC + 4) & ((uintptrj_t) -4) ;

   if (index > 0) pc += 4*index+8;

   return nextSwitchValue(pc);
   }

int32_t
TR_IProfiler::getOrSetSwitchData(TR_IPBCDataEightWords *entry, uint32_t value, bool isSet, bool isLookup)
   {
   uint64_t *p = entry->getDataPointer();
   int32_t data = 0;
   int32_t index = 0;

   // we ignore the index for now, it will be useful when switch analyser
   // becomes smarter to order same target values

   if (isSet)
      (isLookup) ? lookupSwitchIndexForValue (entry->getPC(), value, data, index) :
                   tableSwitchIndexForValue (entry->getPC(), value, data, index);
   else
      data = value;

   for (int8_t i=0; i<SWITCH_DATA_COUNT; i++, p++)
      {
      uint64_t segment = *p;
      uint32_t segmentData  = 0;
      uint32_t segmentCount = 0;

      getSwitchSegmentDataAndCount (segment, &segmentData, &segmentCount);

      // the first 3 records are real switch targets
      // while the last one is for the rest. We use the
      // rest to determine if there are dominant case statements.

      if (isSet && (segmentCount == 0xFFFFFFFF))
         return 1;
      if (i == (SWITCH_DATA_COUNT - 1))
         {
         if (isSet)
            {
            if (segmentCount < 0xFFFFFFFF)
               segment++;
            *p = segment;
            }
         return 0;
         }
      else if (segmentData == data)
         {
         if (isSet)
            {
            if (segmentCount < 0xFFFFFFFF)
               segment++;
            *p = segment;
            }

         return segmentCount;
         }
      else if (isSet && segmentData == 0)
         {
         segment = data;
         segment <<= 32;
         segment &= CONSTANT64(0xFFFFFFFF00000000);
         segment |= 0x1;
         *p = segment;
         return 1;
         }
      }

   return 0;
   }

bool
TR_IProfiler::addSampleData(TR_IPBytecodeHashTableEntry *entry, uintptrj_t data, bool isRIData, uint32_t freq)
   {
   U_8 *entryPC = (U_8*)entry->getPC();

   if (entry->isInvalid())
      return false;

   U_8 byteCodeType = *(entryPC);
   switch (byteCodeType)
      {
      case JBifacmpeq:
      case JBifacmpne:
      case JBifeq:
      case JBifge:
      case JBifgt:
      case JBifle:
      case JBiflt:
      case JBifne:
      case JBificmpeq:
      case JBificmpne:
      case JBificmpge:
      case JBificmpgt:
      case JBificmple:
      case JBificmplt:
      case JBifnull:
      case JBifnonnull:
         // Legend:
         //  taken   not-taken
         // | 0000 | 0000 |
         TR_ASSERT(entry->asIPBCDataFourBytes(), "The new branch target code uses 4 bytes to store branch history");

         if (data)
            {
            if (((entry->getData()) & 0xFFFF0000)==0xFFFF0000)
               {
               size_t data = entry->getData();
               data >>= 1;
               data &= 0x7FFF7FFF;

               entry->setData(data);
               }

            entry->setData(entry->getData() + (1<<16));
            }
         else
            {
            if (((entry->getData()) & 0x0000FFFF)==0x0000FFFF)
               {
               size_t data = entry->getData();
               data >>= 1;
               data &= 0x7FFF7FFF;

               entry->setData(data);
               }

            entry->setData(entry->getData()+1);
            }
         return true;
      case JBinvokestatic:
      case JBinvokespecial:
#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
      case JBinvokestaticsplit:
      case JBinvokespecialsplit:
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
      case JBinvokeinterface:
      case JBinvokeinterface2:
      case JBinvokevirtual:
      case JBcheckcast:
      case JBinstanceof:
         {
         bool isVirtualCall = (byteCodeType == JBinvokeinterface ||
                               byteCodeType == JBinvokeinterface2 ||
                               byteCodeType == JBinvokevirtual);

         if (isRIData)
            {
            if (TR::Options::getCmdLineOptions()->getOption(TR_DontAddHWPDataToIProfiler))
               return true;
            }
         else if (isVirtualCall &&
                  TR::Options::getCmdLineOptions()->getOption(TR_DisableVMCSProfiling))
            {
            return true;
            }

#if defined (RI_VP_Verbose)
         if (isVirtualCall &&
             TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler))
            {
            char * byteCodeName = "Unknown";
            if (byteCodeType == JBinvokeinterface)
               byteCodeName = "JBinvokeinterface";
            else if (byteCodeType == JBinvokeinterface2)
               byteCodeName = "JBinvokeinterface2";
            else if (byteCodeType == JBinvokevirtual)
               byteCodeName = "JBinvokevirtual";

            TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER, "Adding class=0x%p for bytecode=%s (bytecodePC=0x%p) from %s",
                                           data,
                                           byteCodeName,
                                           entry->pc,
                                           isRIData ? "HWProfiler" : "IProfiler");
            }
#endif

         int32_t returnCount = entry->setData(data, freq);
         if (returnCount > _maxCallFrequency)
            _maxCallFrequency = returnCount;
         return true;
         }
      case JBlookupswitch:
         TR_ASSERT(entry->asIPBCDataEightWords(), "The new switch profiling code uses 8 bytes to store switch history");
         getOrSetSwitchData ((TR_IPBCDataEightWords *)entry, (uint32_t)data, true, true);
         return true;
      case JBtableswitch:
         TR_ASSERT(entry->asIPBCDataEightWords(), "The new switch profiling code uses 8 bytes to store switch history");
         getOrSetSwitchData ((TR_IPBCDataEightWords *)entry, (uint32_t)data, true, false);
         return true;
      default:
         return false;
      }

   return false;
   }

TR_IPBytecodeHashTableEntry *
TR_IProfiler::searchForSample(uintptrj_t pc, int32_t bucket)
   {
   TR_IPBytecodeHashTableEntry *entry;

   for (entry = _bcHashTable[bucket]; entry; entry = entry->getNext())
      {
      if (pc == entry->getPC())
         return entry;
      }

   return NULL;
   }

TR_IPBCDataAllocation *
TR_IProfiler::searchForAllocSample(uintptrj_t pc, int32_t bucket)
   {
#if defined(EXPERIMENTAL_IPROFILER)
   TR_ASSERT(0, "This isn't currently supported, and the information is not persisted");
   TR_IPBCDataAllocation *entry;

   for (entry = _allocHashTable[bucket]; entry; entry = (TR_IPBCDataAllocation *)entry->next)
      {
      if (pc == entry->pc)
         return entry;
      }
#endif

   return NULL;
   }


TR_IPBytecodeHashTableEntry *
TR_IProfiler::findOrCreateEntry(int32_t bucket, uintptrj_t pc, bool addIt)
   {
   TR_IPBytecodeHashTableEntry *entry = NULL;

   entry = searchForSample (pc, bucket);
   // if we are just searching and we didn't find profile data for the
   // method just go back
   if (!addIt)
      return entry;

   if (entry)
      return entry;

   // Create a new hash table entry
   U_8 byteCode = *(U_8*) pc;
   if (isCompact(byteCode))
      entry = new TR_IPBCDataFourBytes(pc);
   else
      {
      if (isSwitch(byteCode))
         entry = new TR_IPBCDataEightWords(pc);
      else
         entry = new TR_IPBCDataCallGraph(pc);
      }

   if (!entry)
      return NULL;

   entry->setNext(_bcHashTable[bucket]);
   FLUSH_MEMORY(TR::Compiler->target.isSMP());
   _bcHashTable[bucket] = entry;

   return entry;
   }

TR_IPBCDataAllocation *
TR_IProfiler::findOrCreateAllocEntry(int32_t bucket, uintptrj_t pc, bool addIt)
   {
#if defined(EXPERIMENTAL_IPROFILER)
   TR_IPBCDataAllocation *entry = NULL;

   entry = searchForAllocSample (pc, bucket);
   // if we are just searching and we didn't find profile data for the
   // method just go back
   if (!addIt)
      return entry;

   if (entry)
      return entry;

   // Create a new hash table entry
   //
   entry = new TR_IPBCDataAllocation();

   if (!entry)
      return NULL;

   entry->next = _allocHashTable[bucket];
   entry->pc = pc;
   entry->_lastSeenClassUnloadID = -1; //_compInfo->getPersistentInfo()->getGlobalClassUnloadID();
   FLUSH_MEMORY(TR::Compiler->target.isSMP());
   _allocHashTable[bucket] = entry;

   return entry;
#else
   return NULL;
#endif
   }


TR_IPMethodHashTableEntry *
TR_IProfiler::findOrCreateMethodEntry(J9Method *callerMethod, J9Method *calleeMethod, bool addIt, uint32_t pcIndex)
   {
   TR_IPMethodHashTableEntry *entry = NULL;

   if (!_methodHashTable)
      return NULL;
   // Search the hashtable
   int32_t bucket = methodHash((uintptrj_t)calleeMethod);
   entry = searchForMethodSample((TR_OpaqueMethodBlock*)calleeMethod, bucket);

   if (!addIt)
      return entry;

   if (entry) // hit in the hashtable
      {
      entry->add((TR_OpaqueMethodBlock *)callerMethod, (TR_OpaqueMethodBlock *)calleeMethod, pcIndex);
      }
   else // create a new hash table entry
      {
      memoryConsumed += (int32_t)sizeof(TR_IPMethodHashTableEntry);
      entry = (TR_IPMethodHashTableEntry *)jitPersistentAlloc(sizeof(TR_IPMethodHashTableEntry));
      if (entry)
         {
         memset(entry, 0, sizeof(TR_IPMethodHashTableEntry));
         entry->_next = _methodHashTable[bucket];
         entry->_method = (TR_OpaqueMethodBlock *)calleeMethod;
         // Set-up the first caller which is embedded in the entry
         entry->_caller.setMethod((TR_OpaqueMethodBlock*)callerMethod);
         entry->_caller.setPCIndex(pcIndex);
         entry->_caller.incWeight();

         FLUSH_MEMORY(TR::Compiler->target.isSMP());
         _methodHashTable[bucket] = entry; // chain it
         }
      }
   return entry;
   }


void
TR_IPMethodHashTableEntry::add(TR_OpaqueMethodBlock *caller, TR_OpaqueMethodBlock *callee, uint32_t pcIndex)
   {
   TR_IPMethodData *m = NULL;
   bool useTuples = false;
   if (pcIndex != ~0) // -1 is a magic to signal that we want to callers rather than BCI-caller pairs
                      //We should be safe here since Java imposes limits the size of a method to 64kb
      useTuples = true;


   TR_IPMethodData *it;
   int size = 0;
   // search the list of callers for a match
   for (it = &_caller; it; it = it->next, size++)
      {
      if (it->getMethod() == caller && (!useTuples || it->getPCIndex() == pcIndex))
         break;
      }

   if (it)
      {
      it->incWeight();
      }
   else // caller is not in the list. Let's add it
      {
      // Do not allow more than MAX_IPMETHOD_CALLERS
      if (size >= MAX_IPMETHOD_CALLERS)
         {
         _otherBucket.incWeight();
         }
      else
         {
         TR_IPMethodData* newCaller = (TR_IPMethodData*)jitPersistentAlloc(sizeof(TR_IPMethodData));
         if (newCaller)
            {
            memset(newCaller, 0, sizeof(TR_IPMethodData));
            newCaller->setMethod(caller);
            newCaller->setPCIndex(pcIndex);
            newCaller->incWeight();
            newCaller->next = _caller.next; // add the existing list of callers (except the embedded one) to this new caller
            FLUSH_MEMORY(TR::Compiler->target.isSMP());
            // Add the newCaller after the embedded caller
            _caller.next = newCaller;
            }
         }
      }
   }

bool
TR_IProfiler::invalidateEntryIfInconsistent(TR_IPBytecodeHashTableEntry *entry)
   {
   if (_compInfo->getPersistentInfo()->getGlobalClassUnloadID() != entry->getLastSeenClassUnloadID())
      {
   if (_compInfo->getPersistentInfo()->isInUnloadedMethod(entry->getPC()))
      {
      entry->setInvalid();
      return true;
      }
      else
         {
         entry->setLastSeenClassUnloadID(_compInfo->getPersistentInfo()->getGlobalClassUnloadID());
         }
      }

   return false;
   }

TR_IPBCDataStorageHeader *
TR_IProfiler::searchForPersistentSample (TR_IPBCDataStorageHeader  *root, uintptrj_t pc)
   {

   if (root->pc == pc)
      return root;
   else if (pc < root->pc)
      {
      uint32_t offset = root->left;
      if (offset)
         return searchForPersistentSample((TR_IPBCDataStorageHeader *) (((uint8_t *) root) + offset), pc);
      else
         return NULL;
      }
   else
      {
      uint32_t offset = root->right;
      if (offset)
         return searchForPersistentSample((TR_IPBCDataStorageHeader  *) (((uint8_t *) root) + offset), pc);
      else
         return NULL;
      }
   }

TR_IPBytecodeHashTableEntry *
TR_IProfiler::persistentProfilingSample (TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex,
                                         TR::Compilation *comp, bool *methodProfileExistsInSCC)
   {
   if (TR::Options::sharedClassCache())        // shared classes must be enabled
      {
      J9SharedClassConfig * scConfig = _compInfo->getJITConfig()->javaVM->sharedClassConfig;
      J9SharedClassCacheDescriptor* desc = scConfig->cacheDescriptorList;
      uintptrj_t cacheOffset = (uintptrj_t) desc->cacheStartAddress;
      uintptrj_t cacheSize = (uintptrj_t) desc->cacheSizeBytes;

      TR_J9VMBase *fej9 = (TR_J9VMBase *)_vm;
      uintptrj_t methodStart = (uintptrj_t ) TR::Compiler->mtd.bytecodeStart(method);

      // can only persist profile info if the method is in the shared cache
      if (methodStart < cacheOffset || methodStart > cacheOffset+cacheSize)
         return NULL;

      // check the shared cache
      unsigned char storeBuffer[1000];
      uint32_t bufferLength = 1000;
      J9SharedDataDescriptor descriptor;
      descriptor.address = storeBuffer;
      descriptor.length = bufferLength;
      descriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
      descriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
      J9VMThread *vmThread = ((TR_J9VM *)comp->fej9())->getCurrentVMThread();
      J9ROMMethod * romMethod = (J9ROMMethod*)J9_ROM_METHOD_FROM_RAM_METHOD((J9Method *)method);
      IDATA dataIsCorrupt;
      TR_IPBCDataStorageHeader *store = (TR_IPBCDataStorageHeader *)scConfig->findAttachedData(vmThread, romMethod, &descriptor, &dataIsCorrupt);
      if (store != (TR_IPBCDataStorageHeader *)descriptor.address)  // a stronger check, as found can be error value
         return NULL;

      *methodProfileExistsInSCC = true;
      // Compute the pc we are interested in
      uintptrj_t pc = getSearchPC(method, byteCodeIndex, comp);
      store = searchForPersistentSample(store, pc - cacheOffset);


      if(TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableIprofilerChanges) || TR::Options::getJITCmdLineOptions()->getOption(TR_EnableIprofilerChanges))
         {
         if (store)
            {
            // Load the data from SCC into a brand new HT entry
            // but do not add the entry to the HT just yet
            TR_IPBytecodeHashTableEntry *newEntry = 0;
            U_8 byteCode =  *(U_8 *)pc;
            if (isCompact(byteCode))
               newEntry = new TR_IPBCDataFourBytes(pc);
            else
               {
               if (isSwitch(byteCode))
                  newEntry = new TR_IPBCDataEightWords(pc);
               else
                  newEntry = new TR_IPBCDataCallGraph(pc);
               }
            if (newEntry)
               newEntry->loadFromPersistentCopy(store,comp, cacheOffset);
            return newEntry;
            }
         }
      else
         {

         if (store)
            {
            // Create a new IProfiler hashtable entry and copy the data from the SCC
            TR_IPBytecodeHashTableEntry *newEntry = findOrCreateEntry(bcHash(pc), pc, true);
            newEntry->loadFromPersistentCopy(store, comp, cacheOffset);
            return newEntry;
            }
         }
      return NULL;
      }
   return NULL;
   }

TR_IPBCDataStorageHeader*
TR_IProfiler::persistentProfilingSample (TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex,
                                         TR::Compilation *comp, bool *methodProfileExistsInSCC, uintptrj_t *cacheOffset, TR_IPBCDataStorageHeader *store)
   {
   if (TR::Options::sharedClassCache())        // shared classes must be enabled
      {
      if (!store)
         return NULL;
      J9SharedClassConfig * scConfig = _compInfo->getJITConfig()->javaVM->sharedClassConfig;
      J9SharedClassCacheDescriptor* desc = scConfig->cacheDescriptorList;
      *cacheOffset = (uintptrj_t) desc->cacheStartAddress;
      uintptrj_t cacheSize = (uintptrj_t) desc->cacheSizeBytes;
      TR_J9VMBase *fej9 = (TR_J9VMBase *)_vm;
      uintptrj_t methodStart = (uintptrj_t ) TR::Compiler->mtd.bytecodeStart(method);

      // can only persist profile info if the method is in the shared cache
      if (methodStart < *cacheOffset || methodStart > *cacheOffset+cacheSize)
         return NULL;

      *methodProfileExistsInSCC = true;
      uintptrj_t pc = getSearchPC(method, byteCodeIndex, comp);
      store = searchForPersistentSample(store, pc - *cacheOffset);
      return store;
      }
   return NULL;
   }

TR_IPBCDataStorageHeader *
TR_IProfiler::getJ9SharedDataDescriptorForMethod(J9SharedDataDescriptor * descriptor, unsigned char * buffer, uint32_t length, TR_OpaqueMethodBlock * method, TR::Compilation *comp)
   {
   if (!TR::Options::sharedClassCache())
      return NULL;
   J9SharedClassConfig * scConfig = _compInfo->getJITConfig()->javaVM->sharedClassConfig;
   descriptor->address = buffer;
   descriptor->length = length;
   descriptor->type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
   descriptor->flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
   J9VMThread *vmThread = ((TR_J9VM *)comp->fej9())->getCurrentVMThread();
   J9ROMMethod * romMethod = (J9ROMMethod*)J9_ROM_METHOD_FROM_RAM_METHOD((J9Method *)method);
   IDATA dataIsCorrupt;
   TR_IPBCDataStorageHeader * store = (TR_IPBCDataStorageHeader *) scConfig->findAttachedData(vmThread, romMethod, descriptor, &dataIsCorrupt);
   if (store != (TR_IPBCDataStorageHeader *)descriptor->address)  // a stronger check, as found can be error value
         return NULL;
   return store;
   }

TR_IPMethodHashTableEntry *
TR_IProfiler::searchForMethodSample(TR_OpaqueMethodBlock *omb, int32_t bucket)
   {
//   printf("Searching for method %p in bucket %d\n",omb,bucket);
   TR_IPMethodHashTableEntry *entry;
   for (entry = _methodHashTable[bucket]; entry; entry = entry->_next)
      {
//      printf("entry = %p entry->_method = %p\n",entry,entry->_method);
      if (omb == entry->_method)
         return entry;
      }
   return NULL;
   }

// This method is used at compile time to search both the
// IProfiler/bytecode hash table and the shared cache.
TR_IPBytecodeHashTableEntry *
TR_IProfiler::profilingSample (TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation *comp, uintptrj_t data, bool addIt)
   {
   // Find the bytecode pc we are interested in
   uintptrj_t pc = getSearchPC (method, byteCodeIndex, comp);

   // When we just search in the hashtable we don't need to lock,
   // It should work even if someone else is modifying the hashtable bucket entry
   //

   if (!addIt) // read request
      {
      _STATS_IPEntryRead++;

      U_8 bytecode =  *(U_8 *)pc;
      // Find the pc in the IProfiler/bytecode hashtable
      TR_IPBytecodeHashTableEntry * currentEntry = findOrCreateEntry(bcHash(pc), pc, false);
      TR_IPBytecodeHashTableEntry * persistentEntry = NULL;
      TR_IPBytecodeHashTableEntry * entry = currentEntry;
      TR_IPBCDataStorageHeader *persistentEntryStore = NULL;
      uintptrj_t cacheOffset;
      static bool preferHashtableData = comp->getOption(TR_DisableAOTWarmRunThroughputImprovement);
      if (entry) // bytecode pc is present in the hashtable
         {
         // Increment the number of requests for samples
         // Exclude JBinvokestatic and JBinvokespecial because they are not tracked by interpreter
         if (bytecode != JBinvokestatic
            && bytecode != JBinvokespecial
#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
            && bytecode != JBinvokestaticsplit
            && bytecode != JBinvokespecialsplit
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
            )
            _readSampleRequestsHistory->incTotalReadSampleRequests();
         // If I prefer HT data, or, if I already searched the SCC for this pc,
         // then just return the entry I found in the hashtable
         if (preferHashtableData || entry->isPersistentEntryRead())
            {
            return entry;
            }
         }

      /*
        reserve a piece of memory on the stack, so that this entry will only be alive with in this method
        Create a dummy class with the char array so that
            1. EntryPlaceHolderClass obj will take care of the alignment on stack
            2. the char array will reserve the memory we need
      */
      class EntryPlaceHolderClass
         {
         char buffer[MAX_THREE(sizeof(TR_IPBCDataFourBytes), sizeof(TR_IPBCDataEightWords), sizeof(TR_IPBCDataCallGraph)) + sizeof(void*)];
         };
      EntryPlaceHolderClass entryBuffer;
      if (!comp->getOption(TR_DoNotUsePersistentIprofiler))
         {
         // Time to check the SCC
         bool methodProfileExistsInSCC = false;
         if (preferHashtableData)
            {
            // Desired pc is not in IProfiler hashtable, otherwise we would have returned it
            // Search the SCC for desired pc and populate an IProfiler HT entry with it
            entry = persistentProfilingSample(method, byteCodeIndex, comp, &methodProfileExistsInSCC);
            if (!entry) // Desired PC is not in SCC
               {
               // Increment the number of failed requests for samples
               // Imprecision due to concurrency is allowed because this is just a heuristic
               // We should not count the samples if the method is in SCC
               // Those samples belong to not-taken paths that couldn't have been profiled anyway
               if (bytecode != JBinvokestatic
                  && bytecode != JBinvokespecial
#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
                  && bytecode != JBinvokestaticsplit
                  && bytecode != JBinvokespecialsplit
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
                  && !methodProfileExistsInSCC)
                  {
                  _readSampleRequestsHistory->incFailedReadSampleRequests();
                  _readSampleRequestsHistory->incTotalReadSampleRequests();

                  _STATS_persistedIPReadFail++;

#ifdef PERSISTENCE_VERBOSE
                  char methodSig[500];
                  _vm->printTruncatedSignature(methodSig, 500, method);
                  fprintf(stderr, "Cannot find entry for method %s bci %u while compiling %s\n", methodSig, byteCodeIndex, comp->signature());
#endif
                  }
               }
            else // found entry in SCC
               {
               if (bytecode != JBinvokestatic
                  && bytecode != JBinvokespecial
#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
                  && bytecode != JBinvokestaticsplit
                  && bytecode != JBinvokespecialsplit
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
                  )
                  _readSampleRequestsHistory->incTotalReadSampleRequests();
#ifdef PERSISTENCE_VERBOSE
               fprintf(stderr, "Entry from SCC\n");
#endif
               if (!entry->getData())
                  {
                  _STATS_persistedIPReadHadBadData++;
                  }
               else
                  {
                  _STATS_persistedIPReadSuccess++;
                  }
               }
            } //preferHashtableData
         else
            {
            // Desired PC may already be present in the IProfiler HT,
            // but we want to check the SCC too
            unsigned char storeBuffer[1000];
            uint32_t bufferLength = 1000;
            J9SharedDataDescriptor descriptor;
            persistentEntryStore = getJ9SharedDataDescriptorForMethod(&descriptor, storeBuffer, bufferLength, method, comp);
            if (persistentEntryStore)
               {
               persistentEntryStore = persistentProfilingSample(method, byteCodeIndex, comp, &methodProfileExistsInSCC, &cacheOffset, persistentEntryStore);
               }
            if (!persistentEntryStore)
               {
               return currentEntry; // return the entry from the IProfiler hashtable
               }
            else // We have info in SCC too
               {
               // Load the data from SCC into an entry on stack
               if (isCompact(bytecode))
                  {
                  persistentEntry = new (&entryBuffer) TR_IPBCDataFourBytes(pc);
                  }
               else
                  {
                  if (isSwitch(bytecode))
                     persistentEntry = new (&entryBuffer) TR_IPBCDataEightWords(pc);
                  else
                     persistentEntry = new (&entryBuffer) TR_IPBCDataCallGraph(pc);
                  }
               persistentEntry->loadFromPersistentCopy(persistentEntryStore, comp, cacheOffset);
               }
            }
         }
      else // Entry not found in BC hash table and we cannot search SCC
         {
         // rare case used for diagnostic only
         // Do not count this as a failure (and do not count it as a success)
         }

      if (!preferHashtableData)
         {
         // If I don't have data in IProfiler HT, choose the persistent source
         if(!currentEntry || (currentEntry->getData() == NULL))
            {
            if (persistentEntry && (persistentEntry->getData()))
               {
               _STATS_IPEntryChoosePersistent++;
               currentEntry = findOrCreateEntry(bcHash(pc), pc, true);
               currentEntry->copyFromEntry(persistentEntry, comp);
               // Remember that we already looked into the SCC for this PC
               currentEntry->setPersistentEntryRead();
               return currentEntry;
               }
            }
         // If I don't have relevant data in the SCC, choose the data from IProfiler HT
         else if(!persistentEntry || (persistentEntry->getData() == NULL))
            {
            // Remember that we already looked into the SCC for this PC
            currentEntry->setPersistentEntryRead();
            return currentEntry;
            }
         // We have two sources of data. Must pick the best one
         else
            {
            // Remember that we already looked into the SCC for this PC
            currentEntry->setPersistentEntryRead();
            int32_t currentCount = getSamplingCount(currentEntry, comp);
            int32_t persistentCount = getSamplingCount(persistentEntry, comp);
            if(currentCount >= persistentCount)
               {
               _STATS_IPEntryChoosePersistent++;
               currentEntry->copyFromEntry(persistentEntry, comp);
               return currentEntry;
               }
            else
               {
               _STATS_IPEntryChoosePersistent++;
               currentEntry->copyFromEntry(persistentEntry, comp);
               return currentEntry;
               }
            }
         } //!preferHashtableData
      return entry;
      } // !addIt
   else // Request for adding the data to the hashtable
      {
      return profilingSample(pc, data, true);
      }
   }

int32_t
TR_IProfiler::getSamplingCount( TR_IPBytecodeHashTableEntry *entry, TR::Compilation *comp)
   {
   if(entry->asIPBCDataEightWords())
      return ((TR_IPBCDataEightWords *)entry)->getSumSwitchCount();
   else if (entry->asIPBCDataCallGraph())
      {
      TR_IPBCDataCallGraph *callGraphEntry = (TR_IPBCDataCallGraph *)entry;
      return callGraphEntry->getSumCount(comp, true);
      }
   else if (entry->asIPBCDataFourBytes())
      {
      TR_IPBCDataFourBytes  *branchEntry = (TR_IPBCDataFourBytes *)entry;
      return branchEntry->getSumBranchCount();
      }
   return 0;
   }

int32_t
TR_IPBCDataEightWords::getSumSwitchCount()
   {
   int32_t sum = 1;
   uint64_t *p = (uint64_t *)(getDataPointer());

   for (int8_t i=0; i<SWITCH_DATA_COUNT; i++, p++)
      {
      uint64_t segment = *p;
      uint32_t segmentData  = 0;
      uint32_t segmentCount = 0;

      getSwitchSegmentDataAndCount (segment, &segmentData, &segmentCount);
      static bool debug = feGetEnv("TR_debugiprofile") ? true: false;
      if (debug)
         {
         fprintf(stderr, "branch [%p], data [0x%4x], count [0x%4x]\n", this, segmentData, segmentCount);
         fflush(stderr);
         }
      sum += segmentCount;
      }
   return sum;
   }

// this method is used to search only the hash table
TR_IPBytecodeHashTableEntry *
TR_IProfiler::profilingSample (uintptrj_t pc, uintptrj_t data, bool addIt, bool isRIData, uint32_t freq)
   {
   TR_IPBytecodeHashTableEntry *entry = findOrCreateEntry(bcHash(pc), pc, addIt);

   if (entry && addIt)
      {
      if (invalidateEntryIfInconsistent(entry))
         return NULL;
      addSampleData(entry, data, isRIData, freq);
      }

   return entry;
   }

TR_IPBytecodeHashTableEntry *
TR_IProfiler::profilingSampleRI (uintptrj_t pc, uintptrj_t data, bool addIt, uint32_t freq)
   {
   return profilingSample(pc, data, addIt, true, freq);
   }

TR_IPBCDataAllocation *
TR_IProfiler::profilingAllocSample (uintptrj_t pc, uintptrj_t data, bool addIt)
   {
#if defined(EXPERIMENTAL_IPROFILER)
   TR_IPBCDataAllocation *entry = NULL;

   if (!_allocHashTable)
      return NULL;

   int32_t bucket = allocHash(pc);

   entry = findOrCreateAllocEntry(bucket, pc, addIt);

   return entry;
#else
   return NULL;
#endif
   }

TR_ExternalProfiler *
TR_IProfiler::canProduceBlockFrequencyInfo(TR::Compilation& comp)
   {
   // if we enabled profiling we should be
   // able to produce block frequency info
   if (isIProfilingEnabled())
      {
      intptrj_t initialCount = comp.getMethodSymbol()->getResolvedMethod()->hasBackwardBranches() ?
         comp.getOptions()->getInitialBCount() :
         comp.getOptions()->getInitialCount() ;

      // We should be smart here and do something like:  the size of
      // the method AND the number of times it's been interpreted
      // determines whether or not we think the block frequency is
      // meaningful.  For now, we simply say that if the interpreter
      // has seen the method 5 or fewer times, we don't trust the
      // profile info
      if (initialCount > 5 && !comp.hasIntStreamForEach())
         return this;
      }

   return NULL;
   }

static TR_OpaqueMethodBlock *
getMethodFromBCInfo(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp)
   {
   TR_OpaqueMethodBlock *method = NULL;

   if (comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
      {
      if (bcInfo.getCallerIndex() >= 0)
         method = (TR_OpaqueMethodBlock *)(((TR_AOTMethodInfo *)comp->getInlinedCallSite(bcInfo.getCallerIndex())._vmMethodInfo)->resolvedMethod->getNonPersistentIdentifier());
      else
         method = (TR_OpaqueMethodBlock *)(comp->getCurrentMethod()->getNonPersistentIdentifier());
      }
   else
      {
      if (bcInfo.getCallerIndex() >= 0)
         method = (TR_OpaqueMethodBlock *)(comp->getInlinedCallSite(bcInfo.getCallerIndex())._vmMethodInfo);
      else
         method = (TR_OpaqueMethodBlock *)(comp->getCurrentMethod()->getPersistentIdentifier());
      }

   return method;
   }

uint8_t
TR_IProfiler::getBytecodeOpCode(TR::Node *node, TR::Compilation *comp)
   {
   TR_OpaqueMethodBlock *method = NULL;
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();

   if (node->getInlinedSiteIndex() < -1)
      method = node->getMethod();
   else
      method = getMethodFromBCInfo(bcInfo, comp);

   int32_t methodSize = TR::Compiler->mtd.bytecodeSize(method);
   uintptrj_t methodStart = TR::Compiler->mtd.bytecodeStart(method);

   TR_ASSERT(bcInfo.getByteCodeIndex() < methodSize, "Bytecode index can't be higher than the methodSize");

   return *((uint8_t *)methodStart + bcInfo.getByteCodeIndex());
   }

static TR::ILOpCodes opCodeForBranchFromBytecode (uint8_t byteCodeOpCode)
   {
   // the compare is always on compare and branch
   switch (byteCodeOpCode)
      {
      case JBifacmpeq:
      case JBifnull:
         return TR::ifacmpeq;
      case JBifnonnull:
      case JBifacmpne:
         return TR::ifacmpne;
      case JBifeq:
      case JBificmpeq:
         return TR::ificmpeq;
      case JBifge:
      case JBificmpge:
         return TR::ificmpge;
      case JBifgt:
      case JBificmpgt:
         return TR::ificmpgt;
      case JBifle:
      case JBificmple:
         return TR::ificmple;
      case JBiflt:
      case JBificmplt:
         return TR::ificmplt;
      case JBifne:
      case JBificmpne:
         return TR::ificmpne;
      default:
         return TR::BadILOp;
      }
   }




 bool
 TR_IProfiler::branchHasSameDirection(TR::ILOpCodes nodeOpCode, TR::Node *node, TR::Compilation *comp)
    {
    TR::ILOpCodes byteCodeOpCode = opCodeForBranchFromBytecode(getBytecodeOpCode(node, comp));

    if (byteCodeOpCode!=TR::BadILOp)
       {
       if (TR::ILOpCode::isStrictlyLessThanCmp(byteCodeOpCode) &&
           ((!node->childrenWereSwapped() &&
            TR::ILOpCode::isStrictlyLessThanCmp(nodeOpCode)) ||
           (node->childrenWereSwapped() &&
            TR::ILOpCode::isStrictlyGreaterThanCmp(nodeOpCode))))
          return true;


       if (TR::ILOpCode::isStrictlyGreaterThanCmp(byteCodeOpCode) &&
          ((!node->childrenWereSwapped() &&
            TR::ILOpCode::isStrictlyGreaterThanCmp(nodeOpCode)) ||
           (node->childrenWereSwapped() &&
            TR::ILOpCode::isStrictlyLessThanCmp(nodeOpCode))))
          return true;

       if (TR::ILOpCode::isLessCmp(byteCodeOpCode) &&
          ((!node->childrenWereSwapped() &&
            TR::ILOpCode::isLessCmp(nodeOpCode)) ||
           (node->childrenWereSwapped() &&
            TR::ILOpCode::isGreaterCmp(nodeOpCode))))
          return true;

       if (TR::ILOpCode::isGreaterCmp(byteCodeOpCode) &&
          ((!node->childrenWereSwapped() &&
            TR::ILOpCode::isGreaterCmp(nodeOpCode)) ||
           (node->childrenWereSwapped() &&
            TR::ILOpCode::isLessCmp(nodeOpCode))))
          return true;

       if (TR::ILOpCode::isEqualCmp(byteCodeOpCode) &&
           TR::ILOpCode::isEqualCmp(nodeOpCode))
          return true;

       if (TR::ILOpCode::isNotEqualCmp(byteCodeOpCode) &&     
           TR::ILOpCode::isNotEqualCmp(nodeOpCode))
          return true;
       }

    return false;
    }


bool
TR_IProfiler::branchHasOppositeDirection(TR::ILOpCodes nodeOpCode, TR::Node *node, TR::Compilation *comp)
   {
   TR::ILOpCodes byteCodeOpCode = opCodeForBranchFromBytecode(getBytecodeOpCode(node, comp));
   if (byteCodeOpCode!=TR::BadILOp)
      {
      if (TR::ILOpCode::isStrictlyLessThanCmp(byteCodeOpCode) &&
          (TR::ILOpCode::isGreaterCmp(nodeOpCode) ||
           TR::ILOpCode::isLessCmp(nodeOpCode)))
         return true;

      if (TR::ILOpCode::isStrictlyGreaterThanCmp(byteCodeOpCode) &&
          (TR::ILOpCode::isGreaterCmp(nodeOpCode) ||
           TR::ILOpCode::isLessCmp(nodeOpCode)))
         return true;

      if (TR::ILOpCode::isLessCmp(byteCodeOpCode) &&
          (TR::ILOpCode::isStrictlyLessThanCmp(nodeOpCode) ||
           TR::ILOpCode::isStrictlyGreaterThanCmp(nodeOpCode)))
         return true;

      if (TR::ILOpCode::isGreaterCmp(byteCodeOpCode) &&
          (TR::ILOpCode::isStrictlyLessThanCmp(nodeOpCode) ||
           TR::ILOpCode::isStrictlyGreaterThanCmp(nodeOpCode)))
         return true;

      if (TR::ILOpCode::isEqualCmp(byteCodeOpCode) &&
          TR::ILOpCode::isNotEqualCmp(nodeOpCode))
         return true;

      if (TR::ILOpCode::isNotEqualCmp(byteCodeOpCode) &&
          TR::ILOpCode::isEqualCmp(nodeOpCode))
         return true;
      }
   return false;
   }

uintptrj_t
TR_IProfiler::getProfilingData(TR::Node *node, TR::Compilation *comp)
   {
   if (!isIProfilingEnabled())
      return 0;

   uintptrj_t data = 0;
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   data = getProfilingData(getMethodFromNode(node, comp), bcInfo.getByteCodeIndex(), comp);

   if (data == (uintptrj_t)1)
      return (uintptrj_t)0;

   return data;
   }

uintptrj_t
TR_IProfiler::getProfilingData(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp)
   {
   if (!isIProfilingEnabled())
      return 0;

   uintptrj_t data = getProfilingData(getMethodFromBCInfo(bcInfo, comp), bcInfo.getByteCodeIndex(), comp);

   if (data == (uintptrj_t)1)
      return (uintptrj_t)0;

   return data;
   }

TR_OpaqueMethodBlock *
TR_IProfiler::getMethodFromNode(TR::Node *node, TR::Compilation *comp)
   {
   if (!isIProfilingEnabled())
      return 0;

   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   if (bcInfo.getCallerIndex() >=-1)
      return getMethodFromBCInfo(bcInfo, comp);
   else
      return node->getMethod();
   }

uintptrj_t
getSearchPCFromMethodAndBCIndex(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR_FrontEnd *vm, TR::Compilation * comp)

   {
   uint32_t methodSize = TR::Compiler->mtd.bytecodeSize(method);
   uintptrj_t methodStart = TR::Compiler->mtd.bytecodeStart(method);
   uintptrj_t searchedPC = (uintptrj_t)(methodStart + byteCodeIndex);

   if (byteCodeIndex >= methodSize)
      {

      if (comp->getOutFile())
         {

         TR_Stack<int32_t> & stack = comp->getInlinedCallStack();
         int len = comp->getInlinedCallStack().size();
         traceMsg(comp, "CSI : INLINER STACK :\n");
         for (int i = 0; i < len; i++)
            {
            TR_IPHashedCallSite hcs;
            TR_InlinedCallSite& callsite = comp->getInlinedCallSite (stack[len - i - 1]);
            hcs._method = (J9Method*) callsite._methodInfo;
            hcs._offset = callsite._byteCodeInfo.getByteCodeIndex();
            printHashedCallSite(&hcs, comp->getOutFile()->_stream, comp);
            }
         comp->dumpMethodTrees("CSI Trees : byteCodeIndex < methodSize");
         }
      }

   TR_ASSERT(byteCodeIndex < methodSize, "Bytecode index can't be higher than the methodSize");


   // interfaces are weird
   // [   1],     1, JBinvokeinterface2      <- we get sample for this index
   // [   2],     2, JBnop
   // [   3],     3, JBinvokeinterface         21 <- we ask on this one

   static bool traceIProfiling = ((debug("traceIProfiling") != NULL) );
   if (isInterfaceBytecode(*(U_8*)searchedPC) &&
       byteCodeIndex >= 2 &&
       isInterface2Bytecode(*(U_8 *)(searchedPC - 2)))
      {
      searchedPC -= 2;
      if (traceIProfiling)
         traceMsg(comp, "Adjusted PC=%p by 2 because it's interfaceinvoke\n", searchedPC);
      }

   return searchedPC;
   }



uintptrj_t
getSearchPCFromMethodAndBCIndex(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation * comp)
   {
   return getSearchPCFromMethodAndBCIndex(method, byteCodeIndex, comp->fej9(), comp);
   }

uintptrj_t
TR_IProfiler::getSearchPC(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation * comp)
   {
   return getSearchPCFromMethodAndBCIndex(method, byteCodeIndex, _vm, comp);
   }


uint32_t *
TR_IProfiler::getGlobalAllocationDataPointer(bool isAOT)
   {
   if (!isIProfilingEnabled())
      return NULL;

   return &_globalAllocationCount;
   }

bool
TR_IProfiler::isNewOpCode(U_8 byteCode)
   {
   return (byteCode == JBnew) || (byteCode == JBnewarray) || (byteCode == JBanewarray);
   }

uint32_t *
TR_IProfiler::getAllocationProfilingDataPointer(TR_ByteCodeInfo &bcInfo, TR_OpaqueClassBlock *clazz, TR_OpaqueMethodBlock *method, TR::Compilation *comp)
   {
   if (!isIProfilingEnabled())
      return NULL;

   uintptrj_t searchedPC = getSearchPC (getMethodFromBCInfo(bcInfo, comp), bcInfo.getByteCodeIndex(), comp);

   //if (searchedPC == (uintptrj_t)IPROFILING_PC_INVALID)
   //   return NULL;

   TR_IPBCDataAllocation *entry = profilingAllocSample (searchedPC, 0, true);

   if (!entry || entry->isInvalid())
      return NULL;

   TR_ASSERT(entry->asIPBCDataAllocation(), "We should get allocation datapointer");

   ((TR_IPBCDataAllocation *)entry)->setClass((uintptrj_t)clazz);
   ((TR_IPBCDataAllocation *)entry)->setMethod((uintptrj_t)method);

   return (uint32_t *)(entry->getDataReference());
   }

TR_IPBytecodeHashTableEntry *
TR_IProfiler::getProfilingEntry(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation * comp)
   {
   TR_IPBytecodeHashTableEntry *entry = profilingSample (method, byteCodeIndex, comp);

   static bool traceIProfiling = ((debug("traceIProfiling") != NULL));

   if (traceIProfiling)
      traceMsg(comp, "Asked for profiling data on PC=%p, ", getSearchPC (method, byteCodeIndex, comp));

   if (entry)
      {
      if (invalidateEntryIfInconsistent(entry))
         {
         if (traceIProfiling)
            traceMsg(comp, "got nothing because it was invalidated\n");
         return 0;
         }

      return entry;
      }

   if (traceIProfiling)
      traceMsg(comp, "got nothing\n");

   return 0;
   }

uintptrj_t
TR_IProfiler::getProfilingData(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation * comp)
   {
   TR_IPBytecodeHashTableEntry *entry = getProfilingEntry(method, byteCodeIndex, comp);

   static bool traceIProfiling = ((debug("traceIProfiling") != NULL));

   if (entry)
      {
      if (traceIProfiling && !entry->asIPBCDataEightWords())
         traceMsg(comp, "got value %p\n", entry->getData());

      return entry->getData();
      }

   uintptrj_t searchedPC = getSearchPC (method, byteCodeIndex, comp);
   return 0;
   }

int32_t
TR_IProfiler::getMaxCount(bool isAOT)
   {
   if (!isIProfilingEnabled())
      return 0;

   return _maxCount;
   }

int32_t
TR_IProfiler::getSwitchCountForValue (TR::Node *node, int32_t value, TR::Compilation *comp)
   {
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   if (bcInfo.doNotProfile())
      return 0;
   TR_IPBytecodeHashTableEntry *entry = getProfilingEntry(getMethodFromNode(node, comp), bcInfo.getByteCodeIndex(), comp);

   if (entry && entry->asIPBCDataEightWords())
      {
      uintptrj_t searchedPC = getSearchPC (getMethodFromNode(node, comp), bcInfo.getByteCodeIndex(), comp);
      int32_t data;

      if (node->getOpCodeValue() == TR::lookup)
         data = lookupSwitchBytecodeToOffset (searchedPC, value);
      else
         data = tableSwitchBytecodeToOffset (searchedPC, value);

      return getOrSetSwitchData((TR_IPBCDataEightWords *)entry, (uint32_t)data, false, (node->getOpCodeValue() == TR::lookup));
      }
   return 0;
   }

int32_t
TR_IProfiler::getSumSwitchCount (TR::Node *node, TR::Compilation *comp)
   {
   int32_t sum = 1;
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   if (bcInfo.doNotProfile())
      return sum;
   TR_IPBytecodeHashTableEntry *entry = getProfilingEntry(getMethodFromNode(node, comp), bcInfo.getByteCodeIndex(), comp);

   if (entry && entry->asIPBCDataEightWords())
      {
      uint64_t *p = (uint64_t *)(((TR_IPBCDataEightWords *)entry)->getDataPointer());

      for (int8_t i=0; i<SWITCH_DATA_COUNT; i++, p++)
         {
         uint64_t segment = *p;
         uint32_t segmentData  = 0;
         uint32_t segmentCount = 0;

         getSwitchSegmentDataAndCount (segment, &segmentData, &segmentCount);
         sum += segmentCount;
         }

      }
   return sum;
   }

int32_t
TR_IProfiler::getFlatSwitchProfileCounts (TR::Node *node, TR::Compilation *comp)
   {
   int32_t count = (getSumSwitchCount(node, comp)) / SWITCH_DATA_COUNT;

   return (count>0) ? count : 1;
   }

bool
TR_IProfiler::isSwitchProfileFlat (TR::Node *node, TR::Compilation *comp)
   {
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   if (bcInfo.doNotProfile())
      return true;
   TR_IPBytecodeHashTableEntry *entry = getProfilingEntry(getMethodFromNode(node, comp), bcInfo.getByteCodeIndex(), comp);
   uint32_t max = 0;
   if (entry && entry->asIPBCDataEightWords())
      {
      uint32_t otherData  = 0;
      uint32_t otherCount = 0;

      uint64_t *p = (uint64_t *)(((TR_IPBCDataEightWords *)entry)->getDataPointer());
      getSwitchSegmentDataAndCount (*(p+(SWITCH_DATA_COUNT-1)), &otherData, &otherCount);

      for (int8_t i=0; i<SWITCH_DATA_COUNT-1; i++, p++)
         {
         uint64_t segment = *p;
         uint32_t segmentData  = 0;
         uint32_t segmentCount = 0;

         getSwitchSegmentDataAndCount (segment, &segmentData, &segmentCount);
         if (max < segmentCount) max = segmentCount;
         }

      return (max < otherCount);
      }

   return true;
   }

void matchCallStack(TR::Node *node, TR::Node *dest, int32_t *callIndex, int32_t *bcIndex, TR::Compilation *comp)
   {
   int32_t destCallIndex = dest->getInlinedSiteIndex();
   int32_t destBCIndex = dest->getByteCodeIndex();
   while (!(destCallIndex == node->getInlinedSiteIndex()) && destCallIndex>=0)
      {
      TR_InlinedCallSite & site = comp->getInlinedCallSite(dest->getInlinedSiteIndex());
      destCallIndex = site._byteCodeInfo.getCallerIndex();
      destBCIndex = site._byteCodeInfo.getByteCodeIndex();
      }

   *callIndex = destCallIndex;
   *bcIndex = destBCIndex;
   }

int16_t next2BytesSigned(uintptrj_t pc)       { return *(int16_t *)(pc); }

void
TR_IProfiler::getBranchCounters (TR::Node *node, TR::TreeTop *fallThroughTree, int32_t *taken, int32_t *notTaken, TR::Compilation *comp)
   {
   static bool traceIProfiling = ((debug("traceIProfiling") != NULL));
   uintptrj_t data = getProfilingData (node, comp);

   if (data)
      {
      uint16_t fallThroughCount = (uint16_t)(data & 0x0000FFFF) | 0x1;
      uint16_t branchToCount = (uint16_t)((data & 0xFFFF0000)>>16) | 0x1;
      TR::ILOpCodes nodeOpCode = node->getOpCode().convertCmpToIfCmp();

      if (nodeOpCode == TR::BadILOp) nodeOpCode = node->getOpCodeValue();

      if (branchHasSameDirection(nodeOpCode, node, comp))
         {
         *taken = (int32_t) branchToCount;
         *notTaken = (int32_t) fallThroughCount;
         }
      else if (branchHasOppositeDirection(nodeOpCode, node, comp))
         {
         *notTaken = (int32_t) branchToCount;
         *taken = (int32_t) fallThroughCount;
         }
      else
         {
         // branch was changed in non-consistent way
         // better not set anything
         if (traceIProfiling)
            traceMsg(comp, "I couldn't figure out the branch direction after change for node [%p], so I gave default direction \n", node);
         *taken = (int32_t) branchToCount;
         *notTaken = (int32_t) fallThroughCount;
         }

      if (0 && node && node->getOpCode().isBranch())
         {
         TR::TreeTop *branchTo = node->getBranchDestination();
         TR::TreeTop *fallThrough = fallThroughTree;
         bool matched = false;
         uintptrj_t byteCodePtr = getSearchPC (getMethodFromNode(node, comp), node->getByteCodeIndex(), comp);

         int32_t branchBC = node->getByteCodeIndex() + next2BytesSigned(byteCodePtr+1);
         int32_t fallThruBC = node->getByteCodeIndex() + 3;
         int32_t destCallIndex = 0;
         int32_t destBCIndex = 0;

         if (branchTo)
            {
            TR::Node *dest = branchTo->getNode();

            if (dest)
               {
               matchCallStack(node, dest, &destCallIndex, &destBCIndex, comp);

               if (destCallIndex == node->getInlinedSiteIndex())
                  {
                  if (destBCIndex == branchBC)
                     {
                     if (0 && *taken != branchToCount)
                        {
                        printf("Matched the node by destination\n");
                        printf("Previously determined counts: taken = %d, not taken = %d\n", *taken, *notTaken);
                        printf("New counts: taken = %d, not taken = %d\n", branchToCount, fallThroughCount);
                        }
                     *taken = (int32_t) branchToCount;
                     *notTaken = (int32_t) fallThroughCount;
                     matched = true;
                     }
                  else if (destBCIndex == fallThruBC)
                     {
                     if (0 && *notTaken != branchToCount)
                        {
                        printf("Matched the node by destination\n");
                        printf("Previously determined counts: taken = %d, not taken = %d\n", *notTaken, *taken);
                        printf("New counts: taken = %d, not taken = %d\n", branchToCount, fallThroughCount);
                        }
                     *notTaken = (int32_t) branchToCount;
                     *taken = (int32_t) fallThroughCount;
                     matched = true;
                     }
                  }
               }
            }

         if (!matched && fallThrough)
            {
            TR::Node *fallT = fallThrough->getNode();

            if (fallT)
               {
               matchCallStack(node, fallT, &destCallIndex, &destBCIndex, comp);

               if (destCallIndex == node->getInlinedSiteIndex())
                  {
                  if (destBCIndex == branchBC)
                     {
                     if (0 && *notTaken != branchToCount)
                        {
                        printf("Matched the node by fall through\n");
                        printf("Previously determined counts: taken = %d, not taken = %d\n", *notTaken, *taken);
                        printf("New counts: taken = %d, not taken = %d\n", branchToCount, fallThroughCount);
                        }
                     *notTaken = (int32_t) branchToCount;
                     *taken = (int32_t) fallThroughCount;
                     matched = true;
                     }
                  else if (destBCIndex == fallThruBC)
                     {
                     if (0 && *taken != branchToCount)
                        {
                        printf("Matched the node by fall through\n");
                        printf("Previously determined counts: taken = %d, not taken = %d\n", *taken, *notTaken);
                        printf("New counts: taken = %d, not taken = %d\n", branchToCount, fallThroughCount);
                        }
                     *taken = (int32_t) branchToCount;
                     *notTaken = (int32_t) fallThroughCount;
                     matched = true;
                     }
                  }
               }
            }
         }
      }
   else
      {
      *taken = 0;
      *notTaken = 0;
      }
   }

void
TR_IProfiler::setBlockAndEdgeFrequencies(TR::CFG *cfg, TR::Compilation *comp)
   {
   if (!isIProfilingEnabled())
      return;

   cfg->propagateFrequencyInfoFromExternalProfiler(this);
   _maxCount = cfg->getMaxFrequency();

   static bool traceIProfiling = ((debug("traceIProfiling") != NULL));
   if (traceIProfiling)
      {
      traceMsg(comp, "\nBlock frequency info set by Interpreter profiling\n");

      for (TR::TreeTop *treeTop = comp->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
         {
         TR::Node *node = treeTop->getNode();
         if (node->getOpCodeValue() == TR::BBStart)
            {
            traceMsg(comp, "\nBlock[%d] frequency = %d\n", node->getBlock()->getNumber(), node->getBlock()->getFrequency());
            }
         }
      }
   }

void
TR_IProfiler::resetProfiler()
   {
   _maxCount = DEFAULT_PROFILING_COUNT;
   }

J9Class *
TR_IProfiler::getInterfaceClass(J9Method *aMethod, TR::Compilation *comp)
   {
   J9UTF8 * className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD((aMethod))->romClass);
   int32_t len = J9UTF8_LENGTH(className);
   char * s = classNameToSignature(utf8Data(className), len, comp);
   return (J9Class *)(comp->fej9()->getClassFromSignature(s, len, (TR_OpaqueMethodBlock *)aMethod));
   }

TR_AbstractInfo *
TR_IProfiler::createIProfilingValueInfo (TR_ByteCodeInfo &bcInfo, TR::Compilation *comp)
   {
   if (!isIProfilingEnabled())
      return NULL;

   static bool traceIProfiling = ((debug("traceIProfiling") != NULL));

   TR_OpaqueMethodBlock *method = getMethodFromBCInfo(bcInfo, comp);
   TR_ValueProfileInfo *valueProfileInfo = TR_MethodValueProfileInfo::getValueProfileInfo(method, comp);

   if (!valueProfileInfo)
      {
//#ifdef VERBOSE_INLINING_PROFILING
      TR_IProfiler::_STATS_doesNotWantToGiveProfilingInfo ++;
//#endif
      return NULL;
      }

   if (traceIProfiling)
      {
      traceMsg(comp, "\nQuerying for bcIndex=%d, callerIndex=%d\n", bcInfo.getByteCodeIndex(), bcInfo.getCallerIndex());
      }

   if (_allowedToGiveInlinedInformation &&
        bcInfo.getCallerIndex()>=0/*          &&
       !bcInfo.doNotProfile()*/)
      {
      // if the target method is still interpreted we are likely to have
      // profiling information coming from this caller
      if (comp->getOption(TR_IProfilerPerformTimestampCheck) && method && !_compInfo->isCompiled((J9Method *)method))// method is interpreted
         {
         TR_PersistentClassInfo *currentPersistentClassInfo = _compInfo->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(comp->getCurrentMethod()->containingClass(), comp);
         TR_PersistentClassInfo *calleePersistentClassInfo = _compInfo->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking((TR_OpaqueClassBlock *)J9_CLASS_FROM_METHOD(((J9Method *)method)), comp);

         if (!currentPersistentClassInfo || !calleePersistentClassInfo)
            {
            if (traceIProfiling)
               {
               traceMsg(comp, "\nMissing persistent class or method info returning NULL\n");
               }
//#ifdef VERBOSE_INLINING_PROFILING
            TR_IProfiler::_STATS_cannotGetClassInfo  ++;
//#endif
            return NULL;
            }

         if ((currentPersistentClassInfo->getTimeStamp() == (uint16_t)0x0FFFF) ||
             (calleePersistentClassInfo->getTimeStamp() == (uint16_t)0x0FFFF))
            {
            if (traceIProfiling)
               {
               traceMsg(comp, "\nThe time stamp for callee or caller class has expired, I refuse to give profiling information back\n");
               }
//#ifdef VERBOSE_INLINING_PROFILING
            TR_IProfiler::_STATS_timestampHasExpired  ++;
//#endif
            return NULL;
            }

         if ((currentPersistentClassInfo->getTimeStamp() > calleePersistentClassInfo->getTimeStamp()) &&
             ((int32_t)(currentPersistentClassInfo->getTimeStamp() - calleePersistentClassInfo->getTimeStamp()) > _classLoadTimeStampGap))
            {
            if (traceIProfiling)
               {
               traceMsg(comp, "\nCallee method %s (callerIndex=%d) is interpreted but class time stamps are too far apart, I refuse to give profiling info for this callee method (ownerClass time stamp %d, callee class time stamp %d).\n",
                   _vm->sampleSignature(method, 0, 0, comp->trMemory()), bcInfo.getCallerIndex(), currentPersistentClassInfo->getTimeStamp(), calleePersistentClassInfo->getTimeStamp());
               }
//#ifdef VERBOSE_INLINING_PROFILING
            TR_IProfiler::_STATS_timestampHasExpired ++;
//#endif
            return NULL;
            }
         if (traceIProfiling)
            {
            traceMsg(comp, "\nCallee method %s (callerIndex=%d) is interpreted I'll give profiling information for it, ownerClass time stamp %d, callee class time stamp %d.\n",
               _vm->sampleSignature(method, 0, 0, comp->trMemory()), bcInfo.getCallerIndex(), currentPersistentClassInfo->getTimeStamp(), calleePersistentClassInfo->getTimeStamp());
            }
         }
      }
   else if (bcInfo.getCallerIndex()>=0)
      {
//#ifdef VERBOSE_INLINING_PROFILING
      TR_IProfiler::_STATS_doesNotWantToGiveProfilingInfo ++;
//#endif
      return NULL;
      }


   if (!bcInfo.doNotProfile())
      {
      uintptrj_t data = 0;
      uintptrj_t thisPC = getSearchPC (method, bcInfo.getByteCodeIndex(), comp);
      U_8 thisByteCode = *(U_8*)thisPC;
      uint32_t weight = 0;
      TR_AbstractInfo *valueInfo;
      if (!isNewOpCode(thisByteCode))
         {
         // build the call-graph using profiling info
         // FIXME: make multi-thread safe
         TR_IPBCDataCallGraph * cgData = getCGProfilingData(bcInfo, comp);
         CallSiteProfileInfo *csInfo = NULL;

         if (cgData)
            csInfo = cgData->getCGData();

         if (csInfo)
            {
            data = csInfo->getClazz(0);
            if (!data)
               {
               if (traceIProfiling)
                  {
                  traceMsg(comp, "Call-graph 1 No profiling data for bcIndex=%d, callerIndex=%d\n", bcInfo.getByteCodeIndex(), bcInfo.getCallerIndex());
                  }
               return NULL;
               }
            weight = cgData->getEdgeWeight((TR_OpaqueClassBlock *)data, comp);
            valueInfo = valueProfileInfo->createAndInitializeValueInfo(bcInfo, TR::Address, false, comp, heapAlloc, data, weight, true);
            uintptrj_t *addrOfTotalFrequency;
            uintptrj_t totalFrequency = ((TR_AddressInfo *)valueInfo)->getTotalFrequency(&addrOfTotalFrequency);

            for (int32_t i = 1; i < NUM_CS_SLOTS; i++)
               {
               data = csInfo->getClazz(i);
               if (data)
                  {
                  weight = cgData->getEdgeWeight((TR_OpaqueClassBlock *)data, comp);
                  ((TR_AddressInfo *)valueInfo)->incrementOrCreateExtraAddressInfo(data, &addrOfTotalFrequency, i, weight, true);
                  }
               }
            // add resudial to last total frequency
            *addrOfTotalFrequency = (*addrOfTotalFrequency) + csInfo->_residueWeight;
            }
         else
            {
            if (traceIProfiling)
               {
               traceMsg(comp, "Call-graph 2 Set not to profile bcIndex=%d, callerIndex=%d\n", bcInfo.getByteCodeIndex(), bcInfo.getCallerIndex());
               }
            return NULL;
            }
         }
      else
         {
         data = getProfilingData(bcInfo, comp);
         if (!data)
            {
            if (traceIProfiling)
               {
               traceMsg(comp, "No profiling data for bcIndex=%d, callerIndex=%d\n", bcInfo.getByteCodeIndex(), bcInfo.getCallerIndex());
               }
            return NULL;
            }
         valueInfo = valueProfileInfo->createAndInitializeValueInfo(bcInfo, TR::Address, false, comp, heapAlloc, data, weight, true);
         }

      if (valueInfo && traceIProfiling)
         {
         traceMsg(comp, "\nAdded new value info for bcIndex=%d, callerIndex=%d\n", bcInfo.getByteCodeIndex(), bcInfo.getCallerIndex());
         }

      return valueInfo;
      }

   return NULL;
   }

bool
TR_IProfiler::hasSameBytecodeInfo(TR_ByteCodeInfo &persistentByteCodeInfo, TR_ByteCodeInfo &currentByteCodeInfo, TR::Compilation *comp)
   {
   if (persistentByteCodeInfo.getByteCodeIndex() != currentByteCodeInfo.getByteCodeIndex())
      return false;

   if (currentByteCodeInfo.getCallerIndex() != persistentByteCodeInfo.getCallerIndex())
      return false;

   return true;
   }

TR_AbstractInfo *
TR_IProfiler::createIProfilingValueInfo (TR::Node *node, TR::Compilation *comp)
   {

   static bool traceIProfiling = ((debug("traceIProfiling") != NULL));
   if (traceIProfiling)
      {
      traceMsg(comp, "\nCreating iprofiling value info for node %p\n", node);
      }

   if (node &&
      ((node->getOpCode().isCall() && !node->isTheVirtualCallNodeForAGuardedInlinedCall()) ||
      (node->getOpCodeValue() == TR::checkcast || node->getOpCodeValue() == TR::instanceof)))
      {
      return createIProfilingValueInfo(node->getByteCodeInfo(), comp);
      }
   return NULL;
   }

TR_ValueProfileInfo *
TR_IProfiler::getValueProfileInfo(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp)
   {
   if (!isIProfilingEnabled())
      return NULL;

   static bool traceIProfiling = ((debug("traceIProfiling") != NULL));

   if (traceIProfiling)
      {
      traceMsg(comp, "\nAsking for value info for bcIndex=%d, callerIndex=%d\n", bcInfo.getByteCodeIndex(), bcInfo.getCallerIndex());
      }

   //TR_OpaqueMethodBlock *originatorMethod = (TR_OpaqueMethodBlock *)(comp->getCurrentMethod()->getPersistentIdentifier());
   TR_OpaqueMethodBlock *originatorMethod = getMethodFromBCInfo(bcInfo, comp);

   if (traceIProfiling)
      {
      traceMsg(comp, "\nCurrent compiling method %p\n", originatorMethod);
      }
   TR_ValueProfileInfo  *valueProfileInfo =
      TR_MethodValueProfileInfo::getValueProfileInfo( originatorMethod, comp);

   if (!valueProfileInfo)
      {
      valueProfileInfo = new (comp->trHeapMemory()) TR_ValueProfileInfo;
      //_valueProfileMethod = (TR_OpaqueMethodBlock *)(comp->getCurrentMethod()->getPersistentIdentifier());
      valueProfileInfo->setExternalProfiler(this);
      TR_MethodValueProfileInfo::addValueProfileInfo(originatorMethod, valueProfileInfo, comp);

      for (TR::TreeTop * tt = comp->getStartTree(); tt; tt = tt->getNextTreeTop())
         {
         TR::Node * node = tt->getNode();
         TR::Node * firstChild = node->getNumChildren() > 0 ? node->getFirstChild() : 0;
         TR::Node * secondChild = node->getNumChildren() > 1 ? node->getSecondChild() : 0;
         bool created = false;
         if (node->getByteCodeInfo().getCallerIndex() == bcInfo.getCallerIndex())
            created = createIProfilingValueInfo (node, comp) != 0;
         if (!created && firstChild && firstChild->getByteCodeInfo().getCallerIndex() == bcInfo.getCallerIndex())
            created = createIProfilingValueInfo (firstChild, comp) != 0;
         if (!created && secondChild && secondChild->getByteCodeInfo().getCallerIndex() == bcInfo.getCallerIndex())
            created = createIProfilingValueInfo (secondChild, comp) != 0;
         }
      }
   return valueProfileInfo;
   }

extern "C" {
uint8_t platformLightweightLockingIsSupported();
uint32_t platformTryLock(uint32_t *ptr);
void platformUnlock(uint32_t *ptr);
}

bool
TR_IProfiler::acquireHashTableWriteLock(bool forceFullLock)
   {
   return true;
   }

void
TR_IProfiler::releaseHashTableWriteLock()
   {
   return;
   }

void
TR_IProfiler::outputStats()
   {
   TR::Options *options = TR::Options::getCmdLineOptions();
   if (options && !options->getOption(TR_DisableIProfilerThread))
      {
      fprintf(stderr, "IProfiler: Number of buffers to be processed           =%llu\n", _numRequests);
      fprintf(stderr, "IProfiler: Number of buffers discarded                 =%llu\n", _numRequestsSkipped);
      fprintf(stderr, "IProfiler: Number of buffers handed to iprofiler thread=%llu\n", _numRequestsHandedToIProfilerThread);
      }
   fprintf(stderr, "IProfiler: Number of records processed=%llu\n", _iprofilerNumRecords);
   fprintf(stderr, "IProfiler: Number of hashtable entries=%u\n", countEntries());
   checkMethodHashTable();
   }

void *
TR_IPBytecodeHashTableEntry::alignedPersistentAlloc(size_t size)
   {
#if defined(TR_HOST_64BIT)
   size += 4;
   memoryConsumed += (int32_t)size;
   void *address = (void *) jitPersistentAlloc(size);

   return (void *)(((uintptrj_t)address + 4) & ~0x7);
#else
   memoryConsumed += (int32_t)size;
   return jitPersistentAlloc(size);
#endif
   }



void *
TR_IPBCDataCallGraph::operator new (size_t size) throw()
   {
   return TR_IPBytecodeHashTableEntry::alignedPersistentAlloc(size);
   }

void *
TR_IPBCDataFourBytes::operator new (size_t size) throw()
   {
   return TR_IPBytecodeHashTableEntry::alignedPersistentAlloc(size);
   }

void
TR_IPBCDataFourBytes::createPersistentCopy(uintptrj_t cacheStartAddress, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info)
   {
   TR_IPBCDataFourBytesStorage * store = (TR_IPBCDataFourBytesStorage *) storage;
   storage->pc = _pc - cacheStartAddress;
   storage->left = 0;
   storage->right = 0;
   storage->ID = TR_IPBCD_FOUR_BYTES;
   store->data = data;
   }

void
TR_IPBCDataFourBytes::loadFromPersistentCopy(TR_IPBCDataStorageHeader * storage, TR::Compilation *comp, uintptrj_t cacheStartAddress)
   {
   TR_IPBCDataFourBytesStorage * store = (TR_IPBCDataFourBytesStorage *) storage;
   TR_ASSERT(storage->ID == TR_IPBCD_FOUR_BYTES, "Incompatible types between storage and loading of iprofile persistent data");
   data = store->data;
   }

void
TR_IPBCDataFourBytes::copyFromEntry(TR_IPBytecodeHashTableEntry* originalEntry, TR::Compilation *comp)
   {
   TR_IPBCDataFourBytes *entry = (TR_IPBCDataFourBytes *) originalEntry;
   TR_ASSERT(originalEntry->asIPBCDataFourBytes(), "Incompatible types between storage and loading of iprofile persistent data");
   data = entry->data;
   }

int16_t
TR_IPBCDataFourBytes::getSumBranchCount()
   {
   uint16_t fallThroughCount = (uint16_t)(data & 0x0000FFFF) | 0x1;
   uint16_t branchToCount = (uint16_t)((data & 0xFFFF0000)>>16) | 0x1;
   return (fallThroughCount + branchToCount);
   }

void *
TR_IPBCDataAllocation::operator new (size_t size) throw()
   {
   return TR_IPBytecodeHashTableEntry::alignedPersistentAlloc(size);
   }

void *
TR_IPBCDataEightWords::operator new (size_t size) throw()
   {
   return TR_IPBytecodeHashTableEntry::alignedPersistentAlloc(size);
   }

void
TR_IPBCDataEightWords::createPersistentCopy(uintptrj_t cacheStartAddress, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info)
   {
   TR_IPBCDataEightWordsStorage * store = (TR_IPBCDataEightWordsStorage *) storage;
   storage->pc = _pc - cacheStartAddress;
   storage->ID = TR_IPBCD_EIGHT_WORDS;
   storage->left = 0;
   storage->right = 0;
   for (int i = 0; i < SWITCH_DATA_COUNT; i++)
      store->data[i] = data[i];
   }

void
TR_IPBCDataEightWords::loadFromPersistentCopy(TR_IPBCDataStorageHeader * storage, TR::Compilation *comp, uintptrj_t cacheStartAddress)
   {
   TR_IPBCDataEightWordsStorage * store = (TR_IPBCDataEightWordsStorage *) storage;
   TR_ASSERT(storage->ID == TR_IPBCD_EIGHT_WORDS, "Incompatible types between storage and loading of iprofile persistent data");
   for (int i = 0; i < SWITCH_DATA_COUNT; i++)
      data[i] = store->data[i];
   }

void
TR_IPBCDataEightWords::copyFromEntry(TR_IPBytecodeHashTableEntry * originalEntry, TR::Compilation *comp)
   {
   TR_IPBCDataEightWords* entry = (TR_IPBCDataEightWords*) originalEntry;
   TR_ASSERT(originalEntry->asIPBCDataEightWords(), "Incompatible types between storage and loading of iprofile persistent data");
   for (int i = 0; i < SWITCH_DATA_COUNT; i++)
      data[i] = entry->data[i];
   }



int32_t
TR_IPBCDataCallGraph::setData(uintptrj_t v, uint32_t freq)
   {
   PORT_ACCESS_FROM_PORT(staticPortLib);
   bool found = false;
   int32_t returnCount = 0;
   int32_t maxWeight = 0;
   J9Class* receiverClass;

   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if (_csInfo.getClazz(i) == v)
         {
         // Check for overflow
         uint16_t newWeight = _csInfo._weight[i] + freq;
         if (newWeight >= _csInfo._weight[i])
            _csInfo._weight[i] = newWeight;
         else
            _csInfo._weight[i] = 0xFFFF;
         returnCount = _csInfo._weight[i];
         found = true;
         break;
         }
      else if (_csInfo.getClazz(i) == 0)
         {
         _csInfo.setClazz(i, v);
         _csInfo._weight[i] += freq;
         returnCount = _csInfo._weight[i];
         found = true;
         break;
         }

      if (maxWeight < _csInfo._weight[i])
         maxWeight = _csInfo._weight[i];
      }

   if (!found && _csInfo._residueWeight >= 0x7FFF)
      {
      if (_csInfo._residueWeight > maxWeight)
         {
         if (lockEntry())
            {
            for (int32_t i = 1; i < NUM_CS_SLOTS; i++)
               {
               _csInfo.setClazz(i, 0);
               _csInfo._weight[i] = 0;
               }
            _csInfo._weight[0] = freq;
            _csInfo.setClazz(0, v);
            _csInfo._residueWeight = 0;
            returnCount = freq;
            releaseEntry();
            }
         }
      else
         {
         // Check for overflow
         uint16_t newWeight = _csInfo._residueWeight + freq;
         if (newWeight >= _csInfo._residueWeight)
            _csInfo._residueWeight = newWeight;
         else
            _csInfo._residueWeight = 0xFFFF;
         returnCount = _csInfo._residueWeight;
         }
      }

   return returnCount;
   }

int32_t
TR_IPBCDataCallGraph::getSumCount(TR::Compilation *comp)
   {
   int32_t sumWeight = 0;
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      sumWeight += _csInfo._weight[i];

   return sumWeight + _csInfo._residueWeight;
   }

int32_t
TR_IPBCDataCallGraph::getSumCount(TR::Compilation *comp, bool)
   {
   static bool debug = feGetEnv("TR_debugiprofiler_detail") ? true : false;
   int32_t sumWeight = 0;
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if(debug)
         {
         int32_t len;
         const char * s = _csInfo.getClazz(i) ? comp->fej9()->getClassNameChars((TR_OpaqueClassBlock*)_csInfo.getClazz(i), len) : "0";
         fprintf(stderr,"[%p] slot %d, class %p %s, weight %d : ", this, i, _csInfo.getClazz(i), s, _csInfo._weight[i]);
         fflush(stderr);
         }
      sumWeight += _csInfo._weight[i];
      }
   sumWeight += _csInfo._residueWeight;
   if(debug)
      {
      fprintf(stderr," residueweight %d\n", _csInfo._residueWeight);
      fflush(stderr);
      }
   return sumWeight;
   }

uintptrj_t
TR_IPBCDataCallGraph::getData(TR::Compilation *comp)
   {
   int32_t sumWeight = _csInfo._residueWeight;
   int32_t maxWeight = 0;
   uintptrj_t data = 0;

   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if (!_csInfo.getClazz(i))
         continue;

      if (_csInfo._weight[i] > maxWeight)
         {
         maxWeight = _csInfo._weight[i];
         data = _csInfo.getClazz(i);
         }

      sumWeight += _csInfo._weight[i];
      }

   static bool traceIProfiling = ((debug("traceIProfiling") != NULL));
   if (traceIProfiling && comp)
      {
      traceMsg(comp, "\nMax weight %d, current sum weight %d\n", maxWeight, sumWeight);
      }
   // prevent potential division by zero
   if (sumWeight && ((float)maxWeight/(float)sumWeight) < 0.1f)
      {
//#ifdef VERBOSE_INLINING_PROFILING
      TR_IProfiler::_STATS_weakProfilingRatio ++;
//#endif
      if (0 && comp)
         {
         traceMsg(comp, "interpreter profiler: weak profiling info\n");
         for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
            {
            if (!_csInfo.getClazz(i))
               continue;
            int len=0;
            const char * s =  comp->fej9()->getClassNameChars((TR_OpaqueClassBlock*)(_csInfo.getClazz(i)), len);

            traceMsg(comp, "interpreter profiler: class %p %s with count %d\n", _csInfo.getClazz(i), s, _csInfo._weight[i]);
            }
         traceMsg(comp, "interpreter profiler: residue weight %d\n", _csInfo._residueWeight);
         }
      return 0;
      }

   return data;
   }

void *
TR_IPMethodHashTableEntry::operator new (size_t size) throw()
   {
   return TR_IPBytecodeHashTableEntry::alignedPersistentAlloc(size);
   }

int32_t
TR_IPBCDataCallGraph::getEdgeWeight(TR_OpaqueClassBlock *clazz, TR::Compilation *comp)
   {
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if (_csInfo.getClazz(i) == (uintptrj_t)clazz)
         {
         return _csInfo._weight[i];
         }
      }
   return 0;
   }




void
TR_IPBCDataCallGraph::printWeights(TR::Compilation *comp)
   {
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      int32_t len;
      const char * s = _csInfo.getClazz(i) ? comp->fej9()->getClassNameChars((TR_OpaqueClassBlock*)_csInfo.getClazz(i), len) : "0";

      fprintf(stderr, "%p %s %d\n", _csInfo.getClazz(i), s, _csInfo._weight[i]);
      }
   fprintf(stderr, "%d\n", _csInfo._residueWeight);
   }

void
TR_IPBCDataCallGraph::updateEdgeWeight(TR_OpaqueClassBlock *clazz, int32_t weight)
   {
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if (_csInfo.getClazz(i) == (uintptrj_t)clazz)
         {
         _csInfo._weight[i] = weight;
         break;
         }
      }
   }

bool
TR_IPBCDataCallGraph::lockEntry()
   {
   TR::Monitor *persistenceMonitor = TR::MonitorTable::get()->getIProfilerPersistenceMonitor();
   persistenceMonitor->enter();
   bool success=false;
   if (!isLockedEntry())
      {
      success=true;
      setLockedEntry();
      }
   persistenceMonitor->exit();
   return success;
   }

void
TR_IPBCDataCallGraph::releaseEntry()
   {
   TR::Monitor *persistenceMonitor = TR::MonitorTable::get()->getIProfilerPersistenceMonitor();
   persistenceMonitor->enter();
   resetLockedEntry();
   persistenceMonitor->exit();
   }

bool
TR_IPBCDataCallGraph::isLocked()
   {
   TR::Monitor *persistenceMonitor = TR::MonitorTable::get()->getIProfilerPersistenceMonitor();
   persistenceMonitor->enter();
   bool locked=false;
   if (isLockedEntry())
      locked=true;

   persistenceMonitor->exit();
   return locked;
   }

uint32_t
TR_IPBCDataCallGraph::canBePersisted(uintptrj_t cacheStartAddress, uintptrj_t cacheSize, TR::PersistentInfo *info)
   {
   if (!getCanPersistEntryFlag())
      return IPBC_ENTRY_CANNOT_PERSIST;

   if (!lockEntry()) // Try to lock the entry; if entry is already locked, abort
      return IPBC_ENTRY_PERSIST_LOCK;

   for (int32_t i = 0; i < NUM_CS_SLOTS && _csInfo.getClazz(i);i++) // scan all classes profiled in this entry
      {
      J9Class *clazz = (J9Class *) _csInfo.getClazz(i);
      if (clazz)
         {
         if (info->isUnloadedClass(clazz, true))
            {
            releaseEntry();  // release the lock on the entry
            return IPBC_ENTRY_PERSIST_UNLOADED;
            }

         uintptrj_t romClass = (uintptrj_t) clazz->romClass;
         if (romClass < cacheStartAddress || romClass > cacheStartAddress+cacheSize)
            {
            releaseEntry(); // release the lock on the entry
            return IPBC_ENTRY_PERSIST_NOTINSCC;
            }
         TR_ASSERT(_csInfo.getClazz(i), "Race condition detected: cached value=%p, pc=%p", clazz, _pc);
         }
      }
   return IPBC_ENTRY_CAN_PERSIST;
   }

void
TR_IPBCDataCallGraph::createPersistentCopy(uintptrj_t cacheStartAddress, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info)
   {
   TR_IPBCDataCallGraphStorage * store = (TR_IPBCDataCallGraphStorage *) storage;
   storage->pc = _pc - cacheStartAddress;
   storage->ID = TR_IPBCD_CALL_GRAPH;
   storage->left = 0;
   storage->right = 0;
   for (int32_t i=0; i < NUM_CS_SLOTS;i++)
      {
      J9Class *clazz = (J9Class *) _csInfo.getClazz(i);
      if (clazz)
         {
         TR_ASSERT(!info->isUnloadedClass(clazz, true), "cannot store unloaded class");
         uintptrj_t romClass = (uintptrj_t) clazz->romClass;
         TR_ASSERT(romClass >= cacheStartAddress, "expect the rom class to be in the shared class");
         store->_csInfo.setClazz(i, (romClass - cacheStartAddress));
         TR_ASSERT(_csInfo.getClazz(i), "Race condition detected: cached value=%p, pc=%p", clazz, _pc);
         }
      else
         {
         store->_csInfo.setClazz(i, 0);
         }
      store->_csInfo._weight[i] = _csInfo._weight[i];
      }
   store->_csInfo._residueWeight = _csInfo._residueWeight;
   store->_csInfo._tooBigToBeInlined = _csInfo._tooBigToBeInlined;
   }

void
TR_IPBCDataCallGraph::loadFromPersistentCopy(TR_IPBCDataStorageHeader * storage, TR::Compilation *comp, uintptrj_t cacheStartAddress)
   {
   TR_IPBCDataCallGraphStorage * store = (TR_IPBCDataCallGraphStorage *) storage;
   TR_ASSERT(storage->ID == TR_IPBCD_CALL_GRAPH, "Incompatible types between storage and loading of iprofile persistent data");
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if (store->_csInfo.getClazz(i))
         {
         J9Class * ramClass = ((TR_J9VM *) comp->fej9())->matchRAMclassFromROMclass((J9ROMClass *) (store->_csInfo.getClazz(i) + cacheStartAddress), comp);
         if (ramClass)
            {
            _csInfo.setClazz(i, (uintptrj_t)ramClass);
            _csInfo._weight[i] = store->_csInfo._weight[i];
            }
         else
            {
            _csInfo.setClazz(i, 0);
            _csInfo._weight[i] = 0;
            }
         }
      else
         {
         _csInfo.setClazz(i, 0);
         _csInfo._weight[i] = 0;
         }
      }
   _csInfo._residueWeight = store->_csInfo._residueWeight;
   _csInfo._tooBigToBeInlined = store->_csInfo._tooBigToBeInlined;
   }

void
TR_IPBCDataCallGraph::copyFromEntry(TR_IPBytecodeHashTableEntry * originalEntry, TR::Compilation *comp)
   {
   TR_IPBCDataCallGraph * entry = (TR_IPBCDataCallGraph*) originalEntry;
   TR_ASSERT(originalEntry->asIPBCDataCallGraph(), "Incompatible types between storage and loading of iprofile persistent data");
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if (entry->_csInfo.getClazz(i))
         {
         _csInfo.setClazz(i, entry->_csInfo.getClazz(i));
         _csInfo._weight[i] = entry->_csInfo._weight[i];
         }
      else
         {
         _csInfo.setClazz(i, 0);
         _csInfo._weight[i] = 0;
         }
      }
   _csInfo._residueWeight = entry->_csInfo._residueWeight;
   _csInfo._tooBigToBeInlined = entry->_csInfo._tooBigToBeInlined;
   }

TR_IPBCDataCallGraph*
TR_IProfiler::getCGProfilingData(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp)
   {
   if (!isIProfilingEnabled())
      return NULL;

   return getCGProfilingData(getMethodFromBCInfo(bcInfo, comp), bcInfo.getByteCodeIndex(), comp);
   }

TR_IPBCDataCallGraph*
TR_IProfiler::getCGProfilingData(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation * comp)
   {
   TR_IPBytecodeHashTableEntry *entry = profilingSample(method, byteCodeIndex, comp);

   static bool traceIProfiling = ((debug("traceIProfiling") != NULL));
   if (entry)
      {
      if (invalidateEntryIfInconsistent(entry))
         {
         if (traceIProfiling)
            traceMsg(comp, "got nothing because it was invalidated\n");
         return NULL;
         }
      TR_IPBCDataCallGraph *e = entry->asIPBCDataCallGraph();
      return e;
      }

//#ifdef VERBOSE_INLINING_PROFILING
   TR_IProfiler::_STATS_noProfilingInfo ++;
//#endif
   return NULL;
   }

void
TR_IProfiler::setCallCount(TR_OpaqueMethodBlock *method, int32_t bcIndex, int32_t count, TR::Compilation * comp)
   {
   TR_IPBytecodeHashTableEntry *entry = profilingSample(method, bcIndex, comp, 0, true);
   if (entry && entry->asIPBCDataCallGraph())
      {
      CallSiteProfileInfo *csInfo  = NULL;
      TR_IPBCDataCallGraph *cgData = entry->asIPBCDataCallGraph();
      cgData->setDoNotPersist();
      FLUSH_MEMORY(TR::Compiler->target.isSMP());

      if (cgData)
         csInfo = cgData->getCGData();

      if (csInfo)
         {
         csInfo->setClazz(0, 0);
         csInfo->_weight[0] = count;

         if (count>_maxCallFrequency)
            _maxCallFrequency = count;
         }
      }
   }

void
TR_IProfiler::setCallCount(TR_ByteCodeInfo &bcInfo, int32_t count, TR::Compilation *comp)
   {
   setCallCount(getMethodFromBCInfo(bcInfo, comp), (int32_t)bcInfo.getByteCodeIndex(), count, comp);
   }

int32_t
TR_IProfiler::getCallCount(TR_OpaqueMethodBlock *calleeMethod, TR_OpaqueMethodBlock *method, int32_t bcIndex, TR::Compilation * comp)
   {
   TR_IPBytecodeHashTableEntry *entry = profilingSample(method, bcIndex, comp);

   if (entry && entry->asIPBCDataCallGraph())
      return entry->asIPBCDataCallGraph()->getSumCount(comp);

   uint32_t weight = 0;
   bool foundEntry = getCallerWeight(calleeMethod,method, &weight, bcIndex, comp);
   if (foundEntry)
      return weight;

   return 0;
   }

int32_t
TR_IProfiler::getCallCount(TR_OpaqueMethodBlock *method, int32_t bcIndex, TR::Compilation * comp)
   {
   TR_IPBytecodeHashTableEntry *entry = profilingSample(method, bcIndex, comp);

   if (entry && entry->asIPBCDataCallGraph())
      return entry->asIPBCDataCallGraph()->getSumCount(comp);

   return 0;
   }

void
TR_IProfiler::setWarmCallGraphTooBig(TR_OpaqueMethodBlock *method, int32_t bcIndex, TR::Compilation *comp, bool set)
   {
   TR_IPBytecodeHashTableEntry *entry = profilingSample(method, bcIndex, comp);

   if (entry && entry->asIPBCDataCallGraph())
      return entry->asIPBCDataCallGraph()->setWarmCallGraphTooBig(set);
   }

bool
TR_IProfiler::isWarmCallGraphTooBig(TR_OpaqueMethodBlock *method, int32_t bcIndex, TR::Compilation *comp)
   {
   TR_IPBytecodeHashTableEntry *entry = profilingSample(method, bcIndex, comp);

   if (entry && entry->asIPBCDataCallGraph())
      return entry->asIPBCDataCallGraph()->isWarmCallGraphTooBig();

   return false;
   }

int32_t
TR_IProfiler::getCallCount(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp)
   {
   return getCallCount(getMethodFromBCInfo(bcInfo, comp), (int32_t)bcInfo.getByteCodeIndex(), comp);
   }

int32_t
TR_IProfiler::getCGEdgeWeight (TR::Node *callerNode, TR_OpaqueMethodBlock *callee, TR::Compilation *comp)
   {
   TR_ByteCodeInfo& bcInfo = callerNode->getByteCodeInfo();
   uintptrj_t thisPC = getSearchPC (getMethodFromNode(callerNode, comp), bcInfo.getByteCodeIndex(), comp);

   if (isSpecialOrStatic(*(U_8 *)thisPC))
      return getCallCount(bcInfo, comp);

   TR_IPBCDataCallGraph *cgData = getCGProfilingData(callerNode->getByteCodeInfo(), comp);
   if (cgData)
      {
      return cgData->getEdgeWeight((TR_OpaqueClassBlock *)J9_CLASS_FROM_METHOD(((J9Method *)callee)), comp);
      }
   return 0;
   }


int32_t
TR_IProfiler::getMaxCallCount()
   {
   return _maxCallFrequency;
   }

void
TR_IProfiler::printAllocationReport()
   {
   printf("Total allocation count %d\n", _globalAllocationCount);

   if (!_globalAllocationCount) return;

   for (int32_t bucket = 0; bucket < ALLOC_HASH_TABLE_SIZE; bucket++)
      {
#if defined(EXPERIMENTAL_IPROFILER)
      for (TR_IPBCDataAllocation *entry = _allocHashTable[bucket]; entry; entry = (TR_IPBCDataAllocation *)entry->next)
         {
         if (entry->asIPBCDataAllocation())
            {
            J9Class *clazz = (J9Class *)((TR_IPBCDataAllocation *)entry)->getClass();
            J9Method *method = (J9Method *)((TR_IPBCDataAllocation *)entry)->getMethod();

            if (method && clazz && !_compInfo->getPersistentInfo()->isUnloadedClass((void*)clazz, true) && entry->getData()>0)
               {
               J9UTF8 * clazzUTRF8 = J9ROMCLASS_CLASSNAME(clazz->romClass);
               J9UTF8 * nameUTF8;
               J9UTF8 * signatureUTF8;
               J9UTF8 * methodClazzUTRF8;
               getClassNameSignatureFromMethod(method, methodClazzUTRF8, nameUTF8, signatureUTF8);

               printf("%d\t%5.2lf\t%.*s\t%.*s.%.*s%.*s\n",
                  entry->getData(), (double)((double)(entry->getData())/((double)_globalAllocationCount))*100.0,
                  J9UTF8_LENGTH(clazzUTRF8), J9UTF8_DATA(clazzUTRF8),
                  J9UTF8_LENGTH(methodClazzUTRF8), J9UTF8_DATA(methodClazzUTRF8), J9UTF8_LENGTH(nameUTF8), J9UTF8_DATA(nameUTF8),
                  J9UTF8_LENGTH(signatureUTF8), J9UTF8_DATA(signatureUTF8));
               }
            else if (entry->getData()>0)
               {
               printf("%d\t%5.2lf\tUnknown\tUnknown\n", entry->getData(), (double)((double)(entry->getData())/((double)_globalAllocationCount))*100.0);
               }

            }
         }
#endif
      }
   }

uint32_t
TR_IProfiler::releaseAllEntries()
   {
   uint32_t count = 0;
   for (int32_t bucket = 0; bucket < BC_HASH_TABLE_SIZE; bucket++)
      {
      for (TR_IPBytecodeHashTableEntry *entry = _bcHashTable[bucket]; entry; entry = entry->getNext())
         {
         if (entry->asIPBCDataCallGraph() && entry->asIPBCDataCallGraph()->isLocked())
            {
            count++;
            entry->asIPBCDataCallGraph()->releaseEntry();
            }
         }
      }
   return count;
   }

uint32_t
TR_IProfiler::countEntries()
   {
   uint32_t count = 0;
   for (int32_t bucket = 0; bucket < BC_HASH_TABLE_SIZE; bucket++)
      for (TR_IPBytecodeHashTableEntry *entry = _bcHashTable[bucket]; entry; entry = entry->getNext())
         count++;
   return count;
   }


// helper functions for replay
//
void TR_IProfiler::setupEntriesInHashTable(TR_IProfiler *ip)
   {
   for (int32_t bucket = 0; bucket < BC_HASH_TABLE_SIZE; bucket++)
      {
      TR_IPBytecodeHashTableEntry *entry = _bcHashTable[bucket], *prevEntry = NULL;

      while (entry)
         {
         uintptrj_t pc = entry->getPC();

         if (pc == 0 ||
               pc == 0xffffffff)
            {
            printf("invalid pc for entry %p %p\n", entry, pc);fflush(stdout);
            prevEntry = entry;
            entry = entry->getNext();
            continue;
            }


         TR_IPBytecodeHashTableEntry *newEntry = ip->findOrCreateEntry(bucket, pc, true);
         // check for entries corresponding to
         // unloaded methods, findOrCreateEntry will
         // return NULL above. its ok to ignore these entries
         // as they are invalid anyway
         //
         if (newEntry)
            ip->copyDataFromEntry(entry, newEntry, NULL);
         prevEntry = entry;
         entry = entry->getNext();
         }
      }
   printf("Finished adding entries from core to new iprofiler\n");
   }

void TR_IProfiler::copyDataFromEntry(TR_IPBytecodeHashTableEntry *oldEntry, TR_IPBytecodeHashTableEntry *newEntry, TR_IProfiler *ip)
   {
   U_8 *oldEntryPC = (U_8*)oldEntry->getPC();
   U_8 byteCodeType = *(oldEntryPC);
   if (isSwitch(byteCodeType))
      {
      ;// FIXME: dont care about switches at the moment
      }
   else
      {
      // isCompact(byteCodeType) or callGraph entries
      //
      printf("populating entry for pc %p newentrypc %p\n", oldEntryPC, (U_8*)newEntry->getPC());
      // vft entries for whacking
      //
      void *oldVft = *(void**)(oldEntry);
      void *newVft = *(void**)(newEntry);
      *(void**)(oldEntry) = newVft;
      if (isCompact(byteCodeType))
         {
         uintptrj_t data = oldEntry->getData();
         //printf("got oldvft = %p newvft = %p data %p\n", oldvft, newvft, data);
         newEntry->setData(data);
         }
      else
         {
         // callGraph entry
         //
         CallSiteProfileInfo *oldCSInfo = ((TR_IPBCDataCallGraph*)(oldEntry))->getCGData();
         CallSiteProfileInfo *newCSInfo = ((TR_IPBCDataCallGraph*)(newEntry))->getCGData();
         printf("got oldCSInfo %p\n", oldCSInfo);

         if (oldCSInfo)
            {
            for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
               {
               printf("got clazz %p weight %d\n", oldCSInfo->getClazz(i), oldCSInfo->_weight[i]);
               newCSInfo->setClazz(i, oldCSInfo->getClazz(i));
               newCSInfo->_weight[i] = oldCSInfo->_weight[i];
               }
            }

         if (((TR_IPBCDataCallGraph*)oldEntry)->isWarmCallGraphTooBig())
            ((TR_IPBCDataCallGraph*)newEntry)->setWarmCallGraphTooBig();
         }
      }
   }

void TR_IProfiler::checkMethodHashTable()
   {
   static char *fname = feGetEnv("TR_PrintMethodHashTableFileName");
   if (!fname)
      return;

   printf("TR_PrintMethodHashTableFileName is set; trying to open file %s\n",fname);
   ::FILE *fout = fopen(fname, "a");

   if (!fout)
      {
      printf("Couldn't open the file; re-directing to stderr instead\n");
      fout = stderr;
      }

   fprintf(fout, "printing method hash table\n");fflush(fout);
   for (int32_t bucket = 0; bucket < IPMETHOD_HASH_TABLE_SIZE; bucket++)
      {
      TR_IPMethodHashTableEntry *entry = _methodHashTable[bucket];

      while (entry)
         {
         J9Method *method = (J9Method*)entry->_method;
         fprintf(fout,"method\t");fflush(fout);
#if 1
         J9UTF8 * nameUTF8;
         J9UTF8 * signatureUTF8;
         J9UTF8 * methodClazzUTRF8;
         getClassNameSignatureFromMethod(method, methodClazzUTRF8, nameUTF8, signatureUTF8);

         fprintf(fout,"%.*s.%.*s%.*s\t %p\t",
                J9UTF8_LENGTH(methodClazzUTRF8), J9UTF8_DATA(methodClazzUTRF8), J9UTF8_LENGTH(nameUTF8), J9UTF8_DATA(nameUTF8),
                J9UTF8_LENGTH(signatureUTF8), J9UTF8_DATA(signatureUTF8), method);fflush(fout);
#endif
         int32_t count = 0;
         fprintf(fout,"\t has %d callers and %d -bytecode long:\n", 0, J9_BYTECODE_END_FROM_ROM_METHOD(getOriginalROMMethod(method))-J9_BYTECODE_START_FROM_ROM_METHOD(getOriginalROMMethod(method)));fflush(fout);
         uint32_t i=0;

         for (TR_IPMethodData* it = &entry->_caller; it; it = it->next)
            {
            count++;

            TR_OpaqueMethodBlock *meth = it->getMethod();
            if(meth)
               {
               J9UTF8 * caller_nameUTF8;
               J9UTF8 * caller_signatureUTF8;
               J9UTF8 * caller_methodClazzUTF8;
               getClassNameSignatureFromMethod((J9Method*)meth, caller_methodClazzUTF8, caller_nameUTF8, caller_signatureUTF8);

               fprintf(fout,"%p %.*s%.*s%.*s weight %d pc %p\n", meth,
                  J9UTF8_LENGTH(caller_methodClazzUTF8), J9UTF8_DATA(caller_methodClazzUTF8), J9UTF8_LENGTH(caller_nameUTF8), J9UTF8_DATA(caller_nameUTF8),
                  J9UTF8_LENGTH(caller_signatureUTF8), J9UTF8_DATA(caller_signatureUTF8),
                  it->getWeight(), it->getPCIndex());fflush(fout);
               }
            else
               {
               fprintf(fout,"meth is null\n");
               }
            }
         //Print the other bucket
         fprintf(fout, "other bucket: weight %d\n", entry->_otherBucket.getWeight()); fflush(fout);

         entry = entry->_next;
         fprintf(fout,": %d \n", count);fflush(fout);
         }
      }
   }

void TR_IProfiler::getNumberofCallersAndTotalWeight(TR_OpaqueMethodBlock *calleeMethod, uint32_t *count, uint32_t *weight)
   {
   uint32_t i = 0;
   uint32_t w = 0;

   // Search for the callee in the hashtable
   int32_t bucket = methodHash((uintptrj_t)calleeMethod);
   TR_IPMethodHashTableEntry *entry = searchForMethodSample((TR_OpaqueMethodBlock*)calleeMethod, bucket);
   if (entry)
      {
      w = entry->_otherBucket.getWeight();
      // Iterate through all the callers and add their weight
      for (TR_IPMethodData* it = &entry->_caller; it; it = it->next)
         {
         w += it->getWeight();
         i++;
         }
      }
   *weight = w;
   *count = i;
   }

uint32_t TR_IProfiler::getOtherBucketWeight(TR_OpaqueMethodBlock *calleeMethod)
   {
   int32_t bucket = methodHash((uintptrj_t)calleeMethod);

   TR_IPMethodHashTableEntry *entry = searchForMethodSample((TR_OpaqueMethodBlock*)calleeMethod, bucket);

   if(!entry)   // if there are no entries, we have no callers!
      return 0;

   return entry->_otherBucket.getWeight();


   }


bool TR_IProfiler::getCallerWeight(TR_OpaqueMethodBlock *calleeMethod,TR_OpaqueMethodBlock *callerMethod, uint32_t *weight, uint32_t pcIndex, TR::Compilation *comp)
{
   // First: hash the method

   bool useTuples = (pcIndex != ~0);

    int32_t bucket = methodHash((uintptrj_t)calleeMethod);
   //printf("getCallerWeight:  method = %p methodHash = %d\n",calleeMethod,bucket);

   //adjust pcIndex for interface calls (see getSearchPCFromMethodAndBCIndex)
   //otherwise we won't be able to locate a caller-callee-bcIndex triplet
   //even if it is in a TR_IPMethodHashTableEntry
   uintptrj_t pcAddress = getSearchPCFromMethodAndBCIndex(callerMethod, pcIndex, TR::comp());

   TR_IPMethodHashTableEntry *entry = searchForMethodSample((TR_OpaqueMethodBlock*)calleeMethod, bucket);

   if(!entry)   // if there are no entries, we have no callers!
      {
      *weight = ~0;
      return false;
      }
   uint32_t i=0 ;
   TR_J9VMBase *fej9 = NULL;
   if (comp)
      fej9 = (TR_J9VMBase *) comp->fej9();
   else
      fej9 = TR_J9VM::get(_compInfo->getJITConfig(), 0);

   for (TR_IPMethodData* it = &entry->_caller; it; it = it->next)
      {
      //if (comp)
      //   traceMsg(comp, "comparing %p with %p and pc index %d pc address %p with other pc index %d other pc address %p\n", callerMethod, it->getMethod(), pcIndex, pcAddress, it->getPCIndex(), ((uintptrj_t) it->getPCIndex()) + TR::Compiler->mtd.bytecodeStart(callerMethod));

      if( it->getMethod() == callerMethod && (!useTuples || ((((uintptrj_t) it->getPCIndex()) + TR::Compiler->mtd.bytecodeStart(callerMethod)) == pcAddress)))
         {
         *weight = it->getWeight();
         return true;
         }
      }

   *weight = entry->_otherBucket.getWeight();
   return false;

}

static int32_t J9THREAD_PROC iprofilerThreadProc(void * entryarg)
   {
   J9JITConfig * jitConfig = (J9JITConfig *) entryarg;
   J9JavaVM * vm           = jitConfig->javaVM;
   TR_J9VMBase *fe = TR_J9VM::get(jitConfig, 0);
   TR_IProfiler *iProfiler = fe->getIProfiler();
   J9VMThread *iprofilerThread = NULL;
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   // If I created this thread, iprofiler exists; don't need to check against NULL
   int rc = vm->internalVMFunctions->internalAttachCurrentThread(vm, &iprofilerThread, NULL,
                                  J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_OBJECT |
                                  J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
                                  iProfiler->getIProfilerOSThread());
   iProfiler->getIProfilerMonitor()->enter();
   iProfiler->setAttachAttempted(true);
   if (rc == JNI_OK)
      iProfiler->setIProfilerThread(iprofilerThread);
   iProfiler->getIProfilerMonitor()->notifyAll();
   iProfiler->getIProfilerMonitor()->exit();
   if (rc != JNI_OK)
      return JNI_ERR; // attaching the IProfiler thread failed

#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
   if (vm->javaOffloadSwitchOnWithReasonFunc != 0)
      (*vm->javaOffloadSwitchOnWithReasonFunc)(iprofilerThread, J9_JNI_OFFLOAD_SWITCH_JIT_IPROFILER_THREAD);
#endif

   j9thread_set_name(j9thread_self(), "JIT IProfiler");

   iProfiler->processWorkingQueue();

   vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);
   iProfiler->setIProfilerThread(NULL);
   iProfiler->getIProfilerMonitor()->enter();
   // free the special buffer because we don't need it anymore
   if (iProfiler->getCrtProfilingBuffer())
      {
      j9mem_free_memory(iProfiler->getCrtProfilingBuffer());
      iProfiler->setCrtProfilingBuffer(NULL);
      }
   iProfiler->setIProfilerThreadExitFlag();
   iProfiler->getIProfilerMonitor()->notifyAll();
   j9thread_exit((J9ThreadMonitor*)iProfiler->getIProfilerMonitor()->getVMMonitor());

#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
   if (vm->javaOffloadSwitchOffNoEnvWithReasonFunc != 0)
      (*vm->javaOffloadSwitchOffNoEnvWithReasonFunc)(vm, j9thread_self(), J9_JNI_OFFLOAD_SWITCH_JIT_IPROFILER_THREAD);
#endif

   //fprintf(stderr, "iprofilerThread will exit now numRequests=%u numRequestsHandedToIProfilerThread=%u numRequestsSkipped=%u\n", numRequests, numRequestsHandedToIProfilerThread, numRequestsSkipped);
   return 0;
   }


void TR_IProfiler::startIProfilerThread(J9JavaVM *javaVM)
   {
   PORT_ACCESS_FROM_PORT(_portLib);
   UDATA priority;

   priority = J9THREAD_PRIORITY_NORMAL;

   _iprofilerMonitor = TR::Monitor::create("JIT-iprofilerMonitor");
   if (_iprofilerMonitor)
      {
      // create the thread for interpreter profiling
      if(javaVM->internalVMFunctions->createThreadWithCategory(&_iprofilerOSThread,
                                      TR::Options::_profilerStackSize << 10,
                                      priority,
                                      0,
                                      &iprofilerThreadProc,
                                      javaVM->jitConfig,
                                      J9THREAD_CATEGORY_SYSTEM_JIT_THREAD))
         {
         j9tty_printf(PORTLIB, "Error: Unable to create iprofiler thread\n");
         TR::Options::getCmdLineOptions()->setOption(TR_DisableIProfilerThread);
         // TODO:destroy the monitor that was created (_iprofilerMonitor)
         _iprofilerMonitor = NULL;
         }
      else // Must wait here until the thread gets created; otherwise an early shutdown
         { // does not know whether or not to destroy the thread
         _iprofilerMonitor->enter();
         while (!getAttachAttempted())
            _iprofilerMonitor->wait();
         _iprofilerMonitor->exit();
         }
      }
   else
      {
      j9tty_printf(PORTLIB, "Error: Unable to create JIT-iprofilerMonitor\n");
      TR::Options::getCmdLineOptions()->setOption(TR_DisableIProfilerThread);
      }
   }

void TR_IProfiler::deallocateIProfilerBuffers()
   {
   // To be called when we are sure that no java thread will post additional
   // buffers to the working queue.
   // Probably don't need to take out the monitor, but just in case...
   // This method is only called during JIT shutdown, so what's it really hurting
   if (_iprofilerMonitor)
      _iprofilerMonitor->enter();

   PORT_ACCESS_FROM_PORT(_portLib);
   while (!_freeBufferList.isEmpty())
      {
      IProfilerBuffer *profilingBuffer = _freeBufferList.pop();
      j9mem_free_memory(profilingBuffer->getBuffer());
      j9mem_free_memory(profilingBuffer);
      }
   // Just in case
   while (!_workingBufferList.isEmpty())
      {
      IProfilerBuffer *profilingBuffer = _workingBufferList.pop();
      j9mem_free_memory(profilingBuffer->getBuffer());
      j9mem_free_memory(profilingBuffer);
      }
   _workingBufferTail = NULL;

   if (_iprofilerMonitor)
      _iprofilerMonitor->exit();
   }

void TR_IProfiler::stopIProfilerThread()
   {
   PORT_ACCESS_FROM_PORT(_portLib);
   if (!_iprofilerMonitor)
      return; // possible if the IProfiler thread was never created
   _iprofilerMonitor->enter();
   if (!getIProfilerThread()) // We could not create the iprofilerThread
      {
      _iprofilerMonitor->exit();
      return;
      }

   // get a special buffer which will be used as a signal to stop iprofilerThread
   //
   IProfilerBuffer *specialProfilingBuffer = NULL;
   if (!_freeBufferList.isEmpty())
      {
      specialProfilingBuffer = _freeBufferList.pop();
      }
   else if (!_workingBufferList.isEmpty())
      {
      specialProfilingBuffer = _workingBufferList.pop();
      _numOutstandingBuffers--;
      if (_workingBufferList.isEmpty())
         _workingBufferTail = NULL;
      }
   else
      {
      specialProfilingBuffer = (IProfilerBuffer*)j9mem_allocate_memory(sizeof(IProfilerBuffer), J9MEM_CATEGORY_JIT);
      if (specialProfilingBuffer)
    specialProfilingBuffer->setBuffer(NULL);
      }

   // Deallocate all outstanding buffers
   while (!_workingBufferList.isEmpty())
      {
      IProfilerBuffer *profilingBuffer = _workingBufferList.pop();
      _numOutstandingBuffers--;
      _freeBufferList.add(profilingBuffer);
      }
   _workingBufferTail = NULL;

   // Add the special request
   if (specialProfilingBuffer)
      {
      if (specialProfilingBuffer->getBuffer())
         j9mem_free_memory(specialProfilingBuffer->getBuffer());
      specialProfilingBuffer->setBuffer(NULL);
      specialProfilingBuffer->setSize(0);
      _workingBufferList.add(specialProfilingBuffer);
      _workingBufferTail = specialProfilingBuffer;
      // wait for the iprofilerThread to stop
      while (!_iprofilerThreadExitFlag)
         {
         _iprofilerMonitor->notifyAll();
         _iprofilerMonitor->wait();
         }
      }

   _iprofilerMonitor->exit();
   }

// The following method is executed by the app thread and tries to post a
// iprofiling buffer to the working queue, so that the iprofiling thread
// can process it
//
bool
TR_IProfiler::postIprofilingBufferToWorkingQueue(J9VMThread * vmThread, const U_8* dataStart, UDATA size)
   {
   PORT_ACCESS_FROM_PORT(_portLib);
   //--- first acquire the monitor and try to get a free buffer
   if (!_iprofilerMonitor || _iprofilerMonitor->try_enter())
      return false; // Monitor is contended; better let the app thread do the processing

   // If the profiling thread has already been destroyed, delegate the processing to the java thread
   if (_iprofilerThreadExitFlag)
      {
      _iprofilerMonitor->exit();
      return false;
      }
   IProfilerBuffer *freeBuffer = _freeBufferList.pop();
   if (!freeBuffer)
      {
      U_8* newBuffer = (U_8*)j9mem_allocate_memory(_iprofilerBufferSize, J9MEM_CATEGORY_JIT);
      if (!newBuffer)
         {
         _iprofilerMonitor->exit();
         return false;
         }
      freeBuffer = (IProfilerBuffer*)j9mem_allocate_memory(sizeof(IProfilerBuffer), J9MEM_CATEGORY_JIT);
      if (!freeBuffer)
         {
         j9mem_free_memory(newBuffer);
         _iprofilerMonitor->exit();
         return false;
         }
      freeBuffer->setBuffer(newBuffer);
      }
   setProfilingBufferCursor(vmThread, freeBuffer->getBuffer());
   setProfilingBufferEnd(vmThread, freeBuffer->getBuffer() + _iprofilerBufferSize);

   //--- link current buffer to the processing list
   freeBuffer->setBuffer((U_8*)dataStart);
   freeBuffer->setSize(size);
   freeBuffer->setIsInvalidated(false); // reset while holding VM access
   _workingBufferList.insertAfter(_workingBufferTail, freeBuffer);
   _workingBufferTail = freeBuffer;

   _numRequestsHandedToIProfilerThread++;
   _numOutstandingBuffers++;

   //--- signal the processing thread
   _iprofilerMonitor->notifyAll();
   _iprofilerMonitor->exit();
   return true;
   }

// Method executed by the java thread when jitHookBytecodeProfiling() is called
bool TR_IProfiler::processProfilingBuffer(J9VMThread *vmThread, const U_8* dataStart, UDATA size)
   {
   if (_numOutstandingBuffers >= TR::Options::_iprofilerNumOutstandingBuffers ||
       _compInfo->getPersistentInfo()->getLoadFactor() >= 1) // More active threads than CPUs
      {
      if (100*_numRequestsSkipped >= (uint64_t)TR::Options::_iprofilerBufferMaxPercentageToDiscard * _numRequests)
         {
         // too many skipped requests; let the java thread handle this one
         //records = parseBuffer(vmThread, cursor, size, iProfiler);
         //setProfilingBufferCursor(vmThread, (U_8*)cursor);
         return false; // delegate the processing to the java thread
         }
      else // skip this request altogether
         {
         _numRequestsSkipped++;
         setProfilingBufferCursor(vmThread, (U_8*)dataStart);
         }
      }
   else // The iprofilerThread will handle this request
      {
      if (!postIprofilingBufferToWorkingQueue(vmThread, dataStart, size))
         {
         // If posting fails, we should let the app thread process the buffer
         return false;
         //_numRequestsSkipped++;
         //setProfilingBufferCursor(vmThread, (U_8*)dataStart);
         }
      }
   return true;
   }


// This method is executed by the iprofiling thread
void TR_IProfiler::processWorkingQueue()
   {
   PORT_ACCESS_FROM_PORT(_portLib);
   // wait for something to do
   _iprofilerMonitor->enter();
   do {
      while (_workingBufferList.isEmpty())
         {
         //fprintf(stderr, "IProfiler thread will wait for data outstanding=%d\n", numOutstandingBuffers);
         _iprofilerMonitor->wait();
         }
      // We have some buffer to process
      // Dequeue the buffer to be processed
      //
      _crtProfilingBuffer = _workingBufferList.pop();
      if (_workingBufferList.isEmpty())
         _workingBufferTail = NULL;

      // We don't need the iprofiler monitor now
      _iprofilerMonitor->exit();
      if (_crtProfilingBuffer->getSize() > 0)
         {
         // process the buffer after acquiring VM access
         acquireVMAccessNoSuspend(_iprofilerThread);   // blocking. Will wait for the entire GC
         // Check to see if GC has invalidated this buffer
         if (_crtProfilingBuffer->isValid())
            {
         //fprintf(stderr, "IProfiler thread will process buffer %p of size %u\n", profilingBuffer->getBuffer(), profilingBuffer->getSize());
            parseBuffer(_iprofilerThread, _crtProfilingBuffer->getBuffer(), _crtProfilingBuffer->getSize());
         //fprintf(stderr, "IProfiler thread finished processing\n");
            }
         releaseVMAccess(_iprofilerThread);
         }
      else // Special
         {
         break;
         }
      // attach the buffer to the buffer pool
      _iprofilerMonitor->enter();
      _freeBufferList.add(_crtProfilingBuffer);
      _crtProfilingBuffer = NULL;
      _numOutstandingBuffers--;
      }while(1);
   }

extern "C" void stopInterpreterProfiling(J9JITConfig *jitConfig);

/* Lower value will more aggressivly skip samples as the number of unloaded classes increasses */
static const int IP_THROTTLE = 32;

#if defined(NETWORK_ORDER_BYTECODE)
static inline uint16_t readU16(U_8 *_offset) { return ((U_16)(*_offset)) << 8 | (*(_offset + 1)); }
#elif defined(FIXUP_UNALIGNED)
static inline uint16_t readU16(U_8 *_offset)
   {
   bool hostIsLittleEndian = TR::Compiler->host.cpu.isLittleEndian();
   return hostIsLittleEndian ?
      (((U_16)(*_offset)) | (((U_16)(*(_offset + 1))) << 8)) :
      (((U_16)(*_offset)) << 8 | (*(_offset + 1)));
   }
#else /* !FIXUP_UNALIGNED */
static inline uint16_t readU16(U_8 *_offset) { return (*(U_16*)(_offset)); }
#endif /* !FIXUP_UNALIGNED */


//------------------------------ parseBuffer -------------------------
// Parses a buffer from the VM and populates a hash table
// Method needs to be called with VM access in hand to avoid GC
// unloading classes
// If verboseReparse==true, then we print information about the content
// of the buffer without updating the hash table or any other globals
//---------------------------------------------------------------------
UDATA TR_IProfiler::parseBuffer(J9VMThread * vmThread, const U_8* dataStart, UDATA size, bool verboseReparse)
   {
   if (TR::Options::getCmdLineOptions()->getOption(TR_DisableIProfilerDataCollection) ||
      TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableIProfilerDataCollection))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)_vm;
      stopInterpreterProfiling(fej9->getJ9JITConfig());
      return 0;
      }

   const U_8* cursor = dataStart;
   U_8 * pc;
   UDATA records = 0;
   UDATA totalRecords = 0;
   J9Class* receiverClass;
   J9Method *method = NULL;
   J9Method *lastMethod = NULL;
   uintptrj_t lastMethodSize = 0;
   uintptrj_t lastMethodStart = 0;
   intptrj_t data = 0;
   bool addSample = false;
   J9Method *caller;
   J9Method *callee;
   U_32 switchOperand;
   //TR_ByteCodeType bytecodeType;
   int32_t numUnloadedClasses = _compInfo->getPersistentInfo()->getNumUnloadedClasses();
   int32_t numLoadedClasses = _compInfo->getPersistentInfo()->getNumLoadedClasses();
   int32_t numSamplesToBeSkipped = numUnloadedClasses >> 10;
   int32_t ratio = 0;
   static bool fanInDisabled = TR::Options::getCmdLineOptions()->getOption(TR_DisableInlinerFanIn) ||
                               TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableInlinerFanIn);

   PORT_ACCESS_FROM_PORT(_portLib);

   if( numUnloadedClasses >= TR::Options::_disableIProfilerClassUnloadThreshold ||
       _compInfo->getJITConfig()->runtimeFlags & (J9JIT_CODE_CACHE_FULL | J9JIT_DATA_CACHE_FULL) )
      {
      // At this point the cost of processing IProfiler samples is too high
      // or the code/data cache is full so there is no point in profiling
      stopInterpreterProfiling(_compInfo->getJITConfig());
      return 0;
      }

   if (numUnloadedClasses > 0)
      ratio = numLoadedClasses/numUnloadedClasses;

   if (ratio > 2)
      numSamplesToBeSkipped = 0;
   else
      numSamplesToBeSkipped = std::min(numSamplesToBeSkipped, IP_THROTTLE);

   if (numSamplesToBeSkipped == IP_THROTTLE && !verboseReparse)
      {
      return 0;
      }

   bool isClassLoadPhase = _compInfo->getPersistentInfo()->isClassLoadingPhase();

   J9JavaVM * javaVM = _compInfo->getJITConfig()->javaVM;

   int32_t skipCountMaster = 20+(rand()%10); // TODO: Use the master TR_RandomGenerator from jitconfig?
   int32_t skipCount = skipCountMaster;
   bool profileFlag = true;

   while (cursor < dataStart + size)
      {
      const U_8* cursorCopy = cursor;

      if (!TR::Options::_profileAllTheTime)
         {
         // replenish skipCount if it has been exhausted
         if (skipCount <= 0)
            {
            skipCount = skipCountMaster;
            profileFlag = !profileFlag;  // flip profiling flag
            if (profileFlag)
               {
               // We want to profile less in classLoadPhase and more outside
               if (isClassLoadPhase)
                  skipCount >>= 2;
               else
                  skipCount <<= 1;
               }
            }
         skipCount--;
         }

      if (!verboseReparse)
         totalRecords += 1;

      pc = *(U_8**)cursor;
      cursor += sizeof(pc);


      switch (*pc)
         {
         case JBifacmpeq:
         case JBifacmpne:
         case JBifeq:
         case JBifge:
         case JBifgt:
         case JBifle:
         case JBiflt:
         case JBifne:
         case JBificmpeq:
         case JBificmpne:
         case JBificmpge:
         case JBificmpgt:
         case JBificmple:
         case JBificmplt:
         case JBifnull:
         case JBifnonnull:
            data = *cursor++;
            addSample = profileFlag;
            //bytecodeType = BRANCH_BYTECODE;
            if (TEST_verbose || verboseReparse)
               {
               U_8 branchTaken = (U_8)data;
               j9tty_printf(PORTLIB, "pc=%p (branch bc=%d) taken=%d cursor=%p\n", pc, *pc, branchTaken, cursorCopy);
               }
            break;
         case JBcheckcast:
         case JBinstanceof:
            receiverClass = *(J9Class**)cursor;
            cursor += sizeof(receiverClass);

            data = (intptrj_t)receiverClass;

            addSample = true;
            //bytecodeType = CHECKCAST_BYTECODE;

            if (TEST_verbose || verboseReparse)
               {
               J9ROMClass* romClass = receiverClass->romClass;
               J9UTF8* name = J9ROMCLASS_CLASSNAME(romClass);
               j9tty_printf(PORTLIB, "pc=%p (cast bc=%d) operand=%.*s(%p) cursor=%p\n", pc, *pc, (UDATA)J9UTF8_LENGTH(name), J9UTF8_DATA(name), receiverClass, cursorCopy);
               }
            break;
         case JBinvokestatic:
         case JBinvokespecial:
#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
         case JBinvokestaticsplit:
         case JBinvokespecialsplit:
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
            {
            addSample = false; // never store these in the IProfiler hashtable
            caller = *(J9Method**)cursor;
            cursor += sizeof(J9Method *);
            if (fanInDisabled)
               break;
            J9ConstantPool* ramCP = J9_CP_FROM_METHOD(caller);
            uint16_t cpIndex = readU16(pc + 1);

#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
            if (JBinvokestaticsplit == cpIndex)
               cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG;
            if (JBinvokespecialsplit == cpIndex)
               cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG;
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
            J9Method * callee = jitGetJ9MethodUsingIndex(vmThread, ramCP, cpIndex);

            if ((javaVM->initialMethods.initialStaticMethod == callee) ||
                (javaVM->initialMethods.initialSpecialMethod == callee))
               {
               break;
               }

            uint32_t offset = (uint32_t) (pc - caller->bytecodes);
            findOrCreateMethodEntry(caller, callee , true ,offset);
            if (_compInfo->getLowPriorityCompQueue().isTrackingEnabled() &&  // is feature enabled?
                vmThread == _iprofilerThread) // only IProfiler thread is allowed to execute this
               {
               _compInfo->getLowPriorityCompQueue().tryToScheduleCompilation(vmThread, caller);
               }
            }
            break;
         case JBinvokevirtual:
         case JBinvokeinterface:
         case JBinvokeinterface2:

            receiverClass = *(J9Class**)cursor;
            cursor += sizeof(receiverClass);

            caller = *(J9Method**)cursor;
            cursor += sizeof(J9Method *);

            callee = *(J9Method**)cursor;
            cursor += sizeof(J9Method *);

            if (callee && javaVM->initialMethods.initialVirtualMethod != callee &&
               !fanInDisabled)
               {
               uint32_t offset = (uint32_t) (pc - caller->bytecodes);
               findOrCreateMethodEntry(caller, callee , true , offset);
               if (_compInfo->getLowPriorityCompQueue().isTrackingEnabled() &&  // is feature enabled?
                  vmThread == _iprofilerThread)  // only IProfiler thread is allowed to execute this
                  {
                  _compInfo->getLowPriorityCompQueue().tryToScheduleCompilation(vmThread, caller);
                  }
               }
            data = (intptrj_t)receiverClass;
            addSample = true;
            //bytecodeType = INTERFACE_BYTECODE;

            if ((data & 1) != 0) // If data is low tagged, clear the tag bit
               {
               data &= ~1;
               receiverClass = (J9Class*)data;
               }

            if (TEST_verbose || verboseReparse)
               {
               J9ROMClass* romClass = receiverClass->romClass;
               J9UTF8* name = J9ROMCLASS_CLASSNAME(romClass);
               j9tty_printf(PORTLIB, "pc=%p (invoke bc=%d) operand=%.*s(%p) cursor=%p\n", pc, *pc, (UDATA)J9UTF8_LENGTH(name), J9UTF8_DATA(name), receiverClass, cursorCopy);
               }
            break;
         case JBlookupswitch:
         case JBtableswitch:

            switchOperand = *(U_32*)cursor;
            cursor += sizeof(switchOperand);

            data = switchOperand;
            addSample = (profileFlag && !isClassLoadPhase) || TR::Options::_profileAllTheTime;


            //bytecodeType = SWITCH_BYTECODE;
            if (TEST_verbose || verboseReparse)
               {
               j9tty_printf(PORTLIB, "pc=%p (switch bc=%d) operand=%d cursor=%p\n", pc, *pc, switchOperand, cursorCopy);
               }
            break;
         default:
            if (!verboseReparse)
               {
#ifdef PROD_WITH_ASSUMES
               j9tty_printf(PORTLIB, "Error! Unrecognized bytecode (pc=%p, bc=%d) cursor %p.\n", pc, *pc, cursor);
               j9tty_printf(PORTLIB, "Will reparse the buffer verbosely\n");

               // recursively call parseBuffer to do another parse, but verbose this time
               parseBuffer(vmThread, dataStart, size, true);
#endif
               }
            TR_ASSERT(false, "Unrecognized bytecode in IProfiler buffer");
            Assert_JIT_unreachable();
            return 0;
         }

      if (addSample && !verboseReparse)
         {
         profilingSample ((uintptrj_t)pc, (uintptrj_t)data, true);
         records++;
         }
      }

   if (cursor != dataStart + size)
      {
      //j9tty_printf(PORTLIB, "Error! Parser overran buffer.\n");
      TR_ASSERT(false, "Iprofiler parser overran buffer");
      return 0;
      }
   _iprofilerNumRecords += records;

   return records;
   }


// This method should be called from Jitted code when it has a full buffer. It is called indirectly from
// _jitProfileParseBuffer, in JitRuntime
void TR_IProfiler::jitProfileParseBuffer(J9VMThread *currentThread)
   {
   PORT_ACCESS_FROM_PORT(_portLib);

   // First thing, the current thread's buffer could be null, in which case our job is easy
   if ( !currentThread->profilingBufferEnd )
      {
      U_8* newBuffer = (U_8*) j9mem_allocate_memory(_iprofilerBufferSize, J9MEM_CATEGORY_JIT);
      if ( !newBuffer )
         {
         j9tty_printf(PORTLIB, "Failed to create vmthread profiling buffer in jitProfilerParseBuffer.\n");
         return; //BAD!
         }

      currentThread->profilingBufferCursor = newBuffer;
      currentThread->profilingBufferEnd = newBuffer + _iprofilerBufferSize;
      return;
      }

   // Okay, so we have a non-null buffer
   U_8 *dataStart = currentThread->profilingBufferEnd - _iprofilerBufferSize;
   UDATA size = currentThread->profilingBufferCursor - dataStart;

   // If profiling is not enabled, we are just going to stomp the data we've collected and keep going
   if ( !isIProfilingEnabled() )
      {
      currentThread->profilingBufferCursor = dataStart;
      return;
      }

   // If we've made it here, we have a full buffer that we are going to look through
   incrementNumRequests();

   // If we have an iprofiler thread, then it might process the buffer for us asynchronously
   if ( !TR::Options::getCmdLineOptions()->getOption(TR_DisableIProfilerThread) && processProfilingBuffer(currentThread, dataStart, size) )
      {
      // If we're here, processProfilingBuffer has decided to process the buffer asynchronously
      return;
      }

   // Okay, so last possible option, we are going to process the buffer for ourselves
   parseBuffer(currentThread, dataStart, size);
   currentThread->profilingBufferCursor = dataStart;
   }


//--------------------- invalidateProfilingBuffers -------------------
// Called by GC on class unloading
// Invalidates profiling buffers on the waiting list, but also the buffer
// that might be currently under processing
// The method should not be called if profiling thread is not created
// The waiting time for GC thread is very small because iprofiler thread
// uses iprofilerMonitor in very short critical sections
//--------------------------------------------------------------------
void TR_IProfiler::invalidateProfilingBuffers() // called for class unloading
   {
   // GC has exclusive access, but needs to acquire _iprofilerMonitor
   if (!_iprofilerMonitor)
      return;
   _iprofilerMonitor->enter();
   if (!getIProfilerThread())
      {
      _iprofilerMonitor->exit();
      return;
      }
   IProfilerBuffer *specialProfilingBuffer = NULL;
   if (_crtProfilingBuffer && _crtProfilingBuffer->getSize() > 0)
      {
      // mark this buffer as invalid
      _crtProfilingBuffer->setIsInvalidated(true); // set with exclusive VM access
      }
   while (!_workingBufferList.isEmpty())
      {
      IProfilerBuffer *profilingBuffer = _workingBufferList.pop();
      if (profilingBuffer->getSize() > 0)
         {
         // attach the buffer to the buffer pool
         _freeBufferList.add(profilingBuffer);
         _numOutstandingBuffers--;
         }
      else // When the iprofiler thread sees this special buffer it will exit
         {
         specialProfilingBuffer = profilingBuffer;
         }
      }
   _workingBufferTail = NULL; // queue should be empty now

   if (specialProfilingBuffer)
      {
      // Put this buffer back
      _workingBufferList.add(specialProfilingBuffer);
      _workingBufferTail = specialProfilingBuffer;
      }
   _iprofilerMonitor->exit();
   }


static
void printCsInfo(CallSiteProfileInfo& csInfo, TR::Compilation* comp, void* tag = NULL, ::FILE* fout = stderr)
   {
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if (!csInfo.getClazz(i))
         continue;

      if (comp)
         {
         char *clazzSig = TR::Compiler->cls.classSignature(comp, (TR_OpaqueClassBlock*)csInfo.getClazz(i), comp->trMemory());
         fprintf(fout, "%p CLASS %d %p %s WEIGHT %d\n", tag, i, csInfo.getClazz(i), clazzSig, csInfo._weight[i]);
         }
      else
         {
         fprintf(fout, "%p CLASS %d %p WEIGHT %d\n", tag, i, csInfo.getClazz(i), csInfo._weight[i]);
         }
      fflush(fout);
      }
   }





//
void *
TR_IPHashedCallSite::operator new (size_t size) throw()
   {
   memoryConsumed += (int32_t)size;
   void *alloc = jitPersistentAlloc(size);
   return alloc;
   }

inline
uintptrj_t CallSiteProfileInfo::getClazz(int index)
   {
#if defined(J9VM_GC_COMPRESSED_POINTERS) //compressed references
   //support for convert code, when it is implemented, "uncompress"
   return (uintptrj_t)TR::Compiler->cls.convertClassOffsetToClassPtr((TR_OpaqueClassBlock *)_clazz[index]);
#else
   return (uintptrj_t)_clazz[index]; //things are just stored as regular pointers otherwise
#endif //J9VM_GC_COMPRESSED_POINTERS
   }

inline
void CallSiteProfileInfo::setClazz(int index, uintptrj_t clazzPointer)
   {
#if defined(J9VM_GC_COMPRESSED_POINTERS) //compressed references
   //support for convert code, when it is implemented, do compression
   TR_OpaqueClassBlock * compressedOffset = J9JitMemory::convertClassPtrToClassOffset((J9Class *)clazzPointer); //compressed 32bit pointer
   //if we end up with something in the top 32bits, our compression is no good...
   TR_ASSERT((!(0xFFFFFFFF00000000 & (uintptrj_t)compressedOffset)), "Class pointer contains bits in the top word. Pointer given: %p Compressed: %p", clazzPointer, compressedOffset);
   _clazz[index] = (uint32_t)((uintptrj_t)compressedOffset); //ditch the top zeros
#else
   _clazz[index] = (uintptrj_t)clazzPointer;
#endif //J9VM_GC_COMPRESSED_POINTERS
   }


// Supporting code for dumping IProfiler data to stderr to track possible 
// performance issues due to insuficient or wrong IProfiler info
// Code is currently inactive. To actually use one must issue
// iProfiler->dumpIPBCDataCallGraph(vmThread)
// in some part of the code (typically at shutdown time) 
class TR_AggregationHT
   {
   public:
      TR_PERSISTENT_ALLOC(TR_Memory::IProfiler)

         class TR_CGChainedEntry
         {
         TR_CGChainedEntry    *_next; // for chaining
         TR_IPBCDataCallGraph *_CGentry;
         public:
            TR_CGChainedEntry(TR_IPBCDataCallGraph *entry) : _next(NULL), _CGentry(entry) { }
            uintptr_t getPC() const { return _CGentry->getPC(); }
            TR_CGChainedEntry *getNext() const { return _next; }
            TR_IPBCDataCallGraph *getCGData() const { return _CGentry; }
            void setNext(TR_CGChainedEntry *next) { _next = next; }
         };
      class TR_AggregationHTNode
         {
         TR_AggregationHTNode *_next; // for chaning
         J9ROMMethod *_romMethod; // this is the key
         J9ROMClass  *_romClass;
         TR_CGChainedEntry *_IPData;
         public:
            TR_AggregationHTNode(J9ROMMethod *romMethod, J9ROMClass *romClass, TR_IPBCDataCallGraph *entry) : _next(NULL), _romMethod(romMethod), _romClass(romClass)
               {
               _IPData = new (PERSISTENT_NEW) TR_CGChainedEntry(entry);
               }
            ~TR_AggregationHTNode()
               {
               TR_CGChainedEntry *entry = getFirstCGEntry();
               while (entry)
                  {
                  TR_CGChainedEntry *nextEntry = entry->getNext();
                  jitPersistentFree(entry);
                  entry = nextEntry;
                  }
               }
            TR_AggregationHTNode *getNext() const { return _next; }
            J9ROMMethod *getROMMethod() const { return _romMethod; }
            J9ROMClass *getROMClass() const { return _romClass; }
            TR_CGChainedEntry *getFirstCGEntry() const { return _IPData; }
            void setNext(TR_AggregationHTNode *next) { _next = next; }
            void setFirstCGEntry(TR_CGChainedEntry *e) { _IPData = e; }
         };
      struct SortingPair
         {
         char *_methodName;
         TR_AggregationHT::TR_AggregationHTNode *_IPdata;
         };

      TR_AggregationHT(size_t sz) : _sz(sz), _numTrackedMethods(0)
         {
         _backbone = new (PERSISTENT_NEW) TR_AggregationHTNode*[sz];
         if (!_backbone) // OOM
            {
            _sz = 0;
            }
         else
            {
            for (size_t i = 0; i < sz; i++)
               _backbone[i] = NULL;
            }
         }
      ~TR_AggregationHT()
         {
         for (int32_t bucket = 0; bucket < _sz; bucket++)
            {
            TR_AggregationHTNode *node = _backbone[bucket];
            while (node)
               {
               TR_AggregationHTNode *nextNode = node->getNext();
               node->~TR_AggregationHTNode();
               jitPersistentFree(node);
               node = nextNode;
               }
            }
         jitPersistentFree(_backbone);
         }
      size_t hash(J9ROMMethod *romMethod) { return (((uintptr_t)romMethod) >> 3) % _sz; }
      size_t getSize() const { return _sz; }
      size_t numTrackedMethods() const { return _numTrackedMethods; }
      TR_AggregationHTNode* getBucket(size_t i) const { return _backbone[i]; }
      void add(J9ROMMethod *romMethod, J9ROMClass *romClass, TR_IPBCDataCallGraph *cgEntry)
         {
         size_t index = hash(romMethod);
         // search the bucket for matching romMethod
         TR_AggregationHTNode *crtMethodNode = _backbone[index];
         for (; crtMethodNode; crtMethodNode = crtMethodNode->getNext())
            if (crtMethodNode->getROMMethod() == romMethod)
               {
               // Add a new bc data point to the method entry we found; keep it sorted by pc
               TR_CGChainedEntry *newEntry = new (PERSISTENT_NEW) TR_CGChainedEntry(cgEntry);
               if (!newEntry) // OOM
                  {
                  fprintf(stderr, "Cannot allocated memory. Incomplete info will be printed.\n");
                  return;
                  }
               TR_CGChainedEntry *crtEntry = crtMethodNode->getFirstCGEntry();
               TR_CGChainedEntry *prevEntry = NULL;
               while (crtEntry)
                  {
                  // Ideally we should not have two entries with the same pc in the
                  // Iprofiler HT (the pc is the key in the HT). However, due to the
                  // fact that we don't acquire any locks, such rare occurrences are
                  // possible. We should ignore any such  events.
                  if (crtEntry->getPC() == cgEntry->getPC())
                     {
                     fprintf(stderr, "We cannot find the same PC twice");
                     return;
                     }
                  if (crtEntry->getPC() > cgEntry->getPC())
                     break; // found the position   
                  prevEntry = crtEntry;
                  crtEntry = crtEntry->getNext();
                  }
               if (prevEntry)
                  prevEntry->setNext(newEntry);
               else
                  crtMethodNode->setFirstCGEntry(newEntry);
               newEntry->setNext(crtEntry);
               break;
               }
         // If my romMethod is not already in the HT let's add it
         if (!crtMethodNode)
            {
            // Add a new entry at the beginning
            TR_AggregationHTNode *newMethodNode = new (PERSISTENT_NEW) TR_AggregationHTNode(romMethod, romClass, cgEntry);
            if (!newMethodNode || !newMethodNode->getFirstCGEntry()) // OOM
               {
               fprintf(stderr, "Cannot allocated memory. Incomplete info will be printed.\n");
               return;
               }
            newMethodNode->setNext(_backbone[index]);
            _backbone[index] = newMethodNode;
            _numTrackedMethods++;
            }
         }
      void sortByNameAndPrint(TR_J9VMBase *fe);
   private:
      size_t _sz;
      size_t _numTrackedMethods;
      TR_AggregationHTNode** _backbone;
   };

// Callback for qsort to sort by methodName
int compareByMethodName(const void *a, const void *b)
   {
   return strcmp(((TR_AggregationHT::SortingPair *)a)->_methodName, ((TR_AggregationHT::SortingPair *)b)->_methodName);
   }

void TR_AggregationHT::sortByNameAndPrint(TR_J9VMBase *fe)
   {
   // Scan the aggregationTable and convert from romMethod to methodName so that
   // we can sort and print the information
   fprintf(stderr, "Creating the sorting array ...\n");
   SortingPair *sortingArray = (SortingPair *)jitPersistentAlloc(sizeof(SortingPair) * numTrackedMethods());
   if (!sortingArray)
      {
      fprintf(stderr, "Cannot allocate sorting array. Bailing out.\n");
      return;
      }
   memset(sortingArray, 0, sizeof(SortingPair) * numTrackedMethods());
   size_t methodIndex = 0;
   for (int32_t bucket = 0; bucket < getSize(); bucket++)
      {
      TR_AggregationHTNode *node = getBucket(bucket);
      for (; node; node = node->getNext())
         {
         // A node contains information about all PCs related to a single method
         J9ROMMethod* romMethod = node->getROMMethod();
         J9ROMClass*  romClass = node->getROMClass();
         // Build the name of the method
         J9UTF8* className = J9ROMCLASS_CLASSNAME(romClass);
         J9UTF8* name = J9ROMMETHOD_NAME(romMethod);
         J9UTF8* signature = J9ROMMETHOD_SIGNATURE(romMethod);
         size_t len = J9UTF8_LENGTH(className) + J9UTF8_LENGTH(name) + J9UTF8_LENGTH(signature) + 2;
         char *wholeName = (char *)jitPersistentAlloc(len);
         // If memory cannot be allocated, break
         if (!wholeName)
            {
            fprintf(stderr, "Cannot allocate memory. Incomplete data will be printed.\n");
            break;
            }
         sprintf(wholeName, "%.*s.%.*s%.*s",
            J9UTF8_LENGTH(className), utf8Data(className),
            J9UTF8_LENGTH(name), utf8Data(name),
            J9UTF8_LENGTH(signature), utf8Data(signature));
         // Store info into sorting array
         sortingArray[methodIndex]._methodName = wholeName;
         sortingArray[methodIndex]._IPdata = node;
         methodIndex++;
         }
      }
   // Sort my method name
   fprintf(stderr, "Sorting ...\n");
   qsort(sortingArray, numTrackedMethods(), sizeof(*sortingArray), compareByMethodName);

   // Print sorted info
   fprintf(stderr, "Printing ...\n");
   for (size_t i = 0; i < numTrackedMethods(); ++i)
      {
      fprintf(stderr, "Method: %s\n", sortingArray[i]._methodName);
      J9ROMMethod *romMethod = sortingArray[i]._IPdata->getROMMethod();
      TR_CGChainedEntry *cgEntry = sortingArray[i]._IPdata->getFirstCGEntry();
      // Iterate through bytecodes with info
      for (; cgEntry; cgEntry = cgEntry->getNext())
         {
         TR_IPBCDataCallGraph *ipbcCGData = cgEntry->getCGData();
         U_8* pc = (U_8*)ipbcCGData->getPC();

         size_t bcOffset = pc - (U_8*)J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
         fprintf(stderr, "\tOffset %u\t", bcOffset);
         switch (*pc)
            {
            case JBinvokestatic:     fprintf(stderr, "JBinvokestatic\n"); break;
            case JBinvokespecial:    fprintf(stderr, "JBinvokespecial\n"); break;
            case JBinvokeinterface:  fprintf(stderr, "JBinvokeinterface\n"); break;
            case JBinvokeinterface2: fprintf(stderr, "JBinvokeinterface2\n"); break;
            case JBinvokevirtual:    fprintf(stderr, "JBinvokevirtual\n"); break;
            case JBinstanceof:       fprintf(stderr, "JBinstanceof\n"); break;
            case JBcheckcast:        fprintf(stderr, "JBcheckcast\n"); break;
            default:                 fprintf(stderr, "JBunknown\n");
            }
         CallSiteProfileInfo *cgData = ipbcCGData->getCGData();
         for (int j = 0; j < NUM_CS_SLOTS; j++)
            {
            if (cgData->getClazz(j))
               {
               int32_t len;
               const char * s = fe->getClassNameChars((TR_OpaqueClassBlock*)cgData->getClazz(j), len);
               fprintf(stderr, "\t\tW:%4u\tM:%p\t%.*s\n", cgData->_weight[j], cgData->getClazz(j), len, s);
               }
            }
         fprintf(stderr, "\t\tW:%4u\n", cgData->_residueWeight);
         }
      }
   // Free the memory we allocated
   for (size_t i = 0; i < numTrackedMethods(); i++)
      if (sortingArray[i]._methodName)
         jitPersistentFree(sortingArray[i]._methodName);
   jitPersistentFree(sortingArray);
   }



// This method can be used to print to stderr in readable format all the IPBCDataCallGraph
// info from IProfiler (i.e. all sort of calls, checkcasts and instanceofs)
// This method will be executing on a single thread and it will acquire VM access as needed
// to prevent the GC from modifying the BC hashtable as we walk.
// Application threads may add new data to the IProfiler hashtable, but this is not an impediment.
// Temporary data structures will be allocated using persistent memory which will be dealocated
// at the end.
// Parameter: the vmThread it is executing on.
void TR_IProfiler::dumpIPBCDataCallGraph(J9VMThread* vmThread)
   {
   fprintf(stderr, "Dumping info ...\n");
   TR_AggregationHT aggregationHT(BC_HASH_TABLE_SIZE);
   if (aggregationHT.getSize() == 0) // OOM
      {
      fprintf(stderr, "Cannot allocate memory. Bailing out.\n");
      return;
      }

   // Need to have VM access to block GC from invalidating entries
   // Application threads may still add data, but that is safe
   // Ideally we should use  TR::VMAccessCriticalSection dumpInfoCS(fe);
   // here, but this construct wants asserts that an application thread must have vmAccess.
   // This might not be true at shutdown when dumpInfo is likely to be called
   //
   bool haveAcquiredVMAccess = false;
   if (!(vmThread->publicFlags &  J9_PUBLIC_FLAGS_VM_ACCESS)) // I don't already have VM access
      {
      acquireVMAccessNoSuspend(vmThread);
      haveAcquiredVMAccess = true;
      }

   J9JavaVM *javaVM = vmThread->javaVM;
   J9InternalVMFunctions *vmFunctions = javaVM->internalVMFunctions;
   TR_J9VMBase * fe = TR_J9VMBase::get(javaVM->jitConfig, vmThread);

   fprintf(stderr, "Aggregating per method ...\n");
   for (int32_t bucket = 0; bucket < BC_HASH_TABLE_SIZE; bucket++)
      {
      //fprintf(stderr, "Looking at bucket %d\n", bucket);
      for (TR_IPBytecodeHashTableEntry *entry = _bcHashTable[bucket]; entry; entry = entry->getNext())
         {
         // Skip invalid entries
         if (entry->isInvalid() || invalidateEntryIfInconsistent(entry))
            continue;
         TR_IPBCDataCallGraph *cgEntry = entry->asIPBCDataCallGraph();
         if (cgEntry)
            {
            // Get the pc and find the method this pc belongs to
            U_8* pc = (U_8*)cgEntry->getPC();
            //fprintf(stderr, "\tInspecting pc=%p\n", pc);
            J9ClassLoader* loader;
            J9ROMClass * romClass = vmFunctions->findROMClassFromPC(vmThread, (UDATA)pc, &loader);
            if (romClass)
               {
               //J9ROMMethod * romMethod = vmFunctions->findROMMethodInROMClass(vmThread, romClass, (UDATA)pc, NULL);
               J9ROMMethod *currentMethod = J9ROMCLASS_ROMMETHODS(romClass);
               J9ROMMethod *desiredMethod = NULL;
               //fprintf(stderr, "Scanning %u romMethods...\n", romClass->romMethodCount);
               for (U_32 i = 0; i < romClass->romMethodCount; i++)
                  {
                  if (((UDATA)pc >= (UDATA)currentMethod) && ((UDATA)pc < (UDATA)J9_BYTECODE_END_FROM_ROM_METHOD(currentMethod)))
                     {
                     // found the method
                     desiredMethod = currentMethod;
                     break;
                     }
                  currentMethod = nextROMMethod(currentMethod);
                  }

               if (desiredMethod)
                  {
                  // Add the information to the aggregationTable
                  aggregationHT.add(desiredMethod, romClass, cgEntry);
                  }
               else
                  {
                  fprintf(stderr, "pc=%p does not belong to romMethod range\n", pc);
                  }
               }
            else
               {
               fprintf(stderr, "pc=%p does not belong to a romMethod\n", pc);
               }
            }
         }
      }
   aggregationHT.sortByNameAndPrint(fe);
   if (haveAcquiredVMAccess)
      releaseVMAccessNoSuspend(vmThread);

   fprintf(stderr, "Finished dumping info\n");
   }

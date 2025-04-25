/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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
#include "VMHelpers.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "compile/Compilation.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/PersistentInfo.hpp"
#include "env/jittypes.h"
#include "env/SystemSegmentProvider.hpp"
#include "env/VerboseLog.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/CriticalSection.hpp"
#include "infra/Monitor.hpp"
#include "infra/MonitorTable.hpp"
#include "infra/SimpleRegex.hpp"
#include "runtime/J9Runtime.hpp"
#include "runtime/J9VMAccess.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "control/CompilationRuntime.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/J9JitMemory.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "env/j9method.h"
#include "env/ut_j9jit.h"
#include "ilgen/J9ByteCode.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "runtime/IProfiler.hpp"
#include "runtime/J9Profiler.hpp"
#include "omrformatconsts.h"
#if defined(J9VM_OPT_CRIU_SUPPORT)
#include "runtime/CRRuntime.hpp"
#endif /* if defined(J9VM_OPT_CRIU_SUPPORT) */

#undef  IPROFILER_CONTENDED_LOCKING
#define ALLOC_HASH_TABLE_SIZE 1201
#define TEST_verbose 0
#define TEST_callsite 0
#define TEST_statistics 0
#define TEST_inlining 0
#define TEST_disableCSI 0
#undef PERSISTENCE_VERBOSE

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

TR::PersistentAllocator * TR_IProfiler::_allocator = NULL;

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

   static const char *cRegex = feGetEnv ("TR_printIfRegex");
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
   _history =  (TR_ReadSampleRequestsStats *) TR_IProfiler::allocator()->allocate(historyBufferSize * sizeof(TR_ReadSampleRequestsStats), std::nothrow);
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



uintptr_t
TR_IProfiler::createBalancedBST(uintptr_t *pcEntries, int32_t low, int32_t high, uintptr_t memChunk,
                                TR_J9SharedCache *sharedCache)
   {
   if (high < low)
      return 0;

   TR_IPBCDataStorageHeader * storage = (TR_IPBCDataStorageHeader *) memChunk;
   int32_t middle = (high+low)/2;
   TR_IPBytecodeHashTableEntry *entry = profilingSample (pcEntries[middle], 0, false);
   uint32_t bytes = entry->getBytesFootprint();
   entry->createPersistentCopy(sharedCache, storage, _compInfo->getPersistentInfo());

   uintptr_t leftChild = createBalancedBST(pcEntries, low, middle-1,
                                            memChunk + bytes, sharedCache);

   if (leftChild)
      {
      TR_ASSERT(bytes < 1 << 8, "Error storing iprofile information: left child too far away"); // current size of left child
      storage->left = bytes;
      }

   uintptr_t rightChild = createBalancedBST(pcEntries, middle+1, high,
                                             memChunk + bytes + leftChild, sharedCache);
   if (rightChild)
      {
      TR_ASSERT(bytes + leftChild < 1 << 16, "Error storing iprofile information: right child too far away"); // current size of right child
      storage->right = bytes+leftChild;
      }

   return bytes + leftChild + rightChild;
   }

uint32_t
TR_IProfiler::walkILTreeForEntries(uintptr_t *pcEntries, uint32_t &numEntries, TR_J9ByteCodeIterator *bcIterator, TR_OpaqueMethodBlock *method, TR::Compilation *comp,
                                   vcount_t visitCount, int32_t callerIndex, TR_BitVector *BCvisit, bool &abort)
   {
   abort = false;
   uint32_t bytesFootprint = 0;
   TR_J9ByteCode bc = bcIterator->first(), nextBC;
   for (; bc != J9BCunknown; bc = bcIterator->next())
      {
      uint32_t bci = bcIterator->bcIndex();

      if (bci < TR::Compiler->mtd.bytecodeSize(method) && !BCvisit->isSet(bci))
         {
         uintptr_t thisPC = getSearchPC (method, bci, comp);
         TR_IPBytecodeHashTableEntry *entry = profilingSample (thisPC, 0, false);
         BCvisit->set(bci);

         if (entry && !invalidateEntryIfInconsistent(entry))
            {
            int32_t i;
            // now I check if it can be persisted, lock it, and add it to my list.
            uint32_t canPersist = entry->canBePersisted(comp->fej9()->sharedCache(), _compInfo->getPersistentInfo());
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
                     // In some corner cases of invoke interface, we may come across the same entry
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

bool
TR_IProfiler::elgibleForPersistIprofileInfo(TR::Compilation *comp) const
   {
      return (TR::Options::sharedClassCache() &&
             !comp->getOption(TR_DisablePersistIProfile) &&
             isIProfilingEnabled());
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

   if (elgibleForPersistIprofileInfo(comp) &&
      (!SCfull || !comp->getOption(TR_DisableUpdateJITBytesSize)))
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)_vm;
      J9SharedClassConfig * scConfig = _compInfo->getJITConfig()->javaVM->sharedClassConfig;

      uintptr_t methodSize = (uintptr_t ) TR::Compiler->mtd.bytecodeSize(method);
      uintptr_t methodStart = (uintptr_t ) TR::Compiler->mtd.bytecodeStart(method);

      uint32_t bytesFootprint = 0;
      uint32_t numEntries = 0;

      // Allocate memory for every possible node in this method
      //
      J9ROMMethod * romMethod = comp->fej9()->getROMMethodFromRAMMethod((J9Method *)method);

      TR::Options * optionsJIT = TR::Options::getJITCmdLineOptions();
      TR::Options * optionsAOT = TR::Options::getAOTCmdLineOptions();

      int32_t count = getCount(romMethod, optionsJIT, optionsAOT);
      int32_t currentCount = resolvedMethod->getInvocationCount();

      // can only persist profile info if the method is in the shared cache
      if (comp->fej9()->sharedCache()->isROMMethodInSharedCache(romMethod))
        {
         TR_ASSERT(comp->fej9()->sharedCache()->isPtrToROMClassesSectionInSharedCache((void *)methodStart), "bytecodes not in shared cache");
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
               fprintf(stdout, "Persist: %s count %" OMR_PRId32 " Compiling %s\n", methodSig, count, comp->signature());
               }

            vcount_t visitCount = comp->incVisitCount();
            TR_BitVector *BCvisit = new (comp->trStackMemory()) TR_BitVector(TR::Compiler->mtd.bytecodeSize(method), comp->trMemory(), stackAlloc);
            bool abort=false;
            TR_J9ByteCodeIterator bci(0, static_cast<TR_ResolvedJ9Method *> (resolvedMethod), static_cast<TR_J9VMBase *> (comp->fej9()), comp);
            uintptr_t * pcEntries = (uintptr_t *) comp->trMemory()->allocateMemory(sizeof(uintptr_t) * bci.maxByteCodeIndex(), stackAlloc);
            bytesFootprint += walkILTreeForEntries(pcEntries, numEntries, &bci, method, comp, visitCount, -1, BCvisit, abort);

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
                  intptr_t bytes = createBalancedBST(pcEntries, 0, numEntries-1, (uintptr_t) memChunk, comp->fej9()->sharedCache());
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
                   !comp->getOption(TR_DisableUpdateJITBytesSize))
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
         _STATS_methodNotPersisted_classNotInSCC++;
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
   return _allocator->allocate(size, std::nothrow);
   }

TR::PersistentAllocator *
TR_IProfiler::createPersistentAllocator(J9JITConfig *jitConfig)
   {
   // Create a new persistent allocator, dedicated to IProfiler
   uint32_t memoryType = 0;
#ifdef LINUX
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableIProfilerDataDisclaiming))
      {
      PORT_ACCESS_FROM_JITCONFIG(jitConfig);
      memoryType |= MEMORY_TYPE_VIRTUAL; // Force the usage of mmap for allocation
      TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
      if (!TR::Options::getCmdLineOptions()->getOption(TR_DisclaimMemoryOnSwap) || compInfo->isSwapMemoryDisabled())
         {
         memoryType |= MEMORY_TYPE_DISCLAIMABLE_TO_FILE;
         }
      }
#endif // LINUX
   TR::PersistentAllocatorKit kit(1 << 20/*1 MB*/, *(jitConfig->javaVM), memoryType);
   return new (TR::Compiler->rawAllocator) TR::PersistentAllocator(kit);
   }

TR_IProfiler *
TR_IProfiler::allocate(J9JITConfig *jitConfig)
   {
   setAllocator(createPersistentAllocator(jitConfig));
   TR_IProfiler * profiler = new TR_IProfiler(jitConfig);
   return profiler;
}

TR_IProfiler::TR_IProfiler(J9JITConfig *jitConfig)
   : _isIProfilingEnabled(true),
     _valueProfileMethod(NULL), _lightHashTableMonitor(0), _allowedToGiveInlinedInformation(true),
     _globalAllocationCount (0), _maxCallFrequency(0), _iprofilerThread(0), _iprofilerOSThread(NULL),
     _workingBufferTail(NULL), _numOutstandingBuffers(0), _numRequests(1), _numRequestsDropped(0), _numRequestsSkipped(0),
     _numRequestsHandedToIProfilerThread(0), _iprofilerMonitor(NULL),
     _crtProfilingBuffer(NULL), _iprofilerNumRecords(0), _numMethodHashEntries(0),
     _iprofilerThreadLifetimeState(TR_IprofilerThreadLifetimeStates::IPROF_THR_NOT_CREATED)
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

#if defined(J9VM_OPT_JITSERVER)
   if (_compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      _hashTableMonitor = NULL;
      _bcHashTable = NULL;
#if defined(EXPERIMENTAL_IPROFILER)
      _allocHashTable =  NULL;
#endif
      _methodHashTable = NULL;
      _readSampleRequestsHistory = NULL;
      }
   else
#endif
      {
      // initialize the monitors
      _hashTableMonitor = TR::Monitor::create("JIT-InterpreterProfilingMonitor");
      // bytecode hashtable
      _bcHashTable = (TR_IPBytecodeHashTableEntry**)_allocator->allocate(TR::Options::_iProfilerBcHashTableSize*sizeof(TR_IPBytecodeHashTableEntry*), std::nothrow);
      if (_bcHashTable != NULL)
         memset(_bcHashTable, 0, TR::Options::_iProfilerBcHashTableSize*sizeof(TR_IPBytecodeHashTableEntry*));
      else
         _isIProfilingEnabled = false;

#if defined(EXPERIMENTAL_IPROFILER)
      _allocHashTable = (TR_IPBCDataAllocation**)_allocator->allocate(ALLOC_HASH_TABLE_SIZE*sizeof(TR_IPBCDataAllocation*), std::nothrow);
      if (_allocHashTable != NULL)
         memset(_allocHashTable, 0, ALLOC_HASH_TABLE_SIZE*sizeof(TR_IPBCDataAllocation*));
#endif
      _methodHashTable = (TR_IPMethodHashTableEntry **) _allocator->allocate(TR::Options::_iProfilerMethodHashTableSize * sizeof(TR_IPMethodHashTableEntry *), std::nothrow);
      if (_methodHashTable != NULL)
         memset(_methodHashTable, 0, TR::Options::_iProfilerMethodHashTableSize * sizeof(TR_IPMethodHashTableEntry *));
      _readSampleRequestsHistory = (TR_ReadSampleRequestsHistory *) _allocator->allocate(sizeof (TR_ReadSampleRequestsHistory), std::nothrow);
      if (!_readSampleRequestsHistory || !_readSampleRequestsHistory->init(TR::Options::_iprofilerFailHistorySize))
         {
         _isIProfilingEnabled = false;
         }
      }
   }


void
TR_IProfiler::shutdown()
   {
   _isIProfilingEnabled = false; // This is the only instance where we disable the profiler

   }

static uint16_t cpIndexFromPC(uintptr_t pc) { return *((uint16_t*)(pc + 1)); }
static U_32 vftOffsetFromPC (J9Method *method, uintptr_t pc)
   {
   uint16_t cpIndex = cpIndexFromPC(pc);
   TR_ASSERT(cpIndex != (uint16_t)~0, "cpIndex shouldn't be -1");
   J9RAMConstantPoolItem *literals = (J9RAMConstantPoolItem *) J9_CP_FROM_METHOD(method);
   UDATA vTableSlot = ((J9RAMVirtualMethodRef *)literals)[cpIndex].methodIndexAndArgCount >> 8;
   TR_ASSERT(vTableSlot, "vTableSlot called for unresolved method");

   return (U_32) (TR::Compiler->vm.getInterpreterVTableOffset() - vTableSlot);
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
TR_IProfiler::bcHash(uintptr_t pc)
   {
   return (int32_t)((pc & 0x7FFFFFFF) % TR::Options::_iProfilerBcHashTableSize);
   }

inline int32_t
TR_IProfiler::allocHash(uintptr_t pc)
   {
   return (int32_t)((pc & 0x7FFFFFFF) % ALLOC_HASH_TABLE_SIZE);
   }

inline int32_t
TR_IProfiler::methodHash(uintptr_t data)
   {
   return (int32_t)((data & 0x7FFFFFFF) % TR::Options::_iProfilerMethodHashTableSize);
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
          || (byteCode == JBinvokestaticsplit)
          || (byteCode == JBinvokespecialsplit)
          );
   }

static void
getSwitchSegmentDataAndCount(uint64_t segment, uint32_t *segmentData, uint32_t *segmentCount)
   {
   // each segment is 8 bytes long and contains
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

static int32_t nextSwitchValue(uintptr_t & pc)
   {
   int32_t value = *((int32_t *)pc);
   pc += 4;
   return value;
   }


static void lookupSwitchIndexForValue(uintptr_t startPC, int32_t value, int32_t & target, int32_t & index)
   {
   uintptr_t pc = (startPC + 4) & ((uintptr_t) -4) ;

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

static void tableSwitchIndexForValue(uintptr_t startPC, int32_t value, int32_t & target, int32_t & index)
   {
   uintptr_t pc = (startPC + 4) & ((uintptr_t) -4) ;

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

static int32_t lookupSwitchBytecodeToOffset (uintptr_t startPC, int32_t index)
   {
   uintptr_t pc = (startPC + 4) & ((uintptr_t) -4) ;

   if (index > 0) pc += 8*index+4;

   return nextSwitchValue(pc);
   }

static int32_t tableSwitchBytecodeToOffset (uintptr_t startPC, int32_t index)
   {
   uintptr_t pc = (startPC + 4) & ((uintptr_t) -4) ;

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
TR_IProfiler::addSampleData(TR_IPBytecodeHashTableEntry *entry, uintptr_t data, bool isRIData, uint32_t freq)
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
            uintptr_t existingData = entry->getData();
            if ((existingData & 0xFFFF0000) == 0xFFFF0000)
               {
               // Overflow detected; divide both counters by 2
               existingData >>= 1;
               existingData &= 0x7FFF7FFF;
               }

            entry->setData(existingData + (1<<16));
            }
         else
            {
            uintptr_t existingData = entry->getData();
            if ((existingData & 0x0000FFFF) == 0x0000FFFF)
               {
               // Overflow detected; divide both counters by 2
               existingData >>= 1;
               existingData &= 0x7FFF7FFF;
               }

            entry->setData(existingData + 1);
            }
         return true;
      case JBinvokestatic:
      case JBinvokespecial:
      case JBinvokestaticsplit:
      case JBinvokespecialsplit:
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
TR_IProfiler::searchForSample(uintptr_t pc, int32_t bucket)
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
TR_IProfiler::searchForAllocSample(uintptr_t pc, int32_t bucket)
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
TR_IProfiler::findOrCreateEntry(int32_t bucket, uintptr_t pc, bool addIt)
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

   // While the entry is being allocated, another thread could have added an entry with the same PC.
   // If that happened, it's likely that the duplicate entry is at the head of this list.
   // Check to see if that's the case. This technique does not eliminate the race completely
   // but catches most of duplicate situations.
   TR_IPBytecodeHashTableEntry *headEntry = _bcHashTable[bucket];
   if (headEntry && headEntry->getPC() == pc)
      {
      delete entry; // Newly allocated entry is not needed
      return headEntry;
      }

   entry->setNext(_bcHashTable[bucket]);
   FLUSH_MEMORY(TR::Compiler->target.isSMP());
   _bcHashTable[bucket] = entry;

   return entry;
   }

TR_IPBCDataAllocation *
TR_IProfiler::findOrCreateAllocEntry(int32_t bucket, uintptr_t pc, bool addIt)
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
   int32_t bucket = methodHash((uintptr_t)calleeMethod);
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
      entry = (TR_IPMethodHashTableEntry *)_allocator->allocate(sizeof(TR_IPMethodHashTableEntry), std::nothrow);
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
         _numMethodHashEntries++;
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
         TR_IPMethodData* newCaller = (TR_IPMethodData*)TR_IProfiler::allocator()->allocate(sizeof(TR_IPMethodData), std::nothrow);
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
TR_IProfiler::searchForPersistentSample (TR_IPBCDataStorageHeader  *root, uintptr_t pc)
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
TR_IProfiler::persistentProfilingSample(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex,
                                        TR::Compilation *comp, bool *methodProfileExistsInSCC)
   {
   if (TR::Options::sharedClassCache())        // shared classes must be enabled
      {
      J9SharedClassConfig *scConfig = _compInfo->getJITConfig()->javaVM->sharedClassConfig;
      void *methodStart = (void *)TR::Compiler->mtd.bytecodeStart(method);

      // can only persist profile info if the method is in the shared cache
      if (!comp->fej9()->sharedCache()->isPtrToROMClassesSectionInSharedCache(methodStart))
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
      J9ROMMethod * romMethod = comp->fej9()->getROMMethodFromRAMMethod((J9Method *)method);
      IDATA dataIsCorrupt;

      TR_IPBCDataStorageHeader *store = (TR_IPBCDataStorageHeader *)scConfig->findAttachedData(vmThread, romMethod, &descriptor, &dataIsCorrupt);
      if (store != (TR_IPBCDataStorageHeader *)descriptor.address)  // a stronger check, as found can be error value
         return NULL;

      *methodProfileExistsInSCC = true;
      // Compute the pc we are interested in
      uintptr_t pc = getSearchPC(method, byteCodeIndex, comp);
      store = searchForPersistentSample(store, (uintptr_t)comp->fej9()->sharedCache()->offsetInSharedCacheFromPtrToROMClassesSection((void *)pc));

      if (TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableIprofilerChanges) || TR::Options::getJITCmdLineOptions()->getOption(TR_EnableIprofilerChanges))
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
               newEntry->loadFromPersistentCopy(store, comp);
            return newEntry;
            }
         }
      else
         {
         if (store)
            {
            // Create a new IProfiler hashtable entry and copy the data from the SCC
            TR_IPBytecodeHashTableEntry *newEntry = findOrCreateEntry(bcHash(pc), pc, true);
            newEntry->loadFromPersistentCopy(store, comp);
            return newEntry;
            }
         }
      return NULL;
      }
   return NULL;
   }

TR_IPBCDataStorageHeader*
TR_IProfiler::persistentProfilingSample(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex,
                                        TR::Compilation *comp, bool *methodProfileExistsInSCC, TR_IPBCDataStorageHeader *store)
   {
   if (TR::Options::sharedClassCache())        // shared classes must be enabled
      {
      if (!store)
         return NULL;
      J9SharedClassConfig *scConfig = _compInfo->getJITConfig()->javaVM->sharedClassConfig;
      void *methodStart = (void *) TR::Compiler->mtd.bytecodeStart(method);

      // can only persist profile info if the method is in the shared cache
      if (!comp->fej9()->sharedCache()->isPtrToROMClassesSectionInSharedCache(methodStart))
         return NULL;

      *methodProfileExistsInSCC = true;
      void *pc = (void *)getSearchPC(method, byteCodeIndex, comp);
      store = searchForPersistentSample(store, (uintptr_t)comp->fej9()->sharedCache()->offsetInSharedCacheFromPtrToROMClassesSection(pc));
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
   J9ROMMethod * romMethod = comp->fej9()->getROMMethodFromRAMMethod((J9Method *)method);
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
TR_IProfiler::profilingSample (TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation *comp, uintptr_t data, bool addIt)
   {
   // Find the bytecode pc we are interested in
   uintptr_t pc = getSearchPC (method, byteCodeIndex, comp);

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
      static bool preferHashtableData = comp->getOption(TR_DisableAOTWarmRunThroughputImprovement);
      if (entry) // bytecode pc is present in the hashtable
         {
         // Increment the number of requests for samples
         // Exclude JBinvokestatic and JBinvokespecial because they are not tracked by interpreter
         if (bytecode != JBinvokestatic
            && bytecode != JBinvokespecial
            && bytecode != JBinvokestaticsplit
            && bytecode != JBinvokespecialsplit
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
                  && bytecode != JBinvokestaticsplit
                  && bytecode != JBinvokespecialsplit
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
                  && bytecode != JBinvokestaticsplit
                  && bytecode != JBinvokespecialsplit
                  )
                  _readSampleRequestsHistory->incTotalReadSampleRequests();
#ifdef PERSISTENCE_VERBOSE
               fprintf(stderr, "Entry from SCC\n");
#endif
               if (!entry->hasData())
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
               persistentEntryStore = persistentProfilingSample(method, byteCodeIndex, comp, &methodProfileExistsInSCC, persistentEntryStore);
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
               persistentEntry->loadFromPersistentCopy(persistentEntryStore, comp);
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
         if(!currentEntry || !currentEntry->hasData())
            {
            if (persistentEntry && persistentEntry->hasData())
               {
               _STATS_IPEntryChoosePersistent++;
               currentEntry = findOrCreateEntry(bcHash(pc), pc, true);
               currentEntry->copyFromEntry(persistentEntry);
               // Remember that we already looked into the SCC for this PC
               currentEntry->setPersistentEntryRead();
               return currentEntry;
               }
            }
         // If I don't have relevant data in the SCC, choose the data from IProfiler HT
         else if(!persistentEntry || !persistentEntry->hasData())
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
               return currentEntry;
               }
            else
               {
               _STATS_IPEntryChoosePersistent++;
               currentEntry->copyFromEntry(persistentEntry);
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
      return callGraphEntry->getSumCount(comp);
      }
   else if (entry->asIPBCDataFourBytes())
      {
      TR_IPBCDataFourBytes  *branchEntry = (TR_IPBCDataFourBytes *)entry;
      return branchEntry->getSumBranchCount();
      }
   return 0;
   }

int32_t
TR_IPBCDataEightWords::getSumSwitchCount() const
   {
   int32_t sum = 1;
   const uint64_t *p = (const uint64_t *)(getDataPointer());

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
TR_IProfiler::profilingSample (uintptr_t pc, uintptr_t data, bool addIt, bool isRIData, uint32_t freq)
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
TR_IProfiler::profilingSampleRI (uintptr_t pc, uintptr_t data, bool addIt, uint32_t freq)
   {
   return profilingSample(pc, data, addIt, true, freq);
   }

TR_IPBCDataAllocation *
TR_IProfiler::profilingAllocSample (uintptr_t pc, uintptr_t data, bool addIt)
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
      intptr_t initialCount = comp.getMethodSymbol()->getResolvedMethod()->hasBackwardBranches() ?
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
   if (bcInfo.getCallerIndex() >= 0)
      method = comp->getInlinedCallSite(bcInfo.getCallerIndex())._methodInfo;
   else
      method = comp->getCurrentMethod()->getPersistentIdentifier();

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
   uintptr_t methodStart = TR::Compiler->mtd.bytecodeStart(method);

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

uintptr_t
TR_IProfiler::getProfilingData(TR::Node *node, TR::Compilation *comp)
   {
   if (!isIProfilingEnabled())
      return 0;

   uintptr_t data = 0;
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   data = getProfilingData(getMethodFromNode(node, comp), bcInfo.getByteCodeIndex(), comp);

   if (data == (uintptr_t)1)
      return (uintptr_t)0;

   return data;
   }

uintptr_t
TR_IProfiler::getProfilingData(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp)
   {
   if (!isIProfilingEnabled())
      return 0;

   uintptr_t data = getProfilingData(getMethodFromBCInfo(bcInfo, comp), bcInfo.getByteCodeIndex(), comp);

   if (data == (uintptr_t)1)
      return (uintptr_t)0;

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


uintptr_t
TR_IProfiler::getSearchPCFromMethodAndBCIndex(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex)

   {
   uintptr_t searchedPC = 0;
   uint32_t methodSize = TR::Compiler->mtd.bytecodeSize(method);

   if (byteCodeIndex < methodSize)
      {
      uintptr_t methodStart = TR::Compiler->mtd.bytecodeStart(method);
      searchedPC = (uintptr_t)(methodStart + byteCodeIndex);

      // interfaces are weird
      // [   1],     1, JBinvokeinterface2      <- we get sample for this index
      // [   2],     2, JBnop
      // [   3],     3, JBinvokeinterface         21 <- we ask on this one

      if (isInterfaceBytecode(*(U_8*)searchedPC) &&
         byteCodeIndex >= 2 &&
         isInterface2Bytecode(*(U_8 *)(searchedPC - 2)))
         {
         searchedPC -= 2;
         }
      }
   else
      {
      TR_ASSERT(false, "Bytecode index can't be higher than the methodSize: bci=%u methdSize=%u", byteCodeIndex, methodSize);
      }

   return searchedPC;
   }




uintptr_t
TR_IProfiler::getSearchPCFromMethodAndBCIndex(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation * comp)
   {
   uintptr_t pc = getSearchPCFromMethodAndBCIndex(method, byteCodeIndex);
   // Diagnostic in case of error
   if (pc == 0 && comp->getOutFile())
      {
      TR_Stack<int32_t> & stack = comp->getInlinedCallStack();
      int len = comp->getInlinedCallStack().size();
      traceMsg(comp, "CSI : INLINER STACK :\n");
      for (int i = 0; i < len; i++)
         {
         TR_IPHashedCallSite hcs;
         TR_InlinedCallSite& callsite = comp->getInlinedCallSite(stack[len - i - 1]);
         hcs._method = (J9Method*)callsite._methodInfo;
         hcs._offset = callsite._byteCodeInfo.getByteCodeIndex();
         printHashedCallSite(&hcs, comp->getOutFile()->_stream, comp);
         }
      comp->dumpMethodTrees("CSI Trees : byteCodeIndex < methodSize");
      }
   return pc;
   }

uintptr_t
TR_IProfiler::getSearchPC(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR::Compilation * comp)
   {
   return getSearchPCFromMethodAndBCIndex(method, byteCodeIndex, comp);
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

   uintptr_t searchedPC = getSearchPC (getMethodFromBCInfo(bcInfo, comp), bcInfo.getByteCodeIndex(), comp);

   //if (searchedPC == (uintptr_t)IPROFILING_PC_INVALID)
   //   return NULL;

   TR_IPBCDataAllocation *entry = profilingAllocSample (searchedPC, 0, true);

   if (!entry || entry->isInvalid())
      return NULL;

   TR_ASSERT(entry->asIPBCDataAllocation(), "We should get allocation datapointer");

   ((TR_IPBCDataAllocation *)entry)->setClass((uintptr_t)clazz);
   ((TR_IPBCDataAllocation *)entry)->setMethod((uintptr_t)method);

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

uintptr_t
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

   return 0;
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
      uintptr_t searchedPC = getSearchPC (getMethodFromNode(node, comp), bcInfo.getByteCodeIndex(), comp);
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
      const uint64_t *p = (const uint64_t *)(((TR_IPBCDataEightWords *)entry)->getDataPointer());

      for (int8_t i=0; i<SWITCH_DATA_COUNT; i++, p++)
         {
         uint64_t segment = *p;
         uint32_t segmentData  = 0;
         uint32_t segmentCount = 0;

         getSwitchSegmentDataAndCount(segment, &segmentData, &segmentCount);
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

int16_t next2BytesSigned(uintptr_t pc)       { return *(int16_t *)(pc); }

void
TR_IProfiler::getBranchCounters (TR::Node *node, TR::TreeTop *fallThroughTree, int32_t *taken, int32_t *notTaken, TR::Compilation *comp)
   {
   static bool traceIProfiling = ((debug("traceIProfiling") != NULL));
   uintptr_t data = getProfilingData (node, comp);

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
         uintptr_t byteCodePtr = getSearchPC (getMethodFromNode(node, comp), node->getByteCodeIndex(), comp);

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

TR_AbstractInfo *
TR_IProfiler::createIProfilingValueInfo (TR_ByteCodeInfo &bcInfo, TR::Compilation *comp)
   {
   if (!isIProfilingEnabled())
      return NULL;

   static bool traceIProfiling = ((debug("traceIProfiling") != NULL));

   TR_OpaqueMethodBlock *method = getMethodFromBCInfo(bcInfo, comp);
   TR_ExternalValueProfileInfo *valueProfileInfo = TR_ExternalValueProfileInfo::getInfo(method, comp);

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
         bool allowForAOT = comp->getOption(TR_UseSymbolValidationManager);
         TR_PersistentClassInfo *currentPersistentClassInfo = _compInfo->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking(comp->getCurrentMethod()->containingClass(), comp, allowForAOT);
         TR_PersistentClassInfo *calleePersistentClassInfo = _compInfo->getPersistentInfo()->getPersistentCHTable()->findClassInfoAfterLocking((TR_OpaqueClassBlock *)J9_CLASS_FROM_METHOD(((J9Method *)method)), comp, allowForAOT);

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
      uintptr_t data = 0;
      uintptr_t thisPC = getSearchPC (method, bcInfo.getByteCodeIndex(), comp);
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
            // Create the value info and grab the linked list underneath
            TR_LinkedListProfilerInfo<ProfileAddressType> *list;
            valueInfo = valueProfileInfo->createAddressInfo(bcInfo, comp, data, weight, &list);
            uintptr_t *addrOfTotalFrequency;
            uintptr_t totalFrequency = list->getTotalFrequency(&addrOfTotalFrequency);

            for (int32_t i = 1; i < NUM_CS_SLOTS; i++)
               {
               data = csInfo->getClazz(i);
               if (data)
                  {
                  weight = cgData->getEdgeWeight((TR_OpaqueClassBlock *)data, comp);
                  ProfileAddressType address = static_cast<ProfileAddressType>(data);
                  list->incrementOrCreate(address, &addrOfTotalFrequency, i, weight, &comp->trMemory()->heapMemoryRegion());
                  }
               }
            // add residual to last total frequency
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
         valueInfo = valueProfileInfo->createAddressInfo(bcInfo, comp, data, weight);
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

TR_ExternalValueProfileInfo *
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
   TR_ExternalValueProfileInfo *valueProfileInfo = TR_ExternalValueProfileInfo::getInfo(originatorMethod, comp);

   if (!valueProfileInfo)
      {
      valueProfileInfo = TR_ExternalValueProfileInfo::addInfo(originatorMethod, this, comp);

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
      fprintf(stderr, "IProfiler: Number of buffers to be processed           =%" OMR_PRIu64 "\n", _numRequests);
      fprintf(stderr, "IProfiler: Number of buffers to be dropped             =%" OMR_PRIu64 "\n", _numRequestsDropped);
      fprintf(stderr, "IProfiler: Number of buffers discarded                 =%" OMR_PRIu64 "\n", _numRequestsSkipped);
      fprintf(stderr, "IProfiler: Number of buffers handed to iprofiler thread=%" OMR_PRIu64 "\n", _numRequestsHandedToIProfilerThread);
      }
   fprintf(stderr, "IProfiler: Number of records processed=%" OMR_PRIu64 "\n", _iprofilerNumRecords);
   fprintf(stderr, "IProfiler: Number of hashtable entries=%u\n", countEntries());
   fprintf(stderr, "IProfiler: Number of methodHash entries=%u\n", _numMethodHashEntries);
   checkMethodHashTable();
   }


void *
TR_IPBytecodeHashTableEntry::operator new (size_t size) throw()
   {
   memoryConsumed += (int32_t)size;
   return TR_IProfiler::allocator()->allocate(size, std::nothrow);
   }

void TR_IPBytecodeHashTableEntry::operator delete(void *p) throw()
   {
   TR_IProfiler::allocator()->deallocate(p);
   }

#if defined(J9VM_OPT_JITSERVER)
void
TR_IPBCDataFourBytes::serialize(uintptr_t methodStartAddress, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info)
   {
   TR_IPBCDataFourBytesStorage * store = (TR_IPBCDataFourBytesStorage *) storage;
   storage->pc = _pc - methodStartAddress;
   storage->left = 0;
   storage->right = 0;
   storage->ID = TR_IPBCD_FOUR_BYTES;
   store->data = data;
   }
void
TR_IPBCDataFourBytes::deserialize(TR_IPBCDataStorageHeader *storage)
   {
   loadFromPersistentCopy(storage, NULL);
   }
#endif

void
TR_IPBCDataFourBytes::createPersistentCopy(TR_J9SharedCache *sharedCache, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info)
   {
   TR_IPBCDataFourBytesStorage * store = (TR_IPBCDataFourBytesStorage *) storage;
   uintptr_t offset = (uintptr_t)sharedCache->offsetInSharedCacheFromPtrToROMClassesSection((void *)_pc);
   TR_ASSERT_FATAL(offset <= UINT_MAX, "Offset too large for TR_IPBCDataFourBytes");
   storage->pc = (uint32_t)offset;
   storage->left = 0;
   storage->right = 0;
   storage->ID = TR_IPBCD_FOUR_BYTES;
   store->data = data;
   }

void
TR_IPBCDataFourBytes::loadFromPersistentCopy(TR_IPBCDataStorageHeader * storage, TR::Compilation *comp)
   {
   TR_IPBCDataFourBytesStorage * store = (TR_IPBCDataFourBytesStorage *) storage;
   TR_ASSERT(storage->ID == TR_IPBCD_FOUR_BYTES, "Incompatible types between storage and loading of iprofile persistent data");
   data = store->data;
   }

void
TR_IPBCDataFourBytes::copyFromEntry(TR_IPBytecodeHashTableEntry *originalEntry)
   {
   TR_IPBCDataFourBytes *entry = (TR_IPBCDataFourBytes *) originalEntry;
   TR_ASSERT(originalEntry->asIPBCDataFourBytes(), "Incompatible types between storage and loading of iprofile persistent data");
   data = entry->data;
   }

int32_t
TR_IPBCDataFourBytes::getSumBranchCount() const
   {
   int32_t fallThroughCount = (int32_t)(data & 0x0000FFFF) | 0x1;
   int32_t branchToCount = (int32_t)((data & 0xFFFF0000)>>16) | 0x1;
   return (fallThroughCount + branchToCount);
   }

void
TR_IPBCDataEightWords::createPersistentCopy(TR_J9SharedCache *sharedCache, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info)
   {
   TR_IPBCDataEightWordsStorage * store = (TR_IPBCDataEightWordsStorage *) storage;
   uintptr_t offset = (uintptr_t)sharedCache->offsetInSharedCacheFromPtrToROMClassesSection((void *)_pc);
   TR_ASSERT_FATAL(offset <= UINT_MAX, "Offset too large for TR_IPBCDataEightWords");
   storage->pc = (uint32_t)offset;
   storage->ID = TR_IPBCD_EIGHT_WORDS;
   storage->left = 0;
   storage->right = 0;
   for (int i = 0; i < SWITCH_DATA_COUNT; i++)
      store->data[i] = data[i];
   }

void
TR_IPBCDataEightWords::loadFromPersistentCopy(TR_IPBCDataStorageHeader * storage, TR::Compilation *comp)
   {
   TR_IPBCDataEightWordsStorage * store = (TR_IPBCDataEightWordsStorage *) storage;
   TR_ASSERT(storage->ID == TR_IPBCD_EIGHT_WORDS, "Incompatible types between storage and loading of iprofile persistent data");
   for (int i = 0; i < SWITCH_DATA_COUNT; i++)
      data[i] = store->data[i];
   }

void
TR_IPBCDataEightWords::copyFromEntry(TR_IPBytecodeHashTableEntry *originalEntry)
   {
   TR_IPBCDataEightWords* entry = (TR_IPBCDataEightWords*) originalEntry;
   TR_ASSERT(originalEntry->asIPBCDataEightWords(), "Incompatible types between storage and loading of iprofile persistent data");
   for (int i = 0; i < SWITCH_DATA_COUNT; i++)
      data[i] = entry->data[i];
   }

#if defined(J9VM_OPT_JITSERVER)
void
TR_IPBCDataEightWords::serialize(uintptr_t methodStartAddress, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info)
   {
   TR_IPBCDataEightWordsStorage * store = (TR_IPBCDataEightWordsStorage *) storage;
   storage->pc = _pc - methodStartAddress;
   storage->ID = TR_IPBCD_EIGHT_WORDS;
   storage->left = 0;
   storage->right = 0;
   for (int i = 0; i < SWITCH_DATA_COUNT; i++)
      store->data[i] = data[i];
   }
void
TR_IPBCDataEightWords::deserialize(TR_IPBCDataStorageHeader *storage)
   {
   loadFromPersistentCopy(storage, NULL);
   }
#endif

int32_t
TR_IPBCDataCallGraph::setData(uintptr_t v, uint32_t freq)
   {
   PORT_ACCESS_FROM_PORT(staticPortLib);
   bool found = false;
   int32_t returnCount = 0;
   uint16_t maxWeight = 0;
   J9Class* receiverClass;

   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if (_csInfo.getClazz(i) == v)
         {
         // Check for overflow
         uint16_t oldWeight = _csInfo._weight[i];
         uint16_t newWeight = oldWeight + freq;
         if (newWeight < oldWeight)
            newWeight = 0xFFFF;
         _csInfo._weight[i] = newWeight;
         returnCount = newWeight;
         found = true;
         break;
         }
      else if (_csInfo.getClazz(i) == 0)
         {
         _csInfo.setClazz(i, v);
         _csInfo._weight[i] = freq;
         returnCount = _csInfo._weight[i];
         found = true;
         break;
         }

      if (maxWeight < _csInfo._weight[i])
         maxWeight = _csInfo._weight[i];
      }

   if (!found)
      {
      // Must update the `residue` bucket
      uint16_t oldResidueWeight = _csInfo._residueWeight;
      uint16_t newResidueWeight = oldResidueWeight + freq;
      if (newResidueWeight > 0x7FFF)
         newResidueWeight = 0x7FFF;
      _csInfo._residueWeight = newResidueWeight;
      returnCount = newResidueWeight;

      if (_csInfo._residueWeight > maxWeight)
         {
         if (lockEntry())
            {
            // Reset the entries in reverse order
            // Want to avoid the situation where one entry has the class reset, but a subsequent entry does not
            for (int32_t i = NUM_CS_SLOTS - 1; i > 0; i--)
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
      }

   return returnCount;
   }

int32_t
TR_IPBCDataCallGraph::getSumCount() const
   {
   int32_t sumWeight = _csInfo._residueWeight;
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      sumWeight += _csInfo._weight[i];

   return sumWeight;
   }

int32_t
TR_IPBCDataCallGraph::getSumCount(TR::Compilation *comp)
   {
   static bool debug = feGetEnv("TR_debugiprofiler_detail") ? true : false;
   int32_t sumWeight = _csInfo._residueWeight;
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if(debug)
         {
         int32_t len;
         const char * s = _csInfo.getClazz(i) ? comp->fej9()->getClassNameChars((TR_OpaqueClassBlock*)_csInfo.getClazz(i), len) : "0";
         fprintf(stderr,"[%p] slot %" OMR_PRId32 ", class %#" OMR_PRIxPTR " %s, weight %" OMR_PRId32 " : ", this, i, _csInfo.getClazz(i), s, _csInfo._weight[i]);
         fflush(stderr);
         }
      sumWeight += _csInfo._weight[i];
      }
   if(debug)
      {
      fprintf(stderr," residueweight %d\n", _csInfo._residueWeight);
      fflush(stderr);
      }
   return sumWeight;
   }

uintptr_t
TR_IPBCDataCallGraph::getData(TR::Compilation *comp)
   {
   int32_t sumWeight;
   int32_t maxWeight;
   uintptr_t data = _csInfo.getDominantClass(sumWeight, maxWeight);

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
   memoryConsumed += (int32_t)size;
   return TR_IProfiler::allocator()->allocate(size, std::nothrow);
   }

int32_t
TR_IPBCDataCallGraph::getEdgeWeight(TR_OpaqueClassBlock *clazz, TR::Compilation *comp)
   {
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if (_csInfo.getClazz(i) == (uintptr_t)clazz)
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

      fprintf(stderr, "%#" OMR_PRIxPTR " %s %d\n", _csInfo.getClazz(i), s, _csInfo._weight[i]);
      }
   fprintf(stderr, "%d\n", _csInfo._residueWeight);
   }

void
TR_IPBCDataCallGraph::updateEdgeWeight(TR_OpaqueClassBlock *clazz, int32_t weight)
   {
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if (_csInfo.getClazz(i) == (uintptr_t)clazz)
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

#if defined(J9VM_OPT_JITSERVER)
/**
 * API used by JITClient to check whether the current entry can be persisted
 *
 * @param info PersistentInfo pointer used for checking if the class has been unloaded
 *
 * @return IPBC_ENTRY_PERSIST_LOCK, IPBC_ENTRY_PERSIST_UNLOADED or IPBC_ENTRY_CAN_PERSIST
 */
uint32_t
TR_IPBCDataCallGraph::canBeSerialized(TR::PersistentInfo *info)
   {
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
         TR_ASSERT(_csInfo.getClazz(i), "Race condition detected: cached value=%p, pc=%p", clazz, _pc);
         }
      }
   return IPBC_ENTRY_CAN_PERSIST;
   }


/**
 * API used by JITClient to serialize IP data of a method
 *
 * @param methodStartAddress Start address of the bytecodes for the method
 * @param storage Storage area where we serialize entries
 * @param info PersistentInfo pointer used for checking if the class has been unloaded
 *
 * @return void
 */
void
TR_IPBCDataCallGraph::serialize(uintptr_t methodStartAddress, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info)
   {
   TR_IPBCDataCallGraphStorage * store = (TR_IPBCDataCallGraphStorage *) storage;
   TR_ASSERT(_pc >= methodStartAddress, "_pc=%p should be larger than methodStartAddress=%p\n", (void*)_pc, (void*)methodStartAddress);
   storage->pc = _pc - methodStartAddress;
   storage->ID = TR_IPBCD_CALL_GRAPH;
   storage->left = 0;
   storage->right = 0;
   for (int32_t i=0; i < NUM_CS_SLOTS;i++)
      {
      J9Class *clazz = (J9Class *) _csInfo.getClazz(i);
      if (clazz)
         {
         TR_ASSERT(!info->isUnloadedClass(clazz, true), "cannot store unloaded class");
         store->_csInfo.setClazz(i, (uintptr_t)clazz);
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


/**
 * API used by JITServer to deserialize IP data of a method sent from JITClient
 *
 * @param storage Storage area where the serialized entries resides
 *
 * @return void
 */
void
TR_IPBCDataCallGraph::deserialize(TR_IPBCDataStorageHeader *storage)
   {
   TR_IPBCDataCallGraphStorage * store = (TR_IPBCDataCallGraphStorage *) storage;
   TR_ASSERT(storage->ID == TR_IPBCD_CALL_GRAPH, "Incompatible types between storage and loading of iprofile persistent data");
   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      _csInfo.setClazz(i, store->_csInfo.getClazz(i));
      _csInfo._weight[i] = store->_csInfo._weight[i];
      }
   _csInfo._residueWeight = store->_csInfo._residueWeight;
   _csInfo._tooBigToBeInlined = store->_csInfo._tooBigToBeInlined;
   }
#endif

uint32_t
TR_IPBCDataCallGraph::canBePersisted(TR_J9SharedCache *sharedCache, TR::PersistentInfo *info)
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

         if (!sharedCache->isClassInSharedCache(clazz))
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
TR_IPBCDataCallGraph::createPersistentCopy(TR_J9SharedCache *sharedCache, TR_IPBCDataStorageHeader *storage, TR::PersistentInfo *info)
   {
   TR_IPBCDataCallGraphStorage * store = (TR_IPBCDataCallGraphStorage *) storage;
   uintptr_t offset = (uintptr_t)sharedCache->offsetInSharedCacheFromPtrToROMClassesSection((void *)_pc);
   TR_ASSERT_FATAL(offset <= UINT_MAX, "Offset too large for TR_IPBCDataCallGraph");
   storage->pc = (uint32_t)offset;
   storage->ID = TR_IPBCD_CALL_GRAPH;
   storage->left = 0;
   storage->right = 0;
   // We can only afford to store one entry, the one with most samples
   // The samples for the remaining entries will be added to the "other" category
   uint16_t maxWeight = 0;
   int32_t indexMaxWeight = -1; // Index for the entry with most samples
   uint32_t sumWeights = 0;
   for (int32_t i=0; i < NUM_CS_SLOTS;i++)
      {
      J9Class *clazz = (J9Class *) _csInfo.getClazz(i);
      if (clazz)
         {
         bool isUnloadedClass = info->isUnloadedClass(clazz, true);
         if (!isUnloadedClass)
            {
            if (_csInfo._weight[i] > maxWeight)
               {
               maxWeight = _csInfo._weight[i];
               indexMaxWeight = i;
               }
            sumWeights += _csInfo._weight[i];
            }
         }
      }
   sumWeights += _csInfo._residueWeight;

   store->_csInfo.setClazz(0, 0); // Assume invalid entry 0 and overwite later
   store->_csInfo._weight[0] = 0;
   store->_csInfo._residueWeight = sumWeights - maxWeight;
   store->_csInfo._tooBigToBeInlined = _csInfo._tooBigToBeInlined;

   // Having VM access in hand prevents class unloading and redefinition
   TR::VMAccessCriticalSection criticalSection(sharedCache->fe());

   if (indexMaxWeight >= 0) // If there is a valid dominant class
      {
      TR_OpaqueClassBlock *clazz =  (TR_OpaqueClassBlock*)_csInfo.getClazz(indexMaxWeight);
      if (!info->isUnloadedClass(clazz, true))
         {
         if (sharedCache->isClassInSharedCache(clazz))
            {
            uintptr_t classChainOffset = sharedCache->rememberClass(clazz);
            if (TR_SharedCache::INVALID_CLASS_CHAIN_OFFSET != classChainOffset)
               {
               store->_csInfo.setClazz(0, classChainOffset);
               store->_csInfo._weight[0] = _csInfo._weight[indexMaxWeight];
               // I need to find the chain of the first class loaded by the class loader of this RAMClass
               // getClassChainOffsetIdentifyingLoader() fails the compilation if not found, hence we use a special implementation
               uintptr_t classChainOffsetOfCLInSharedCache = sharedCache->getClassChainOffsetIdentifyingLoaderNoThrow(clazz);
               // The chain that identifies the class loader is stored at index 1
               store->_csInfo.setClazz(1, classChainOffsetOfCLInSharedCache);
               if (TR_SharedCache::INVALID_CLASS_CHAIN_OFFSET == classChainOffsetOfCLInSharedCache)
                  if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
                     TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "createPersistentCopy: Cannot store CallGraphEntry because classChain identifying classloader is 0");
               }
            else
               {
               if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "createPersistentCopy: Cannot store CallGraphEntry because cannot remember class");
               }
            }
         else // Most frequent class is not in SCC, so I cannot store IProfiler info about this in SCC
            {
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
               TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "createPersistentCopy: Cannot store CallGraphEntry because ROMClass is not in SCC");
            }
         }
      else
         {
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
            TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "createPersistentCopy: Cannot store CallGraphEntry because RAMClass is unloaded");
         }
      }
   else
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "createPersistentCopy: Cannot store CallGraphEntry because there is no data");
      }
   }

void
TR_IPBCDataCallGraph::loadFromPersistentCopy(TR_IPBCDataStorageHeader * storage, TR::Compilation *comp)
   {
   TR_IPBCDataCallGraphStorage * store = (TR_IPBCDataCallGraphStorage *) storage;
   TR_ASSERT(storage->ID == TR_IPBCD_CALL_GRAPH, "Incompatible types between storage and loading of iprofile persistent data");
   TR_J9SharedCache *sharedCache = comp->fej9()->sharedCache();

   // First index has the dominant class
   _csInfo.setClazz(0, 0);
   _csInfo._weight[0] = 0;
   uintptr_t csInfoChainOffset = store->_csInfo.getClazz(0);
   auto classChainIdentifyingLoaderOffsetInSCC = store->_csInfo.getClazz(1);
   if (csInfoChainOffset && classChainIdentifyingLoaderOffsetInSCC)
      {
      uintptr_t *classChain = (uintptr_t*)sharedCache->pointerFromOffsetInSharedCache(csInfoChainOffset);
      if (classChain)
         {
         // I need to convert from ROMClass into RAMClass
         void *classChainIdentifyingLoader = sharedCache->pointerFromOffsetInSharedCache(classChainIdentifyingLoaderOffsetInSCC);
         if (classChainIdentifyingLoader)
            {
            TR::VMAccessCriticalSection criticalSection(comp->fej9());
            J9ClassLoader *classLoader = (J9ClassLoader *)sharedCache->lookupClassLoaderAssociatedWithClassChain(classChainIdentifyingLoader);
            if (classLoader)
               {
               TR_OpaqueClassBlock *j9class = sharedCache->lookupClassFromChainAndLoader(classChain, classLoader, comp);
               if (j9class)
                  {
                  // Optimizer and the codegen assume receiver classes of a call from profiling data are initialized,
                  // otherwise they shouldn't show up in the profile. But classes from iprofiling data from last run
                  // may be uninitialized in load time, as the program behavior may change in the second run. Thus
                  // we need to verify that a class is initialized, otherwise optimizer or codegen will make wrong
                  // transformation based on invalid assumption.
                  //
                  if (comp->fej9()->isClassInitialized(j9class))
                     {
                     _csInfo.setClazz(0, (uintptr_t)j9class);
                     _csInfo._weight[0] = store->_csInfo._weight[0];
                     }
                  else
                     {
                     if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
                        {
                        J9UTF8 *classNameUTF8 = J9ROMCLASS_CLASSNAME(sharedCache->startingROMClassOfClassChain(classChain));
                        TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"loadFromPersistentCopy: Cannot covert ROMClass to RAMClass because RAMClass is not initialized. Class: %.*s",
                                                       J9UTF8_LENGTH(classNameUTF8), J9UTF8_DATA(classNameUTF8));
                        }
                     }
                  }
               else
                  {
                  // This is the second most frequent failure. Do we have the wrong classLoader?
                  if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
                     {
                     J9UTF8 *classNameUTF8 = J9ROMCLASS_CLASSNAME(sharedCache->startingROMClassOfClassChain(classChain));
                     TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "loadFromPersistentCopy: Cannot convert ROMClass to RAMClass because lookupClassFromChainAndLoader failed. Class: %.*s",
                                                    J9UTF8_LENGTH(classNameUTF8), J9UTF8_DATA(classNameUTF8));
                     }
                  }
               }
            else
               {
               // This is the most important failure case
               if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
                  {
                  J9UTF8 *classNameUTF8 = J9ROMCLASS_CLASSNAME(sharedCache->startingROMClassOfClassChain(classChain));
                  TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "loadFromPersistentCopy: Cannot convert ROMClass to RAMClass. Cannot find classloader. Class: %.*s",
                                                 J9UTF8_LENGTH(classNameUTF8), J9UTF8_DATA(classNameUTF8));
                  }
               }
            }
         else
            {
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
               {
               J9UTF8 *classNameUTF8 = J9ROMCLASS_CLASSNAME(sharedCache->startingROMClassOfClassChain(classChain));
               TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "loadFromPersistentCopy: Cannot convert ROMClass to RAMClass. Cannot find chain identifying classloader. Class: %.*s",
                                              J9UTF8_LENGTH(classNameUTF8), J9UTF8_DATA(classNameUTF8));
               }
            }
         }
      else
         {
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "loadFromPersistentCopy: Cannot convert ROMClass to RAMClass. Cannot get the class chain of ROMClass");
            }
         }
      }
   else
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "loadFromPersistentCopy: Cannot convert ROMClass to RAMClass. Don't have required information in the entry");
         }
      }
   // Populate the remaining entries with 0
   for (int32_t i = 1; i < NUM_CS_SLOTS; i++)
      {
      _csInfo.setClazz(i, 0);
      _csInfo._weight[i] = 0;
      }
   _csInfo._residueWeight = store->_csInfo._residueWeight;
   _csInfo._tooBigToBeInlined = store->_csInfo._tooBigToBeInlined;
   }

void
TR_IPBCDataCallGraph::copyFromEntry(TR_IPBytecodeHashTableEntry *originalEntry)
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
         csInfo->_weight[0] = count;
         csInfo->setClazz(0, (uintptr_t)comp->fej9()->getClassOfMethod(method));

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
      return entry->asIPBCDataCallGraph()->getSumCount();

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
      return entry->asIPBCDataCallGraph()->getSumCount();

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
   uintptr_t thisPC = getSearchPC (getMethodFromNode(callerNode, comp), bcInfo.getByteCodeIndex(), comp);

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
TR_IProfiler::releaseAllEntries(uint32_t &unexpectedLockedEntries)
   {
   uint32_t count = 0;
   for (int32_t bucket = 0; bucket < TR::Options::_iProfilerBcHashTableSize; bucket++)
      {
      for (TR_IPBytecodeHashTableEntry *entry = _bcHashTable[bucket]; entry; entry = entry->getNext())
         {
         if (entry->asIPBCDataCallGraph() && entry->asIPBCDataCallGraph()->isLocked())
            {
            // Because there is a known race in findOrCreateEntry(), there is a
            // chance that this entry is still locked because another entry for
            // the same PC was added after it. These should not contribute to
            // the number of unexpectedly locked entries.
            auto otherEntry = profilingSample(entry->getPC(), 0, false);
            if (entry == otherEntry)
               unexpectedLockedEntries++;
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
   for (int32_t bucket = 0; bucket < TR::Options::_iProfilerBcHashTableSize; bucket++)
      for (TR_IPBytecodeHashTableEntry *entry = _bcHashTable[bucket]; entry; entry = entry->getNext())
         count++;
   return count;
   }


// helper functions for replay
//
void TR_IProfiler::setupEntriesInHashTable(TR_IProfiler *ip)
   {
   for (int32_t bucket = 0; bucket < TR::Options::_iProfilerBcHashTableSize; bucket++)
      {
      TR_IPBytecodeHashTableEntry *entry = _bcHashTable[bucket], *prevEntry = NULL;

      while (entry)
         {
         uintptr_t pc = entry->getPC();

         if (pc == 0 ||
               pc == 0xffffffff)
            {
            printf("invalid pc for entry %p %#" OMR_PRIxPTR "\n", entry, pc);
            fflush(stdout);
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
         uintptr_t data = oldEntry->getData();
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
               printf("got clazz %#" OMR_PRIxPTR " weight %d\n", oldCSInfo->getClazz(i), oldCSInfo->_weight[i]);
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

   ::FILE *fout = fopen(fname, "a");

   if (!fout)
      {
      printf("Couldn't open the file; re-directing to stderr instead\n");
      fout = stderr;
      }
   // Do not define this end var if class unloading is possible.
   // Class unloading can render some of cached methods stale, leading to crashes
   // when trying to find method names.
   static char *methodNames = feGetEnv("TR_PrintMethodHashTableMethodNames");

   TR_StatsHisto<TR_IPMethodHashTableEntry::MAX_IPMETHOD_CALLERS-1> faninHisto("Fanin caller list length histo", 1, TR_IPMethodHashTableEntry::MAX_IPMETHOD_CALLERS);

   fprintf(fout, "Printing method hash table\n");fflush(fout);
   for (int32_t bucket = 0; bucket < TR::Options::_iProfilerMethodHashTableSize; bucket++)
      {
      for (TR_IPMethodHashTableEntry *entry = _methodHashTable[bucket]; entry; entry = entry->_next)
         {
         J9Method *method = (J9Method*)entry->_method;
         fprintf(fout, "Callee method %p", method);
         if (methodNames)
            {
            J9UTF8 * nameUTF8;
            J9UTF8 * signatureUTF8;
            J9UTF8 * methodClazzUTRF8;
            getClassNameSignatureFromMethod(method, methodClazzUTRF8, nameUTF8, signatureUTF8);
            fprintf(fout,"\t%.*s.%.*s%.*s",
                    J9UTF8_LENGTH(methodClazzUTRF8), J9UTF8_DATA(methodClazzUTRF8),
                    J9UTF8_LENGTH(nameUTF8), J9UTF8_DATA(nameUTF8),
                    J9UTF8_LENGTH(signatureUTF8), J9UTF8_DATA(signatureUTF8));
            fprintf(fout,"\t is %" OMR_PRIdPTR " bytecode long", J9_BYTECODE_END_FROM_ROM_METHOD(getOriginalROMMethod(method)) - J9_BYTECODE_START_FROM_ROM_METHOD(getOriginalROMMethod(method)));
            }
         fprintf(fout, "\n");
         fflush(fout);
         int32_t count = 0;
         uint32_t i=0;

         for (TR_IPMethodData* it = &entry->_caller; it; it = it->next)
            {
            count++;
            TR_OpaqueMethodBlock *caller = it->getMethod();
            if(caller)
               {
               fprintf(fout,"\t%8p pcIndex %3" OMR_PRIu32 " weight %3" OMR_PRIu32 "\t",
                       caller, it->getPCIndex(), it->getWeight());
               if (methodNames)
                  {
                  J9UTF8 * caller_nameUTF8;
                  J9UTF8 * caller_signatureUTF8;
                  J9UTF8 * caller_methodClazzUTF8;
                  getClassNameSignatureFromMethod((J9Method*)caller, caller_methodClazzUTF8, caller_nameUTF8, caller_signatureUTF8);

                  fprintf(fout, "%.*s%.*s%.*s",
                     J9UTF8_LENGTH(caller_methodClazzUTF8), J9UTF8_DATA(caller_methodClazzUTF8),
                     J9UTF8_LENGTH(caller_nameUTF8), J9UTF8_DATA(caller_nameUTF8),
                     J9UTF8_LENGTH(caller_signatureUTF8), J9UTF8_DATA(caller_signatureUTF8));
                  }
                  fprintf(fout, "\n");
                  fflush(fout);
               }
            else
               {
               fprintf(fout,"caller method is null\n");
               }
            }
         //Print the other bucket
         fprintf(fout, "\tother bucket: weight %d\n", entry->_otherBucket.getWeight());
         fprintf(fout,"Caller list length = %d\n", count);
         fflush(fout);
         faninHisto.update(count);
         }
      }
      faninHisto.report(fout);
      fflush(fout);
   }

void
TR_IProfiler::getFaninInfo(TR_OpaqueMethodBlock *calleeMethod, uint32_t *count, uint32_t *weight, uint32_t *otherBucketWeight)
   {
   uint32_t i = 0;
   uint32_t w = 0;
   uint32_t other = 0;

   // Search for the callee in the hashtable
   int32_t bucket = methodHash((uintptr_t) calleeMethod);
   TR_IPMethodHashTableEntry *entry = searchForMethodSample((TR_OpaqueMethodBlock*) calleeMethod, bucket);
   if (entry)
      {
      other = entry->_otherBucket.getWeight();
      w = other;
      // Iterate through all the callers and add their weight
      for (TR_IPMethodData* it = &entry->_caller; it; it = it->next)
         {
         w += it->getWeight();
         i++;
         }
      }
   *weight = w;
   *count = i;
   if (otherBucketWeight)
      *otherBucketWeight = other;
   return;
   }

bool TR_IProfiler::getCallerWeight(TR_OpaqueMethodBlock *calleeMethod,TR_OpaqueMethodBlock *callerMethod, uint32_t *weight, uint32_t pcIndex, TR::Compilation *comp)
{
   // First: hash the method

   bool useTuples = (pcIndex != ~0);

    int32_t bucket = methodHash((uintptr_t)calleeMethod);
   //printf("getCallerWeight:  method = %p methodHash = %d\n",calleeMethod,bucket);

   //adjust pcIndex for interface calls (see getSearchPCFromMethodAndBCIndex)
   //otherwise we won't be able to locate a caller-callee-bcIndex triplet
   //even if it is in a TR_IPMethodHashTableEntry
   uintptr_t pcAddress = getSearchPCFromMethodAndBCIndex(callerMethod, pcIndex, comp);

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
      //   traceMsg(comp, "comparing %p with %p and pc index %d pc address %p with other pc index %d other pc address %p\n", callerMethod, it->getMethod(), pcIndex, pcAddress, it->getPCIndex(), ((uintptr_t) it->getPCIndex()) + TR::Compiler->mtd.bytecodeStart(callerMethod));

      if( it->getMethod() == callerMethod && (!useTuples || ((((uintptr_t) it->getPCIndex()) + TR::Compiler->mtd.bytecodeStart(callerMethod)) == pcAddress)))
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
   if (rc == JNI_OK)
      {
      iProfiler->setIProfilerThread(iprofilerThread);
      j9thread_set_name(j9thread_self(), "JIT IProfiler");
      iProfiler->setIProfilerThreadLifetimeState(TR_IProfiler::IPROF_THR_INITIALIZED);
      }
   else
      {
      iProfiler->setIProfilerThreadLifetimeState(TR_IProfiler::IPROF_THR_FAILED_TO_ATTACH);
      }
   iProfiler->getIProfilerMonitor()->notifyAll();
   iProfiler->getIProfilerMonitor()->exit();

   // attaching the IProfiler thread failed
   if (rc != JNI_OK)
      return JNI_ERR;

#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
   if (vm->javaOffloadSwitchOnWithReasonFunc != 0)
      (*vm->javaOffloadSwitchOnWithReasonFunc)(iprofilerThread, J9_JNI_OFFLOAD_SWITCH_JIT_IPROFILER_THREAD);
#endif

   iProfiler->processWorkingQueue();

   vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);
   iProfiler->setIProfilerThread(NULL);
   iProfiler->getIProfilerMonitor()->enter();
   iProfiler->setIProfilerThreadLifetimeState(TR_IProfiler::IPROF_THR_DESTROYED);
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
      else
         {
         // Must wait here until the thread gets created; otherwise an early shutdown
         // does not know whether or not to destroy the thread
         _iprofilerMonitor->enter();
         while (getIProfilerThreadLifetimeState() == TR_IProfiler::IPROF_THR_NOT_CREATED)
            _iprofilerMonitor->wait();
         _iprofilerMonitor->exit();

         // At this point the IProfiler thread should have attached successfully and changed
         // the state to IPROF_THR_INITIALIZED, or failed to attach and changed the state to
         // IPROF_THR_FAILED_TO_ATTACH
         if (getIProfilerThreadLifetimeState() == TR_IProfiler::IPROF_THR_FAILED_TO_ATTACH)
            {
            _iprofilerThread = NULL;
            _iprofilerMonitor = NULL;
            }
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

   setIProfilerThreadLifetimeState(TR_IProfiler::IPROF_THR_STOPPING);
   while (getIProfilerThreadLifetimeState() == TR_IProfiler::IPROF_THR_STOPPING)
      {
      _iprofilerMonitor->notifyAll();
      _iprofilerMonitor->wait();
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

   // If the IProfiler Thread is not initialized or waiting for work, then it is either
   // suspended, stopping, or stopped. In all cases, there's no point posting the buffer
   // to the working queue as the IProfiler Thread will not be processing it.
   if (getIProfilerThreadLifetimeState() != TR_IProfiler::IPROF_THR_WAITING_FOR_WORK
       && getIProfilerThreadLifetimeState() != TR_IProfiler::IPROF_THR_INITIALIZED)
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

void
TR_IProfiler::discardFilledIProfilerBuffers()
   {
   while (!_workingBufferList.isEmpty())
      {
      _freeBufferList.add(_workingBufferList.pop());
      _numOutstandingBuffers--;
      }
   _workingBufferTail = NULL;
   }

// This method is executed by the iprofiling thread
void
TR_IProfiler::processWorkingQueue()
   {
   PORT_ACCESS_FROM_PORT(_portLib);

   // wait for something to do
   _iprofilerMonitor->enter();
   do {
      // Wait for work until a buffer is added to the working list
      //
      // However, during shutdown, it is possible for the shutdown thread to
      // send a notify while the IProfiler thread has not yet acquired the
      // monitor. This results in both threads waiting on the same monitor.
      // By checking the Lifetime State, the IProfiler Thread will only wait
      // if the JVM is not currently shutting down.
      while (getIProfilerThreadLifetimeState() == TR_IProfiler::IPROF_THR_INITIALIZED
             && _workingBufferList.isEmpty())
         {
         setIProfilerThreadLifetimeState(TR_IProfiler::IPROF_THR_WAITING_FOR_WORK);

         //fprintf(stderr, "IProfiler thread will wait for data outstanding=%d\n", numOutstandingBuffers);
         _iprofilerMonitor->wait();

         // The state could have been changed either for checkpoint or shutdown, ensure
         // consistency with the state before changing it to IPROF_THR_INITIALIZED
         if (getIProfilerThreadLifetimeState() == TR_IProfiler::IPROF_THR_WAITING_FOR_WORK)
            setIProfilerThreadLifetimeState(TR_IProfiler::IPROF_THR_INITIALIZED);
         }

      // IProfiler thread should be shut down
      if (getIProfilerThreadLifetimeState() == TR_IProfiler::IPROF_THR_STOPPING)
         {
         discardFilledIProfilerBuffers();
         _iprofilerMonitor->exit();
         break;
         }
      else if (!_workingBufferList.isEmpty())
         {
         // We have some buffer to process
         // Dequeue the buffer to be processed
         //
         _crtProfilingBuffer = _workingBufferList.pop();
         if (_workingBufferList.isEmpty())
            _workingBufferTail = NULL;

         // We don't need the iprofiler monitor now
         _iprofilerMonitor->exit();

         TR_ASSERT_FATAL(_crtProfilingBuffer->getSize() > 0, "size of _crtProfilingBuffer (%p) <= 0", _crtProfilingBuffer);

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

         // attach the buffer to the buffer pool
         _iprofilerMonitor->enter();
         _freeBufferList.add(_crtProfilingBuffer);
         _crtProfilingBuffer = NULL;
         _numOutstandingBuffers--;
         }
      else if (getIProfilerThreadLifetimeState() == TR_IProfiler::IPROF_THR_SUSPENDING)
         {
#if defined(J9VM_OPT_CRIU_SUPPORT)
         // Check if the IProfiler Thread should be suspended for checkpoint
         J9JavaVM *javaVM = _compInfo->getJITConfig()->javaVM;
         if (javaVM->internalVMFunctions->isCheckpointAllowed(javaVM))
            {
            // The monitors must be acquired in the right order, therefore
            // release the IProfiler monitor prior to attempting to suspend
            // the IProfiler Thread for checkpoint
            _iprofilerMonitor->exit();

            // Because we no longer have the iprofiler monitor, shutdown can
            // occur here. However, this is ok because the first thing
            // suspendIProfilerThreadForCheckpoint does is acquire the comp
            // monitor and check if we're still trying to suspend threads
            // for checkpoint.
            suspendIProfilerThreadForCheckpoint();

            // Reacquire the IProfiler monitor
            _iprofilerMonitor->enter();
            }
         else
#endif // defined(J9VM_OPT_CRIU_SUPPORT)
            {
            TR_ASSERT_FATAL(false, "Iprofiler cannot be in state IPROF_THR_SUSPENDING if checkpoint is not allowed.\n");
            }
         }
      else
         {
         TR_ASSERT_FATAL(false, "Iprofiler in invalid state %d\n", getIProfilerThreadLifetimeState());
         }
      } while(true);
   }

extern "C" void stopInterpreterProfiling(J9JITConfig *jitConfig);

/* Lower value will more aggressively skip samples as the number of unloaded classes increases */
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
   uintptr_t lastMethodSize = 0;
   uintptr_t lastMethodStart = 0;
   intptr_t data = 0;
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

   J9JavaVM *javaVM = _compInfo->getJITConfig()->javaVM;

#if defined(J9VM_OPT_CRIU_SUPPORT)
   if (javaVM->internalVMFunctions->isDebugOnRestoreEnabled(javaVM) && javaVM->internalVMFunctions->isCheckpointAllowed(javaVM))
      {
      int32_t dropRate = (int32_t)((((float)(_numRequestsDropped + _numRequestsSkipped)) / ((float)_numRequests)) * 1000);
      if (TR::Options::_IprofilerPreCheckpointDropRate >= 1000 || dropRate <= TR::Options::_IprofilerPreCheckpointDropRate)
         {
         _numRequestsDropped++;
         return 0;
         }
      }
#endif

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

   int32_t skipCountMain = 20+(rand()%10); // TODO: Use the main TR_RandomGenerator from jitconfig?
   int32_t skipCount = skipCountMain;
   bool profileFlag = true;

   while (cursor < dataStart + size)
      {
      const U_8* cursorCopy = cursor;

      if (!TR::Options::_profileAllTheTime)
         {
         // replenish skipCount if it has been exhausted
         if (skipCount <= 0)
            {
            skipCount = skipCountMain;
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

            data = (intptr_t)receiverClass;

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
         case JBinvokestaticsplit:
         case JBinvokespecialsplit:
            {
            addSample = false; // never store these in the IProfiler hashtable
            caller = *(J9Method**)cursor;
            cursor += sizeof(J9Method *);
            if (fanInDisabled)
               break;
            J9ConstantPool* ramCP = J9_CP_FROM_METHOD(caller);
            // Read the cpIndex (or the index into the split table) and extend it to UDATA.
            // The extension is needed because J9_STATIC_SPLIT_TABLE_INDEX_FLAG and
            // J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG do not fit on 16 bits.
            UDATA cpIndex = readU16(pc + 1);

            // For split bytecodes, the cpIndex is actually the index into the split table.
            if (JBinvokestaticsplit == *pc)
               cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG;
            if (JBinvokespecialsplit == *pc)
               cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG;
            J9Method *callee = jitGetJ9MethodUsingIndex(vmThread, ramCP, cpIndex);

            if (!callee)
               break;

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

            callee = VM_VMHelpers::filterVMInitialMethods(vmThread, callee);

            if ((callee != NULL) && !fanInDisabled)
               {
               uint32_t offset = (uint32_t) (pc - caller->bytecodes);
               findOrCreateMethodEntry(caller, callee , true , offset);
               if (_compInfo->getLowPriorityCompQueue().isTrackingEnabled() &&  // is feature enabled?
                  vmThread == _iprofilerThread)  // only IProfiler thread is allowed to execute this
                  {
                  _compInfo->getLowPriorityCompQueue().tryToScheduleCompilation(vmThread, caller);
                  }
               }
            data = (intptr_t)receiverClass;
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
            // switches are rare compared to branches, so we can afford to profile them all
            addSample = true;


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
            /* Template="Unrecognized bytecode (pc=%p, bc=%d) cursor %p in buffer %p of size %d" */
            Trc_JIT_IProfiler_unrecognized(vmThread, pc, *pc, cursor, data, size);
            Assert_JIT_unreachable();
            return 0;
         }

      if (addSample && !verboseReparse)
         {
         profilingSample ((uintptr_t)pc, (uintptr_t)data, true);
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

   OMR::CriticalSection invalidateBuffers(_iprofilerMonitor);

   if (!getIProfilerThread())
      return;

   // mark the current buffer as invalid; set with exclusive VM access
   if (_crtProfilingBuffer)
      _crtProfilingBuffer->setIsInvalidated(true);

   // add buffers in working queue to free list
   discardFilledIProfilerBuffers();
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
         fprintf(fout, "%p CLASS %d %#" OMR_PRIxPTR " %s WEIGHT %d\n", tag, i, csInfo.getClazz(i), clazzSig, csInfo._weight[i]);
         }
      else
         {
         fprintf(fout, "%p CLASS %d %#" OMR_PRIxPTR " WEIGHT %d\n", tag, i, csInfo.getClazz(i), csInfo._weight[i]);
         }
      fflush(fout);
      }
   }





//
void *
TR_IPHashedCallSite::operator new (size_t size) throw()
   {
   memoryConsumed += (int32_t)size;
   return TR_IProfiler::allocator()->allocate(size, std::nothrow);
   }


uintptr_t CallSiteProfileInfo::getClazz(int index)
   {
   if (TR::Compiler->om.compressObjectReferences())
      //support for convert code, when it is implemented, "uncompress"
      return (uintptr_t)TR::Compiler->cls.convertClassOffsetToClassPtr((TR_OpaqueClassBlock *)(uintptr_t)_clazz[index]);
   else
      return (uintptr_t)_clazz[index]; //things are just stored as regular pointers otherwise
   }


void CallSiteProfileInfo::setClazz(int index, uintptr_t clazzPointer)
   {
   if (TR::Compiler->om.compressObjectReferences())
      {
      //support for convert code, when it is implemented, do compression
      TR_OpaqueClassBlock * compressedOffset = J9JitMemory::convertClassPtrToClassOffset((J9Class *)clazzPointer); //compressed 32bit pointer
      //if we end up with something in the top 32bits, our compression is no good...
      TR_ASSERT((!(0xFFFFFFFF00000000 & (uintptr_t)compressedOffset)), "Class pointer contains bits in the top word. Pointer given: %p Compressed: %p", clazzPointer, compressedOffset);
      _clazz[index] = (uint32_t)((uintptr_t)compressedOffset); //ditch the top zeros
      }
   else
      _clazz[index] = (uintptr_t)clazzPointer;
   }

uintptr_t
CallSiteProfileInfo::getDominantClass(int32_t &sumW, int32_t &maxW)
   {
   int32_t sumWeight = _residueWeight;
   int32_t maxWeight = 0;
   uintptr_t data = 0;

   for (int32_t i = 0; i < NUM_CS_SLOTS; i++)
      {
      if (!getClazz(i))
         continue;

      if (_weight[i] > maxWeight)
         {
         maxWeight = _weight[i];
         data = getClazz(i);
         }

      sumWeight += _weight[i];
      }
   sumW = sumWeight;
   maxW = maxWeight;
   return data;
   }

// Supporting code for dumping IProfiler data to stderr to track possible
// performance issues due to insufficient or wrong IProfiler info
// Code is currently inactive. To actually use one must issue
// iProfiler->dumpIPBCDataCallGraph(vmThread)
// in some part of the code (typically at shutdown time)
TR_AggregationHT::TR_AggregationHTNode::TR_AggregationHTNode(J9ROMMethod *romMethod, J9ROMClass *romClass, TR_IPBytecodeHashTableEntry *entry):
   _next(NULL), _romMethod(romMethod), _romClass(romClass)
   {
   _IPData = new (*TR_IProfiler::allocator()) TR_IPChainedEntry(entry);
   }

TR_AggregationHT::TR_AggregationHTNode::~TR_AggregationHTNode()
   {
   TR_IPChainedEntry *entry = getFirstIPEntry();
   while (entry)
      {
      TR_IPChainedEntry *nextEntry = entry->getNext();
      TR_IProfiler::allocator()->deallocate(entry);
      entry = nextEntry;
      }
   }

TR_AggregationHT::TR_AggregationHT(size_t sz) : _sz(sz), _numTrackedMethods(0)
   {
   // TODO: use scratch memory
   _backbone = new (*TR_IProfiler::allocator()) TR_AggregationHTNode*[sz];
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

TR_AggregationHT::~TR_AggregationHT()
   {
   for (int32_t bucket = 0; bucket < _sz; bucket++)
      {
      TR_AggregationHTNode *node = _backbone[bucket];
      while (node)
         {
         TR_AggregationHTNode *nextNode = node->getNext();
         node->~TR_AggregationHTNode();
         TR_IProfiler::allocator()->deallocate(node);
         node = nextNode;
         }
      }
   TR_IProfiler::allocator()->deallocate(_backbone);
   }

// Add the given cgEntry from the IP table into the aggregationHT
// The caller also provides the romMethod/romClass that contains the
// bytecode described by this cgEntry
void
TR_AggregationHT::add(J9ROMMethod *romMethod, J9ROMClass *romClass, TR_IPBytecodeHashTableEntry *cgEntry)
   {
   size_t index = hash(romMethod);
   // search the bucket for matching romMethod
   TR_AggregationHTNode *crtMethodNode = _backbone[index];
   for (; crtMethodNode; crtMethodNode = crtMethodNode->getNext())
      {
      if (crtMethodNode->getROMMethod() == romMethod)
         {
         // Add a new bc data point to the method entry we found; keep it sorted by pc
         TR_IPChainedEntry *newEntry = new (*TR_IProfiler::allocator()) TR_IPChainedEntry(cgEntry);
         if (!newEntry) // OOM
            {
            // printfs are ok since this method will be used for diagnostics only
            fprintf(stderr, "Cannot allocated memory. Incomplete info will be printed.\n");
            return;
            }
         TR_IPChainedEntry *crtEntry = crtMethodNode->getFirstIPEntry();
         TR_IPChainedEntry *prevEntry = NULL;
         while (crtEntry)
            {
            // Ideally we should not have two entries with the same pc in the
            // Iprofiler HT (the pc is the key in the HT). However, due to the
            // fact that we don't acquire any locks, such rare occurrences are
            // possible. We should ignore any such  events.
            if (crtEntry->getPC() == cgEntry->getPC())
               {
               TR_IPBCDataCallGraph *cg = cgEntry->asIPBCDataCallGraph();
               int32_t cnt = cg ? cg->getSumCount() : 0;
               fprintf(stderr, "We cannot find the same PC twice. PC=%" OMR_PRIuPTR " romMethod=%p sumCount=%d\n",
                       cgEntry->getPC(), romMethod, cnt);
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
      }
   // If my romMethod is not already in the HT let's add it
   if (!crtMethodNode)
      {
      // Add a new entry at the beginning
      TR_AggregationHTNode *newMethodNode = new (*TR_IProfiler::allocator()) TR_AggregationHTNode(romMethod, romClass, cgEntry);
      if (!newMethodNode || !newMethodNode->getFirstIPEntry()) // OOM
         {
         fprintf(stderr, "Cannot allocated memory. Incomplete info will be printed.\n");
         return;
         }
      newMethodNode->setNext(_backbone[index]);
      _backbone[index] = newMethodNode;
      _numTrackedMethods++;
      }
   }

// Callback for qsort to sort by methodName
int compareByMethodName(const void *a, const void *b)
   {
   return strcmp(((TR_AggregationHT::SortingPair *)a)->_methodName, ((TR_AggregationHT::SortingPair *)b)->_methodName);
   }

void
TR_AggregationHT::sortByNameAndPrint()
   {
   // Scan the aggregationTable and convert from romMethod to methodName so that
   // we can sort and print the information
   fprintf(stderr, "Creating the sorting array ...\n");
   SortingPair *sortingArray = (SortingPair *)TR_IProfiler::allocator()->allocate(sizeof(SortingPair) * numTrackedMethods(), std::nothrow);
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
         char *wholeName = (char *)TR_IProfiler::allocator()->allocate(len, std::nothrow);
         // If memory cannot be allocated, break
         if (!wholeName)
            {
            fprintf(stderr, "Cannot allocate memory. Incomplete data will be printed.\n");
            break;
            }
         snprintf(wholeName, len, "%.*s.%.*s%.*s",
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
      TR_IPChainedEntry *cgEntry = sortingArray[i]._IPdata->getFirstIPEntry();
      // Iterate through bytecodes with info from this method
      for (; cgEntry; cgEntry = cgEntry->getNext())
         {
         TR_IPBCDataCallGraph *ipbcCGData = cgEntry->getIPData()->asIPBCDataCallGraph();
         if (!ipbcCGData)
            continue;
         U_8* pc = (U_8*)ipbcCGData->getPC();

         size_t bcOffset = pc - (U_8*)J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
         fprintf(stderr, "\tOffset %" OMR_PRIuSIZE "\t", bcOffset);
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
               const char * s = utf8Data(J9ROMCLASS_CLASSNAME(TR::Compiler->cls.romClassOf((TR_OpaqueClassBlock*)cgData->getClazz(j))), len);
               fprintf(stderr, "\t\tW:%4u\tM:%#" OMR_PRIxPTR "\t%.*s\n", cgData->_weight[j], cgData->getClazz(j), len, s);
               }
            }
         fprintf(stderr, "\t\tW:%4u\n", cgData->_residueWeight);
         }
      }
   // Free the memory we allocated
   for (size_t i = 0; i < numTrackedMethods(); i++)
      if (sortingArray[i]._methodName)
         TR_IProfiler::allocator()->deallocate(sortingArray[i]._methodName);
   TR_IProfiler::allocator()->deallocate(sortingArray);
   }

// Given a bytecode PC, return the ROMMethod that contains that bytecode PC.
// Also return the romClass that contains the ROMMethod we found.
// This method is relatively expensive and should not be called on a critical path.
J9ROMMethod *
TR_IProfiler::findROMMethodFromPC(J9VMThread *vmThread, uintptr_t methodPC, J9ROMClass *&romClass)
  {
   J9JavaVM *javaVM = vmThread->javaVM;
   J9InternalVMFunctions *vmFunctions = javaVM->internalVMFunctions;

   J9ClassLoader* loader;
   romClass = vmFunctions->findROMClassFromPC(vmThread, (UDATA)methodPC, &loader);

   J9ROMMethod *currentMethod = J9ROMCLASS_ROMMETHODS(romClass);
   J9ROMMethod *desiredMethod = NULL;
   if (romClass)
      {
      // Find the method with the corresponding PC
      for (U_32 i = 0; i < romClass->romMethodCount; i++)
         {
         if (((UDATA)methodPC >= (UDATA)currentMethod)
             && ((UDATA)methodPC < (UDATA)J9_BYTECODE_END_FROM_ROM_METHOD(currentMethod)))
            {
            // found the method
            desiredMethod = currentMethod;
            break;
            }
         currentMethod = nextROMMethod(currentMethod);
         }
      }
   return desiredMethod;
   }

// All information that is stored in the IProfiler table will be stored
// in the indicated AggregationTable, aggregated by J9ROMMethod
// The `collectOnlyCallGraphEntries` parameter indicates whether we should
// only look at the callGraph entries used for virtual/interface invokes
void
TR_IProfiler::traverseIProfilerTableAndCollectEntries(TR_AggregationHT *aggregationHT, J9VMThread* vmThread, bool collectOnlyCallGraphEntries)
{
   J9JavaVM *javaVM = vmThread->javaVM;
   J9InternalVMFunctions *vmFunctions = javaVM->internalVMFunctions;
   TR_J9VMBase * fe = TR_J9VMBase::get(javaVM->jitConfig, vmThread);

   TR::VMAccessCriticalSection dumpCallGraph(fe); // prevent class unloading

   for (int32_t bucket = 0; bucket < TR::Options::_iProfilerBcHashTableSize; bucket++)
      {
      for (TR_IPBytecodeHashTableEntry *entry = _bcHashTable[bucket]; entry; entry = entry->getNext())
         {
         // Skip invalid entries
         if (entry->isInvalid() || invalidateEntryIfInconsistent(entry))
            continue;
         // Skip non-callgraph entries, if so desired
         if (collectOnlyCallGraphEntries && !entry->asIPBCDataCallGraph())
            continue;

         // Get the pc and find the method this pc belongs to
         J9ROMClass *romClass = NULL;
         J9ROMMethod * desiredMethod = findROMMethodFromPC(vmThread, entry->getPC(), romClass);
         if (desiredMethod)
            {
            // Add the information to the aggregationTable
            aggregationHT->add(desiredMethod, romClass, entry);
            }
         else
            {
            fprintf(stderr, "Cannot find RomMethod that contains pc=%p \n", (uint8_t*)entry->getPC());
            }
         }
      } // for each bucket
   }


// This method can be used to print to stderr in readable format all the IPBCDataCallGraph
// info from IProfiler (i.e. all sort of calls, checkcasts and instanceofs)
// This method will be executing on a single thread and it will acquire VM access as needed
// to prevent the GC from modifying the BC hashtable as we walk.
// Application threads may add new data to the IProfiler hashtable, but this is not an impediment.
// Temporary data structures will be allocated using persistent memory which will be deallocated
// at the end.
// Parameter: the vmThread it is executing on.
void
TR_IProfiler::dumpIPBCDataCallGraph(J9VMThread* vmThread)
   {
   fprintf(stderr, "Dumping info ...\n");
   TR_AggregationHT aggregationHT(TR::Options::_iProfilerBcHashTableSize);
   if (aggregationHT.getSize() == 0) // OOM
      {
      fprintf(stderr, "Cannot allocate memory. Bailing out.\n");
      return;
      }
   traverseIProfilerTableAndCollectEntries(&aggregationHT, vmThread, true/*collectOnlyCallGraphEntries*/);
   aggregationHT.sortByNameAndPrint();

   fprintf(stderr, "Finished dumping info\n");
   }

uintptr_t
TR_IProfiler::createBalancedBST(TR_IPBytecodeHashTableEntry **ipEntries, int32_t low, int32_t high, uintptr_t memChunk, TR_J9SharedCache *sharedCache)
   {
   if (high < low)
      return 0;

   TR_IPBCDataStorageHeader * storage = (TR_IPBCDataStorageHeader *) memChunk;
   int32_t middle = (high+low)/2;
   TR_IPBytecodeHashTableEntry *entry = ipEntries[middle];
   uint32_t bytes = entry->getBytesFootprint();
   entry->createPersistentCopy(sharedCache, storage, _compInfo->getPersistentInfo());

   uintptr_t leftChild = createBalancedBST(ipEntries, low, middle - 1, memChunk + bytes, sharedCache);
   if (leftChild)
      {
      TR_ASSERT(bytes < 1 << 8, "Error storing iprofile information: left child too far away"); // current size of left child
      storage->left = bytes;
      }

   uintptr_t rightChild = createBalancedBST(ipEntries, middle + 1, high, memChunk + bytes + leftChild, sharedCache);
   if (rightChild)
      {
      TR_ASSERT(bytes + leftChild < 1 << 16, "Error storing iprofile information: right child too far away"); // current size of right child
      storage->right = bytes+leftChild;
      }

   return bytes + leftChild + rightChild;
   }

// Persist all IProfiler entries into the SCC.
// This is done by aggregating all IProfiler entries per
// ROMMethod with the help of a TR_AggregationHT table.
// Then, for each ROMMethod with IP info we store the
// entries arranged as a BST (for fast retrieval later).
void
TR_IProfiler::persistAllEntries()
   {
   J9JavaVM * javaVM = _compInfo->getJITConfig()->javaVM;
   J9VMThread *vmThread = javaVM->internalVMFunctions->currentVMThread(javaVM);
   TR_J9VMBase * fe = TR_J9VMBase::get(_compInfo->getJITConfig(), vmThread, TR_J9VMBase::AOT_VM);
   TR_J9SharedCache *sharedCache = fe->sharedCache();
   static bool SCfull = false;

   if (!(TR::Options::sharedClassCache() && sharedCache))
      return;

   J9SharedClassConfig *scConfig = javaVM->sharedClassConfig;

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
      TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "IProfiler persisting all entries to SCC");

   int32_t entriesAlreadyPersisted = _STATS_entriesPersisted;

   try
      {
      TR::RawAllocator rawAllocator(javaVM);
      J9::SegmentAllocator segmentAllocator(MEMORY_TYPE_JIT_SCRATCH_SPACE | MEMORY_TYPE_VIRTUAL, *javaVM);
      J9::SystemSegmentProvider regionSegmentProvider(1 << 20, 1 << 20, TR::Options::getScratchSpaceLimit(), segmentAllocator, rawAllocator);
      TR::Region region(regionSegmentProvider, rawAllocator);
      TR_Memory trMemory(*_compInfo->persistentMemory(), region);

      TR_AggregationHT aggregationHT(TR::Options::_iProfilerBcHashTableSize);
      if (aggregationHT.getSize() == 0) // OOM
         {
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
            TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "IProfiler: Cannot allocate memory. Bailing out persisting all entries to SCC");
         return;
         }
      traverseIProfilerTableAndCollectEntries(&aggregationHT, vmThread);

      size_t methodIndex = 0;
      for (int32_t bucket = 0; bucket < aggregationHT.getSize(); bucket++)
         {
         for (TR_AggregationHT::TR_AggregationHTNode *node = aggregationHT.getBucket(bucket); node; node = node->getNext())
            {
            _STATS_methodPersistenceAttempts++;
            // If there is no more space, continue the loop to update the stats
            if (SCfull)
               {
               _STATS_methodNotPersisted_other++;
               continue;
               }
            J9ROMMethod* romMethod = node->getROMMethod();

            // Can only persist profile info if the method is in the shared cache
            if (!sharedCache->isROMMethodInSharedCache(romMethod))
               {
               _STATS_methodNotPersisted_classNotInSCC++;
               continue;
               }

            // If the method is already persisted, we don't need to do anything else
            unsigned char storeBuffer[1000];
            uint32_t bufferLength = sizeof(storeBuffer);
            J9SharedDataDescriptor descriptor;
            descriptor.address = storeBuffer;
            descriptor.length = bufferLength;
            descriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
            descriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
            IDATA dataIsCorrupt;
            const U_8 *found = scConfig->findAttachedData(vmThread, romMethod, &descriptor, &dataIsCorrupt);
            if (found)
               {
               _STATS_methodNotPersisted_alreadyStored++;
               continue;
               }
            // Count how many entries we have for this ROMMethod and how much space we need
            size_t numEntries = 0;
            size_t bytesFootprint = 0;
            TR_IPBytecodeHashTableEntry* ipEntries[1000]; // Capped size for number of profiled entries per method
            for (TR_AggregationHT::TR_IPChainedEntry *ipEntry = node->getFirstIPEntry(); ipEntry; ipEntry = ipEntry->getNext())
               {
               TR_IPBytecodeHashTableEntry *ipData = ipEntry->getIPData();

               if (!invalidateEntryIfInconsistent(ipData))
                  {
                  if (numEntries >= sizeof(ipEntries))
                     break; // stop here because we have too many entries for this method
                  // Check whether info can be persisted.
                  // Reasons for not being to include: locked entries, unloaded methods, target class not in SCC
                  // Note that canBePersisted() locks the entry
                  uint32_t canPersist = ipData->canBePersisted(sharedCache, _compInfo->getPersistentInfo());
                  if (canPersist == IPBC_ENTRY_CAN_PERSIST)
                     {
                     bytesFootprint += ipData->getBytesFootprint();
                     ipEntries[numEntries] = ipData;
                     numEntries++;
                     }
                  else
                     {
                     // Stats for why we cannot persist
                     switch (canPersist)
                        {
                        case IPBC_ENTRY_PERSIST_LOCK:
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
               else // Entry is invalid
                  {

                  }
               }
            // Attempt to store numEntries whose PCs are stored in pcEntries
            if (numEntries)
               {
               void * memChunk = trMemory.allocateMemory(bytesFootprint, stackAlloc);
               // We already have the data
               intptr_t bytes = createBalancedBST(ipEntries, 0, numEntries-1, (uintptr_t) memChunk, sharedCache);
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
                  //bytesToPersist = bytesFootprint;
   #ifdef PERSISTENCE_VERBOSE
                  fprintf(stderr, "\tNot Persisted: SCC full\n");
   #endif
                  }
               // Release all entries in ipEntries[] that were locked by us
               for (uint32_t i = 0; i < numEntries; i++)
                  {
                  TR_IPBCDataCallGraph *cgEntry = ipEntries[i]->asIPBCDataCallGraph();
                  if (cgEntry)
                     cgEntry->releaseEntry();
                  }
               }
            else // Nothing can be persisted for this method
               {
#ifdef PERSISTENCE_VERBOSE
               fprintf(stderr, "\tNo entry can be persisted for this method (locked/invalid/notOnSCC) \n");
#endif
               }
            }
         }
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "IProfiler: persisted a total of %d entries, of which %d were persisted at shutdown",
                                        _STATS_entriesPersisted, _STATS_entriesPersisted - entriesAlreadyPersisted);
      }
   catch (const std::exception &e)
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseIProfilerPersistence))
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "IProfiler: Failed to store all entries to SCC");
      }
   }

// Generates histograms IP info related to virtual/interface calls.
// (1) Histogram for the "weight" of the dominant target (as a percentage of all targets)
// (2) Histogram for the number of distinct targets of a particular call
// To be used as diagnostic, not in production, at shutdown time (see JITShutdown()).
void
TR_IProfiler::traverseIProfilerTableAndGenerateHistograms(J9JITConfig *jitConfig)
{
   TR_J9VMBase *fe = TR_J9VMBase::get(jitConfig, NULL);

   TR_StatsHisto<20> maxWeightHisto("Histo max weight of target", 0, 100);
   TR_StatsHisto<3> numTargetsHisto("Histo num profiled targets", 1, 4);

   //TR::VMAccessCriticalSection dumpCallGraph(fe); // prevent class unloading

   for (int32_t bucket = 0; bucket < TR::Options::_iProfilerBcHashTableSize; bucket++)
      {
      for (TR_IPBytecodeHashTableEntry *entry = _bcHashTable[bucket]; entry; entry = entry->getNext())
         {
         // Skip invalid entries
         if (entry->isInvalid() || invalidateEntryIfInconsistent(entry))
            continue;
         // Skip the artificial entries
         if (!entry->getCanPersistEntryFlag())
            continue;
         // Skip non-callgraph entries
         TR_IPBCDataCallGraph *cgEntry = entry->asIPBCDataCallGraph();
         if (!cgEntry)
            continue;
         CallSiteProfileInfo *cgData = cgEntry->getCGData();

         uint32_t sumWeight = 0;
         uint32_t maxWeight = 0;
         int maxIndex = -1;
         int numTargets = 0;
         for (int j = 0; j < NUM_CS_SLOTS; j++)
            {
            sumWeight += cgData->_weight[j];
            if (maxWeight < cgData->_weight[j])
               {
               maxWeight = cgData->_weight[j];
               maxIndex = j;
               }
            if (cgData->getClazz(j) && cgData->_weight[j] > 0)
               numTargets++;
            }
         sumWeight += cgData->_residueWeight;
         if (cgData->_residueWeight)
            numTargets++;
         if (sumWeight > 1)
            numTargetsHisto.update(numTargets);
         if (numTargets == 0)
            {
            fprintf(stderr, "Entry with no weight\n");
            for (int j = 0; j < NUM_CS_SLOTS; j++)
               fprintf(stderr, "Class %" OMR_PRIuPTR ", weight=%u\n", cgData->getClazz(j), cgData->_weight[j]);
            }

         double percentage = 0;
         if (maxIndex != -1)
            percentage = maxWeight*100.0/(double)sumWeight;
         else
            fprintf(stderr, "maxIndex is 1\n");
         if (sumWeight > 1)
            maxWeightHisto.update(percentage);
         if (sumWeight > 1)
            {
            if (numTargets == 1)
               {
               if (percentage < 100.0)
                  {
                  fprintf(stderr, "Single target but percentage is %f  maxWeight=%u maxIndex=%d sumWeight=%u\n", percentage, maxWeight, maxIndex, sumWeight);
                  }
               }
            }
         }
      } // for each bucket
      maxWeightHisto.report(stderr);
      numTargetsHisto.report(stderr);
   }

#if defined(J9VM_OPT_CRIU_SUPPORT)
void
TR_IProfiler::suspendIProfilerThreadForCheckpoint()
   {
   _compInfo->acquireCompMonitor(_iprofilerThread);
   if (_compInfo->getCRRuntime()->shouldSuspendThreadsForCheckpoint())
      {
      // Must acquire this with the comp monitor in hand to ensure
      // consistency with the checkpointing thread.
      _iprofilerMonitor->enter();

      // At this point the IProfiler Thread can only be in state IPROF_THR_SUSPENDING;
      // the checkpointing thread will either have set this state while the iprofiler
      // thread was waiting for work or when it was processing some buffer. It is not
      // possible for the the Shutdown Thread to have set the state to
      // IPROF_THR_STOPPING because shouldSuspendThreadsForCheckpoint() returned true.
      TR_ASSERT_FATAL(getIProfilerThreadLifetimeState() == TR_IProfiler::IPROF_THR_SUSPENDING, "IProfiler Lifetime State is %d!", getIProfilerThreadLifetimeState());

      // Update the IProfiler Lifetime State
      setIProfilerThreadLifetimeState(TR_IProfiler::IPROF_THR_SUSPENDED);

      // Notify the checkpointing thread about the state change.
      //
      // Note, unlike the checkpointing thread, this thread does NOT
      // release the iprofiler monitor before acquring the CR monitor.
      // This ensures that the IProfiler Thread Lifetime State does not
      // change because of something like Shutdown. However, this
      // can only cause a deadlock if the checkpointing thread
      // decides to re-acquire the iprofiler monitor with the CR monitor
      // in hand.
      _compInfo->getCRRuntime()->acquireCRMonitor();
      _compInfo->getCRRuntime()->getCRMonitor()->notifyAll();
      _compInfo->getCRRuntime()->releaseCRMonitor();

      if (TR::Options::isAnyVerboseOptionSet())
         TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Suspending IProfiler Thread for Checkpoint");

      // Release the comp monitor before suspending.
      _compInfo->releaseCompMonitor(_iprofilerThread);

      // Wait until restore, at which point the lifetime state
      // will be TR_IProfiler::IPROF_THR_RESUMING
      while (getIProfilerThreadLifetimeState() == TR_IProfiler::IPROF_THR_SUSPENDED)
         {
         _iprofilerMonitor->wait();
         }

      if (TR::Options::isAnyVerboseOptionSet())
         TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Resuming IProfiler Thread from Checkpoint");

      // Release the iprofiler monitor before reacquring both the
      // comp monitor and iprofiler monitor. This is necessary to
      // ensure consistency with the checkpointing thread.
      _iprofilerMonitor->exit();
      _compInfo->acquireCompMonitor(_iprofilerThread);
      _iprofilerMonitor->enter();

      // Ensure the sampler thread was resumed because of a restore
      // rather than something else (such as shutdown)
      if (getIProfilerThreadLifetimeState() == TR_IProfiler::IPROF_THR_RESUMING)
         {
         if (TR::Options::isAnyVerboseOptionSet())
            TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Resetting IProfier Thread Lifetime State");
         setIProfilerThreadLifetimeState(TR_IProfiler::IPROF_THR_INITIALIZED);
         }
      else
         {
         if (TR::Options::isAnyVerboseOptionSet())
            TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "IProfiler Thread Lifetime State is %p which is not %p!", getIProfilerThreadLifetimeState(), TR_IProfiler::IPROF_THR_RESUMING);
         }

      // Release the reacquired iprofier thread monitor.
      _iprofilerMonitor->exit();
      }
   _compInfo->releaseCompMonitor(_iprofilerThread);
   }
#endif

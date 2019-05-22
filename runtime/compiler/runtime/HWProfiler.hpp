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

#ifndef HWPROFILER_HPP
#define HWPROFILER_HPP

#include <stdint.h>
#include "jilconsts.h"
#include "j9.h"
#include "j9cp.h"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/VMJ9.h"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "infra/Monitor.hpp"
#include "infra/Link.hpp"
#include "optimizer/Inliner.hpp"

#define HASH_TABLE_SIZE  138007 // Sufficiently big to prevent collisions on large apps such as WAS.

#define METADATA_MAPPING_EYECATCHER 0xBC1AFFFF

/* Redefine until these flags are pushed to j9generated.h */
#define J9PORT_RI_ENABLED ((U_32) 0x1)
#define J9PORT_RI_INITIALIZED ((U_32) 0x2)

#if defined(J9VM_JIT_RUNTIME_INSTRUMENTATION)
#define IS_THREAD_RI_INITIALIZED(vmThread) ((vmThread->riParameters->flags & J9PORT_RI_INITIALIZED) != 0)
#define IS_THREAD_RI_ENABLED(vmThread) ((vmThread->riParameters->flags & J9PORT_RI_ENABLED) != 0)
#else
#define IS_THREAD_RI_INITIALIZED(vmThread) false
#define IS_THREAD_RI_ENABLED(vmThread) false
#endif

class TR_OpaqueMethodBlock;
namespace TR { class Monitor; }

class TR_HWPRecord
   {
public:
   TR_HWPRecord *next;
   uintptrj_t pc;
   uint8_t *ia;
   uint32_t count;
   TR_HWPRecord ()
      {
      next = NULL;
      pc = 0;
      ia = NULL;
      count = 0;
      }
   };

/**
 * Container class for HW Profiler buffers
 */
class HWProfilerBuffer : public TR_Link0<HWProfilerBuffer>
   {
   public:
      uint8_t *getBuffer() {return _buffer;}
      void setBuffer(U_8* buffer) { _buffer = buffer; }
      uintptrj_t getSize() {return _size;}
      void setSize(UDATA size) {_size = size;}
      uintptrj_t getBufferFilledSize() {return _bufferFilledSize;}
      void setBufferFilledSize(UDATA size) {_bufferFilledSize = size;}
      bool isValid() const { return !_isInvalidated; }
      void setIsInvalidated(bool b) { _isInvalidated = b; }
      uint32_t getType() { return _type; }
      void setType(uint32_t t) { _type = t; }
      
   private:
      uint8_t *_buffer;
      uintptrj_t _size;
      uintptrj_t _bufferFilledSize;
      volatile bool _isInvalidated;
      uint32_t _type;
   };

/**
 * HW Profiler Base Class. Each platform that supports HW Profiling extends this
 * for platform specific purposes. This class is used to encapsulate the code
 * that is common between the different platforms.
 */
class TR_HWProfiler
   {
public:
   
   /**
    * Constructor.
    * @param jitConfig the J9JITConfig
    */
   TR_HWProfiler (J9JITConfig *jitConfig);

   // --------------------------------------------------------------------------------------
   // HW Profiler Management Methods
   
   /**
    * Is hardware profiling available on the current thread.
    * @param vmThread The VM thread to check for profiling.
    * @return true if profiling is supported; false otherwise.
    */
   virtual bool isHWProfilingAvailable(J9VMThread *vmThread);

   /**
    * Set whether or not HW profiling is supported.
    * @param supported - whether hardware profiling is supported.
    */
   virtual void setHWProfilingAvailable(bool supported);

   /**
    * Pure virtual method to initialize hardware profiling on given app thread.
    * @param vmThread The VM thread to initialize profiling.
    * @return true if initialization is successful; false otherwise.
    */
   virtual bool initializeThread(J9VMThread *vmThread) = 0;

   /**
    * Pure virtual method to deinitialize hardware profiling on given app thread
    * @param vmThread The VM thread to deinitialize profiling.
    * @return true if deinitialization is successful; false otherwise.
    */
   virtual bool deinitializeThread(J9VMThread *vmThread) = 0;

   /**
    * Start a thread to process profiling buffers
    * @param javaVM The javaVM for the current VM Instance.
    */
   void startHWProfilerThread(J9JavaVM *javaVM);

   /**
    * Stop the thread used to process profiling buffers
    * @param javaVM The javaVM for the current VM Instance.
    */
   void stopHWProfilerThread(J9JavaVM *javaVM);

   /**
    * Return the J9VMThread for the hardware profiling thread.
    * @return Pointer to J9VMThread representing the hardware profiling processing thread.
    */
   J9VMThread* getHWProfilerThread() { return _hwProfilerThread; }

   /**
    * Set the J9VMThread for the hardware profiling thread to a given J9VMThread
    * @param vmThread The J9VMThread to set for hardware profiling
    */
   void setHWProfilerThread(J9VMThread* thread) { _hwProfilerThread = thread; }

   /**
    * Return the OSThread for the hardware profiling thread.
    * @return The OS Thread representing the hardware profiling processing thread.
    */
   j9thread_t getHWProfilerOSThread() { return _hwProfilerOSThread; }

   /**
    * Return the HW Profiler monitor
    * @return The TR::Monitor for hardware profiler
    */
   TR::Monitor *getHWProfilerMonitor() { return _hwProfilerMonitor; }

   /**
    * A volatile field which tells whether HW Profiler thread attachment succeeded.
    * @return whether we've successfully attached to the hw profiler thread with hwProfilerThreadProc.
    */
   bool getAttachAttempted() const { return _hwProfilerThreadAttachAttempted; }
   /**
    * Set volatile field which tells whether HW Profiler thread attachment succeeded.
    * @param bool Whether we tried to attach to hw profiler thread with hwProfilerThreadProc.
    */
   void setAttachAttempted(bool b) { _hwProfilerThreadAttachAttempted = b; }
   
   /**
    * Set the flag to denote the HW Profiler thread has exited
    */
   void setHWProfilerThreadExitFlag() { _hwProfilerThreadExitFlag = true; }
   
   
   // --------------------------------------------------------------------------------------
   // HW Profiler Buffer Processing Methods
   
   /**
    * Pure virtual method to process buffers from a given app thread.
    * @param vmThread The VM thread to query
    * @param fe The Front End
    * @return false if the thread cannot be used for HW Profiling; true otherwise.
    */
   virtual bool processBuffers(J9VMThread *vmThread, TR_J9VMBase *fe) = 0;

   /**
    * Pure virtual method to process the data in the buffers.
    * @param vmThread The VM thread
    * @param dataStart The start of the data buffer.
    * @param size      Size of the data buffer.
    * @param bufferFilledSize The amount of the buffer that is filled
    * @param dataTag   An optional platform-dependent tag for the data in the buffer.
    */
   virtual void processBufferRecords(J9VMThread *vmThread, 
                                     uint8_t *dataStart, 
                                     uintptrj_t size, 
                                     uintptrj_t bufferFilledSize, 
                                     uint32_t dataTag = 0) = 0;

   /**
    * Pure virtual method to allocate a buffer for HW Profiling.
    * @param size The size of the buffer to be allocated
    * @return a pointer to the buffer
    */
   virtual void *allocateBuffer(uint64_t size) = 0;
   
   /**
    * Pure virtual method to free a buffer allocated for HW Profiling.
    * @param buffer The buffer to be freed
    * @param Optional parameter for the size of the buffer to be freed (needed depending on the implementation)
    */
   virtual void freeBuffer(void * buffer, uint64_t size = 0) = 0;
   
   /**
    * Add given buffer to the working queue to be processed.  Returns
    * start address of a new buffer.
    * @param dataStart The start address of current data buffer.
    * @param size      The size of the data buffer.
    * @param dataTag   An optional platform-dependent tag for the data in the buffer.
    * @param allocateNewBuffer Option parameter to control whether or not to allocate a new buffer
    * @return  Start Address of a new data buffer.
    */
   U_8 * swapBufferToWorkingQueue(const uint8_t* dataStart, 
                                  UDATA size, 
                                  UDATA bufferFilledSize, 
                                  uint32_t dataTag = 0, 
                                  bool allocateNewBuffer = true);
   
   /**
    * Called by GC on class unloading. This method invalidates buffers on the waiting list
    * but also the buffer that might be currently under processing.
    * This method should not be called if profiling thread is not created
    */
   void invalidateProfilingBuffers();

   /**
    * Method executed by the hardware profiling thread to process RI buffers.
    */
   void processWorkingQueue();

   /**
    * Returns the current buffer being processed by the Profiling thread.  Used by
    * stopProfilingThread to free up last special buffer that terminates the thread.
    * @return The current/last buffer being processed by Profiling thread.
    */
   HWProfilerBuffer *getCurrentBufferBeingProcessed() { return _currentBufferBeingProcessed; }

   /**
    * Set current buffer being processed by Profiling thread.
    * @param buffer The buffer being processed.
    */
   void setCurrentBufferBeingProcessed(HWProfilerBuffer *buffer) { _currentBufferBeingProcessed = buffer; }
   
   
   // --------------------------------------------------------------------------------------
   // HW Profiler Compilation Control Methods
   
   /**
    * This method is used to determine whether or not a method should be recompiled.
    * The idea is that in each persistent body info, there exists two counters;
    * the "startCount" and the "count". There is also a global counter of instructions sampled.
    * Recompilation decisions are based on the ratio: count / (totalCount - startCount) .
    * @param bodyInfo The TR_PersistentJittedBodyInfo
    * @param startPC The startPC of the method that got a tick
    * @param startCount Used to denote the beginning of the recompilation interval
    * @param count Used to determine the number of ticks the method received
    * @param totalCount Used to denote the end of the recompilation interval
    * @param fe The FrontEnd Object
    * @param vmThread The J9VMThread object
    * @return Returns true if a new interval is needed; else false.
    */
   bool recompilationLogic(TR_PersistentJittedBodyInfo *bodyInfo, 
                           void *startPC, 
                           uint64_t startCount,
                           uint64_t count, 
                           uint64_t totalCount, 
                           TR_FrontEnd *fe, 
                           J9VMThread *vmThread);

   /**
    * Sets the Buffer Processing State in order to control when to process buffers:
    * negative number means do not process any buffers, 0 means process every buffer,
    * positive number n means process every nth buffer.
    * @param state The value of the Buffer Processing State
    */
   void setProcessBufferState(int32_t state) {_hwProfilerProcessBufferState = state; }
   
   /**
    * Returns the current Buffer Processing State
    * @return The value of the Buffer Processing State
    */
   int32_t getProcessBufferState() {return _hwProfilerProcessBufferState; }
   
   /**
    * Turns off Buffer Processing depending on how many recompilation decisions have been made.
    * The following method is prone to race conditions.
    * There is a minor race condition because _recompDecisionsTotal and _recompDecisionsYes
    * can be incremented while another thread tries to access them and a major
    * race condition because _recompDecisionsTotalStart _recompDecisionsYesStart are set to
    * distinctively new values while another thread reads them. We can solve the later
    * by allowing only one thread to execute this method. This thread could be the
    * sampling thread or the HW profiling thread. We prefer the sampling thread because
    * we can execute the method only once in a while (every 0.5 seconds)
    * @return true if buffer processing is turned off; false otherwise
    */
   bool checkAndTurnBufferProcessingOff();
   
   /**
    * Turns on Buffer Processing depending on the queue size or the number of downgrades.
    * @param true if buffer processing is turned on; false otherwise
    */
   bool checkAndTurnBufferProcessingOn();
   
   /**
    * Turns off Buffer Processing.
    */
   void turnBufferProcessingOffTemporarily();
   
   /**
    * Restores the Buffer Processing State to its original value.
    */
   void restoreBufferProcessingFunctionality();
   
   /**
    * Returns the number of downgrades since turning off buffer processing
    * @return the number of downgrades since turning off buffer processing
    */
   uint32_t getNumDowngradesSinceTurnedOff() const { return _numDowngradesSinceTurnedOff; }
   
   /**
    * Increments the number of downgrades since turning off buffer processing
    */
   void incNumDowngradesSinceTurnedOff() { _numDowngradesSinceTurnedOff++; }
   
   /**
    * Returns the Compilation Queue Size Threshold needed to downgrade methods
    * @return the Compilation Queue Size Threshold needed to downgrade methods
    */
   int32_t getQSzThresholdToDowngrade() const { return _qszThresholdToDowngrade; }
   
   /**
    * Sets the Compilation Queue Size Threshold needed to downgrade methods.
    * @param v the Compilation Queue Size Threshold
    */
   void setQSzThresholdToDowngrade(int32_t v) { _qszThresholdToDowngrade = v; }
   
   /**
    * Gets the Expired flag.
    * @return the Expired flag
    */
   bool isExpired() const { return _expired; }
   
   /**
    * Sets the Expired flag; once this is set the HW Profiler will not be used anymore.
    */
   void setExpired() { _expired = true; } // no way to turn it back on
   
   
   // --------------------------------------------------------------------------------------
   // HW Profiler Miscellaneous Helper Methods
   
   /**
    * Used to Remove all Hash Table Entries stored in the HW Profiler.
    * Not used by Z or P.
    */
   virtual void releaseAllEntries() {}

   /**
    * Prints out HW Profiler stats. Platform specific implementations may print out their
    * own stats and then call this method
    */
   virtual void printStats();
   
   /**
    * Used to get the Bytecode PC from the BytecodeInfo.
    * @param node the TR::Node
    * @param comp The TR::Compilation object
    * @return the Bytecode PC
    */
   uintptrj_t getPCFromBCInfo(TR::Node *node, TR::Compilation *comp);
   
   /**
    * Used to get the Bytecode PC from the J9Method and Bytecode Index.
    * @param method The TR_OpaqueMethodBlock
    * @param byteCodeIndex The Bytecode Index
    * @param comp the TR::Compilation object
    * @return the Bytecode PC
    */
   uintptrj_t getPCFromMethodAndBCIndex(TR_OpaqueMethodBlock *method, 
                                        uint32_t byteCodeIndex, 
                                        TR::Compilation * comp);
   
   /**
    * Used to get the Ram Method from the Bytecode Info.
    * @param bcInfo the TR_ByteCodeInfo passed in by reference
    * @param comp The TR::Compilation object
    * @returns the ram method (TR_OpaqueMethodBlock)
    */
   TR_OpaqueMethodBlock *getMethodFromBCInfo(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp);
   
   /**
    * Used to create HW Profiler Records that will be registered against the HW Profiler
    * in order to do Value Profiling among other things.
    * @param comp the TR::Compilation object
    */
   void createRecords(TR::Compilation *comp);
   
   /**
    * Used to register the HW Profiler Records against the HW Profiler.
    * For Virtual Dispatch, the mapping between Bytecode PC and Jit Instruction Address
    * is stored in the J9JITExceptionTable of that method body.
    * @param metaData the J9JITExceptionTable
    * @param comp the TR::Compilation object
    */
   void registerRecords(J9JITExceptionTable *metaData, TR::Compilation *comp);
   
   /**
    * Used to get the Bytecode PC from the JIT Instruction Address using the mapping stored
    * in the J9JITExceptionTable.
    * @param vmThread the J9VMTHread
    * @param IA the JIT Instruction Address
    * @return the Bytecode PC
    */
   uintptrj_t getBytecodePCFromIA(J9VMThread *vmThread, uint8_t *IA);
   
   /**
    * Used to create a TR_HWPBytecodePCToIAMap struct that will be stored in the
    * J9JITExceptiontable.
    * @param ia the JIT Instruction Address
    * @param bcIndex the Bytecode Index
    * @param method the ram method
    * @param comp the TR::Compilation object
    * @return A TR_HWPBytecodePCToIAMap struct by value
    */
   TR_HWPBytecodePCToIAMap createBCMap(uint8_t *ia, 
                                       uint32_t bcIndex, 
                                       TR_OpaqueMethodBlock *method, 
                                       TR::Compilation *comp);
   
   /**
    * Used to update the total memory used by the mapping stored in the J9JITExceptionTable.
    * @param size the size of the memory used by the current mapping
    */
   void updateMemoryUsedByMetadataMapping(uint32_t size) { _totalMemoryUsedByMetadataMapping += size; }
   
   protected:
   
   /**
    * A static method that implements a hashing function used for the Profiling Hashtable.
    * Not used by P or Z.
    * @param pc The JIT Instruction Address
    * @return the hash table index
    */
   static int32_t IAHash (uintptrj_t pc);
   
   // JIT Config Cache
   J9JITConfig                    *_jitConfig;
   
   // TR_Compilation cache
   TR::CompilationInfo             *_compInfo;

   // Profiler thread
   j9thread_t                      _hwProfilerOSThread;
   J9VMThread                     *_hwProfilerThread;
   // Profiler monitor
   TR::Monitor                    *_hwProfilerMonitor;

   // Whether we were able to attach to hw profiler thread with hwProfilerThreadProc.
   volatile bool                   _hwProfilerThreadAttachAttempted;
   // Flag used to mark whether HW Profiler has exited
   volatile bool                   _hwProfilerThreadExitFlag;
   
   bool                            _isHWProfilingAvailable;

   // Buffer Pools
   TR_LinkHead0<HWProfilerBuffer>   _freeBufferList;
   TR_LinkHead0<HWProfilerBuffer>   _workingBufferList;
   HWProfilerBuffer                *_workingBufferTail;

   // Current Buffer being processed by profiling thread.
   HWProfilerBuffer                *_currentBufferBeingProcessed;

   // Number of buffers outstanding to be processed
   volatile int32_t                _numOutstandingBuffers;
   
   uint32_t                        _bufferFilledSum;
   uint32_t                        _bufferSizeSum;
   uint32_t                        _numBuffersCompletelyFilled;
   
   uint64_t                        _recompilationInterval;
   TR_Hotness                      _lastOptLevel;
   float                           _warmOptLevelThreshold;
   float                           _reducedWarmOptLevelThreshold;
   float                           _aotWarmOptLevelThreshold;
   float                           _hotOptLevelThreshold;
   float                           _scorchingOptLevelThreshold;
   uint64_t                        _numRecompilationsInduced;
   uint64_t                        _numReducedWarmRecompilationsInduced;
   uint64_t                        _numReducedWarmRecompilationsUpgraded;
   uint64_t                        _recompDecisionsTotal;
   uint64_t                        _recompDecisionsYes;
   uint64_t                        _recompDecisionsTotalStart;
   uint64_t                        _recompDecisionsYesStart;
   uint32_t                        _numDowngradesSinceTurnedOff;
   
   // negative value means buffer processing is disabled
   // 0 means buffer processing happens every time
   // positive value 'n' means process buffer every 'n' times
   int32_t                         _hwProfilerProcessBufferState;
   int32_t                         _qszThresholdToDowngrade;
   bool                            _expired; // RI was turned off and never to be used again

   uint64_t                        _numRequests;
   uint64_t                        _numRequestsSkipped;
   
   uint64_t                        _totalMemoryUsedByMetadataMapping;
   
public:
   static uint32_t                 _STATS_TotalBuffersProcessed;
   static uint32_t                 _STATS_BuffersProcessedByAppThread;
   static uint64_t                 _STATS_TotalEntriesProcessed;
   static uint32_t                 _STATS_TotalInstructionsTracked;
   static uint32_t                 _STATS_NumCompDowngradesDueToRI;
   static uint32_t                 _STATS_NumUpgradesDueToRI;
   };
#endif


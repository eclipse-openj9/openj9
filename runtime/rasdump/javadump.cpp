/*******************************************************************************
 * Copyright (c) 2003, 2020 IBM Corp. and others
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

/* Includes */
#include <string.h>
#include <stdlib.h>
#ifdef WIN32
#include <malloc.h>
#elif defined(LINUX) || defined(AIXPPC)
#include <alloca.h>
#elif defined(J9ZOS390)
#include <stdlib.h>
#endif
#include "rasdump_internal.h"
#include "j2sever.h"
#include "HeapIteratorAPI.h"
#include "j9dmpnls.h"
#include "omrlinkedlist.h"
#include "j9protos.h"
#include "TextFileStream.hpp"
#include "rommeth.h"
#include "j9dump.h"
#include "omrthread.h"
#include "avl_api.h"
#include "shcflags.h"
#include "j9port.h"
#include "sharedconsts.h"
#include "omrgcconsts.h"
#include "omr.h"
#include "omrutilbase.h"
#include "j9version.h"
#include "vendor_version.h"

#include "zip_api.h"

#include <limits.h>
#include "ute.h"

#include "ut_j9dmp.h"

#if defined(J9VM_ENV_DATA64)
#define SEGMENT_HEADER             "NULL           segment            start              alloc              end                type       size\n"
#else
#define SEGMENT_HEADER             "NULL           segment    start      alloc      end        type       size\n"
#endif

/* Format specifiers for printing memory quantities. */
#define FORMAT_SIZE_DECIMAL "%*zu"
#define FORMAT_SIZE_HEX "0x%.*zX"

#define HIST_TYPE_GC 1
#define HIST_TYPE_CT 2

/* Safety margin for stack allocation of AVL tree for thread walk */
#define STACK_SAFETY_MARGIN 25000

/* Callback Function prototypes */
UDATA writeFrameCallBack          (J9VMThread* vmThread, J9StackWalkState* state);
UDATA writeExceptionFrameCallBack (J9VMThread* vmThread, void* userData, J9ROMClass* romClass, J9ROMMethod* romMethod, J9UTF8* sourceFile, UDATA lineNumber, J9ClassLoader* classLoader);
void  writeLoaderCallBack         (void* classLoader, void* userData);
void  writeLibrariesCallBack      (void* classLoader, void* userData);
void  writeClassesCallBack        (void* classLoader, void* userData);
static UDATA outerMemCategoryCallBack (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state);
static UDATA innerMemCategoryCallBack (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state);

/* Function prototypes */
static jvmtiIterationControl heapIteratorCallback   (J9JavaVM* vm, J9MM_IterateHeapDescriptor*   heapDescriptor,    void* userData);
static jvmtiIterationControl spaceIteratorCallback  (J9JavaVM* vm, J9MM_IterateSpaceDescriptor*  spaceDescriptor,   void* userData);
static jvmtiIterationControl regionIteratorCallback (J9JavaVM* vm, J9MM_IterateRegionDescriptor* regionDescription, void* userData);
static UDATA getObjectMonitorCount(J9JavaVM *vm);
static UDATA getAllocatedVMThreadCount (J9JavaVM *vm);

/* sig_protect functions and handlers */
extern "C" {
UDATA protectedWriteSection         (struct J9PortLibrary *, void *);
UDATA handlerWriteSection           (struct J9PortLibrary *, U_32, void *, void *);
UDATA protectedStartDoWithSignal    (struct J9PortLibrary *, void *);
UDATA protectedStartDo              (struct J9PortLibrary *, void *);
UDATA protectedNextDo               (struct J9PortLibrary *, void *);
UDATA protectedWalkJavaStack        (struct J9PortLibrary *, void *);
UDATA protectedGetVMThreadName      (struct J9PortLibrary *, void *);
UDATA protectedGetVMThreadObjectState (struct J9PortLibrary *, void *);
UDATA protectedGetVMThreadRawState  (struct J9PortLibrary *, void *);
UDATA handlerJavaThreadWalk         (struct J9PortLibrary *, U_32, void *, void *);
UDATA handlerNativeThreadWalk       (struct J9PortLibrary *, U_32, void *, void *);
UDATA handlerGetVMThreadName        (struct J9PortLibrary *, U_32, void *, void *);
UDATA handlerGetVMThreadObjectState (struct J9PortLibrary *, U_32, void *, void *);
UDATA handlerGetVMThreadRawState    (struct J9PortLibrary *, U_32, void *, void *);
UDATA protectedWriteGCHistoryLines  (struct J9PortLibrary *, void *);
UDATA protectedIterateStackTrace    (struct J9PortLibrary *, void *);
UDATA protectedWriteThreadBlockers  (struct J9PortLibrary *, void *);
UDATA protectedGetOwnedObjectMonitors  (struct J9PortLibrary *, void *);
UDATA protectedWriteJavaLangThreadInfo (struct J9PortLibrary *, void *);
UDATA protectedWriteThreadsWithNativeStacks (struct J9PortLibrary *, void *);
UDATA protectedWriteThreadsJavaOnly (struct J9PortLibrary *, void *);
UDATA protectedWriteThreadsUsageSummary (struct J9PortLibrary *, void *);
UDATA handlerWriteThreadBlockers    (struct J9PortLibrary *, U_32, void *, void *);
UDATA handlerGetThreadsUsageInfo    (struct J9PortLibrary *, U_32, void *, void *);
UDATA handlerGetOwnedObjectMonitors (struct J9PortLibrary *, U_32, void *, void *);
UDATA handlerIterateStackTrace      (struct J9PortLibrary *, U_32, void *, void *);
UDATA handlerWriteJavaLangThreadInfo(struct J9PortLibrary *, U_32, void *, void *);
UDATA handlerWriteStacks            (struct J9PortLibrary *, U_32, void *, void *);

/* associated structures for passing arguments are below the JavaCoreDumpWriter declaration */
}

static IDATA vmthread_comparator(struct J9AVLTree *tree, struct J9AVLTreeNode *insertNode, struct J9AVLTreeNode *walkNode);
static IDATA vmthread_locator(struct J9AVLTree *tree, UDATA tid , struct J9AVLTreeNode *walkNode);

/* Functions used by hash table prototypes */
static UDATA lockHashFunction(void* key, void* user);
static UDATA lockHashEqualFunction(void* left, void* right, void* user);

static UDATA rasDumpPreemptLock = 0;

typedef struct regioniterationblock {
	bool _newIteration;
	const void *_regionStart;
	UDATA _regionSize;
} regioniterationblock;

typedef struct vmthread_avl_node {
	J9AVLTreeNode node;
	J9VMThread *vmthread;
	j9object_t lockObject;
	J9VMThread *lockOwner;
	UDATA vmThreadState;
	UDATA javaThreadState;
	UDATA javaPriority;
} vmthread_avl_node;

typedef struct blocked_thread_record {
	omrthread_monitor_t monitor;
	J9VMThread *waitingThread;
	UDATA waitingThreadState;
} blocked_thread_record;

typedef struct memcategory_total {
	U_32 *category_bitmask;
	UDATA liveBytes;
	UDATA liveAllocations;
	U_32 codeToMatch;
	BOOLEAN codeMatched;
} memcategory_total;

typedef struct memcategory_data_frame
{
	U_32 category;
	UDATA liveBytes;
	UDATA liveAllocations;
} memcategory_data_frame;

/* Macros for working with the category_bitmask in the memcategory_total structure. The range of category codes is not contiguous, so we have
 * to map entries from the end of the range (unknown & port library) onto the end of the entries from the start of the range*/
#define MAP_CATEGORY_TO_BITMASK_ENTRY(category) ( ((category) >= OMRMEM_LANGUAGE_CATEGORY_LIMIT) ? ((writer->_TotalCategories - 1) - (OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(category))): (category) )

#define CATEGORY_WORD_INDEX(category) (MAP_CATEGORY_TO_BITMASK_ENTRY(category) / 32)
#define CATEGORY_WORD_MASK(category) (1 << ( MAP_CATEGORY_TO_BITMASK_ENTRY(category) % 32 ))

#define CATEGORY_IS_ANCESTOR(total, category) ( (total)->category_bitmask[CATEGORY_WORD_INDEX(category)] & CATEGORY_WORD_MASK(category) )
#define SET_CATEGORY_AS_ANCESTOR(total, category) ( (total)->category_bitmask[CATEGORY_WORD_INDEX(category)] |= CATEGORY_WORD_MASK(category) )

static const UDATA syncEventsMask =
	J9RAS_DUMP_ON_CLASS_LOAD |
	J9RAS_DUMP_ON_CLASS_UNLOAD |
	J9RAS_DUMP_ON_EXCEPTION_THROW |
	J9RAS_DUMP_ON_EXCEPTION_CATCH |
	J9RAS_DUMP_ON_THREAD_START |
	J9RAS_DUMP_ON_THREAD_END |
	J9RAS_DUMP_ON_THREAD_BLOCKED |
	J9RAS_DUMP_ON_EXCEPTION_DESCRIBE |
	J9RAS_DUMP_ON_OBJECT_ALLOCATION |
	J9RAS_DUMP_ON_SLOW_EXCLUSIVE_ENTER |
	J9RAS_DUMP_ON_EXCEPTION_SYSTHROW |
	J9RAS_DUMP_ON_TRACE_ASSERT |
	J9RAS_DUMP_ON_USER_REQUEST;

/**************************************************************************************************/
/*                                                                                                */
/* Class for writing java core dump files                                                         */
/*                                                                                                */
/**************************************************************************************************/
class JavaCoreDumpWriter
{
public :
	/* Constructor */
	JavaCoreDumpWriter(const char* fileName, J9RASdumpContext* context, J9RASdumpAgent *agent);

	/* Destructor */
	~JavaCoreDumpWriter();

private :
	/* Prevent use of the copy constructor and assignment operator */
	JavaCoreDumpWriter(const JavaCoreDumpWriter& source);
	JavaCoreDumpWriter& operator=(const JavaCoreDumpWriter& source);

	/* Allow the callback functions access */
	friend UDATA writeFrameCallBack          (J9VMThread* vmThread, J9StackWalkState* state);
	friend UDATA writeExceptionFrameCallBack (J9VMThread* vmThread, void* userData, J9ROMClass* romClass, J9ROMMethod* romMethod, J9UTF8* sourceFile, UDATA lineNumber, J9ClassLoader* classLoader);
	friend void  writeLoaderCallBack         (void* classLoader, void* userData);
	friend void  writeLibrariesCallBack      (void* classLoader, void* userData);
	friend void  writeClassesCallBack        (void* classLoader, void* userData);
	friend UDATA outerMemCategoryCallBack (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state);
	friend UDATA innerMemCategoryCallBack (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state);

	friend jvmtiIterationControl heapIteratorCallback   (J9JavaVM* vm, J9MM_IterateHeapDescriptor*   heapDescriptor,    void* userData);
	friend jvmtiIterationControl spaceIteratorCallback  (J9JavaVM* vm, J9MM_IterateSpaceDescriptor*  spaceDescriptor,   void* userData);
	friend jvmtiIterationControl regionIteratorCallback (J9JavaVM* vm, J9MM_IterateRegionDescriptor* regionDescription, void* userData);

	/* sig_protect wrappers functions and handlers */
	friend UDATA protectedWriteSection       (struct J9PortLibrary *, void *);
	friend UDATA handlerWriteSection         (struct J9PortLibrary *, U_32, void *, void *);
	friend UDATA handlerJavaThreadWalk       (struct J9PortLibrary *, U_32, void *, void *);
	friend UDATA protectedWalkJavaStack      (struct J9PortLibrary *, void *);
	friend UDATA protectedWriteGCHistoryLines (struct J9PortLibrary *, void *);
	friend UDATA protectedIterateStackTrace  (struct J9PortLibrary *, void *);
	friend UDATA protectedWriteThreadBlockers (struct J9PortLibrary *, void *);
	friend UDATA protectedGetOwnedObjectMonitors (struct J9PortLibrary *, void *);
	friend UDATA protectedWriteJavaLangThreadInfo(struct J9PortLibrary *, void *);
	friend UDATA protectedWriteThreadsWithNativeStacks(J9PortLibrary*, void*);
	friend UDATA protectedWriteThreadsJavaOnly(J9PortLibrary*, void*);
	friend UDATA protectedWriteThreadsUsageSummary(J9PortLibrary*, void*);
	friend UDATA handlerWriteThreadBlockers  (struct J9PortLibrary *, U_32, void *, void *);
	friend UDATA handlerGetThreadsUsageInfo  (struct J9PortLibrary *, U_32, void *, void *);
	friend UDATA handlerGetOwnedObjectMonitors(struct J9PortLibrary *, U_32, void *, void *);
	friend UDATA handlerIterateStackTrace    (struct J9PortLibrary *, U_32, void *, void *);
	friend UDATA handlerWriteJavaLangThreadInfo(struct J9PortLibrary *, U_32, void *, void *);
	friend UDATA handlerWriteStacks(struct J9PortLibrary *, U_32, void *, void *);

	/* Allow the functions used by hash tables access */
	friend UDATA lockHashFunction(void* key, void* user);
	friend UDATA lockHashEqualFunction(void* left, void* right, void* user);

	/* Directed node in lock graph */
	struct DeadLockGraphNode {
		J9VMThread *thread;
		DeadLockGraphNode *next;
		J9ThreadAbstractMonitor *lock;
		j9object_t lockObject;
		UDATA cycle;
	};

	/* Internal convenience method for testing the context */
	bool avoidLocks(void);

	/* Internal methods for writing the first level sections */
	void writeHeader(void);
	void writeTitleSection(void);
	void writeProcessorSection(void);
	void writeEnvironmentSection(void);
	void writeMemorySection(void);
	void writeMemoryCountersSection(void);
	void writeMonitorSection(void);
	void writeThreadSection(void);
	void writeClassSection(void);
#if defined(OMR_OPT_CUDA)
	void writeCudaSection(void);
#endif /* defined(OMR_OPT_CUDA) */
	/* OMR_OPT_HOOKDUMP */
	void writeHookSection(void);
#if defined(J9VM_OPT_SHARED_CLASSES)
	void writeSharedClassIPCInfo(const char* textStart, const char* textEnd, IDATA id, UDATA padToLength);
	void writeSharedClassLockInfo(const char* lockName, IDATA lockSemid, void* lockTID);
	void writeSharedClassSection(void);
	void writeSharedClassSectionTopLayerStatsHelper(J9SharedClassJavacoreDataDescriptor* javacoreData, bool multiLayerStats);
	void writeSharedClassSectionTopLayerStatsSummaryHelper(J9SharedClassJavacoreDataDescriptor* javacoreData);
	void writeSharedClassSectionAllLayersStatsHelper(J9SharedClassJavacoreDataDescriptor* javacoreData);

#endif
	void writeTrailer(void);

	/* Internal methods for writing the nested sections */
	void        writeVMRuntimeState          (U_32 vmRuntimeState);
	void        writeExceptionDetail         (j9object_t* exceptionRef);
	void        writeGPCategory              (void *gpInfo, const char* prefix, U_32 category);
	void        writeGPValue                 (const char* prefix, const char* name, U_32 kind, void* value);
	void        writeJitMethod               (J9VMThread* vmThread);
	void        writeSegments                (J9MemorySegmentList* list, BOOLEAN isCodeCacheSegment);
	void        writeTraceHistory            (U_32 type);
	void        writeGCHistoryLines          (UtThreadData** thr, UtTracePointIterator* iterator, const char* typePrefix);
	void        writeDeadLocks               (void);
	void        findThreadCycle              (J9VMThread* vmThread, J9HashTable* deadlocks);
	void        writeDeadlockNode            (DeadLockGraphNode* node, int count);
	void        writeMonitorObject           (J9ThreadMonitor* monitor, j9object_t obj, blocked_thread_record *threadStore);
	void        writeMonitor                 (J9ThreadMonitor* monitor);
	void        writeSystemMonitor           (J9ThreadMonitor* monitor);
	void        writeObject                  (j9object_t obj);
	void        writeThread                  (J9VMThread* vmThread, J9PlatformThread *nativeThread, UDATA vmstate, UDATA javaState, UDATA javaPriority, j9object_t lockObject, J9VMThread *lockOwnerThread);
	void        writeThreadName              (J9VMThread* vmThread);
	void        writeThreadBlockers          (J9VMThread* vmThread, UDATA vmstate, j9object_t lockObject, J9VMThread *lockOwnerThread );
	UDATA       writeFrame                   (J9StackWalkState* state);
	UDATA       writeExceptionFrame          (void *userData, J9ROMClass* romClass, J9ROMMethod* romMethod, J9UTF8* sourceFile, UDATA lineNumber);
	void        writeLoader                  (J9ClassLoader* classLoader);
	void        writeLibraries               (J9ClassLoader* classLoader);
	void        writeClasses                 (J9ClassLoader* classLoader);
	void        writeEventDrivenTitle        (void);
	void        writeUserRequestedTitle      (void);
	void        writeNativeAllocator         (const char * name, U_32 depth, BOOLEAN isRoot, UDATA liveBytes, UDATA liveAllocations);
	IDATA       getOwnedObjectMonitors       (J9VMThread* vmThread, J9ObjectMonitorInfo* monitorInfos);
	void        writeJavaLangThreadInfo      (J9VMThread* vmThread);
	void        writeCPUinfo                 (void);
#if defined(LINUX)
	void        writeCgroupMetrics(void);
#endif
	void        writeThreadsWithNativeStacks(void);
	void        writeThreadsJavaOnly(void);
	void        writeThreadTime              (const char * timerName, I_64 nanoTime);
	void        writeThreadsUsageSummary     (void);
	void        writeHookInfo                (struct OMRHookInfo4Dump *hookInfo);
	void        writeHookInterface           (struct J9HookInterface **hookInterface);
	/* Other internal methods */
	j9object_t getClassLoaderObject(J9ClassLoader* loader);
	UDATA createPadding(const char* str, UDATA fieldWidth, char padChar, char* buffer);
	void writeThreadState(UDATA threadState);

	/* Declared data */
	/* NB : The initialization order is significant */
	J9RASdumpContext* _Context;
	J9JavaVM*         _VirtualMachine;
	J9PortLibrary*    _PortLibrary;
	const char*       _FileName;
	TextFileStream    _OutputStream;
	bool              _FileMode;
	bool              _Error;
	bool              _AvoidLocks;
	bool              _PreemptLocked;
	bool              _ThreadsWalkStarted;
	J9RASdumpAgent *  _Agent;
	memcategory_data_frame* _CategoryStack;
	U_32              _CategoryStackTop;
	const char *      _SpaceDescriptorName;
	I_32              _TotalCategories;
	UDATA             _AllocatedVMThreadCount;

	/* Static declared data */
	static const unsigned int _MaximumExceptionNameLength;
	static const U_32 _MaximumFormattedTracePointLength;
	static const unsigned int _MaximumCommandLineLength;
	static const unsigned int _MaximumTimeStampLength;
	static const unsigned int _MaximumGPValueLength;
	static const unsigned int _MaximumJavaStackDepth;
	static const int _MaximumGCHistoryLines;
	static const int _MaximumMonitorInfosPerThread;
};

/* Static declared data instantiation */
const unsigned int JavaCoreDumpWriter::_MaximumExceptionNameLength(128);
const unsigned int JavaCoreDumpWriter::_MaximumFormattedTracePointLength(512);
const unsigned int JavaCoreDumpWriter::_MaximumCommandLineLength(512);
const unsigned int JavaCoreDumpWriter::_MaximumTimeStampLength(30);
const unsigned int JavaCoreDumpWriter::_MaximumGPValueLength(512);
const unsigned int JavaCoreDumpWriter::_MaximumJavaStackDepth(100000);
const int JavaCoreDumpWriter::_MaximumGCHistoryLines(2000);
const int JavaCoreDumpWriter::_MaximumMonitorInfosPerThread(32);

class sectionClosure {
private:
	sectionClosure() {};

public:
	void (JavaCoreDumpWriter::*sectionFunction)(void);
	JavaCoreDumpWriter *jcw;

	sectionClosure(void (JavaCoreDumpWriter::*func)(void), JavaCoreDumpWriter *writer) :
		sectionFunction(func),
		jcw(writer)
	{};

	void invoke(void) {
		(jcw->*sectionFunction)();
	}
};

struct walkClosure {
	J9Heap *heap;
	void *gpInfo;
	JavaCoreDumpWriter *jcw;
	/* used for both J9ThreadWalkState and J9StackWalkState */
	void *state;
};


#define CALL_PROTECT(section, retVal) \
	do { \
		sectionClosure closure(&JavaCoreDumpWriter::section, this); \
		UDATA sink; \
		retVal = j9sig_protect(protectedWriteSection, &closure, handlerWriteSection, this, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &sink) || retVal; \
	} while (0);

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::JavaCoreDumpWriter() method implementation                                 */
/*                                                                                                */
/**************************************************************************************************/
JavaCoreDumpWriter::JavaCoreDumpWriter(
	const char*       fileName,
	J9RASdumpContext* context,
	J9RASdumpAgent*   agent
) :
	_Context(context),
	_VirtualMachine(_Context->javaVM),
	_PortLibrary(_VirtualMachine->portLibrary),
	_FileName(fileName),
	_OutputStream(_PortLibrary),
	_FileMode(false),
	_Error(false),
	_AvoidLocks(false),
	_PreemptLocked(false),
	_ThreadsWalkStarted(false),
	_Agent(agent),
	_TotalCategories(-1)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	bool bufferWrites=false;
	_AllocatedVMThreadCount = getAllocatedVMThreadCount(_VirtualMachine);

	/* Determine whether getting further locks should be avoided
	 * There is a small timing window where we can crash close to
	 * startup and the vmThreadListMutex is NULL.
	 * Failing to check for NULL leads to a endless spin.*/
	if(NULL == _VirtualMachine->vmThreadListMutex) {
		_AvoidLocks = true;
	} else if (omrthread_monitor_try_enter(_VirtualMachine->vmThreadListMutex)) {
		/* Failed to get lock so avoid asking for further ones if it's a GPF or abort */
		_AvoidLocks = ((_Context->eventFlags & (J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_ABORT_SIGNAL)) != 0);
	} else {
		/* Got the lock so release it */
		omrthread_monitor_exit(_VirtualMachine->vmThreadListMutex);
		_AvoidLocks = false;
	}

	/* Write a message to standard error saying we are about to write a dump file */
	reportDumpRequest(_PortLibrary,_Context,"Java",_FileName);

	/* don't buffer if we don't have the locks (incl exclusive) or it's a GP. */
	bufferWrites = !_AvoidLocks
	  && ((_Context->eventFlags & (J9RAS_DUMP_ON_GP_FAULT | J9RAS_DUMP_ON_ABORT_SIGNAL)) == 0)
	  && ((_Agent->prepState & J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS) == J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS);

	/* It's a single file so open it */
	_OutputStream.open(_FileName, bufferWrites);

	/* Write the sections, these return void so we throw away the per section return value.
	 * We consolidate the return values for all of the sections so we know after we finish
	 * if any of them failed.
	 */
	CALL_PROTECT(writeTitleSection, _Error);
	CALL_PROTECT(writeProcessorSection, _Error);
	CALL_PROTECT(writeEnvironmentSection, _Error);
	CALL_PROTECT(writeMemoryCountersSection, _Error);
	CALL_PROTECT(writeMemorySection, _Error);

	/* The monitor section is crash prone as objects mutate under it.
	 * Lock ordering imposed by the lock inflation path means that we have to get the monitorTableMutex ahead of the
	 * thread lock as we will attempt to get it again for uninflated locks when calling getVMThreadRawState while looking
	 * for waiting threads on any given monitor
	 */
	omrthread_monitor_enter(_VirtualMachine->monitorTableMutex);
	omrthread_t self = omrthread_self();
	if (!omrthread_lib_try_lock(self)) {
		/* got both locks so we shouldn't deadlock getting thread state */
		CALL_PROTECT(writeMonitorSection, _Error);
		omrthread_lib_unlock(self);
	} else {
		/* Write the section header */
		_OutputStream.writeCharacters(
			"0SECTION       LOCKS subcomponent dump routine\n"
			"NULL           ===============================\n"
			"1LKMONPOOLDUMP Monitor Pool Dump unavailable [locked]\n"
			"1LKREGMONDUMP  JVM System Monitor Dump unavailable [locked]\n"
			"NULL           ------------------------------------------------------------------------\n");
	}
	omrthread_monitor_exit(_VirtualMachine->monitorTableMutex);

	/* If request=preempt (for native stack collection) we attempt to acquire the mutex and note if we got it */
	if (_Agent->requestMask & J9RAS_DUMP_DO_PREEMPT_THREADS) {
		if (compareAndSwapUDATA(&rasDumpPreemptLock, 0, 1) == 0) {
			_PreemptLocked = true; /* we got the lock */
		}
	}
	CALL_PROTECT(writeThreadSection, _Error);
	if (_PreemptLocked) {
		compareAndSwapUDATA(&rasDumpPreemptLock, 1, 0);
		_PreemptLocked = false;
	}

#if defined(OMR_OPT_CUDA)
	CALL_PROTECT(writeCudaSection, _Error);
#endif /* defined(OMR_OPT_CUDA) */

	/* OMR_OPT_HOOKDUMP */
	CALL_PROTECT(writeHookSection, _Error);

#if defined(J9VM_OPT_SHARED_CLASSES)
	CALL_PROTECT(writeSharedClassSection, _Error);
#endif
	CALL_PROTECT(writeClassSection, _Error);
	CALL_PROTECT(writeTrailer, _Error);

	/* Record the status of the operation */
	_FileMode = _FileMode || _OutputStream.isOpen();
	_Error    = _Error    || _OutputStream.isError();

	/* Close the file */
	_OutputStream.close();

	/* Write a message to standard error saying we have written a dump file */
	if (_Error) {
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_ERROR_IN_DUMP_STR, "Java", _FileName);
		Trc_dump_reportDumpError_Event2("Java", _FileName);
	} else if (_FileMode) {
		j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_WRITTEN_DUMP_STR, "Java", _FileName);
		Trc_dump_reportDumpEnd_Event2("Java", _FileName);
	} else {
		j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_NO_CREATE, _FileName);
		Trc_dump_reportDumpEnd_Event2("Java", "stderr");
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::~JavaCoreDumpWriter() method implementation                                */
/*                                                                                                */
/**************************************************************************************************/
JavaCoreDumpWriter::~JavaCoreDumpWriter()
{
	/* Nothing to do currently */
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::avoidLocks() method implementation                                         */
/*                                                                                                */
/**************************************************************************************************/
bool
JavaCoreDumpWriter::avoidLocks(void)
{
	return _AvoidLocks;
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeHeader() method implementation                                        */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeHeader(void)
{
	_OutputStream.writeCharacters(
		"NULL           ------------------------------------------------------------------------\n"
	);
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeTitleSection() method implementation                                  */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeTitleSection(void)
{
	/* Write the section header */
	_OutputStream.writeCharacters(
		"0SECTION       TITLE subcomponent dump routine\n"
		"NULL           ===============================\n"
	);

	PORT_ACCESS_FROM_JAVAVM(_VirtualMachine);

	char cset[64];
	IDATA getCSet = j9file_get_text_encoding(cset, sizeof(cset));

	if (getCSet != 0) {
		strcpy(cset,"[not available]");
	}
	_OutputStream.writeCharacters("1TICHARSET     ");
	_OutputStream.writeCharacters(cset);
	_OutputStream.writeCharacters("\n");

	if (J9RAS_DUMP_ON_USER_REQUEST == _Context->eventFlags) {
		writeUserRequestedTitle();
	} else {
		writeEventDrivenTitle();
	}

	/* Write the date and time that the dump was generated */
	U_64 now = j9time_current_time_millis();
	RasDumpGlobalStorage* dump_storage;
	struct J9StringTokens* stringTokens;

	dump_storage = (RasDumpGlobalStorage*)_VirtualMachine->j9rasdumpGlobalStorage;

	/* lock access to the tokens */
	omrthread_monitor_enter(dump_storage->dumpLabelTokensMutex);
	stringTokens = (struct J9StringTokens*)dump_storage->dumpLabelTokens;
	j9str_set_time_tokens(stringTokens, now);
	/* release access to the tokens */
	omrthread_monitor_exit(dump_storage->dumpLabelTokensMutex);

	char timeStamp[_MaximumTimeStampLength];

	j9str_subst_tokens(timeStamp, _MaximumTimeStampLength, "%Y/%m/%d at %H:%M:%S", stringTokens);

	_OutputStream.writeCharacters("1TIDATETIME    Date: ");
	_OutputStream.writeCharacters(timeStamp);
	_OutputStream.writeInteger(now % 1000, ":%03d"); /* add the milliseconds */
	_OutputStream.writeCharacters("\n");

	_OutputStream.writeCharacters("1TINANOTIME    System nanotime: ");
	_OutputStream.writeInteger64(j9time_nano_time(), "%llu");
	_OutputStream.writeCharacters("\n");

	/* Write the file name */
	_OutputStream.writeCharacters("1TIFILENAME    Javacore filename:    ");
	_OutputStream.writeCharacters(_FileName);
	_OutputStream.writeCharacters("\n");

	/* Record whether this is exclusive or not */
	_OutputStream.writeCharacters("1TIREQFLAGS    Request Flags: ");
	_OutputStream.writeInteger(_Agent->requestMask);
	if (_Agent->requestMask) {
		UDATA moreRequests = _Agent->requestMask;

		_OutputStream.writeCharacters(" (");

		moreRequests = moreRequests >> 1;
		if ((_Agent->requestMask & J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS) == J9RAS_DUMP_DO_EXCLUSIVE_VM_ACCESS) {
			_OutputStream.writeCharacters("exclusive");
			if (moreRequests) {
				_OutputStream.writeCharacters("+");
			}
		}

		moreRequests = moreRequests >> 1;
		if ((_Agent->requestMask & J9RAS_DUMP_DO_COMPACT_HEAP) == J9RAS_DUMP_DO_COMPACT_HEAP) {
			_OutputStream.writeCharacters("compact");
			if (moreRequests) {
				_OutputStream.writeCharacters("+");
			}
		}

		moreRequests = moreRequests >> 1;
		if ((_Agent->requestMask & J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK) == J9RAS_DUMP_DO_PREPARE_HEAP_FOR_WALK) {
			_OutputStream.writeCharacters("prepwalk");
			if (moreRequests) {
				_OutputStream.writeCharacters("+");
			}
		}

		moreRequests = moreRequests >> 1;
		if ((_Agent->requestMask & J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS) == J9RAS_DUMP_DO_SUSPEND_OTHER_DUMPS) {
			_OutputStream.writeCharacters("serial");
			if (moreRequests) {
				_OutputStream.writeCharacters("+");
			}
		}

		moreRequests = moreRequests >> 1;
		if ((_Agent->requestMask & J9RAS_DUMP_DO_HALT_ALL_THREADS) == J9RAS_DUMP_DO_HALT_ALL_THREADS) {
		}

		moreRequests = moreRequests >> 1;
		if ((_Agent->requestMask & J9RAS_DUMP_DO_ATTACH_THREAD) == J9RAS_DUMP_DO_ATTACH_THREAD) {
			_OutputStream.writeCharacters("attach");
			if (moreRequests) {
				_OutputStream.writeCharacters("+");
			}
		}

		moreRequests = moreRequests >> 1;
		if ((_Agent->requestMask & J9RAS_DUMP_DO_MULTIPLE_HEAPS) == J9RAS_DUMP_DO_MULTIPLE_HEAPS) {
			_OutputStream.writeCharacters("multiple");
			if (moreRequests) {
				_OutputStream.writeCharacters("+");
			}
		}

		if ((_Agent->requestMask & J9RAS_DUMP_DO_PREEMPT_THREADS) == J9RAS_DUMP_DO_PREEMPT_THREADS) {
			_OutputStream.writeCharacters("preempt");
		}

		_OutputStream.writeCharacters(")");
	}

	_OutputStream.writeCharacters("\n");

	_OutputStream.writeCharacters("1TIPREPSTATE   Prep State: ");
	_OutputStream.writeInteger(_Agent->prepState);
	if (_Agent->prepState) {
		UDATA moreState = _Agent->prepState;

		_OutputStream.writeCharacters(" (");

		moreState = moreState >> 1;
		if ((_Agent->prepState & J9RAS_DUMP_GOT_LOCK) == J9RAS_DUMP_GOT_LOCK) {
			_OutputStream.writeCharacters("rasdump_lock");
			if (moreState) {
				_OutputStream.writeCharacters("+");
			}
		}

		moreState = moreState >> 1;
		if ((_Agent->prepState & J9RAS_DUMP_GOT_VM_ACCESS) == J9RAS_DUMP_GOT_VM_ACCESS) {
			_OutputStream.writeCharacters("vm_access");
			if (moreState) {
				_OutputStream.writeCharacters("+");
			}
		}

		moreState = moreState >> 1;
		if ((_Agent->prepState & J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS) == J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS) {
			_OutputStream.writeCharacters("exclusive_vm_access");
			if (moreState) {
				_OutputStream.writeCharacters("+");
			}
		}

		moreState = moreState >> 1;
		if ((_Agent->prepState & J9RAS_DUMP_HEAP_COMPACTED) == J9RAS_DUMP_HEAP_COMPACTED) {
			_OutputStream.writeCharacters("heap_compacted");
			if (moreState) {
				_OutputStream.writeCharacters("+");
			}
		}

		moreState = moreState >> 1;
		if ((_Agent->prepState & J9RAS_DUMP_HEAP_PREPARED) == J9RAS_DUMP_HEAP_PREPARED) {
			_OutputStream.writeCharacters("heap_prepared");
			if (moreState) {
				_OutputStream.writeCharacters("+");
			}
		}

		moreState = moreState >> 1;
		if ((_Agent->prepState & J9RAS_DUMP_THREADS_HALTED) == J9RAS_DUMP_THREADS_HALTED) {
			_OutputStream.writeCharacters("threads_halted");
			if (moreState) {
				_OutputStream.writeCharacters("+");
			}
		}

		moreState = moreState >> 1;
		if ((_Agent->prepState & J9RAS_DUMP_ATTACHED_THREAD) == J9RAS_DUMP_ATTACHED_THREAD) {
			_OutputStream.writeCharacters("attached_thread");
			if (moreState) {
				_OutputStream.writeCharacters("+");
			}
		}

		moreState = moreState >> 1;
		if ((_Agent->prepState & J9RAS_DUMP_PREEMPT_THREADS) == J9RAS_DUMP_PREEMPT_THREADS) {
			_OutputStream.writeCharacters("preempt_threads");
			if (moreState) {
				_OutputStream.writeCharacters("+");
			}
		}

		if ((_Agent->prepState & J9RAS_DUMP_TRACE_DISABLED) == J9RAS_DUMP_TRACE_DISABLED) {
			_OutputStream.writeCharacters("trace_disabled");
		}
		_OutputStream.writeCharacters(")");
	}
	_OutputStream.writeCharacters("\n");

	if ((_Agent->prepState & J9RAS_DUMP_GOT_EXCLUSIVE_VM_ACCESS) == 0) {
		_OutputStream.writeCharacters("1TIPREPINFO    Exclusive VM access not taken: data may not be consistent across javacore sections\n");
	}

	/* Write the section trailer */
	_OutputStream.writeCharacters("NULL           ------------------------------------------------------------------------\n");
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeEventDrivenTitle() method implementation                              */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeEventDrivenTitle(void)
{
	/* Write the dump event description */
	_OutputStream.writeCharacters("1TISIGINFO     Dump Event \"");
	_OutputStream.writeCharacters(mapDumpEvent(_Context->eventFlags));
	_OutputStream.writeCharacters("\" (");
	_OutputStream.writeInteger(_Context->eventFlags, "%08zX");
	_OutputStream.writeCharacters(")");

	/* Write the event data */
	J9RASdumpEventData* eventData = _Context->eventData;
	if (eventData) {
		_OutputStream.writeCharacters(" Detail \"");
		_OutputStream.writeCharacters(eventData->detailData, eventData->detailLength);
		_OutputStream.writeCharacters("\"");
		writeExceptionDetail((j9object_t*)eventData->exceptionRef);
	}

	_OutputStream.writeCharacters(" received \n");
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeUserRequestedTitle() method implementation                              */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeUserRequestedTitle(void)
{
	/* Write the dump event description */
	_OutputStream.writeCharacters("1TISIGINFO     Dump Requested By User (");
	_OutputStream.writeInteger(_Context->eventFlags, "%08zX");
	_OutputStream.writeCharacters(")");

	/* Write the event data */
	J9RASdumpEventData* eventData = _Context->eventData;
	if (eventData) {
		_OutputStream.writeCharacters(" Through ");
		_OutputStream.writeCharacters(eventData->detailData, eventData->detailLength);
	}

	_OutputStream.writeCharacters("\n");
}


/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeProcessorSection() method implementation                              */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeProcessorSection(void)
{
	PORT_ACCESS_FROM_JAVAVM(_VirtualMachine);

	/* Write the section header */
	_OutputStream.writeCharacters(
		"0SECTION       GPINFO subcomponent dump routine\n"
		"NULL           ================================\n"
	);

#ifdef J9VM_RAS_EYECATCHERS
	/* Write the operating system description */
	J9RAS* j9ras = _VirtualMachine->j9ras;

	const char* osName         = (char*)(j9ras->osname);
	const char* osVersion      = (char*)(j9ras->osversion);
	const char* osArchitecture = (char*)(j9ras->osarch);
	int         numberOfCpus   = j9ras->cpus;

#else /* !J9VM_RAS_EYECATCHERS */

	const char* osName         = j9sysinfo_get_OS_type();
	const char* osVersion      = j9sysinfo_get_OS_version();
	const char* osArchitecture = j9sysinfo_get_CPU_architecture();
	int         numberOfCpus   = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);

	if (osName == NULL) {
		osName = "[not available]";
	}

	if (osVersion == NULL) {
		osVersion = "[not available]";
	}

	if (osArchitecture == NULL) {
		osArchitecture = "[not available]";
	}
#endif /* !J9VM_RAS_EYECATCHERS */

	/* Write the operating system description */
	_OutputStream.writeCharacters("2XHOSLEVEL     OS Level         : ");
	_OutputStream.writeCharacters(osName);
	_OutputStream.writeCharacters(" ");
	_OutputStream.writeCharacters(osVersion);
	_OutputStream.writeCharacters("\n");

	/* Write the processor description */
	_OutputStream.writeCharacters("2XHCPUS        Processors -\n");
	_OutputStream.writeCharacters("3XHCPUARCH       Architecture   : ");
	_OutputStream.writeCharacters(osArchitecture);
	_OutputStream.writeCharacters("\n");

	_OutputStream.writeCharacters("3XHNUMCPUS       How Many       : ");
	_OutputStream.writeInteger(numberOfCpus, "%i");
	_OutputStream.writeCharacters("\n");

	/* Write the NUMA support description */
	_OutputStream.writeCharacters("3XHNUMASUP       ");
	if (j9port_control(J9PORT_CTLDATA_VMEM_NUMA_IN_USE, 0)) {
		_OutputStream.writeCharacters("NUMA support enabled");
	} else {
		_OutputStream.writeCharacters("NUMA is either not supported or has been disabled by user");
	}
	_OutputStream.writeCharacters("\n");

	/* Write the processor registers */
	J9VMThread* vmThread = _Context->onThread;
	if (vmThread && vmThread->gpInfo) {


		_OutputStream.writeCharacters("NULL           \n");
		writeGPCategory(vmThread->gpInfo, "1XHEXCPCODE    ", J9PORT_SIG_SIGNAL);

		_OutputStream.writeCharacters("NULL           \n");
		writeGPCategory(vmThread->gpInfo, "1XHEXCPMODULE  ", J9PORT_SIG_MODULE);

		_OutputStream.writeCharacters("NULL           \n");
		_OutputStream.writeCharacters("1XHREGISTERS   Registers:\n");
		writeGPCategory(vmThread->gpInfo, "2XHREGISTER      ", J9PORT_SIG_GPR);
		writeGPCategory(vmThread->gpInfo, "2XHREGISTER      ", J9PORT_SIG_FPR);
		writeGPCategory(vmThread->gpInfo, "2XHREGISTER      ", J9PORT_SIG_VR);
		writeGPCategory(vmThread->gpInfo, "2XHREGISTER      ", J9PORT_SIG_CONTROL);

		writeJitMethod(vmThread);

		_OutputStream.writeCharacters("NULL           \n");
		_OutputStream.writeCharacters("1XHFLAGS       VM flags:");
		_OutputStream.writeVPrintf("%.*zX", sizeof(void *) * 2, vmThread->omrVMThread->vmState);
		_OutputStream.writeCharacters("\n");

	} else {
		_OutputStream.writeCharacters(
			"NULL           \n"
			"1XHERROR2      Register dump section only produced for SIGSEGV, SIGILL or SIGFPE.\n"
		);
	}

	/* Write the section trailer */
	_OutputStream.writeCharacters(
		"NULL           \n"
		"NULL           ------------------------------------------------------------------------\n"
	);
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeEnvironmentSection() method implementation                            */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeEnvironmentSection(void)
{
	/* Write the section header */
	_OutputStream.writeCharacters(
		"0SECTION       ENVINFO subcomponent dump routine\n"
		"NULL           =================================\n"
	);

	/* Write the Java version data */
	if( _VirtualMachine->j9ras->serviceLevel != NULL ) {
		_OutputStream.writeCharacters("1CIJAVAVERSION ");
		_OutputStream.writeCharacters(_VirtualMachine->j9ras->serviceLevel);
		_OutputStream.writeCharacters("\n");
	}

	/* Write the VM build data */
	_OutputStream.writeCharacters("1CIVMVERSION   ");
	_OutputStream.writeCharacters(EsBuildVersionString);
	_OutputStream.writeCharacters("\n");

#if defined(OPENJ9_TAG)
	/* Write the OpenJ9 tag when it has a value */
	if (strlen(OPENJ9_TAG) > 0) {
		_OutputStream.writeCharacters("1CIJ9VMTAG     " OPENJ9_TAG "\n");
	}
#endif

	/* Write the VM version data */
	_OutputStream.writeCharacters("1CIJ9VMVERSION ");
	_OutputStream.writeCharacters(_VirtualMachine->internalVMFunctions->getJ9VMVersionString(_VirtualMachine));
	_OutputStream.writeCharacters("\n");

	/* Write the JIT runtime flags and version */
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	_OutputStream.writeCharacters("1CIJITVERSION  ");

	J9JITConfig* jitConfig = _VirtualMachine->jitConfig;
	if (jitConfig) {
		if (jitConfig->jitLevelName) {
			_OutputStream.writeCharacters(jitConfig->jitLevelName);
		}
	} else {
		_OutputStream.writeCharacters("unavailable (JIT disabled)");
	}
	_OutputStream.writeCharacters("\n");
#endif

	/* Write the OMR version data */
	_OutputStream.writeCharacters("1CIOMRVERSION  ");
	_OutputStream.writeCharacters(_VirtualMachine->memoryManagerFunctions->omrgc_get_version(_VirtualMachine->omrVM));
	_OutputStream.writeCharacters("\n");

#if defined(VENDOR_SHORT_NAME) && defined(VENDOR_SHA)
	/* Write the Vendor version data */
	_OutputStream.writeCharacters("1CI" VENDOR_SHORT_NAME "VERSION  " VENDOR_SHA "\n");
#endif /* VENDOR_SHORT_NAME && VENDOR_SHA */

#if defined(OPENJDK_TAG) && defined(OPENJDK_SHA)
	/* Write the JCL version data */
	_OutputStream.writeCharacters("1CIJCLVERSION  " OPENJDK_SHA " based on " OPENJDK_TAG "\n");
#endif

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	_OutputStream.writeCharacters("1CIJITMODES    ");

	if (jitConfig) {
		if (jitConfig->runtimeFlags & J9JIT_JIT_ATTACHED) {
			_OutputStream.writeCharacters("JIT enabled");
		} else {
			_OutputStream.writeCharacters("JIT disabled");
		}
		if (jitConfig->runtimeFlags & J9JIT_AOT_ATTACHED) {
			_OutputStream.writeCharacters(", AOT enabled");
		} else {
			_OutputStream.writeCharacters(", AOT disabled");
		}
		if (jitConfig->fsdEnabled) {
			_OutputStream.writeCharacters(", FSD enabled");
		} else {
			_OutputStream.writeCharacters(", FSD disabled");
		}
		if (_VirtualMachine && (_VirtualMachine->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_CAN_REDEFINE_CLASSES)) {
			_OutputStream.writeCharacters(", HCR enabled");
		} else {
			_OutputStream.writeCharacters(", HCR disabled");
		}
	} else {
	_OutputStream.writeCharacters("unavailable (JIT disabled)");
	}

	_OutputStream.writeCharacters("\n");
#endif

	/* Write the running mode data */
	_OutputStream.writeCharacters("1CIRUNNINGAS   Running as ");

	/* NB : I can't find any code for making this decision in the existing implementation */
#ifdef J9VM_BUILD_J2SE
	_OutputStream.writeCharacters("a standalone");
#else
	_OutputStream.writeCharacters("an embedded");
#endif
	_OutputStream.writeCharacters(" JVM\n");

	_OutputStream.writeCharacters("1CIVMIDLESTATE VM Idle State: ");
	writeVMRuntimeState(_VirtualMachine->internalVMFunctions->getVMRuntimeState(_VirtualMachine));
	_OutputStream.writeCharacters("\n");

	OMRPORT_ACCESS_FROM_J9PORT(_PortLibrary);
	BOOLEAN inContainer = omrsysinfo_is_running_in_container();
	_OutputStream.writeCharacters("1CICONTINFO    Running in container : ");
	_OutputStream.writeCharacters( inContainer ? "TRUE\n" : "FALSE\n");
	uint64_t availableSubsystems = omrsysinfo_cgroup_get_enabled_subsystems();
	_OutputStream.writeCharacters("1CICGRPINFO    JVM support for cgroups enabled : ");
	_OutputStream.writeCharacters((availableSubsystems > 0) ? "TRUE\n" : "FALSE\n");

	PORT_ACCESS_FROM_JAVAVM(_VirtualMachine);

	/* Write the JVM start date and time */
	RasDumpGlobalStorage* dump_storage;
	struct J9StringTokens* stringTokens;

	dump_storage = (RasDumpGlobalStorage*)_VirtualMachine->j9rasdumpGlobalStorage;

	/* lock access to the tokens */
	omrthread_monitor_enter(dump_storage->dumpLabelTokensMutex);
	stringTokens = (struct J9StringTokens*)dump_storage->dumpLabelTokens;
	j9str_set_time_tokens(stringTokens, _VirtualMachine->j9ras->startTimeMillis);
	/* release access to the tokens */
	omrthread_monitor_exit(dump_storage->dumpLabelTokensMutex);

	char timeStamp[_MaximumTimeStampLength];

	j9str_subst_tokens(timeStamp, _MaximumTimeStampLength, "%Y/%m/%d at %H:%M:%S", stringTokens);

	_OutputStream.writeCharacters("1CISTARTTIME   JVM start time: ");
	_OutputStream.writeCharacters(timeStamp);
	_OutputStream.writeInteger(_VirtualMachine->j9ras->startTimeMillis % 1000, ":%03d"); /* add the milliseconds */
	_OutputStream.writeCharacters("\n");

	_OutputStream.writeCharacters("1CISTARTNANO   JVM start nanotime: ");
	_OutputStream.writeInteger64(_VirtualMachine->j9ras->startTimeNanos, "%llu");
	_OutputStream.writeCharacters("\n");

	/* Write the pid */
	_OutputStream.writeCharacters("1CIPROCESSID   Process ID: ");
	/* Write pid as decimal. */
	_OutputStream.writeInteger(_VirtualMachine->j9ras->pid, "%d");
	_OutputStream.writeCharacters(" (");
	/* Write pid as hexadecimal. (Default number format) */
	_OutputStream.writeInteger(_VirtualMachine->j9ras->pid);
	_OutputStream.writeCharacters(")\n");

	/* Write the command line data */
	char  commandLineBuffer[_MaximumCommandLineLength];
	IDATA result;

	result = j9sysinfo_get_env("IBM_JAVA_COMMAND_LINE", commandLineBuffer, _MaximumCommandLineLength);
	if (result == 0) {
		/* Ensure null-terminated */
		commandLineBuffer[_MaximumCommandLineLength-1] = '\0';

		_OutputStream.writeCharacters("1CICMDLINE     ");
		_OutputStream.writeCharacters(commandLineBuffer);
		_OutputStream.writeCharacters("\n");

	} else if (result > 0) {
		/* Long command line - need malloc'd buffer */
		char* commandLineBuffer = (char*)j9mem_allocate_memory(result, OMRMEM_CATEGORY_VM);
		if (commandLineBuffer) {
			if (j9sysinfo_get_env("IBM_JAVA_COMMAND_LINE", commandLineBuffer, result) == 0) {
				commandLineBuffer[result-1] = '\0';
				_OutputStream.writeCharacters("1CICMDLINE     ");
				_OutputStream.writeCharacters(commandLineBuffer);
				_OutputStream.writeCharacters("\n");
			} else {
				_OutputStream.writeCharacters("1CICMDLINE     [error]\n");
			}

			j9mem_free_memory(commandLineBuffer);

		} else {
			_OutputStream.writeCharacters("1CICMDLINE     [not enough space]\n");
		}

	} else {
		_OutputStream.writeCharacters("1CICMDLINE     [not available]\n");
	}

	/* Write the home directory data */
	_OutputStream.writeCharacters("1CIJAVAHOMEDIR Java Home Dir:   ");
	_OutputStream.writeCharacters((char*)_VirtualMachine->javaHome);
	_OutputStream.writeCharacters("\n");

	/* Write the dll directory data */
	_OutputStream.writeCharacters("1CIJAVADLLDIR  Java DLL Dir:    ");
	_OutputStream.writeCharacters((char*)_VirtualMachine->javaHome);
	_OutputStream.writeCharacters(DIR_SEPARATOR_STR "bin\n");

	/* Write the class path data */
	J9ClassLoader* classLoader = _VirtualMachine->systemClassLoader;

	_OutputStream.writeCharacters("1CISYSCP       Sys Classpath:   ");

	for (UDATA i = 0; i < classLoader->classPathEntryCount; i++) {
		_OutputStream.writeCharacters((char*)(classLoader->classPathEntries[i].path));
		_OutputStream.writeCharacters(";");
	}

	_OutputStream.writeCharacters("\n");

	/* Write the user arguments section */
	JavaVMInitArgs* args = _VirtualMachine->vmArgsArray->actualVMArgs;

	_OutputStream.writeCharacters("1CIUSERARGS    UserArgs:\n");

	for (int j = 0; j < args->nOptions; j++) {
		_OutputStream.writeCharacters("2CIUSERARG               ");
		_OutputStream.writeCharacters(args->options[j].optionString);

		if (args->options[j].extraInfo) {
			_OutputStream.writeCharacters(" ");
			_OutputStream.writePointer(args->options[j].extraInfo);
		}

		_OutputStream.writeCharacters("\n");
	}

	/* Write the user limits */
	J9SysinfoLimitIteratorState limitState;
	J9SysinfoUserLimitElement limitElement;
	bool columnHeadersPrinted = false;

	_OutputStream.writeCharacters("NULL\n");
	_OutputStream.writeCharacters("1CIUSERLIMITS  ");
	_OutputStream.writeCharacters("User Limits (in bytes except for NOFILE and NPROC)\n");
	_OutputStream.writeCharacters("NULL           ------------------------------------------------------------------------\n");

	result = j9sysinfo_limit_iterator_init(&limitState);
	if (0 == result) {
		if (0 == limitState.numElements) {
			/* no user limits to print */
			_OutputStream.writeCharacters("2CIULIMITERR   No user limit information\n");
		} else {
			/* print the data in columns */
			while (j9sysinfo_limit_iterator_hasNext(&limitState)) {
				char padding[20];
				IDATA paddingLength;
				UDATA fieldWidth = 21;

				result = j9sysinfo_limit_iterator_next(&limitState, &limitElement);

				if (columnHeadersPrinted == false) {
					/* print the headers (but only once) */
					_OutputStream.writeCharacters("NULL           type                            soft limit           hard limit\n");
					columnHeadersPrinted = true;
				}

				if (0 == result) {
					_OutputStream.writeCharacters("2CIUSERLIMIT   ");
					if (strlen(limitElement.name) > fieldWidth) {
						_OutputStream.writeCharacters(limitElement.name, fieldWidth);
					} else {
						_OutputStream.writeCharacters(limitElement.name);
					}
					paddingLength = createPadding(limitElement.name, fieldWidth, ' ', padding);
					_OutputStream.writeCharacters(padding, paddingLength);

					if (J9PORT_LIMIT_UNLIMITED == limitElement.softValue) {
						_OutputStream.writeCharacters("            unlimited");
					} else {
						_OutputStream.writeInteger64(limitElement.softValue, "%21llu");
					}

					if (J9PORT_LIMIT_UNLIMITED == limitElement.hardValue) {
						_OutputStream.writeCharacters("            unlimited");
					} else {
						_OutputStream.writeInteger64(limitElement.hardValue, "%21llu");
					}

					_OutputStream.writeCharacters("\n");
				} else {
					_OutputStream.writeCharacters("2CIULIMITERR   ");
					_OutputStream.writeCharacters(limitElement.name);

					paddingLength = createPadding(limitElement.name, fieldWidth, ' ', padding);
					_OutputStream.writeCharacters(padding, paddingLength);
					_OutputStream.writeCharacters("          unavailable          unavailable\n");
				}
			}
		}
	} else {
		/* the iterator didn't initialize so no user limits to print */
		_OutputStream.writeCharacters("2CIULIMITERR   Not supported on this platform\n");
	}

	/* Write the environment variables */
	J9SysinfoEnvIteratorState envState;
	J9SysinfoEnvElement envElement;
	void *buffer = NULL;
	UDATA bufferSizeBytes = 0;

	_OutputStream.writeCharacters("NULL\n");

	_OutputStream.writeCharacters("1CIENVVARS     Environment Variables");
#if defined(WIN32)
	_OutputStream.writeCharacters(" (may include system defined environment variables)");
#endif
	_OutputStream.writeCharacters("\n");

	_OutputStream.writeCharacters("NULL           ------------------------------------------------------------------------\n");

	/* call init with zero length buffer to get the required buffer size */
	result = j9sysinfo_env_iterator_init(&envState, buffer, bufferSizeBytes);

	if (result < 0) {
		/* a problem occurred */
		_OutputStream.writeCharacters("2CIENVVARERR   Cannot access environment variables\n");
	} else {
		/* the init has returned the size of buffer required so now go and allocate it */
		bufferSizeBytes = result;
		buffer = j9mem_allocate_memory(bufferSizeBytes, OMRMEM_CATEGORY_VM);
		if (NULL == buffer) {
			/* out of memory */
			_OutputStream.writeCharacters("2CIENVVARERR   Cannot access environment variables\n");
		} else {
			/* walk over the environment variables and write them into the javacore file */
			result = j9sysinfo_env_iterator_init(&envState, buffer, bufferSizeBytes);

			while (j9sysinfo_env_iterator_hasNext(&envState)) {
				result = j9sysinfo_env_iterator_next(&envState, &envElement);

				if (0 == result) {
					_OutputStream.writeCharacters("2CIENVVAR      ");
					_OutputStream.writeCharacters(envElement.nameAndValue);
					_OutputStream.writeCharacters("\n");
				}
			}
			/* tidy up memory */
			j9mem_free_memory(buffer);
		}
	}

	/* Write the list of additional system information */
	if (!J9_LINKED_LIST_IS_EMPTY(_VirtualMachine->j9ras->systemInfo)) {
		_OutputStream.writeCharacters(
			"NULL           \n"
			"1CISYSINFO     System Information\n"
			"NULL           ------------------------------------------------------------------------\n");
	}
	J9RASSystemInfo* systemInfo = J9_LINKED_LIST_START_DO(_VirtualMachine->j9ras->systemInfo);
	while (systemInfo != NULL) {
		switch (systemInfo->key) {
		case J9RAS_SYSTEMINFO_SCHED_COMPAT_YIELD:
			{
				char *sched_compat_yield = (char *)&systemInfo->data;
				_OutputStream.writeCharacters("2CISYSINFO     " J9RAS_SCHED_COMPAT_YIELD_FILE " = ");
				_OutputStream.writeVPrintf("%c ", sched_compat_yield[0]);
				_OutputStream.writeCharacters("\n");
			}
			break;

		case J9RAS_SYSTEMINFO_HYPERVISOR:
			{
				char *hypervisorName = (char *)systemInfo->data;
				_OutputStream.writeCharacters("2CISYSINFO     Hypervisor name = ");
				_OutputStream.writeCharacters(hypervisorName);
				_OutputStream.writeCharacters("\n");
			}
			break;
		case J9RAS_SYSTEMINFO_CORE_PATTERN:
			{
				char *corepattern = (char *)systemInfo->data;
				_OutputStream.writeCharacters("2CISYSINFO     " J9RAS_CORE_PATTERN_FILE " = ");
				_OutputStream.writeCharacters(corepattern);
				_OutputStream.writeCharacters("\n");
			}
			break;
		case J9RAS_SYSTEMINFO_CORE_USES_PID:
			{
				char *coreusespid = (char *)systemInfo->data;
				_OutputStream.writeCharacters("2CISYSINFO     " J9RAS_CORE_USES_PID_FILE " = ");
				_OutputStream.writeCharacters(coreusespid);
				_OutputStream.writeCharacters("\n");
			}
			break;
		default:
			break;
		}
		systemInfo = J9_LINKED_LIST_NEXT_DO(_VirtualMachine->j9ras->systemInfo, systemInfo);
	}

	/* Write section for entitled CPU information */
	writeCPUinfo();

#if defined(LINUX)
	/* Write section for Cgroup Information */
	writeCgroupMetrics();
#endif

	/* Write the section trailer */
	_OutputStream.writeCharacters(
		"NULL           \n"
		"NULL           ------------------------------------------------------------------------\n"
	);
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::createPadding() method implementation                            */
/*                                                                                                */
/**************************************************************************************************/
UDATA
JavaCoreDumpWriter::createPadding(const char* str, UDATA fieldWidth, char padChar, char* buffer)
{
	IDATA difference = fieldWidth - strlen(str);

	if (difference > 0) {
		for (int i = 0; i < difference; i++) {
			buffer[i] = padChar;
		}
	} else {
		difference = 0;
	}
	buffer[difference] = '\0';
	return difference;
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeMemorySection() method implementation                                 */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeMemorySection(void)
{
	/* Write the section header */
	_OutputStream.writeCharacters(
		"0SECTION       MEMINFO subcomponent dump routine\n"
		"NULL           =================================\n"
	);

	/* Write the GC heap sub-section. Note since 2.6 VM this uses GC iterators rather than the VM segment list */
	_OutputStream.writeCharacters(
		"NULL           \n"
		"1STHEAPTYPE    Object Memory\n"
	);
	_VirtualMachine->memoryManagerFunctions->j9mm_iterate_heaps(_VirtualMachine, _PortLibrary, 0, heapIteratorCallback, this);

	/* Write the VM memory segments sub-section */
	_OutputStream.writeCharacters(
		"1STSEGTYPE     Internal Memory\n"
		SEGMENT_HEADER
	);
	writeSegments(_VirtualMachine->memorySegments, false);

	/* Write the class memory segments sub-section */
	_OutputStream.writeCharacters(
		"NULL           \n"
		"1STSEGTYPE     Class Memory\n"
		SEGMENT_HEADER
	);
	writeSegments(_VirtualMachine->classMemorySegments, false);

	/* Write the jit memory segments sub-section */
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	if (_VirtualMachine->jitConfig) {
		_OutputStream.writeCharacters(
			"NULL           \n"
			"1STSEGTYPE     JIT Code Cache\n"
			SEGMENT_HEADER
		);
		writeSegments(_VirtualMachine->jitConfig->codeCacheList, true);

		/* Write the limit specified for the code cache size as well. */
		int decimalLength = sizeof(void*) == 4 ? 10 : 20;
		_OutputStream.writeCharacters("1STSEGLIMIT    ");
		_OutputStream.writeCharacters("Allocation limit:    ");
		_OutputStream.writeVPrintf(FORMAT_SIZE_DECIMAL, decimalLength, _VirtualMachine->jitConfig->codeCacheTotalKB*1024); // Needs to be codeCacheTotalKB
		_OutputStream.writeCharacters(" (");
		_OutputStream.writeVPrintf(FORMAT_SIZE_HEX, sizeof(void *) * 2, _VirtualMachine->jitConfig->codeCacheTotalKB*1024); // Needs to be codeCacheTotalKB
		_OutputStream.writeCharacters(")\n");
		_OutputStream.writeCharacters(
			"NULL           \n"
			"1STSEGTYPE     JIT Data Cache\n"
			SEGMENT_HEADER
		);
		writeSegments(_VirtualMachine->jitConfig->dataCacheList, false);
		/* Write the limit specified for the data cache size as well. */
		_OutputStream.writeCharacters("1STSEGLIMIT    ");
		_OutputStream.writeCharacters("Allocation limit:    ");
		_OutputStream.writeVPrintf(FORMAT_SIZE_DECIMAL, decimalLength, _VirtualMachine->jitConfig->dataCacheTotalKB*1024);
		_OutputStream.writeCharacters(" (");
		_OutputStream.writeVPrintf(FORMAT_SIZE_HEX, sizeof(void *) * 2, _VirtualMachine->jitConfig->dataCacheTotalKB*1024);
		_OutputStream.writeCharacters(")\n");
	}
#endif

	/* Write the garbage collector history sub-section */
	_OutputStream.writeCharacters(
		"NULL           \n"
		"1STGCHTYPE     GC History  \n"
	);

	writeTraceHistory(HIST_TYPE_GC);


	/* Write the section trailer */
	_OutputStream.writeCharacters(
		"NULL           \n"
		"NULL           ------------------------------------------------------------------------\n"
	);
}

/**
 * Callback used by to count memory categories with j9mem_walk_categories
 */
static UDATA
countMemoryCategoriesCallback (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state)
{
	(*(I_32 *)state->userData1)++;
	return J9MEM_CATEGORIES_KEEP_ITERATING;
}

/**
 * Callback for "inner" walk of memory categories (called by outer walk)
 *
 * Maintains a total count of all categories beneath total->codeToMatch
 */
static UDATA
innerMemCategoryCallBack (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state)
{
	memcategory_total * total = (memcategory_total *) state->userData1;
	JavaCoreDumpWriter * writer = (JavaCoreDumpWriter*) state->userData2;

	if (total->codeMatched) {
		if (!isRoot && CATEGORY_IS_ANCESTOR(total, parentCategoryCode)) {
			SET_CATEGORY_AS_ANCESTOR(total, categoryCode);
			total->liveBytes += liveBytes;
			total->liveAllocations += liveAllocations;
		} else {
			/* The category walk is depth first. If this node is a root, or this node's parent isn't an ancestor,
			 * we've walked past the children of the node we're interested in.
			 */
			return J9MEM_CATEGORIES_STOP_ITERATING;
		}
	} else {
		/* We haven't found the node we're interested in yet */
		if (categoryCode == total->codeToMatch) {
			total->codeMatched = TRUE;
		}
	}

	return J9MEM_CATEGORIES_KEEP_ITERATING;
}

/**
 * Callback for "outer" walk of memory categories.
 *
 * Starts the inner walk and prints the ASCII art lines to the file.
 */
static UDATA
outerMemCategoryCallBack (U_32 categoryCode, const char * categoryName, UDATA liveBytes, UDATA liveAllocations, BOOLEAN isRoot, U_32 parentCategoryCode, OMRMemCategoryWalkState * state)
{
	U_32 i;
	U_32 depth;
	JavaCoreDumpWriter * writer = (JavaCoreDumpWriter*) state->userData1;
	memcategory_total total;
	U_32 oldStackTop = writer->_CategoryStackTop;
	PORT_ACCESS_FROM_PORT(writer->_PortLibrary);

	if (isRoot) {
		depth = 0;
		writer->_CategoryStack[0].category = categoryCode;
		writer->_CategoryStackTop = 1;
	} else {
		/* Determine our position in the tree by checking the _CategoryStack for our parent */
		for (i=0; i < writer->_CategoryStackTop; i++) {
			if (writer->_CategoryStack[i].category == parentCategoryCode) {
				break;
			}
		}

		depth = i+1;
		writer->_CategoryStack[i+1].category = categoryCode;
		writer->_CategoryStackTop = i+2;
	}

	/* If we popped frames from the category stack, write out the "Other" rows
	 * for popped frames.
	 */
	if (oldStackTop >= writer->_CategoryStackTop) {
		for (i=oldStackTop; i >= writer->_CategoryStackTop; i--) {
			int deferredDepth = i - 1;
			memcategory_data_frame * frame = &writer->_CategoryStack[deferredDepth];
			if (frame->liveAllocations != 0) {
				writer->writeNativeAllocator("Other", deferredDepth + 1, 0, frame->liveBytes, frame->liveAllocations);
			}
		}
	}

	/* Reset the current live* entries in the category stack */
	writer->_CategoryStack[depth].liveBytes = 0;
	writer->_CategoryStack[depth].liveAllocations = 0;

	/* Do an "inner" categories walk to total up the live bytes and allocations
	 * for all children of this node.
	 */
	memset(&total, 0, sizeof(memcategory_total));
	total.category_bitmask = (U_32*)alloca(writer->_TotalCategories * sizeof(U_32));
	memset(total.category_bitmask, 0, writer->_TotalCategories * sizeof(U_32));
	total.liveBytes = liveBytes;
	total.liveAllocations = liveAllocations;
	total.codeToMatch = categoryCode;
	total.codeMatched = FALSE;
	SET_CATEGORY_AS_ANCESTOR(&total, categoryCode);

	OMRMemCategoryWalkState walkState;
	memset(&walkState, 0, sizeof(OMRMemCategoryWalkState));

	walkState.walkFunction = &innerMemCategoryCallBack;
	walkState.userData1 = &total;
	walkState.userData2 = writer;

	j9mem_walk_categories(&walkState);

	if (total.liveAllocations > 0) {
		writer->writeNativeAllocator(categoryName, depth, isRoot, total.liveBytes, total.liveAllocations);

		/* Store liveBytes and liveAllocations away to print the "Other" row after the children have been printed (see top of function) */
		if (total.liveAllocations != liveAllocations && liveAllocations > 0) {
			writer->_CategoryStack[depth].liveBytes = liveBytes;
			writer->_CategoryStack[depth].liveAllocations = liveAllocations;
		}
	}

	return J9MEM_CATEGORIES_KEEP_ITERATING;
}

void
JavaCoreDumpWriter::writeMemoryCountersSection(void)
{
	I_32 i;
	PORT_ACCESS_FROM_PORT(_PortLibrary);

	/* Write the section header */
	_OutputStream.writeCharacters(
		"0SECTION       NATIVEMEMINFO subcomponent dump routine\n"
		"NULL           =================================\n"
	);

	_CategoryStackTop = 0;

	OMRMemCategoryWalkState walkState;

	memset(&walkState, 0, sizeof(OMRMemCategoryWalkState));

	_TotalCategories = 0;
	walkState.walkFunction = countMemoryCategoriesCallback;
	walkState.userData1 = &_TotalCategories;
	j9mem_walk_categories(&walkState);

	_CategoryStack = (memcategory_data_frame*) alloca(_TotalCategories * sizeof(memcategory_data_frame*));

	memset(&walkState, 0, sizeof(OMRMemCategoryWalkState));
	walkState.walkFunction = &outerMemCategoryCallBack;
	walkState.userData1 = this;

	/* Category walk is done in two pieces. The outer walk prints the lines in the javacore and calls
	 * the inner walk, used for totalling up the values of the categories beneath that value.
	 */
	j9mem_walk_categories(&walkState);

	/* Print any final "Other" categories */
	for (i=_CategoryStackTop - 1; i >= 0; i--) {
		memcategory_data_frame * frame = &_CategoryStack[i];
		if (frame->liveAllocations != 0) {
			writeNativeAllocator("Other", i + 1, 0, frame->liveBytes, frame->liveAllocations);
		}
	}

	/* Write the section trailer */
	_OutputStream.writeCharacters(
		"NULL           \n"
		"NULL           ------------------------------------------------------------------------\n"
	);
}

/**
 * Builds the ASCII-art table for native memory categories.
 *
 * Called by innerMemCategoryCallBack
 */
void
JavaCoreDumpWriter::writeNativeAllocator(const char * name, U_32 depth, BOOLEAN isRoot, UDATA liveBytes, UDATA liveAllocations)
{
	U_32 i;
	/* Print the separating row containing down lines */
	if (depth > 0) {
		_OutputStream.writeInteger(depth, "%u");
		_OutputStream.writeCharacters("MEMUSER     ");
		for (i=0; i < depth; i++) {
			_OutputStream.writeCharacters("  |");
		}

		_OutputStream.writeCharacters("\n");
	} else {
		_OutputStream.writeCharacters("0MEMUSER\n");
	}

	/* Print the row for this category */
	_OutputStream.writeInteger(depth + 1, "%u");
	_OutputStream.writeCharacters("MEMUSER       ");
	if (! isRoot) {
		for (i=0; i < (depth - 1); i++) {
			_OutputStream.writeCharacters("|  ");
		}
		_OutputStream.writeCharacters("+--");
	}
	_OutputStream.writeCharacters(name);
	_OutputStream.writeCharacters(": ");

	_OutputStream.writeIntegerWithCommas(liveBytes);

	_OutputStream.writeCharacters(" bytes");

	_OutputStream.writeCharacters(" / ");
	_OutputStream.writeInteger(liveAllocations, "%zu");
	_OutputStream.writeCharacters(" allocation");
	if (liveAllocations > 1) {
		_OutputStream.writeCharacters("s");
	}
	_OutputStream.writeCharacters("\n");
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeMonitorSection() method implementation                                */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeMonitorSection(void)
{
	/* The code calling this method must have taken the monitorTableMutex and the thread library monitor_mutex
	 * (in that order) prior to calling and must release those locks on return from this method.
	 */
	J9ThreadMonitor* monitor = NULL;
	omrthread_monitor_walk_state_t walkState;
	blocked_thread_record *threadStore;
	UDATA blockedCount = 0;
	bool restartedWalk = 0;
	J9VMThread* vmThread = _Context->onThread;
	PORT_ACCESS_FROM_PORT(_PortLibrary);

	/* Write the section header */
	_OutputStream.writeCharacters(
		"0SECTION       LOCKS subcomponent dump routine\n"
		"NULL           ===============================\n"
	);

	/* Write the object locks */
	_OutputStream.writeCharacters(
		"NULL           \n"
		"1LKPOOLINFO    Monitor pool info:\n"
		"2LKPOOLTOTAL     Current total number of monitors: "
	);

	_OutputStream.writeInteger(getObjectMonitorCount(_VirtualMachine), "%zu");
	_OutputStream.writeCharacters("\n");
	_OutputStream.writeCharacters("NULL           \n");

	/* Stack-allocate a store for blocked thread information, to save having to re-walk the threads. First
	 * check that we have enough stack space, and bail out if not (typically ~10,000 threads). See RTC 87530.
	 */
	UDATA freeStack = vmThread ? vmThread->currentOSStackFree : _VirtualMachine->defaultOSStackSize;
	if (((_AllocatedVMThreadCount + 1) * sizeof(blocked_thread_record) + STACK_SAFETY_MARGIN) > freeStack) {
		_OutputStream.writeCharacters("1LKALLOCERR    Insufficient stack space for thread monitor walk\n");
		/* Write the section trailer */
		_OutputStream.writeCharacters(
			"NULL           \n"
			"NULL           ------------------------------------------------------------------------\n");
		return; /* bail out of the monitor section */
	}
	/* Allow + 1 for null termination of list. */
	threadStore = (blocked_thread_record*)alloca((_AllocatedVMThreadCount+1) * sizeof(blocked_thread_record));

	/* populate the thread store with information on blocked threads to stop us re-walking the threads */
	J9VMThread* walkThread = J9_LINKED_LIST_START_DO(_VirtualMachine->mainThread);
	for (UDATA i = 0; walkThread != NULL && i < _AllocatedVMThreadCount; i++) {
		omrthread_monitor_t monitor;
		J9VMThread *lockOwner;
		void *args[] = {walkThread, NULL, &monitor, &lockOwner, NULL};
		UDATA stateClean = 0;
		UDATA stateFault = stateClean;

		if (i == 0) {
			// The walk may have started or restarted which is why initialization is in the loop.
			memset(threadStore, 0, (_AllocatedVMThreadCount+1) * sizeof(blocked_thread_record));
		}

		if (j9sig_protect(protectedGetVMThreadRawState, args, handlerGetVMThreadRawState, &stateFault, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &stateClean) == J9PORT_SIG_EXCEPTION_OCCURRED) {
			// Nothing to do if we couldn't get the details for this thread.
		} else {
			// Only store interesting threads, list will be null terminated.
			if( ((stateClean == J9VMTHREAD_STATE_BLOCKED) ||
				(stateClean == J9VMTHREAD_STATE_WAITING) ||
				(stateClean == J9VMTHREAD_STATE_WAITING_TIMED))
				) {
				threadStore[blockedCount].monitor = monitor;
				threadStore[blockedCount].waitingThread = walkThread;
				threadStore[blockedCount].waitingThreadState = stateClean;
				blockedCount++;
			}
		}

		walkThread = J9_LINKED_LIST_NEXT_DO(_VirtualMachine->mainThread, walkThread);
		if (walkThread != NULL && walkThread->publicFlags == J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION) {
			/* restart the walk */
			if (!restartedWalk) {
				walkThread = J9_LINKED_LIST_START_DO(_VirtualMachine->mainThread);
				i = 0;
				restartedWalk = 1;
				continue;
			} else {
				_OutputStream.writeCharacters(
					"1LKTHRERR            <aborting search for blocked and waiting threads due to exiting thread>\n"
					"NULL           \n" );
				break;
			}
		}
	}

	/* Write the object monitors */
	_OutputStream.writeCharacters("1LKMONPOOLDUMP Monitor Pool Dump (flat & inflated object-monitors):\n");

	omrthread_monitor_init_walk(&walkState);

	while ( NULL != (monitor = omrthread_monitor_walk_no_locking(&walkState)) ) {
		J9ThreadAbstractMonitor* lock = (J9ThreadAbstractMonitor*)monitor;
		if ((lock->flags & J9THREAD_MONITOR_OBJECT) == J9THREAD_MONITOR_OBJECT) {
			writeMonitorObject(monitor, (j9object_t)lock->userData, threadStore);
		}
	}

	/* Write the system monitors */
	_OutputStream.writeCharacters(
		"NULL           \n"
		"1LKREGMONDUMP  JVM System Monitor Dump (registered monitors):\n"
	);

	omrthread_monitor_init_walk(&walkState);

	while ( NULL != (monitor = omrthread_monitor_walk_no_locking(&walkState)) ) {
		J9ThreadAbstractMonitor* lock = (J9ThreadAbstractMonitor*)monitor;
		if ((lock->flags & J9THREAD_MONITOR_OBJECT) != J9THREAD_MONITOR_OBJECT) {
			writeMonitorObject(monitor, NULL, threadStore);
		}
	}

	/* Write the deadlocks */
	writeDeadLocks();

	/* Write the section trailer */
	_OutputStream.writeCharacters(
		"NULL           \n"
		"NULL           ------------------------------------------------------------------------\n"
	);
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeThreadSection() method implementation                                 */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeThreadSection(void)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	/* Write the section header */
	_OutputStream.writeCharacters(
		"0SECTION       THREADS subcomponent dump routine\n"
		"NULL           =================================\n"
	);

	/* Write the thread counts */
	_OutputStream.writeCharacters("NULL\n");
	_OutputStream.writeCharacters(
		"1XMPOOLINFO    JVM Thread pool info:\n");
	_OutputStream.writeCharacters(
		"2XMPOOLTOTAL       Current total number of pooled threads: ");
	_OutputStream.writeInteger(_AllocatedVMThreadCount, "%i");
	_OutputStream.writeCharacters("\n");
	_OutputStream.writeCharacters(
		"2XMPOOLLIVE        Current total number of live threads: ");
	_OutputStream.writeInteger(_VirtualMachine->totalThreadCount, "%i");
	_OutputStream.writeCharacters("\n");
		_OutputStream.writeCharacters(
		"2XMPOOLDAEMON      Current total number of live daemon threads: ");
	_OutputStream.writeInteger(_VirtualMachine->daemonThreadCount, "%i");
	_OutputStream.writeCharacters("\n");

#if !defined(OSX)
	/* if thread preempt is enabled, and we have the lock, then collect the native stacks */
	if ((_Agent->requestMask & J9RAS_DUMP_DO_PREEMPT_THREADS) && _PreemptLocked
#if defined(WIN32)
		/* On Windows don't attempt to collect native stacks for the thread start and end hook events because
		 * the Windows DbgHelp functions are prone to hangs if called when threads are starting or stopping.
		 */
		&& !(_Context->eventFlags & J9RAS_DUMP_ON_THREAD_START)
		&& !(_Context->eventFlags & J9RAS_DUMP_ON_THREAD_END)
#endif /* defined(WIN32) */
	) {
		struct walkClosure closure;
		UDATA sink = 0;
		closure.jcw = this;
		closure.state = NULL;
		j9sig_protect(protectedWriteThreadsWithNativeStacks,
				&closure, handlerWriteStacks, this,
				J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN,
				&sink);
	}
#endif /* !defined(OSX) */

	if( !_ThreadsWalkStarted ) {
		struct walkClosure closure;
		UDATA sink = 0;
		closure.jcw = this;
		closure.state = NULL;
		j9sig_protect(protectedWriteThreadsJavaOnly,
				&closure, handlerWriteStacks, this,
				J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN,
				&sink);
	}

	if ((_Agent->requestMask & J9RAS_DUMP_DO_PREEMPT_THREADS) && (_PreemptLocked == false) ) {
		/* another thread had the preempt lock */
		_OutputStream.writeCharacters("1XMWLKTHDINF   Multiple dumps in progress, native stacks not collected\n");
	}

	/* Only display the thread trace if we have a current thread, if this is event where "current thread" is meaningful
	 * and this isn't a thrstop event. Trace may receive the call to the J9HOOK_VM_THREAD_END hook first and clean up
	 * the trace data for this thread first.
	 */
	if( _Context->onThread && (_Context->eventFlags & syncEventsMask) && !(_Context->eventFlags & J9RAS_DUMP_ON_THREAD_END)) {
		// Write current thread trace.
		_OutputStream.writeCharacters("1XECTHTYPE     Current thread history (J9VMThread:");
		_OutputStream.writePointer(_Context->onThread);
		_OutputStream.writeCharacters(")\n");
		writeTraceHistory(HIST_TYPE_CT);
		_OutputStream.writeCharacters("NULL\n");
	}

	/* avoidLocks is set if a thread is holding the vmThreadListMutex, if that is the case, not a good
	 * idea to be walking the list of threads */
	if (!avoidLocks()) {
		struct walkClosure closure;
		UDATA sink = 0;
		closure.jcw = this;
		closure.state = NULL;
		j9sig_protect(protectedWriteThreadsUsageSummary,
					  &closure, handlerGetThreadsUsageInfo, this,
					  J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &sink);
	}

	// End the threads section here.
	_OutputStream.writeCharacters(
			"NULL           ------------------------------------------------------------------------\n"
	);
}

void
JavaCoreDumpWriter::writeThreadsUsageSummary(void)
{
	J9ThreadsCpuUsage cpuUsage;
	I_64 totalTime = 0;
	IDATA rc = 0;

	if (J9_ARE_NO_BITS_SET(_VirtualMachine->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_ENABLE_CPU_MONITOR)) {
		return;
	}
	memset(&cpuUsage, 0, sizeof(J9ThreadsCpuUsage));

	/* The following is done without holding the vmThreadListMutex as that can cause deadlocks */
	rc = omrthread_get_jvm_cpu_usage_info(&cpuUsage);
	if (rc < 0) {
		return;
	}

	_OutputStream.writeCharacters(
				"1XMTHDSUMMARY  Threads CPU Usage Summary\n"
				"NULL           =========================\n"
			);

	if (J9_ARE_ALL_BITS_SET(_VirtualMachine->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_REDUCE_CPU_MONITOR_OVERHEAD)) {
		_OutputStream.writeCharacters("NULL\n1XMTHDCATINFO  Warning: to get more accurate CPU times for the GC, the option -XX:-ReduceCPUMonitorOverhead can be used. See the user guide for more information.\nNULL\n");
	}

	totalTime = cpuUsage.applicationCpuTime + cpuUsage.resourceMonitorCpuTime + cpuUsage.systemJvmCpuTime;
	_OutputStream.writeCharacters("1XMTHDCATEGORY ");
	writeThreadTime("All JVM attached threads", (totalTime*1000));
	_OutputStream.writeCharacters("\n1XMTHDCATEGORY |");
	if (cpuUsage.resourceMonitorCpuTime > 0) {
		_OutputStream.writeCharacters("\n2XMTHDCATEGORY +--");
		writeThreadTime("Resource-Monitor", (cpuUsage.resourceMonitorCpuTime*1000));
		_OutputStream.writeCharacters("\n1XMTHDCATEGORY |");
	}
	_OutputStream.writeCharacters("\n2XMTHDCATEGORY +--");
	writeThreadTime("System-JVM", (cpuUsage.systemJvmCpuTime*1000));
	_OutputStream.writeCharacters("\n2XMTHDCATEGORY |  |");
	_OutputStream.writeCharacters("\n3XMTHDCATEGORY |  +--");
	writeThreadTime("GC", (cpuUsage.gcCpuTime*1000));
	_OutputStream.writeCharacters("\n2XMTHDCATEGORY |  |");
	_OutputStream.writeCharacters("\n3XMTHDCATEGORY |  +--");
	writeThreadTime("JIT", (cpuUsage.jitCpuTime*1000));
	_OutputStream.writeCharacters("\n1XMTHDCATEGORY |");
	/* Print application time even if it is zero,
	 * however print user defined categories only if they are greater than zero */
	if (cpuUsage.applicationCpuTime >= 0) {
		_OutputStream.writeCharacters("\n2XMTHDCATEGORY +--");
		writeThreadTime("Application", (cpuUsage.applicationCpuTime*1000));
		if (cpuUsage.applicationUserCpuTime[0] > 0) {
			_OutputStream.writeCharacters("\n2XMTHDCATEGORY |  |");
			_OutputStream.writeCharacters("\n3XMTHDCATEGORY |  +--");
			writeThreadTime("Application-User1", (cpuUsage.applicationUserCpuTime[0]*1000));
		}
		if (cpuUsage.applicationUserCpuTime[1] > 0) {
			_OutputStream.writeCharacters("\n2XMTHDCATEGORY |  |");
			_OutputStream.writeCharacters("\n3XMTHDCATEGORY |  +--");
			writeThreadTime("Application-User2", (cpuUsage.applicationUserCpuTime[1]*1000));
		}
		if (cpuUsage.applicationUserCpuTime[2] > 0) {
			_OutputStream.writeCharacters("\n2XMTHDCATEGORY |  |");
			_OutputStream.writeCharacters("\n3XMTHDCATEGORY |  +--");
			writeThreadTime("Application-User3", (cpuUsage.applicationUserCpuTime[2]*1000));
		}
		if (cpuUsage.applicationUserCpuTime[3] > 0) {
			_OutputStream.writeCharacters("\n2XMTHDCATEGORY |  |");
			_OutputStream.writeCharacters("\n3XMTHDCATEGORY |  +--");
			writeThreadTime("Application-User4", (cpuUsage.applicationUserCpuTime[3]*1000));
		}
		if (cpuUsage.applicationUserCpuTime[4] > 0) {
			_OutputStream.writeCharacters("\n2XMTHDCATEGORY |  |");
			_OutputStream.writeCharacters("\n3XMTHDCATEGORY |  +--");
			writeThreadTime("Application-User5", (cpuUsage.applicationUserCpuTime[4]*1000));
		}
	}
	_OutputStream.writeCharacters("\nNULL\n");
}

void
JavaCoreDumpWriter::writeHookInfo(struct OMRHookInfo4Dump *hookInfo)
{
	char timeStamp[_MaximumTimeStampLength + 1];
	OMRPORT_ACCESS_FROM_OMRVM(_VirtualMachine->omrVM);

	_OutputStream.writeCharacters("4HKCALLSITE        ");
	if (NULL != hookInfo->callsite) {
		_OutputStream.writeCharacters(hookInfo->callsite);
	} else {
		_OutputStream.writePointer(hookInfo->func_ptr);
	}
	_OutputStream.writeCharacters("\n");
	_OutputStream.writeCharacters("4HKSTARTTIME       Start Time: ");
	/* convert time from microseconds to milliseconds */
	uint64_t startTimeMs = hookInfo->startTime / 1000;
	omrstr_ftime(timeStamp, _MaximumTimeStampLength, "%Y-%m-%dT%H:%M:%S", startTimeMs);
	/* nul-terminate timestamp in case omrstr_ftime didn't have enough room to do so */
	timeStamp[_MaximumTimeStampLength] = '\0';
	_OutputStream.writeCharacters(timeStamp);
	_OutputStream.writeInteger64(startTimeMs % 1000, ".%03llu");
	_OutputStream.writeCharacters("\n");
	_OutputStream.writeCharacters("4HKDURATION        Duration: ");
	_OutputStream.writeInteger64(hookInfo->duration, "%llu");
	_OutputStream.writeCharacters("us\n");
}

void
JavaCoreDumpWriter::writeHookInterface(struct J9HookInterface **hookInterface)
{
	J9CommonHookInterface *commonInterface = (J9CommonHookInterface *)hookInterface;

	_OutputStream.writeCharacters("NULL           ------------------------------------------------------------------------\n");
	for (UDATA i = 1; commonInterface->eventSize > i; i++) {
		OMREventInfo4Dump *eventDump = J9HOOK_DUMPINFO(commonInterface, i);

		if (0 != eventDump->count) {
			_OutputStream.writeCharacters("2HKEVENTID     ");
			_OutputStream.writeInteger(i, "%zu");
			_OutputStream.writeCharacters("\n");

			_OutputStream.writeCharacters("3HKCALLCOUNT     ");
			_OutputStream.writeInteger(eventDump->count, "%zu");
			_OutputStream.writeCharacters("\n");

			_OutputStream.writeCharacters("3HKTOTALTIME     ");
			_OutputStream.writeInteger(eventDump->totalTime, "%zu");
			_OutputStream.writeCharacters("us\n");

			if ((NULL != eventDump->lastHook.callsite) || (NULL != eventDump->lastHook.func_ptr)) {
				_OutputStream.writeCharacters("3HKLAST          Last Callback\n");
				writeHookInfo(&eventDump->lastHook);
				_OutputStream.writeCharacters("3HKLONGST        Longest Callback\n");
				writeHookInfo(&eventDump->longestHook);
			}
			_OutputStream.writeCharacters("NULL\n");
		}

		/* reset the eventDump statistics */
		eventDump->count = 0;
		eventDump->totalTime = 0;

		eventDump->longestHook.startTime = 0;
		eventDump->longestHook.callsite = NULL;
		eventDump->longestHook.func_ptr = NULL;
		eventDump->longestHook.duration = 0;

		eventDump->lastHook.startTime = 0;
		eventDump->lastHook.callsite = NULL;
		eventDump->lastHook.func_ptr = NULL;
		eventDump->lastHook.duration = 0;
	}
}

void
JavaCoreDumpWriter::writeThreadsWithNativeStacks(void)
{

	J9VMThread* vmThread = _Context->onThread;
	J9PlatformThread *nativeThread = NULL;
	J9ThreadWalkState state;
	J9AVLTree vmthreads;
	vmthread_avl_node *vmthreadStore;
	J9Heap *heap;
	UDATA i = 0;
	UDATA vmstate = 0;
	UDATA javaState = 0;
	UDATA javaPriority = 0;
	struct walkClosure closure;
	const char *errorMessage = NULL;
	/* backs the heap we use for thread introspection. Size is a guess for context + reasonable stack */
	char backingStore[8096];
	bool restartedWalk = 0;

	PORT_ACCESS_FROM_PORT(_PortLibrary);

	/* This function can fail early if there is insufficient stack space for the AVL tree. If we have
	 * not got to the point where we set _ThreadsWalkStarted, the calling code in writeThreadSection()
	 * will re-try, writing the Java threads and stacks only. See PR 81717 and PR 40206.
	 */
	UDATA freeStack = vmThread ? vmThread->currentOSStackFree : _VirtualMachine->defaultOSStackSize;
	if ((_AllocatedVMThreadCount * sizeof(vmthread_avl_node) + STACK_SAFETY_MARGIN) > freeStack) {
		_OutputStream.writeCharacters("NULL\n");
		_OutputStream.writeCharacters("1XMWLKTHDINF   Insufficient stack space for native stack collection\n");
		return; /* bail out, writeThreadSection() will retry with Java threads only */
	}

	vmthreadStore = (vmthread_avl_node*)alloca(_AllocatedVMThreadCount * sizeof(vmthread_avl_node));
	memset(vmthreadStore, 0, _AllocatedVMThreadCount * sizeof(vmthread_avl_node));

	heap = j9heap_create(backingStore, sizeof(backingStore), 0);

	memset(&state, 0, sizeof(state));

	_ThreadsWalkStarted = true;

	/* first and second phase of native thread stack collection timeout set to 10 seconds each */
	state.deadline1 = (j9time_current_time_millis()/1000) + 10;
	state.deadline2 = state.deadline1 + 10;

	closure.state = &state;
	closure.heap = heap;
	closure.jcw = this;

	/* populate the VM thread avl tree or dump the java stacks if they won't fit */
	J9VMThread* walkThread = J9_LINKED_LIST_START_DO(_VirtualMachine->mainThread);
	for (i = 0; walkThread != NULL && i < _AllocatedVMThreadCount; i++) {
		j9object_t lockObject;
		J9VMThread *lockOwner;
		void *args[] = {walkThread, &lockObject, NULL, &lockOwner, NULL};
		UDATA stateClean = 0;
		UDATA stateFault = stateClean;
		UDATA vmThreadState = 0;
		UDATA javaThreadState = 0;
		UDATA javaPriority = 0;

		if (i == 0) {
			/* build the avl tree for lookup. Having this in the loop allows us to restart the walk easily if needed */
			memset(&vmthreads, 0, sizeof(J9AVLTree));
			vmthreads.insertionComparator = vmthread_comparator;
			vmthreads.searchComparator = vmthread_locator;
		}

		vmthreadStore[i].vmthread = walkThread;

		/* Obtain java state through getVMThreadObjectState() for outputting to javacore */
		if (J9PORT_SIG_EXCEPTION_OCCURRED == j9sig_protect(protectedGetVMThreadObjectState, args, handlerGetVMThreadObjectState, &stateFault, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &stateClean)) {
			javaThreadState = J9VMTHREAD_STATE_UNREADABLE;
		} else {
			javaThreadState = stateClean;
		}

		if (J9PORT_SIG_EXCEPTION_OCCURRED == j9sig_protect(protectedGetVMThreadRawState, args, handlerGetVMThreadRawState, &stateFault, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &stateClean)) {
			vmThreadState = J9VMTHREAD_STATE_UNREADABLE;
		} else {
			vmThreadState = stateClean;
		}

		if (walkThread->threadObject) {
			javaPriority = _VirtualMachine->internalVMFunctions->getJavaThreadPriority(_VirtualMachine, walkThread);
		}

		vmthreadStore[i].vmThreadState = vmThreadState;
		vmthreadStore[i].javaThreadState = javaThreadState;
		vmthreadStore[i].lockObject = lockObject;
		vmthreadStore[i].lockOwner = lockOwner;
		vmthreadStore[i].javaPriority = javaPriority;
		avl_insert(&vmthreads, (J9AVLTreeNode*)&vmthreadStore[i]);

		walkThread = J9_LINKED_LIST_NEXT_DO(_VirtualMachine->mainThread, walkThread);
		if (walkThread != NULL && walkThread->publicFlags == J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION) {
			/* restart the walk */
			if (!restartedWalk) {
				walkThread = J9_LINKED_LIST_START_DO(_VirtualMachine->mainThread);
				i = 0;
				restartedWalk = 1;
				continue;
			} else {
				errorMessage = "Truncating collection of java threads due to multiple threads stopping during walk, some java thread details will be omitted";
				break;
			}
		}
	}

	UDATA returnValue;
	nativeThread = NULL;

	if (vmThread && vmThread->gpInfo) {
		/* Extract the OS thread */
		closure.gpInfo = vmThread->gpInfo;

		returnValue = j9sig_protect(protectedStartDoWithSignal, &closure, handlerNativeThreadWalk, this, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, (UDATA*)&nativeThread);
	} else {
		returnValue = j9sig_protect(protectedStartDo, &closure, handlerNativeThreadWalk, this, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, (UDATA*)&nativeThread);
	}

	if (returnValue == J9PORT_SIG_EXCEPTION_OCCURRED) {
		errorMessage = "GPF received while walking native threads\n";

		/* we need to set up the next thread so we continue the walk if possible */
		while (j9sig_protect(protectedNextDo, &closure, handlerNativeThreadWalk, this, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, (UDATA*)&nativeThread) == J9PORT_SIG_EXCEPTION_OCCURRED) {
			errorMessage = "GPF received while walking native threads\n";
		}
	}

	/** Write the current thread out (if appropriate) **/
	if ((vmThread && vmThread->gpInfo) || (_Context->eventFlags & syncEventsMask)) {
		/* if synchronous */
		J9PlatformThread pseudoThread;
		vmthread_avl_node *node;
		j9object_t lockObject = NULL;
		J9VMThread *lockOwnerThread = NULL;
		/* Write the failing thread sub section */
		_OutputStream.writeCharacters(
			"NULL            \n"
			"1XMCURTHDINFO  Current thread\n"
		);

		if (nativeThread == NULL) {
			nativeThread = &pseudoThread;
			memset(nativeThread, 0, sizeof(J9PlatformThread));

			/* if there's no native thread then we need to figure out what tid vmThread is stored
			 * against so we can delete it and prevent it being duplicated when we dump outstanding
			 * java threads
			 */
			if (vmThread && vmThread->osThread) {
				pseudoThread.thread_id = omrthread_get_osId(vmThread->osThread);
				if (pseudoThread.thread_id == 0) {
					pseudoThread.thread_id = (UDATA) (((U_8*)vmThread->osThread) + sizeof(J9AbstractThread));
				}
			}
		}

		node = (vmthread_avl_node *)avl_search(&vmthreads, nativeThread->thread_id);

		if (node) {
			avl_delete(&vmthreads, (J9AVLTreeNode*)node);
			walkThread = node->vmthread;
			vmstate = node->vmThreadState;
			javaState = node->javaThreadState;
			javaPriority = node->javaPriority;
			lockObject = node->lockObject;
			lockOwnerThread = node->lockOwner;
		} else {
			walkThread = NULL;
			vmstate = 0;
			javaState = 0;
			javaPriority = 0;
		}

		if (nativeThread == &pseudoThread) {
			if (j9introspect_backtrace_thread(nativeThread, heap, vmThread->gpInfo) == 0) {
				/* failed to produce backtrace from signal */
				nativeThread = NULL;
			} else {
				/* try symbol resolution */
				j9introspect_backtrace_symbols(nativeThread, heap);
			}
		}

		/* write out our failing thread */
		writeThread(walkThread, nativeThread, vmstate, javaState, javaPriority, lockObject, lockOwnerThread);

		/* set up the next thread for processing */
		while (j9sig_protect(protectedNextDo, &closure, handlerNativeThreadWalk, this, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, (UDATA*)&nativeThread) != 0) {
			errorMessage = "GPF received while walking native threads\n";
		}
	} else {
		/* this is an externally prompted event so don't pull out any thread as special */
	}

	if (nativeThread != NULL || vmthreads.rootNode != NULL) {
		/* Write the all threads sub section */
		_OutputStream.writeCharacters(
			"NULL           \n"
			"1XMTHDINFO     Thread Details\n"
			"NULL           \n"
		);

		/* dump combined native/java or pure native threads */
		while (nativeThread != NULL) {
			J9VMThread *javaThread = NULL;
			j9object_t lockObject = NULL;
			J9VMThread *lockOwnerThread = NULL;
			if (vmthreads.rootNode != NULL) {
				vmthread_avl_node *node = (vmthread_avl_node *)avl_search(&vmthreads, nativeThread->thread_id);
				if (node) {
					avl_delete(&vmthreads, (J9AVLTreeNode*)node);
					javaThread = node->vmthread;
					vmstate = node->vmThreadState;
					javaState = node->javaThreadState;
					javaPriority = node->javaPriority;
					lockObject = node->lockObject;
					lockOwnerThread = node->lockOwner;
				} else {
					javaThread = NULL;
					vmstate = 0;
					javaState = 0;
					javaPriority = 0;
				}
			}

			writeThread(javaThread, nativeThread, vmstate, javaState, javaPriority, lockObject, lockOwnerThread);

			/* set up the next thread safely */
			while (j9sig_protect(protectedNextDo, &closure, handlerNativeThreadWalk, this, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, (UDATA*)&nativeThread) != 0) {
				errorMessage = "GPF received while walking native threads\n";
			}
		}

		/* dump any additional java threads */
		while (vmthreads.rootNode != NULL) {
			vmthread_avl_node *node = (vmthread_avl_node*)vmthreads.rootNode;
			avl_delete(&vmthreads, (J9AVLTreeNode*)node);
			writeThread(node->vmthread, NULL, node->vmThreadState, node->javaThreadState, node->javaPriority, node->lockObject, node->lockOwner);
		}
	}

	/* If there were any errors in the walk then detail them now */
	if (state.error || errorMessage != NULL) {
		_OutputStream.writeCharacters("1XMWLKTHDERR   The following was reported while collecting native stacks:\n");

		if (state.error) {
			_OutputStream.writeCharacters("2XMWLKTHDERR             ");
			_OutputStream.writeCharacters(state.error_string);
			_OutputStream.writeInteger(state.error, "(%i");
			_OutputStream.writeInteger(state.error_detail, ", %i)\n");
		}

		if (errorMessage != NULL) {
			_OutputStream.writeCharacters("2XMWLKTHDERR             ");
			_OutputStream.writeCharacters(errorMessage);
			_OutputStream.writeCharacters("\n");
		}

		_OutputStream.writeCharacters("NULL\n");
	}

}


void
JavaCoreDumpWriter::writeThreadsJavaOnly(void)
{

	J9VMThread* vmThread = _Context->onThread;
	J9VMThread* currentThread = NULL;
	bool restartedWalk = 0;

	PORT_ACCESS_FROM_PORT(_PortLibrary);

	_ThreadsWalkStarted = true;

	if ((vmThread && vmThread->gpInfo) || (_Context->eventFlags & syncEventsMask)) {
		currentThread = vmThread;
	}

	/** Write the current thread out (if appropriate) **/
	if ( currentThread != NULL) {
		j9object_t lockObject;
		J9VMThread *lockOwner;
		void *args[] = {currentThread, &lockObject, NULL, &lockOwner, NULL};
		UDATA stateClean = 0;
		UDATA stateFault = stateClean;
		UDATA vmThreadState = 0;
		UDATA javaThreadState = 0;
		UDATA javaPriority = 0;

		/* if synchronous */
		/* Write the failing thread sub section */

		/* Obtain java state through getVMThreadObjectState() for outputting to javacore */
		if (J9PORT_SIG_EXCEPTION_OCCURRED == j9sig_protect(protectedGetVMThreadObjectState, args, handlerGetVMThreadObjectState, &stateFault, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &stateClean)) {
			javaThreadState = J9VMTHREAD_STATE_UNREADABLE;
		} else {
			javaThreadState = stateClean;
		}

		if (J9PORT_SIG_EXCEPTION_OCCURRED == j9sig_protect(protectedGetVMThreadRawState, args, handlerGetVMThreadRawState, &stateFault, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &stateClean)) {
			vmThreadState = J9VMTHREAD_STATE_UNREADABLE;
		} else {
			vmThreadState = stateClean;
		}

		if (currentThread->threadObject) {
			javaPriority = _VirtualMachine->internalVMFunctions->getJavaThreadPriority(_VirtualMachine, currentThread);
		}

		_OutputStream.writeCharacters(
			"NULL            \n"
			"1XMCURTHDINFO  Current thread\n"
		);
		/* write out our failing thread */
		writeThread(currentThread, NULL, vmThreadState, javaThreadState, javaPriority, lockObject, lockOwner);
	}

	/* dump the java stacks for all threads*/
	J9VMThread* walkThread = J9_LINKED_LIST_START_DO(_VirtualMachine->mainThread);
	for (UDATA i = 0; walkThread != NULL && i < _AllocatedVMThreadCount; i++) {
		j9object_t lockObject;
		J9VMThread *lockOwner;
		void *args[] = {walkThread, &lockObject, NULL, &lockOwner, NULL};
		UDATA stateClean = 0;
		UDATA stateFault = stateClean;
		UDATA vmThreadState = 0;
		UDATA javaThreadState = 0;
		UDATA javaPriority = 0;

		/* If we have a current thread it will already have been written. */
		if( walkThread != currentThread ) {
			/* Obtain java state through getVMThreadObjectState() for outputting to javacore */
			if (J9PORT_SIG_EXCEPTION_OCCURRED == j9sig_protect(protectedGetVMThreadObjectState, args, handlerGetVMThreadObjectState, &stateFault, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &stateClean)) {
				javaThreadState = J9VMTHREAD_STATE_UNREADABLE;
			} else {
				javaThreadState = stateClean;
			}

			if (J9PORT_SIG_EXCEPTION_OCCURRED == j9sig_protect(protectedGetVMThreadRawState, args, handlerGetVMThreadRawState, &stateFault, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &stateClean)) {
				vmThreadState = J9VMTHREAD_STATE_UNREADABLE;
			} else {
				vmThreadState = stateClean;
			}

			if (walkThread->threadObject) {
				javaPriority = _VirtualMachine->internalVMFunctions->getJavaThreadPriority(_VirtualMachine, walkThread);
			}
			if( i == 0 ) {
				_OutputStream.writeCharacters(
						"NULL           \n"
						"1XMTHDINFO     Thread Details\n"
						"NULL           \n"
				);
			}
			writeThread(walkThread, NULL, vmThreadState, javaThreadState, javaPriority, lockObject, lockOwner);
		}

		walkThread = J9_LINKED_LIST_NEXT_DO(_VirtualMachine->mainThread, walkThread);
		if (walkThread != NULL && walkThread->publicFlags == J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION) {
			/* restart the walk */
			if (!restartedWalk) {
				walkThread = J9_LINKED_LIST_START_DO(_VirtualMachine->mainThread);
				i = 0;
				restartedWalk = 1;
				continue;
			} else {
				break;
			}
		}
	}

	_OutputStream.writeCharacters("NULL           ------------------------------------------------------------------------\n");
}


/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeClassSection() method implementation                                  */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeClassSection(void)
{
	/* Write the section and first sub-section header */
	_OutputStream.writeCharacters(
		"0SECTION       CLASSES subcomponent dump routine\n"
		"NULL           =================================\n"
		"1CLTEXTCLLOS   \tClassloader summaries\n"
		"1CLTEXTCLLSS   \t\t12345678: 1=primordial,2=extension,3=shareable,4=middleware,5=system,6=trusted,7=application,8=delegating\n"
	);

	pool_do(_VirtualMachine->classLoaderBlocks,writeLoaderCallBack, this);

	/* Write the sub-section header */
	_OutputStream.writeCharacters(
		"1CLTEXTCLLIB   \tClassLoader loaded libraries\n"
	);

	pool_do(_VirtualMachine->classLoaderBlocks, writeLibrariesCallBack, this);

	/* Write the sub-section header */
	_OutputStream.writeCharacters(
		"1CLTEXTCLLOD   \tClassLoader loaded classes\n"
	);

	pool_do(_VirtualMachine->classLoaderBlocks, writeClassesCallBack, this);

	/* Write the section trailer */
	_OutputStream.writeCharacters(
		"NULL           ------------------------------------------------------------------------\n"
	);
}

/**
 * JavaCoreDumpWriter::writeHookSection() method implementation
 *
 * 0SECTION       HOOK subcomponent dump routine
 * NULL           ==============================
 * 1NOTE          This data is reset after writing each javacore file
 * NULL           ------------------------------------------------------------------------
 * 1HKINTERFACE   MM_OMRHookInterface
 * NULL           ------------------------------------------------------------------------
 * 2HKEVENTID     1
 * 3HKCALLCOUNT     19
 * 3HKTOTALTIME     212us
 * 3HKLAST          Last Callback
 * 4HKCALLSITE        trcengine.c:395
 * 4HKSTARTTIME       Start Time: 2019-10-03T11:48:34.076
 * 4HKDURATION        Duration: 10us
 * 3HKLONGST        Longest Callback
 * 4HKCALLSITE        trcengine.c:395
 * 4HKSTARTTIME       Start Time: 2019-10-03T11:48:34.391
 * 4HKDURATION        Duration: 50us
 * NULL
 * 2HKEVENTID     2
 * 3HKCALLCOUNT     2
 * 3HKTOTALTIME     0us
 * 3HKLAST          Last Callback
 * 4HKCALLSITE        jvmtiHook.c:1654
 * 4HKSTARTTIME       Start Time: 2019-10-03T11:48:34.236
 * 4HKDURATION        Duration: 0us
 * 3HKLONGST        Longest Callback
 * 4HKCALLSITE        jvmtiHook.c:1654
 * 4HKSTARTTIME       Start Time: 2019-10-03T11:48:34.236
 * 4HKDURATION        Duration: 0us
 * NULL
 * ...
 * 1HKINTERFACE   MM_PrivateHookInterface
 * NULL           ------------------------------------------------------------------------
 * ...
 * 1HKINTERFACE   MM_HookInterface
 * NULL           ------------------------------------------------------------------------
 * ...
 * 1HKINTERFACE   J9VMHookInterface
 * NULL           ------------------------------------------------------------------------
 * ...
 * 1HKINTERFACE   J9VMZipCachePoolHookInterface
 * NULL           ------------------------------------------------------------------------
 * ...
 * 1HKINTERFACE   J9JITHookInterface
 * NULL           ------------------------------------------------------------------------
 * ...
 * NULL
 * NULL           ------------------------------------------------------------------------
 */

void
JavaCoreDumpWriter::writeHookSection(void)
{
	/* Write the section header */
	_OutputStream.writeCharacters("0SECTION       HOOK subcomponent dump routine\n");
	_OutputStream.writeCharacters("NULL           ==============================\n");
	_OutputStream.writeCharacters("1NOTE          This data is reset after each javacore file is written\n");
	_OutputStream.writeCharacters("NULL           ------------------------------------------------------------------------\n");
	_OutputStream.writeCharacters("1HKINTERFACE   MM_OMRHookInterface\n");
	writeHookInterface(_VirtualMachine->memoryManagerFunctions->j9gc_get_omr_hook_interface(_VirtualMachine->omrVM));
	_OutputStream.writeCharacters("1HKINTERFACE   MM_PrivateHookInterface\n");
	writeHookInterface(_VirtualMachine->memoryManagerFunctions->j9gc_get_private_hook_interface(_VirtualMachine));
	_OutputStream.writeCharacters("1HKINTERFACE   MM_HookInterface\n");
	writeHookInterface(_VirtualMachine->memoryManagerFunctions->j9gc_get_hook_interface(_VirtualMachine));
	_OutputStream.writeCharacters("1HKINTERFACE   J9VMHookInterface\n");
	writeHookInterface(_VirtualMachine->internalVMFunctions->getVMHookInterface(_VirtualMachine));
	_OutputStream.writeCharacters("1HKINTERFACE   J9VMZipCachePoolHookInterface\n");
	writeHookInterface(zip_getVMZipCachePoolHookInterface((J9ZipCachePool *) _VirtualMachine->zipCachePool));
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	struct J9HookInterface **jitHook = _VirtualMachine->internalVMFunctions->getJITHookInterface(_VirtualMachine);

	if (NULL != jitHook) {
		_OutputStream.writeCharacters("1HKINTERFACE   J9JITHookInterface\n");
		writeHookInterface(jitHook);
	}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */

	/* Write the section trailer */
	_OutputStream.writeCharacters("NULL           ------------------------------------------------------------------------\n");
}

#if defined(OMR_OPT_CUDA)

/**
 * Write a value scaled by 10 in decimal form.
 *
 * @param[in] stream  the output stream
 * @param[in] value   the scaled value
 */
static void
writeCudaScaledValue(TextFileStream & stream, U_32 value)
{
	stream.writeInteger(value / 10, "%zu");
	stream.writeInteger(value % 10, ".%zu");
}

/**
 * Write information about the CUDA environment; a sample for two GPUs follows.
 *
 * 0SECTION       CUDA subcomponent dump routine
 * NULL           ==============================
 * 1CUDADRIVER    Driver version:   6.5
 * 1CUDARUNTIME   Runtime version:  5.5
 * 1CUDACOUNT     Device count:     2
 * NULL
 * 2CUDADEVICE    Device 0: "Tesla K40m"
 * 2CUDAPCIDOM    PCI Domain Id:       2
 * 2CUDAPCIBUS    PCI Bus Id:          1
 * 2CUDAPCILOC    PCI Location Id:     0
 * 2CUDACCAP      Compute capability:  3.5
 * 2CUDACMODE     Compute mode:        Default
 * 2CUDAMEMTOTAL  Total memory:        11520 MBytes (12,079,136,768 bytes)
 * 2CUDAMEMAVAIL  Available memory:    10984 MBytes (11,517,558,784 bytes)
 * NULL
 * 2CUDADEVICE    Device 1: "Tesla K20Xm"
 * 2CUDAPCIDOM    PCI Domain Id:       0
 * 2CUDAPCIBUS    PCI Bus Id:          83
 * 2CUDAPCILOC    PCI Location Id:     0
 * 2CUDACCAP      Compute capability:  3.5
 * 2CUDACMODE     Compute mode:        Default
 * 2CUDAMEMTOTAL  Total memory:        6144 MBytes (6,442,254,336 bytes)
 * 2CUDAMEMAVAIL  Available memory:    5759 MBytes (6,038,749,184 bytes)
 * NULL
 * NULL           ------------------------------------------------------------------------
 */
void
JavaCoreDumpWriter::writeCudaSection(void)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	J9CudaConfig *config = OMRPORT_FROM_J9PORT(PORTLIB)->cuda_configData;

	if (config && config->getSummaryData) {
		J9CudaSummaryDescriptor summary;

		memset(&summary, 0, sizeof(summary));

		_OutputStream.writeCharacters("0SECTION       CUDA subcomponent dump routine\n");
		_OutputStream.writeCharacters("NULL           ==============================\n");

		IDATA error = 0;

		error = config->getSummaryData(OMRPORT_FROM_J9PORT(PORTLIB), &summary);

		if (0 != error) {
			_OutputStream.writeCharacters("1CUDASUMMARY   Unable to get summary data; error: ");
			_OutputStream.writeInteger(error, "%zd\n");
			// write the section trailer
			_OutputStream.writeCharacters("NULL\n");
			_OutputStream.writeCharacters("NULL           ------------------------------------------------------------------------\n");
			return;
		}

		// 1CUDADRIVER    Driver version:   6.5
		_OutputStream.writeCharacters("1CUDADRIVER    Driver version:   ");
		if (0 == summary.driverVersion) {
			_OutputStream.writeCharacters("N/A");
		} else {
			writeCudaScaledValue(_OutputStream, summary.driverVersion);
		}
		_OutputStream.writeCharacters("\n");

		// 1CUDARUNTIME   Runtime version:  5.5
		_OutputStream.writeCharacters("1CUDARUNTIME   Runtime version:  ");
		if (0 == summary.runtimeVersion) {
			_OutputStream.writeCharacters("N/A");
		} else {
			writeCudaScaledValue(_OutputStream, summary.runtimeVersion);
		}
		_OutputStream.writeCharacters("\n");

		// 1CUDACOUNT     Device count:     2
		_OutputStream.writeCharacters("1CUDACOUNT     Device count:     ");
		_OutputStream.writeInteger(summary.deviceCount, "%zu\n");

		if (config->getDeviceData) {
			for (U_32 deviceId = 0; deviceId < summary.deviceCount; ++deviceId) {
				_OutputStream.writeCharacters("NULL\n");

				J9CudaDeviceDescriptor deviceData;

				memset(&deviceData, 0, sizeof(deviceData));

				error = config->getDeviceData(OMRPORT_FROM_J9PORT(PORTLIB), deviceId, &deviceData);

				if (0 != error) {
					_OutputStream.writeCharacters("1CUDADEVICE    Unable to get data for device ");
					_OutputStream.writeInteger(deviceId, "%zu");
					_OutputStream.writeCharacters("; error: ");
					_OutputStream.writeInteger(error, "%zd\n");
					continue;
				}

				// 2CUDADEVICE    Device 0: "Tesla K40m"
				_OutputStream.writeCharacters("2CUDADEVICE    Device ");
				_OutputStream.writeInteger(deviceId, "%zu");
				_OutputStream.writeCharacters(": \"");
				_OutputStream.writeCharacters(deviceData.deviceName);
				_OutputStream.writeCharacters("\"\n");

				// 2CUDAPCIDOM    PCI Domain Id:       2
				_OutputStream.writeCharacters("2CUDAPCIDOM    PCI Domain Id:       ");
				_OutputStream.writeInteger(deviceData.pciDomainId, "%zu\n");

				// 2CUDAPCIBUS    PCI Bus Id:          1
				_OutputStream.writeCharacters("2CUDAPCIBUS    PCI Bus Id:          ");
				_OutputStream.writeInteger(deviceData.pciBusId, "%zu\n");

				// 2CUDAPCILOC    PCI Location Id:     0
				_OutputStream.writeCharacters("2CUDAPCILOC    PCI Location Id:     ");
				_OutputStream.writeInteger(deviceData.pciDeviceId, "%zu\n");

				// 2CUDACCAP      Compute capability:  3.5
				_OutputStream.writeCharacters("2CUDACCAP      Compute capability:  ");
				writeCudaScaledValue(_OutputStream, deviceData.computeCapability);
				_OutputStream.writeCharacters("\n");

				// 2CUDACMODE     Compute mode:        Default
				_OutputStream.writeCharacters("2CUDACMODE     Compute mode:        ");
				switch (deviceData.computeMode) {
				case J9CUDA_COMPUTE_MODE_DEFAULT:
					_OutputStream.writeCharacters("Default");
					break;
				case J9CUDA_COMPUTE_MODE_PROCESS_EXCLUSIVE:
					_OutputStream.writeCharacters("Process Exclusive");
					break;
				case J9CUDA_COMPUTE_MODE_PROHIBITED:
					_OutputStream.writeCharacters("Prohibited");
					break;
				case J9CUDA_COMPUTE_MODE_THREAD_EXCLUSIVE:
					_OutputStream.writeCharacters("Thread Exclusive");
					break;
				default:
					_OutputStream.writeCharacters("Unknown");
					break;
				}
				_OutputStream.writeCharacters("\n");

				// 2CUDAMEMTOTAL  Total memory:        11520 MBytes (12,079,136,768 bytes)
				_OutputStream.writeCharacters("2CUDAMEMTOTAL  Total memory:        ");
				_OutputStream.writeInteger64(deviceData.totalMemory >> 20, "%llu");
				_OutputStream.writeCharacters(" MBytes (");
				_OutputStream.writeIntegerWithCommas(deviceData.totalMemory);
				_OutputStream.writeCharacters(" bytes)\n");

				// 2CUDAMEMAVAIL  Available memory:    10984 MBytes (11,517,558,784 bytes)
				_OutputStream.writeCharacters("2CUDAMEMAVAIL  Available memory:    ");
				if (0 == ~deviceData.availableMemory) {
					_OutputStream.writeCharacters("Unavailable\n");
				} else {
					_OutputStream.writeInteger64(deviceData.availableMemory >> 20, "%llu");
					_OutputStream.writeCharacters(" MBytes (");
					_OutputStream.writeIntegerWithCommas(deviceData.availableMemory);
					_OutputStream.writeCharacters(" bytes)\n");
				}
			}
		}

		// write the section trailer
		_OutputStream.writeCharacters("NULL\n");
		_OutputStream.writeCharacters("NULL           ------------------------------------------------------------------------\n");
	}
}

#endif /* defined(OMR_OPT_CUDA) */

#if defined(J9VM_OPT_SHARED_CLASSES)
/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeSharedClassSection() method implementation                            */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeSharedClassIPCInfo(const char* textStart, const char* textEnd, IDATA id, UDATA padToLength)
{
	UDATA totalChars = strlen(textStart) + strlen(textEnd);
	IDATA idDiv = id;
	const char* unknown = "unknown";

	if (id == -1) {
		totalChars += strlen(unknown);
	} else {
		do {
			++totalChars;
			idDiv /= 10;
		} while (idDiv != 0);
	}

	_OutputStream.writeCharacters(textStart);
	if (id == -1) {
		_OutputStream.writeCharacters(unknown);
	} else {
		_OutputStream.writeInteger(id, "%zi");
	}
	_OutputStream.writeCharacters(textEnd);

	for (UDATA i=totalChars; i<padToLength; i++) {
		_OutputStream.writeCharacters(" ");
	}
}

void
JavaCoreDumpWriter::writeSharedClassLockInfo(const char* lockName, IDATA lockSemid, void* lockTID)
{
	_OutputStream.writeCharacters(lockName);
	if (lockSemid == -2) {
		_OutputStream.writeCharacters("File lock                ");
	} else {
		writeSharedClassIPCInfo("IPC Sem (id ", ")", lockSemid, 25);
	}
	if (lockTID) {
		_OutputStream.writePointer(lockTID);
		_OutputStream.writeCharacters("\n");
	} else {
		_OutputStream.writeCharacters("Unowned\n");
	}
}

void
JavaCoreDumpWriter::writeSharedClassSectionTopLayerStatsHelper(J9SharedClassJavacoreDataDescriptor* javacoreData, bool multiLayerStats)
{
	_OutputStream.writeCharacters(
			"1SCLTEXTCRTW       Cache Created With\n"
			"NULL               ------------------\n"
			"NULL\n"
	);

	if (0 != (javacoreData->extraFlags & J9SHR_EXTRA_FLAGS_NO_LINE_NUMBERS)) {
		_OutputStream.writeCharacters(
				"2SCLTEXTXNL            -Xnolinenumbers       = true\n"
		);
	} else {
		_OutputStream.writeCharacters(
				"2SCLTEXTXNL            -Xnolinenumbers       = false\n"
		);
	}

	if (0 != (javacoreData->extraFlags & J9SHR_EXTRA_FLAGS_BCI_ENABLED)) {
		_OutputStream.writeCharacters(
				"2SCLTEXTBCI            BCI Enabled           = true\n"
		);
	} else {
		_OutputStream.writeCharacters(
				"2SCLTEXTBCI            BCI Enabled           = false\n"
		);
	}

	if (0 != (javacoreData->extraFlags & J9SHR_EXTRA_FLAGS_RESTRICT_CLASSPATHS)) {
		_OutputStream.writeCharacters(
				"2SCLTEXTBCI            Restrict Classpaths   = true\n"
		);
	} else {
		_OutputStream.writeCharacters(
				"2SCLTEXTBCI            Restrict Classpaths   = false\n"
		);
	}

	_OutputStream.writeCharacters(
				"NULL\n"
				"1SCLTEXTCSUM       Cache Summary\n"
				"NULL               ------------------\n"
				"NULL\n"
	);

	if (0 != (javacoreData->extraFlags & J9SHR_EXTRA_FLAGS_NO_LINE_NUMBER_CONTENT)) {
		_OutputStream.writeCharacters(
				"2SCLTEXTNLC            No line number content                    = true\n"
		);
	} else {
		_OutputStream.writeCharacters(
				"2SCLTEXTNLC            No line number content                    = false\n"
		);
	}

	if (0 != (javacoreData->extraFlags & J9SHR_EXTRA_FLAGS_LINE_NUMBER_CONTENT)) {
		_OutputStream.writeCharacters(
				"2SCLTEXTLNC            Line number content                       = true\n"
		);
	} else {
		_OutputStream.writeCharacters(
				"2SCLTEXTLNC            Line number content                       = false\n"
		);
	}

	_OutputStream.writeCharacters(
				"NULL\n"
	);

	_OutputStream.writeCharacters(
			"2SCLTEXTRCS            ROMClass start address                    = "
	);
	_OutputStream.writePointer(javacoreData->romClassStart);

	_OutputStream.writeCharacters(
			"\n2SCLTEXTRCE            ROMClass end address                      = "
	);
	_OutputStream.writePointer(javacoreData->romClassEnd);

	_OutputStream.writeCharacters(
			"\n2SCLTEXTMSA            Metadata start address                    = "
	);
	_OutputStream.writePointer(javacoreData->metadataStart);

	_OutputStream.writeCharacters(
			"\n2SCLTEXTCEA            Cache end address                         = "
	);
	_OutputStream.writePointer(javacoreData->cacheEndAddress);

	_OutputStream.writeCharacters(
			"\n2SCLTEXTRTF            Runtime flags                             = "
	);
	_OutputStream.writeInteger64(javacoreData->runtimeFlags, "0x%.16llX");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTCGN            Cache generation                          = "
	);
	_OutputStream.writeInteger(javacoreData->cacheGen, "%zu");

	if (multiLayerStats) {
		_OutputStream.writeCharacters(
			"\n2SCLTEXTCLY            Cache layer                               = "
		);
		_OutputStream.writeInteger(javacoreData->topLayer, "%zd");
	}

	_OutputStream.writeCharacters(
			"\nNULL"
			"\n2SCLTEXTCSZ            Cache size                                = "
	);
	_OutputStream.writeInteger(javacoreData->cacheSize, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTSMB            Softmx bytes                              = "
	);
	_OutputStream.writeInteger(javacoreData->softMaxBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTFRB            Free bytes                                = "
	);
	_OutputStream.writeInteger(javacoreData->freeBytes, "%zu");


	_OutputStream.writeCharacters(
			"\n2SCLTEXTARB            Reserved space for AOT bytes              = "
	);
	_OutputStream.writeInteger(javacoreData->minAOT, "%zd");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTAMB            Maximum space for AOT bytes               = "
	);
	_OutputStream.writeInteger(javacoreData->maxAOT, "%zd");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTJRB            Reserved space for JIT data bytes         = "
	);
	_OutputStream.writeInteger(javacoreData->minJIT, "%zd");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTJMB            Maximum space for JIT data bytes          = "
	);
	_OutputStream.writeInteger(javacoreData->maxJIT, "%zd");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTRWB            ReadWrite bytes                           = "
	);
	_OutputStream.writeInteger(javacoreData->readWriteBytes, "%zu");

	if (NO_CORRUPTION != javacoreData->corruptionCode) {
		_OutputStream.writeCharacters(
			"\n2SCLTEXTCOC            Corruption Code                           = "
		);
		_OutputStream.writeInteger(javacoreData->corruptionCode, "%zd");

		_OutputStream.writeCharacters(
			"\n2SCLTEXTCOV            Corrupt Value                             = "
		);
		_OutputStream.writeInteger(javacoreData->corruptValue);
	}

	if (!multiLayerStats) {
		_OutputStream.writeCharacters(
			"\n2SCLTEXTMDA            Metadata bytes                            = "
		);
	}

	_OutputStream.writeInteger(javacoreData->otherBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTDAS            Class debug area size                     = "
	);
	_OutputStream.writeInteger(javacoreData->debugAreaSize, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTDAU            Class debug area % used                   = "
	);
	_OutputStream.writeInteger(javacoreData->debugAreaUsed, "%zu");
	_OutputStream.writeCharacters("%");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTDAN            Class LineNumberTable bytes               = "
	);
	_OutputStream.writeInteger(javacoreData->debugAreaLineNumberTableBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTDAV            Class LocalVariableTable bytes            = "
	);
	_OutputStream.writeInteger(javacoreData->debugAreaLocalVariableTableBytes, "%zu");
	_OutputStream.writeCharacters("\n");
}

void
JavaCoreDumpWriter::writeSharedClassSectionTopLayerStatsSummaryHelper(J9SharedClassJavacoreDataDescriptor* javacoreData)
{
	_OutputStream.writeCharacters(
			"NULL"
			"\n2SCLTEXTCPF            Cache is "
	);
	_OutputStream.writeInteger(javacoreData->percFull, "%zu");

	if (javacoreData->softMaxBytes == javacoreData->cacheSize) {
		_OutputStream.writeCharacters("% full\n");
	} else {
		_OutputStream.writeCharacters("% soft full\n");
	}

	_OutputStream.writeCharacters(
			"NULL\n"
			"1SCLTEXTCMST       Cache Memory Status\n"
			"NULL               ------------------\n"
			"1SCLTEXTCNTD           Cache Name                    Feature                  Memory type              Cache path\n"
			"NULL\n"
	);
	_OutputStream.writeCharacters(
			"2SCLTEXTCMDT           "
	);
	_OutputStream.writeCharacters(javacoreData->cacheName);
	for (UDATA i = strlen(javacoreData->cacheName); i < 30; i++) {
		_OutputStream.writeCharacters(" ");
	}
	if (J9_ARE_ALL_BITS_SET(javacoreData->feature, J9SH_FEATURE_COMPRESSED_POINTERS)) {
		_OutputStream.writeCharacters("CR                       ");
	} else if (J9_ARE_ALL_BITS_SET(javacoreData->feature, J9SH_FEATURE_NON_COMPRESSED_POINTERS)) {
		_OutputStream.writeCharacters("Non-CR                   ");
	} else {
		_OutputStream.writeCharacters("Default                  ");
	}
	if (javacoreData->shmid == -2) {
		_OutputStream.writeCharacters("Memory mapped file       ");
	} else {
		writeSharedClassIPCInfo("IPC Memory (id ", ")", javacoreData->shmid, 25);
	}
	_OutputStream.writeCharacters(javacoreData->cacheDir);
	_OutputStream.writeCharacters("\n");

	_OutputStream.writeCharacters(
			"NULL\n"
			"1SCLTEXTCMST       Cache Lock Status\n"
			"NULL               ------------------\n"
			"1SCLTEXTCNTD           Lock Name                     Lock type                TID owning lock\n"
			"NULL\n"
			);

	writeSharedClassLockInfo(
			"2SCLTEXTCWRL           Cache write lock              ", javacoreData->semid, javacoreData->writeLockTID
	);

	writeSharedClassLockInfo(
			"2SCLTEXTCRWL           Cache read/write lock         ", javacoreData->semid, javacoreData->readWriteLockTID
	);
}

void
JavaCoreDumpWriter::writeSharedClassSectionAllLayersStatsHelper(J9SharedClassJavacoreDataDescriptor* javacoreData)
{
	_OutputStream.writeCharacters(
			"2SCLTEXTRCB            ROMClass bytes                            = "
	);
	_OutputStream.writeInteger(javacoreData->romClassBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTAOB            AOT code bytes                            = "
	);
	_OutputStream.writeInteger(javacoreData->aotBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTADB            AOT data bytes                            = "
	);
	_OutputStream.writeInteger(javacoreData->aotDataBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTAHB            AOT class hierarchy bytes                 = "
	);
	_OutputStream.writeInteger(javacoreData->aotClassChainDataBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTATB            AOT thunk bytes                           = "
	);
	_OutputStream.writeInteger(javacoreData->aotThunkDataBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTJHB            JIT hint bytes                            = "
	);
	_OutputStream.writeInteger(javacoreData->jitHintDataBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTJPB            JIT profile bytes                         = "
	);
	_OutputStream.writeInteger(javacoreData->jitProfileDataBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNOB            Java Object bytes                         = "
	);
	_OutputStream.writeInteger(javacoreData->objectBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTZCB            Zip cache bytes                           = "
	);
	_OutputStream.writeInteger(javacoreData->zipCacheDataBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTSHB            Startup hint bytes                        = "
	);
	_OutputStream.writeInteger(javacoreData->startupHintBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTCSB            CRV Snippet bytes                         = "
	);
	_OutputStream.writeInteger(javacoreData->crvSnippetBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTJCB            JCL data bytes                            = "
	);
	_OutputStream.writeInteger(javacoreData->jclDataBytes, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTBDA            Byte data bytes                           = "
	);
	_OutputStream.writeInteger(javacoreData->indexedDataBytes, "%zu");

	_OutputStream.writeCharacters(
			"\nNULL"
			"\n2SCLTEXTNRC            Number ROMClasses                         = "
	);
	_OutputStream.writeInteger(javacoreData->numROMClasses, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNAM            Number AOT Methods                        = "
	);
	_OutputStream.writeInteger(javacoreData->numAOTMethods, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNAD            Number AOT Data Entries                   = "
	);
	_OutputStream.writeInteger(javacoreData->numAotDataEntries, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNAH            Number AOT Class Hierarchy                = "
	);
	_OutputStream.writeInteger(javacoreData->numAotClassChains, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNAT            Number AOT Thunks                         = "
	);
	_OutputStream.writeInteger(javacoreData->numAotThunks, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNJH            Number JIT Hints                          = "
	);
	_OutputStream.writeInteger(javacoreData->numJitHints, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNJP            Number JIT Profiles                       = "
	);
	_OutputStream.writeInteger(javacoreData->numJitProfiles, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNCP            Number Classpaths                         = "
	);
	_OutputStream.writeInteger(javacoreData->numClasspaths, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNUR            Number URLs                               = "
	);
	_OutputStream.writeInteger(javacoreData->numURLs, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNTK            Number Tokens                             = "
	);
	_OutputStream.writeInteger(javacoreData->numTokens, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNOJ            Number Java Objects                       = "
	);
	_OutputStream.writeInteger(javacoreData->numObjects, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNZC            Number Zip Caches                         = "
	);
	_OutputStream.writeInteger(javacoreData->numZipCaches, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNSH            Number Startup Hint Entries               = "
	);
	_OutputStream.writeInteger(javacoreData->numStartupHints, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNCS            Number CRV Snippets                       = "
	);
	_OutputStream.writeInteger(javacoreData->numCRVSnippets, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNJC            Number JCL Entries                        = "
	);
	_OutputStream.writeInteger(javacoreData->numJclEntries, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTNST            Number Stale classes                      = "
	);
	_OutputStream.writeInteger(javacoreData->numStaleClasses, "%zu");

	_OutputStream.writeCharacters(
			"\n2SCLTEXTPST            Percent Stale classes                     = "
	);
	_OutputStream.writeInteger(javacoreData->percStale, "%zu");
	_OutputStream.writeCharacters("\n");
}

void
JavaCoreDumpWriter::writeSharedClassSection(void)
{
	J9SharedClassJavacoreDataDescriptor javacoreData;

	if (!_VirtualMachine->sharedClassConfig) {
		return;
	}
	if (!_VirtualMachine->sharedClassConfig->getJavacoreData) {
		return;
	}

	memset(&javacoreData, 0, sizeof(J9SharedClassJavacoreDataDescriptor));
	if (_VirtualMachine->sharedClassConfig->getJavacoreData(_Context->javaVM, &javacoreData)) {
		/* Write the section and first sub-section header */
		_OutputStream.writeCharacters(
			"0SECTION       SHARED CLASSES subcomponent dump routine\n"
			"NULL           ========================================\n"
			"NULL\n"
		);

		bool multiLayerStats = (0 < javacoreData.topLayer);

		if (multiLayerStats) {
			_OutputStream.writeCharacters(
				"1SCLTEXTCSTL   Cache Statistics for Top Layer\n"
				"NULL\n"
			);
			writeSharedClassSectionTopLayerStatsHelper(&javacoreData, multiLayerStats);
			writeSharedClassSectionTopLayerStatsSummaryHelper(&javacoreData);
			_OutputStream.writeCharacters(
				"NULL\n"
				"1SCLTEXTCSAL   Cache Statistics for All Layers\n"
				"NULL\n"
			);
			writeSharedClassSectionAllLayersStatsHelper(&javacoreData);
		} else {
			writeSharedClassSectionTopLayerStatsHelper(&javacoreData, multiLayerStats);
			writeSharedClassSectionAllLayersStatsHelper(&javacoreData);
			writeSharedClassSectionTopLayerStatsSummaryHelper(&javacoreData);
		}

		/* Write the section trailer */
		_OutputStream.writeCharacters(
			"NULL\n"
			"NULL\n"
			"NULL           ------------------------------------------------------------------------\n"
		);
	}
}
#endif




/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeTrailer() method implementation                                       */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeTrailer(void)
{
	_OutputStream.writeCharacters(
		"0SECTION       Javadump End section\n"
		"NULL           ---------------------- END OF DUMP -------------------------------------\n"
	);
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeVMRuntimeState() method implementation                               */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeVMRuntimeState(U_32 vmRuntimeState)
{
	switch (vmRuntimeState) {
	case J9VM_RUNTIME_STATE_ACTIVE:
		_OutputStream.writeCharacters("ACTIVE");
		break;
	case J9VM_RUNTIME_STATE_IDLE:
		_OutputStream.writeCharacters("IDLE");
		break;
	default:
		_OutputStream.writeCharacters("UNKNOWN");
		break;
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeExceptionDetail() method implementation                               */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeExceptionDetail(j9object_t* exceptionRef)
{
	char        stackBuffer[_MaximumExceptionNameLength];
	char*       buf       = stackBuffer;
	UDATA       len       = 0;
	J9VMThread* vmThread  = _Context->onThread;
	J9Class*    eiieClass = NULL;

	PORT_ACCESS_FROM_JAVAVM(_VirtualMachine);

	if (exceptionRef && *exceptionRef) {
		j9object_t message = J9VMJAVALANGTHROWABLE_DETAILMESSAGE(vmThread, *exceptionRef);
		if (NULL != message) {
			buf = _VirtualMachine->internalVMFunctions->copyStringToUTF8WithMemAlloc(vmThread, message, J9_STR_NULL_TERMINATE_RESULT, "", 0, stackBuffer, _MaximumExceptionNameLength, &len);
		}

		if (0 != len) {
			_OutputStream.writeCharacters(" \"");
			_OutputStream.writeCharacters(buf, len);
			_OutputStream.writeCharacters("\"");
		}

		if (buf != stackBuffer) {
			j9mem_free_memory(buf);
		}

		/* check and describe any nested exceptions */
		eiieClass = _VirtualMachine->internalVMFunctions->internalFindKnownClass(vmThread, J9VMCONSTANTPOOL_JAVALANGEXCEPTIONININITIALIZERERROR, J9_FINDKNOWNCLASS_FLAG_EXISTING_ONLY);

		if (J9OBJECT_CLAZZ(vmThread, *exceptionRef) == eiieClass) {
			char detailMessageStackBuffer[_MaximumExceptionNameLength];
			char*                  nestedBuf = NULL;
			UDATA                  nestedLen = 0;
			j9object_t             nestedException = NULL;
			J9UTF8*                nestedExceptionClassName = NULL;

#if JAVA_SPEC_VERSION >= 12
			nestedException = J9VMJAVALANGTHROWABLE_CAUSE(vmThread, *exceptionRef);
#else
			nestedException = J9VMJAVALANGEXCEPTIONININITIALIZERERROR_EXCEPTION(vmThread, *exceptionRef);
#endif /* JAVA_SPEC_VERSION */

			if (nestedException) {
				nestedExceptionClassName = J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ(vmThread, nestedException)->romClass);
				if (nestedExceptionClassName) {
					_OutputStream.writeCharacters(" Nested Exception: \"");
					_OutputStream.writeCharacters((char*)J9UTF8_DATA(nestedExceptionClassName), (UDATA)J9UTF8_LENGTH(nestedExceptionClassName));
					_OutputStream.writeCharacters("\"");
				}

				message = J9VMJAVALANGTHROWABLE_DETAILMESSAGE(vmThread, nestedException);
				if (NULL != message) {
					nestedBuf = _VirtualMachine->internalVMFunctions->copyStringToUTF8WithMemAlloc(vmThread, message, J9_STR_NULL_TERMINATE_RESULT, "", 0, detailMessageStackBuffer, _MaximumExceptionNameLength, &nestedLen);
				}

				if (0 != nestedLen) {
					_OutputStream.writeCharacters(" Detail:  \"");
					_OutputStream.writeCharacters(nestedBuf, nestedLen);
					_OutputStream.writeCharacters("\"");
				}

				if (nestedBuf != detailMessageStackBuffer) {
					j9mem_free_memory(nestedBuf);
				}
			}
		}
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeGPCategory() method implementation                                    */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeGPCategory(void* gpInfo, const char* prefix, U_32 category)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);

	const char* name;
	void*       value;
	U_32        kind;

	U_32 items = j9sig_info_count(gpInfo, category);

	for (U_32 n = 0; n < items; n++) {
		kind = j9sig_info(gpInfo, category, n, &name, &value);
		writeGPValue(prefix, name, kind, value);
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeGPValue() method implementation                                       */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeGPValue(const char* prefix, const char* name, U_32 kind, void* value)
{
	/* Write the prefix and name */
	_OutputStream.writeCharacters(prefix);
	_OutputStream.writeCharacters(name);
	_OutputStream.writeCharacters(": ");

	/* Write the value according to type */
	switch (kind) {
	case J9PORT_SIG_VALUE_16:        _OutputStream.writeInteger(*(U_16*)value, "%04X"); break;
	case J9PORT_SIG_VALUE_32:        _OutputStream.writeInteger(*(U_32*)value, "%08.8X"); break;
	case J9PORT_SIG_VALUE_64:        _OutputStream.writeInteger64(*(U_64*)value, "%016.16llX"); break;
	case J9PORT_SIG_VALUE_128:
		{
			const U_128 * const v = (U_128 *) value;
			const U_64 h = v->high64;
			const U_64 l = v->low64;

			_OutputStream.writeVPrintf("%016.16llX%016.16llX", h, l);
		}
		break;
	case J9PORT_SIG_VALUE_STRING:
		if (value) {
			/* CMVC 160410: copy value to a local string as kernel symbols may be inaccessible to file write() */
			char valueString[_MaximumGPValueLength];
			if (strlen((char *)value) < _MaximumGPValueLength) {
				 strcpy(valueString, (char *)value);
				 _OutputStream.writeCharacters(valueString);
			} else {
				strncpy(valueString, (char *)value, _MaximumGPValueLength-1);
				valueString[_MaximumGPValueLength-1] = '\0';
				_OutputStream.writeCharacters(valueString);
				_OutputStream.writeCharacters(" [truncated]");
			}
		} else {
			_OutputStream.writeCharacters("[unknown]");
		}
		break;

	case J9PORT_SIG_VALUE_ADDRESS:   _OutputStream.writeVPrintf("%.*zX", sizeof(void *) * 2, (IDATA)(*(void **)value)); break;
	case J9PORT_SIG_VALUE_FLOAT_64:  _OutputStream.writeInteger64(*(U_64*)value, "%016.16llX"); break;
	case J9PORT_SIG_VALUE_UNDEFINED: _OutputStream.writeCharacters("[unknown]"); break;
	default:                         break;
	}

	_OutputStream.writeCharacters("\n");
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeJitMethod() method implementation                                     */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeJitMethod(J9VMThread* vmThread)
{
#ifdef J9VM_INTERP_NATIVE_SUPPORT

	J9JITConfig* jitConfig = vmThread ? vmThread->javaVM->jitConfig : NULL;
	if (jitConfig) {

		J9Method* ramMethod       = NULL;
		bool      insideJitMethod = false;
		bool      isCompiling     = false;

		if ((vmThread->omrVMThread->vmState & J9VMSTATE_MAJOR) == J9VMSTATE_JIT_CODEGEN) {
			ramMethod = vmThread->jitMethodToBeCompiled;
			isCompiling = true;

		} else {
			const char* name;
			void*      value;

			PORT_ACCESS_FROM_JAVAVM(_VirtualMachine);

			U_32 kind = j9sig_info(vmThread->gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_PC, &name, &value);

			J9JITExceptionTable* table;

			switch (kind) {
			case J9PORT_SIG_VALUE_ADDRESS:
				table = jitConfig->jitGetExceptionTableFromPC(vmThread, *(UDATA*)value);
				if (table) {
					ramMethod = table->ramMethod;
					insideJitMethod = true;
				}
				break;

			default:
				break;
		}
	}

	if (isCompiling || insideJitMethod) {
		_OutputStream.writeCharacters("1XHEXCPMODULE  ");
		_OutputStream.writeCharacters(isCompiling ? "Compiling method: " : "Inside compiled method: ");

		if (ramMethod) {
			J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
			J9ROMClass *romClass = J9_CLASS_FROM_METHOD(ramMethod)->romClass;

			_OutputStream.writeCharacters(J9ROMCLASS_CLASSNAME(romClass));
			_OutputStream.writeCharacters(".");
			_OutputStream.writeCharacters(J9ROMMETHOD_NAME(romMethod));
			_OutputStream.writeCharacters(J9ROMMETHOD_SIGNATURE(romMethod));
			_OutputStream.writeCharacters("\n");

		} else {
			_OutputStream.writeCharacters("<unknown>\n");
		}

		return;
	}
}

#endif  /* J9VM_INTERP_NATIVE_SUPPORT */
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeSegments() method implementation                                      */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeSegments(J9MemorySegmentList* list, BOOLEAN isCodeCacheSegment)
{
	/* Loop through the segments writing their data */
	J9MemorySegment* segment = list ? list->nextSegment : NULL;
	UDATA sizeTotal = 0;
	UDATA allocTotal = 0;
	UDATA freeTotal = 0;

	while (segment != 0) {
		UDATA warmAlloc;
		UDATA coldAlloc;

		if (segment->type == MEMORY_TYPE_SHARED_META) {
			/* Discard the class cache metadata segment (it overlaps the last shared ROM class segment in the cache) */
			segment = segment->nextSegment;
			continue;
		}
		if (isCodeCacheSegment) {
			/* Set default values for warmAlloc and coldAlloc pointers */
			warmAlloc = (UDATA)segment->heapBase;
			coldAlloc = (UDATA)segment->heapTop;

			/* The JIT code cache grows from both ends of the segment: the warmAlloc pointer upwards from the start of the segment
			 * and the coldAlloc pointer downwards from the end of the segment. The free space in a JIT code cache segment is the
			 * space between the warmAlloc and coldAlloc pointers. See jit/codert/MultiCodeCache.hpp, the contract with the JVM is
			 * that the address of the TR_MCCCodeCache structure is stored at the beginning of the segment.
			 */
#ifdef J9VM_INTERP_NATIVE_SUPPORT
			UDATA *mccCodeCache = *((UDATA**)segment->heapBase);
			if (mccCodeCache ) {
				J9JITConfig* jitConfig = _VirtualMachine->jitConfig;
				if (jitConfig) {
					warmAlloc = (UDATA)jitConfig->codeCacheWarmAlloc(mccCodeCache);
					coldAlloc = (UDATA)jitConfig->codeCacheColdAlloc(mccCodeCache);
				}
			}
#endif
		}
		_OutputStream.writeCharacters("1STSEGMENT     ");
		_OutputStream.writePointer(segment, true);
		_OutputStream.writeCharacters(" ");
		_OutputStream.writePointer(segment->heapBase, true);
		_OutputStream.writeCharacters(" ");
		if (isCodeCacheSegment) {
			/* For the JIT code cache segments, we fake up the allocation pointer as the sum of the cold and warm allocations */
			_OutputStream.writePointer((UDATA *)((UDATA)segment->heapTop - (coldAlloc - warmAlloc)), true);
		} else {
			_OutputStream.writePointer(segment->heapAlloc, true);
		}
		_OutputStream.writeCharacters(" ");
		_OutputStream.writePointer(segment->heapTop, true);
		_OutputStream.writeCharacters(" ");
		_OutputStream.writeInteger(segment->type, "0x%08zX");
		_OutputStream.writeCharacters(" ");
		_OutputStream.writeVPrintf(FORMAT_SIZE_HEX, sizeof (void *) * 2, segment->size);
		_OutputStream.writeCharacters("\n");

		/* accumulate the totals */
		sizeTotal += segment->size;
		if (isCodeCacheSegment) {
			allocTotal += segment->size - (coldAlloc - warmAlloc);
			freeTotal += coldAlloc - warmAlloc;
		} else {
			allocTotal += ((UDATA)segment->heapAlloc - (UDATA)segment->heapBase);
			freeTotal += (segment->size - ((UDATA)segment->heapAlloc - (UDATA)segment->heapBase));
		}

		segment = segment->nextSegment;
	}

	int decimalLength = sizeof(void*) == 4 ? 10 : 20;

	/* print out the totals */
	_OutputStream.writeCharacters("NULL\n");
	_OutputStream.writeCharacters("1STSEGTOTAL    ");
	_OutputStream.writeCharacters("Total memory:        ");
	_OutputStream.writeVPrintf(FORMAT_SIZE_DECIMAL, decimalLength, sizeTotal);
	_OutputStream.writeCharacters(" (");
	_OutputStream.writeVPrintf(FORMAT_SIZE_HEX, sizeof(void *) * 2, sizeTotal);
	_OutputStream.writeCharacters(")\n");
	_OutputStream.writeCharacters("1STSEGINUSE    ");
	_OutputStream.writeCharacters("Total memory in use: ");
	_OutputStream.writeVPrintf(FORMAT_SIZE_DECIMAL, decimalLength, allocTotal);
	_OutputStream.writeCharacters(" (");
	_OutputStream.writeVPrintf(FORMAT_SIZE_HEX, sizeof(void *) * 2, allocTotal);
	_OutputStream.writeCharacters(")\n");
	_OutputStream.writeCharacters("1STSEGFREE     ");
	_OutputStream.writeCharacters("Total memory free:   ");
	_OutputStream.writeVPrintf(FORMAT_SIZE_DECIMAL, decimalLength, freeTotal);
	_OutputStream.writeCharacters(" (");
	_OutputStream.writeVPrintf(FORMAT_SIZE_HEX, sizeof(void *) * 2, freeTotal);
	_OutputStream.writeCharacters(")\n");
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeTraceHistory() method implementation                                  */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeTraceHistory(U_32 type)
{
	/* Extract the history data */
	RasGlobalStorage*     j9ras        = (RasGlobalStorage*)_VirtualMachine->j9rasGlobalStorage;
	UtInterface*          uteInterface = (UtInterface*)(j9ras ? j9ras->utIntf : NULL);
	UtTracePointIterator* iterator     = NULL;
	const char* prefix = NULL;
	const char* bufferName = NULL;
	PORT_ACCESS_FROM_PORT(_PortLibrary);

	/* If trace isn't running, there is nothing to do */
	if (!(uteInterface && uteInterface->server)) {
		return;
	}

	if( HIST_TYPE_GC == type ) {
		prefix = "ST";
		bufferName = "gclogger";
	} else if (HIST_TYPE_CT == type) {
		prefix = "XE";
		bufferName = "currentThread";
	} else {
		return;
	}

	/* We may be a non-VM thread (e.g. signal thread) so use a temporary UtThreadData */
	UtThreadData thrData;
	UtThreadData* thrSlot = &thrData;
	UtThreadData** thr = &thrSlot;

	/* inhibit tracing using our faked thread data */
	thrData.recursion = 1;

	/* gclogger is the trace group used to capture gc data in the low frequency buffer */
	iterator = uteInterface->server->GetTracePointIteratorForBuffer(thr, bufferName);
	if (iterator != NULL) {
		struct walkClosure closure;
		void *args[] = {NULL, NULL, NULL};
		UDATA sink = 0;

		closure.jcw = this;
		closure.state = args;
		args[0] = thr;
		args[1] = iterator;
		args[2] = (void*)prefix;

		/* sig_protect iterating the tracepoints so we don't fail to free the iterator if something goes wrong. */
		j9sig_protect(protectedWriteGCHistoryLines,
				&closure, handlerWriteSection, this,
				J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN,
				&sink);

		uteInterface->server->FreeTracePointIterator( thr, iterator );
		iterator = NULL;
	}
}

void JavaCoreDumpWriter::writeGCHistoryLines(UtThreadData** thr, UtTracePointIterator* iterator, const char *type)
{
	RasGlobalStorage*     j9ras        = (RasGlobalStorage*)_VirtualMachine->j9rasGlobalStorage;
	UtInterface*          uteInterface = (UtInterface*)(j9ras ? j9ras->utIntf : NULL);
	char                  stackBuffer[_MaximumFormattedTracePointLength];
	int                   lineCount = 0;

	/* If trace isn't running, there is nothing to do */
	if (!(uteInterface && uteInterface->server)) {
		return;
	}

	/* Use the iterator to get the required tracepoints */
	while ( NULL != uteInterface->server->FormatNextTracePoint( iterator, stackBuffer, _MaximumFormattedTracePointLength) ){
		_OutputStream.writeCharacters("3");
		_OutputStream.writeCharacters(type);
		_OutputStream.writeCharacters("HSTTYPE     ");
		_OutputStream.writeCharacters(stackBuffer);
		_OutputStream.writeCharacters(" \n");

		if( ++lineCount > _MaximumGCHistoryLines ) {
			_OutputStream.writeCharacters("3");
			_OutputStream.writeCharacters(type);
			_OutputStream.writeCharacters("HSTERR      GC history section truncated at ");
			_OutputStream.writeInteger(_MaximumGCHistoryLines, "%zu");
			_OutputStream.writeCharacters(" lines\n");
			return;
		}
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeDeadLocks() method implementation                                     */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeDeadLocks(void)
{
	/* If the state of the VM is bad, skip the section */
	if (avoidLocks()) {
		return;
	}

	PORT_ACCESS_FROM_JAVAVM(_VirtualMachine);

	J9HashTable* deadlocks = hashTableNew (
		OMRPORT_FROM_J9PORT(PORTLIB), J9_GET_CALLSITE(), 0,
		sizeof(DeadLockGraphNode), 0, 0,
		OMRMEM_CATEGORY_VM,
		lockHashFunction,
		lockHashEqualFunction,
		NULL, NULL
	);

	/* If the memory can't be allocated, skip the section */
	if (deadlocks == NULL) {
		return;
	}

	J9VMThread* walkThread = J9_LINKED_LIST_START_DO(_VirtualMachine->mainThread);
	while (walkThread != NULL) {
		findThreadCycle(walkThread, deadlocks);
		walkThread = J9_LINKED_LIST_NEXT_DO(_VirtualMachine->mainThread, walkThread);
		if (walkThread != NULL && walkThread->publicFlags == J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION) {
			break;
		}
	}

	J9HashTableState hashState;
	UDATA cycle = 0;

	DeadLockGraphNode* node = (DeadLockGraphNode*)hashTableStartDo(deadlocks, &hashState);
	while (node != NULL) {

		cycle++;

		while (node) {
			if (node->cycle > 0) {

				/* Found a deadlock! */
				if (node->cycle == cycle) {
					/* Output a header for each deadlock */
					_OutputStream.writeCharacters(
						"NULL           \n"
						"1LKDEADLOCK    Deadlock detected !!!\n"
						"NULL           ---------------------\n"
						"NULL           \n"
					);

					DeadLockGraphNode *head = node;
					int count = 0;

					do {
						/* Loop round complete cycle */
						writeDeadlockNode(node, ++count);
						node = node->next;
					} while (node != head);

					_OutputStream.writeCharacters("2LKDEADLOCKTHR  Thread \"");
					writeThreadName(node->thread);
					_OutputStream.writeCharacters("\" (");
					_OutputStream.writePointer(node->thread);
					_OutputStream.writeCharacters(")\n");
				}

				/* Skip already visited nodes */
				break;

			} else {
				node->cycle = cycle;
			}
			node = node->next;
		}

		node = (DeadLockGraphNode*)hashTableNextDo(&hashState);
	}

	hashTableFree(deadlocks);
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::findThreadCycle() method implementation                                    */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::findThreadCycle(J9VMThread* vmThread, J9HashTable* deadlocks)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	J9ThreadAbstractMonitor *lock;
	J9VMThread *owner;
	UDATA status;
	j9object_t lockObject;

	DeadLockGraphNode  node;
	DeadLockGraphNode* prev = &node;

	/* Look for deadlock cycle */
	do {
		void *args[] = {vmThread,  &lockObject, &lock, &owner, NULL};
		UDATA stateClean = 0;
		UDATA stateFault = stateClean;

		if (j9sig_protect(protectedGetVMThreadRawState, args, handlerGetVMThreadRawState, &stateFault, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &stateClean) == J9PORT_SIG_EXCEPTION_OCCURRED) {
			return;
		} else {
			status = stateClean;
		}

		if (owner == NULL || owner == vmThread) {
			return;
		} else if (status == J9VMTHREAD_STATE_BLOCKED) {
			node.lock = lock;
			node.lockObject = lockObject;
			node.cycle = 0;
		} else if ((status == J9VMTHREAD_STATE_WAITING) || (status == J9VMTHREAD_STATE_WAITING_TIMED)) {
			node.lock = lock;
			node.lockObject = lockObject;
			node.cycle = 0;
		} else if((status == J9VMTHREAD_STATE_PARKED) || (status == J9VMTHREAD_STATE_PARKED_TIMED)) {
			node.lock = NULL;
			node.lockObject = lockObject;
			node.cycle = 0;
		} else {
			return;
		}

		/* Record current thread and update last node */
		node.thread = vmThread;
		prev->next = (DeadLockGraphNode*)hashTableAdd(deadlocks, &node);

		/* Move round graph */
		vmThread = owner;
		prev = prev->next;

		/* Peek ahead to see if we're in a possible cycle */
		node.thread = vmThread;
		prev->next = (DeadLockGraphNode*)hashTableFind(deadlocks, &node);

	} while (prev->next == NULL);
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeDeadlockNode() method implementation                                  */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeDeadlockNode(DeadLockGraphNode* node, int count)
{
	J9ThreadAbstractMonitor* lock = node->lock;
	j9object_t lockObject = node->lockObject;

	_OutputStream.writeCharacters("2LKDEADLOCKTHR  Thread \"");
	writeThreadName(node->thread);
	_OutputStream.writeCharacters("\" (");
	_OutputStream.writePointer(node->thread);
	_OutputStream.writeCharacters(")\n");

	if (count == 1) {
		_OutputStream.writeCharacters("3LKDEADLOCKWTR    is waiting for:\n");
	} else {
		_OutputStream.writeCharacters("3LKDEADLOCKWTR    which is waiting for:\n");
	}

	if ((lock != NULL) && ((lock->flags & J9THREAD_MONITOR_OBJECT) == J9THREAD_MONITOR_OBJECT)) {
		// Java monitor object
		_OutputStream.writeCharacters("4LKDEADLOCKMON      ");
		writeMonitor((J9ThreadMonitor*)lock);
		_OutputStream.writeCharacters("\n");
		_OutputStream.writeCharacters("4LKDEADLOCKOBJ      ");
		writeObject((j9object_t)(lock->userData));
		_OutputStream.writeCharacters("\n");
	} else if (lock != NULL) {
		// System monitor
		_OutputStream.writeCharacters("4LKDEADLOCKREG      ");
		writeSystemMonitor((J9ThreadMonitor*)lock);
		_OutputStream.writeCharacters("\n");
	} else if((lock == NULL) && (lockObject != NULL)) {
		// j.u.c lock
		_OutputStream.writeCharacters("4LKDEADLOCKOBJ      ");
		writeObject(lockObject);
		_OutputStream.writeCharacters("\n");
	}

	_OutputStream.writeCharacters("3LKDEADLOCKOWN    which is owned by:\n");
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeMonitorObject() method implementation                                 */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeMonitorObject(J9ThreadMonitor* monitor, j9object_t obj, blocked_thread_record *threadStore)
{
	J9VMThread* owner = NULL;
	UDATA       count = 0;

	J9ThreadAbstractMonitor* lock = (J9ThreadAbstractMonitor*)monitor;
	/* lock->owner is volatile and may change underneath us, cache the value since this is a snapshot of VM state. */
	J9Thread*   lockOwner = lock->owner;

	if (obj) {
		J9VMThread tenantMarker;

		memset(&tenantMarker, 0, sizeof(J9VMThread));


		owner = getObjectMonitorOwner(_VirtualMachine, &tenantMarker, obj, &count);
	} else if (lockOwner) {
		owner = getVMThreadFromOMRThread(_VirtualMachine, lockOwner);
		count = lock->count;
	}

	/* Skip monitor if not interesting */
	if ((obj || (lock->name == 0)) && (owner == 0) && (lockOwner == 0) && (lock->waiting == 0)) {
		return;
	}

	/* Describe the monitor */
	if (obj) {
		_OutputStream.writeCharacters("2LKMONINUSE      ");
		writeMonitor(monitor);
		_OutputStream.writeCharacters("\n");
		_OutputStream.writeCharacters("3LKMONOBJECT       ");
		writeObject(obj);
		_OutputStream.writeCharacters(": ");
	} else {
		_OutputStream.writeCharacters("2LKREGMON          ");
		writeSystemMonitor(monitor);
	}

	/* Describe its owning thread */
	bool inflated = (lock->flags & J9THREAD_MONITOR_INFLATED) != 0;

	if (owner || lockOwner) {
		if (inflated) {
			_OutputStream.writeCharacters("owner \"");
		} else {
			_OutputStream.writeCharacters("Flat locked by \"");
		}
		/* See jvmfree.c : recycleVMThread which says:
		 * dead threads are stored in "halted for inspection mode"
		 */
		if( owner && owner->publicFlags == J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION ) {
			// This thread should be on the dead thread list.
			// (Or it may have died while we were looking at it if we don't have
			// exclusive VM access.)
			_OutputStream.writeCharacters("<dead thread>");
		} else {
			writeThreadName(owner);
		}
		_OutputStream.writeCharacters("\" (");
		if( owner ) {
			_OutputStream.writeCharacters( "J9VMThread:");
			_OutputStream.writePointer((void*)owner );
		} else {
			_OutputStream.writeCharacters( "native thread ID:");
			_OutputStream.writeInteger(omrthread_get_osId(lockOwner));
		}
		_OutputStream.writeCharacters("), entry count ");
		_OutputStream.writeInteger(count, "%zu");
	} else {
		_OutputStream.writeCharacters("<unowned>");
	}

	_OutputStream.writeCharacters("\n");

	int threadIndex = 0;
	int blockedThreadCount = 0;

	while (threadStore[threadIndex].waitingThread != NULL) {
		omrthread_monitor_t blocking_monitor = threadStore[threadIndex].monitor;
		UDATA status = threadStore[threadIndex].waitingThreadState;
		J9VMThread* walkThread = threadStore[threadIndex].waitingThread;
		if ((blocking_monitor == monitor) && (status == J9VMTHREAD_STATE_BLOCKED)) {
			/* Output the list header */
			if (blockedThreadCount == 0) {
				_OutputStream.writeCharacters("3LKWAITERQ            Waiting to enter:\n");
			}

			_OutputStream.writeCharacters("3LKWAITER                \"");
			writeThreadName(walkThread);
			_OutputStream.writeCharacters("\" (J9VMThread:");
			_OutputStream.writePointer(walkThread);
			_OutputStream.writeCharacters(")\n");

			blockedThreadCount++;
		}
		threadIndex++;
	}

	threadIndex = 0;
	int waitingThreadCount = 0;

	while (threadStore[threadIndex].waitingThread != NULL) {
		omrthread_monitor_t blocking_monitor = threadStore[threadIndex].monitor;
		UDATA status = threadStore[threadIndex].waitingThreadState;
		J9VMThread* walkThread = threadStore[threadIndex].waitingThread;
		if ((blocking_monitor == monitor) && ((status == J9VMTHREAD_STATE_WAITING) || (status == J9VMTHREAD_STATE_WAITING_TIMED))) {
				/* Output the list header */
				if (waitingThreadCount == 0) {
					_OutputStream.writeCharacters("3LKNOTIFYQ            Waiting to be notified:\n");
				}

				_OutputStream.writeCharacters("3LKWAITNOTIFY            \"");
				writeThreadName(walkThread);
				_OutputStream.writeCharacters("\" (J9VMThread:");
				_OutputStream.writePointer(walkThread);
				_OutputStream.writeCharacters(")\n");

				waitingThreadCount++;
		}
		threadIndex++;
	}

}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeMonitor() method implementation                                       */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeMonitor(J9ThreadMonitor* monitor)
{
	_OutputStream.writeCharacters("sys_mon_t:");
	_OutputStream.writePointer(monitor);
	_OutputStream.writeCharacters(" infl_mon_t: ");
	_OutputStream.writePointer(((U_8*)monitor) + sizeof(J9ThreadAbstractMonitor));
	_OutputStream.writeCharacters(":");

}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeSystemMonitor() method implementation                                 */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeSystemMonitor(J9ThreadMonitor* monitor)
{
	const char* name = omrthread_monitor_get_name(monitor);

	_OutputStream.writeCharacters((name ? name : "[system]"));
	_OutputStream.writeCharacters(" lock (");
	_OutputStream.writePointer(monitor);
	_OutputStream.writeCharacters("): ");
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeObject() method implementation                                        */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeObject(j9object_t obj)
{
	J9ROMClass* romClass;
	if (J9VM_IS_INITIALIZED_HEAPCLASS_VM(_VirtualMachine, obj)) {
		romClass = J9VM_J9CLASS_FROM_HEAPCLASS_VM(_VirtualMachine, obj)->romClass;
	} else {
		romClass = J9OBJECT_CLAZZ_VM(_VirtualMachine, obj)->romClass;
	}

	J9UTF8* className = J9ROMCLASS_CLASSNAME(romClass);

	_OutputStream.writeCharacters(className);
	_OutputStream.writeCharacters("@");
	_OutputStream.writePointer(obj);
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeThreadState() method implementation                                        */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeThreadState(UDATA threadState)
{
	switch (threadState) {
	case J9VMTHREAD_STATE_RUNNING:
		_OutputStream.writeCharacters("R");
		break;
	case J9VMTHREAD_STATE_BLOCKED:
		_OutputStream.writeCharacters("B");
		break;
	case J9VMTHREAD_STATE_WAITING:
	case J9VMTHREAD_STATE_WAITING_TIMED:
	case J9VMTHREAD_STATE_SLEEPING:
		_OutputStream.writeCharacters("CW");
		break;
	case J9VMTHREAD_STATE_PARKED:
	case J9VMTHREAD_STATE_PARKED_TIMED:
		_OutputStream.writeCharacters("P");
		break;
	case J9VMTHREAD_STATE_SUSPENDED:
		_OutputStream.writeCharacters("S");
		break;
	case J9VMTHREAD_STATE_DEAD:
		_OutputStream.writeCharacters("Z");
		break;
	case J9VMTHREAD_STATE_INTERRUPTED:
		_OutputStream.writeCharacters("I");
		break;
	case J9VMTHREAD_STATE_UNKNOWN:
		_OutputStream.writeCharacters("?");
		break;
	default:
		_OutputStream.writeCharacters("??");
		break;
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeThread() method implementation                                        */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeThread(J9VMThread* vmThread, J9PlatformThread *nativeThread, UDATA vmstate, UDATA javaState, UDATA javaPriority, j9object_t lockObject, J9VMThread *lockOwnerThread )
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	J9AbstractThread* osThread =  NULL;

	/* Extract the corresponding OS thread */
	if (vmThread != NULL) {
		osThread = (J9AbstractThread*)vmThread->osThread;

		/* Write the first thread descriptor word */
		_OutputStream.writeCharacters("3XMTHREADINFO      \"");
		writeThreadName(vmThread);
		_OutputStream.writeCharacters("\" J9VMThread:");
		_OutputStream.writePointer(vmThread);
		_OutputStream.writeCharacters(", omrthread_t:");
		_OutputStream.writePointer(osThread);
		_OutputStream.writeCharacters(", java/lang/Thread:");
		_OutputStream.writePointer(vmThread->threadObject);

		/* Replace vmstate with java state in the "3XMTHREADINFO" entry */
		_OutputStream.writeCharacters(", state:");
		writeThreadState(javaState);

		_OutputStream.writeCharacters(", prio=");
		_OutputStream.writeInteger(javaPriority, "%zu");


		_OutputStream.writeCharacters("\n");

		if (vmThread->threadObject) {
			struct walkClosure closure;
			void *args[] = {vmThread};
			UDATA sink = 0;

			closure.jcw = this;
			closure.state = args;
			args[0] = vmThread;

			j9sig_protect(protectedWriteJavaLangThreadInfo,
						&closure, handlerWriteJavaLangThreadInfo, this,
						J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN,
						&sink);
		}

		/* Write the second thread descriptor word */
		if (osThread) {
			void *stackStart = NULL;
			void *stackEnd = NULL;

			_OutputStream.writeCharacters("3XMTHREADINFO1            (native thread ID:");

			if (osThread->tid) {
				_OutputStream.writeInteger(osThread->tid);
			} else {
				_OutputStream.writePointer(((U_8*)osThread) + sizeof(J9AbstractThread));
			}

			_OutputStream.writeCharacters(", native priority:");
			_OutputStream.writeInteger(osThread->priority);
			_OutputStream.writeCharacters(", native policy:UNKNOWN");

			/* Add vmstate and publicFlags to the end of the "3XMTHREADINFO1" entry */
			_OutputStream.writeCharacters(", vmstate:");
			writeThreadState(vmstate);

			_OutputStream.writeCharacters(", vm thread flags:");
			_OutputStream.writeInteger(vmThread->publicFlags, "0x%08x");
			_OutputStream.writeCharacters(")\n");

			if (omrthread_get_stack_range((omrthread_t)osThread, &stackStart, &stackEnd) == J9THREAD_SUCCESS) {
				_OutputStream.writeCharacters("3XMTHREADINFO2            (native stack address range");
				_OutputStream.writeCharacters(" from:");
				_OutputStream.writePointer(stackStart);
				_OutputStream.writeCharacters(", to:");
				_OutputStream.writePointer(stackEnd);
				_OutputStream.writeCharacters(", size:");
				_OutputStream.writeInteger(stackEnd>stackStart?
						(UDATA)stackEnd-(UDATA)stackStart:(UDATA)stackStart-(UDATA)stackEnd);
				_OutputStream.writeCharacters(")\n");
			}

			I_64 cpuTime = omrthread_get_cpu_time((omrthread_t)osThread);
			I_64 userTime = omrthread_get_user_time((omrthread_t)osThread);
			if ((-1 != cpuTime) || (-1 != userTime)) {
				_OutputStream.writeCharacters("3XMCPUTIME               CPU usage ");
				if (-1 != cpuTime) {
					writeThreadTime("total", cpuTime);
					if (-1 != userTime) {
						_OutputStream.writeCharacters(", ");
					}
				}
				if (-1 != userTime) {
					writeThreadTime("user", userTime);
				}
				if ((-1 != cpuTime) && (-1 != userTime)) {
					I_64 systemTime = cpuTime - userTime;
					_OutputStream.writeCharacters(", ");
					writeThreadTime("system", systemTime);
				}

				/* Write the category of the thread */
				_OutputStream.writeCharacters(", current category=");

				UDATA category = omrthread_get_category((omrthread_t)osThread);
				switch (category) {
				case J9THREAD_CATEGORY_RESOURCE_MONITOR_THREAD:
					_OutputStream.writeCharacters("\"Resource-Monitor\"");
					break;
				case J9THREAD_CATEGORY_SYSTEM_THREAD:
					_OutputStream.writeCharacters("\"System-JVM\"");
					break;
				case J9THREAD_CATEGORY_SYSTEM_GC_THREAD:
					_OutputStream.writeCharacters("\"GC\"");
					break;
				case J9THREAD_CATEGORY_SYSTEM_JIT_THREAD:
					_OutputStream.writeCharacters("\"JIT\"");
					break;
				case J9THREAD_CATEGORY_APPLICATION_THREAD:
					_OutputStream.writeCharacters("\"Application\"");
					break;
				case J9THREAD_USER_DEFINED_THREAD_CATEGORY_1:
					_OutputStream.writeCharacters("\"Application-User1\"");
					break;
				case J9THREAD_USER_DEFINED_THREAD_CATEGORY_2:
					_OutputStream.writeCharacters("\"Application-User2\"");
					break;
				case J9THREAD_USER_DEFINED_THREAD_CATEGORY_3:
					_OutputStream.writeCharacters("\"Application-User3\"");
					break;
				case J9THREAD_USER_DEFINED_THREAD_CATEGORY_4:
					_OutputStream.writeCharacters("\"Application-User4\"");
					break;
				case J9THREAD_USER_DEFINED_THREAD_CATEGORY_5:
					_OutputStream.writeCharacters("\"Application-User5\"");
					break;
				default:
					_OutputStream.writeCharacters("Unknown");
					break;
				}
				_OutputStream.writeCharacters("\n");
			}
		} else {
			_OutputStream.writeCharacters("3XMTHREADINFO1            (native thread ID:");
			_OutputStream.writeInteger(0);
			_OutputStream.writeCharacters(", native priority:");
			_OutputStream.writeInteger(0);
			_OutputStream.writeCharacters(", native policy:UNKNOWN");

			/* Add vmstate and publicFlags at the end of the "3XMTHREADINFO1" entry */
			_OutputStream.writeCharacters(", vmstate:");
			writeThreadState(vmstate);

			_OutputStream.writeCharacters(", vm thread flags:");
			_OutputStream.writeInteger(vmThread->publicFlags, "0x%08x");
			_OutputStream.writeCharacters(")\n");
		}


		if( !avoidLocks() ) {
			struct walkClosure closure;
			void *args[] = {NULL, NULL, NULL, NULL};
			UDATA sink = 0;

			closure.jcw = this;
			closure.state = args;
			args[0] = vmThread;
			args[1] = &vmstate;
			args[2] = lockObject;
			args[3] = lockOwnerThread;
			/* sig_protect as we have to access the heap and monitors */
			j9sig_protect(protectedWriteThreadBlockers,
						&closure, handlerWriteThreadBlockers, this,
						J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN,
						&sink);
		}

		if (vmThread->threadObject) {
			UDATA bytesAllocated = _VirtualMachine->memoryManagerFunctions->j9gc_get_bytes_allocated_by_thread(vmThread);
			_OutputStream.writeCharacters("3XMHEAPALLOC             Heap bytes allocated since last GC cycle=");
			_OutputStream.writeInteger(bytesAllocated, "%zu");
			_OutputStream.writeCharacters(" (");
			_OutputStream.writeInteger(bytesAllocated);
			_OutputStream.writeCharacters(")\n");
		}

		/* Write the java stack */
		if (vmThread->threadObject) {
			J9StackWalkState stackWalkState;
			struct walkClosure closure;
			struct walkClosure monitorClosure;
			UDATA sink;
			UDATA depth = 0;
			J9ObjectMonitorInfo monitorInfos[_MaximumMonitorInfosPerThread];
			IDATA monitorCount = 0;
			void *monitorArgs[] = {vmThread, monitorInfos, &monitorCount};
			monitorClosure.jcw = this;
			monitorClosure.state = monitorArgs;

			memset(&monitorInfos, 0, _MaximumMonitorInfosPerThread*sizeof(J9ObjectMonitorInfo));

			j9sig_protect(protectedGetOwnedObjectMonitors, &monitorClosure, handlerGetOwnedObjectMonitors, this, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &sink);

			stackWalkState.walkThread = vmThread;

			stackWalkState.flags =
				J9_STACKWALK_ITERATE_FRAMES |
				J9_STACKWALK_INCLUDE_NATIVES |
				J9_STACKWALK_VISIBLE_ONLY |
				J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;

			stackWalkState.skipCount = 0;
			stackWalkState.userData1 = (void*)this;
			stackWalkState.userData2 = &depth; /* Use this for a depth count. */
			stackWalkState.frameWalkFunction = writeFrameCallBack;
			stackWalkState.errorMode = J9_STACKWALK_ERROR_MODE_IGNORE;
			stackWalkState.userData3 = &monitorInfos;
			stackWalkState.userData4 = (void *) monitorCount;

			closure.jcw = this;
			closure.state = &stackWalkState;

			if (j9sig_protect(protectedWalkJavaStack, &closure, handlerJavaThreadWalk, this, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &sink) == 0) {
				if (depth == 0) {
					/* No stack frames => look for exception */
					if (!avoidLocks()) {
						j9object_t* throwable = NULL;

						/* Have we stashed an uncaught exception? */
						if (vmThread == _Context->onThread && _Context->eventData) {
							throwable = (j9object_t*)_Context->eventData->exceptionRef;
						}

						/* Otherwise default to current exception slot */
						if (throwable == NULL) {
							throwable = &(vmThread->currentException);
						}

						if (throwable && *throwable) {
							struct walkClosure stackClosure;
							void *parameters[] = {vmThread, throwable, &stackWalkState};

							stackClosure.jcw = this;
							stackClosure.state = parameters;

							if(j9sig_protect(protectedIterateStackTrace,
									&stackClosure, handlerIterateStackTrace, this,
									J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, &sink) == 0 ) {
								if( depth == 0 ) { /* depth == 0 means call succeeded but no frames were written.*/
									_OutputStream.writeCharacters("3XMTHREADINFO3           No Java callstack associated with throwable\n");
								}
							}

						} else {
							_OutputStream.writeCharacters("3XMTHREADINFO3           No Java callstack associated with this thread\n");
						}
					} else {
						_OutputStream.writeCharacters("3XMTHREADINFO3           No Java callstack available without taking locks\n");
					}
				} else {
					/* Stack will have been written by protectedWalkJavaStack */
				}
			}
		} else {
			_OutputStream.writeCharacters("3XMTHREADINFO3           No Java callstack associated with this thread\n");
		}
	} else {
		if (nativeThread) {
			_OutputStream.writeCharacters("3XMTHREADINFO      Anonymous native thread\n");
			_OutputStream.writeCharacters("3XMTHREADINFO1            (native thread ID:");
			_OutputStream.writeInteger(nativeThread->thread_id);
			_OutputStream.writeCharacters(", native priority: ");
			_OutputStream.writeInteger(nativeThread->priority);
			_OutputStream.writeCharacters(", native policy:UNKNOWN)\n");
		}
	}

	if (nativeThread && nativeThread->callstack) {
		J9PlatformStackFrame *frame;

		_OutputStream.writeCharacters("3XMTHREADINFO3           Native callstack:\n");

		frame = nativeThread->callstack;
		while (frame) {
			_OutputStream.writeCharacters("4XENATIVESTACK               ");
			if (frame->symbol) {
				_OutputStream.writeCharacters(frame->symbol);
			} else {
				_OutputStream.writePointer((void*)frame->instruction_pointer);
			}
			_OutputStream.writeCharacters("\n");

			frame = frame->parent_frame;
		}
	} else {
#if defined(J9ZOS390) || defined(J9ZTPF)
		_OutputStream.writeCharacters("3XMTHREADINFO3           No native callstack available on this platform\n");
#else /* defined(J9ZOS390) || defined(J9ZTPF) */
		_OutputStream.writeCharacters("3XMTHREADINFO3           No native callstack available for this thread\n");
#endif /* defined(J9ZOS390) || defined(J9ZTPF) */
		_OutputStream.writeCharacters("NULL\n");
	}

	_OutputStream.writeCharacters("NULL\n");

}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeThreadName() method implementation                                    */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeThreadName(J9VMThread* vmThread)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	if (vmThread) {
		void *args[] = {_VirtualMachine, vmThread};
		const char *nameClean = "";
		const char *nameFault = nameClean;

		/* This can crash if the thread object is being moved around while we're trying to get the
		 * name out of it (i.e. we're sharing exclusive with GC). If we fault while trying to get the
		 * name we return "<name unavailable>" instead
		 */
		if (j9sig_protect(protectedGetVMThreadName, args, handlerGetVMThreadName, (UDATA*)&nameFault, J9PORT_SIG_FLAG_SIGALLSYNC|J9PORT_SIG_FLAG_MAY_RETURN, (UDATA*)&nameClean) == J9PORT_SIG_EXCEPTION_OCCURRED) {
			_OutputStream.writeCharacters(nameFault);
		} else if( nameClean != NULL ) {
			_OutputStream.writeCharacters(nameClean);
		} else {
			_OutputStream.writeCharacters("<name locked>");
		}
		releaseOMRVMThreadName(vmThread->omrVMThread);
	} else {
		_OutputStream.writeCharacters("[osthread]");
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeThreadBlockers() method implementation                                        */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeThreadBlockers(J9VMThread* vmThread, UDATA vmstate, j9object_t lockObject, J9VMThread *lockOwnerThread )
{
	if ( vmThread == NULL ) {
		return;
	}

	j9object_t lockOwnerObject = NULL;
	if ( vmstate == J9VMTHREAD_STATE_BLOCKED ) {

		if( lockObject != NULL ) {
			_OutputStream.writeCharacters("3XMTHREADBLOCK     Blocked on: " );
		} else {
			return; // Probably a system monitor, nothing interesting to write.
		}

	} else if ( (vmstate == J9VMTHREAD_STATE_WAITING) || (vmstate == J9VMTHREAD_STATE_WAITING_TIMED) ) {

		if( lockObject != NULL ) {
			_OutputStream.writeCharacters("3XMTHREADBLOCK     Waiting on: " );
		} else {
			return; // Probably a system monitor, nothing interesting to write.
		}

	} else if ( vmstate == J9VMTHREAD_STATE_PARKED || vmstate == J9VMTHREAD_STATE_PARKED_TIMED ) {

		/*
		 * The lock object is a reference passed to the park() call, assuming the call
		 * was made with the version that takes an object ref. (The standard implementation
		 * in java.util.concurrent does.)
		 *
		 * Lock owner information has to be introspected from the lockObject.
		 * This can only be done for known objects and may be unavailable if
		 * there is custom locking in the application written using
		 * java.util.concurrent
		 *
		 * We only need to do this if getVMThreadRawState didn't return a J9VMThread
		 * for lockOwnerThread.
		 * (This happens when the owning thread has terminated, see below.)
		 */
		if ( lockObject && !lockOwnerThread ) {
			J9Class *aosClazz;
			aosClazz = J9VMJAVAUTILCONCURRENTLOCKSABSTRACTOWNABLESYNCHRONIZER_OR_NULL(vmThread->javaVM);
			/* skip this step if aosClazz doesn't exist */
			if ( aosClazz ) {
				J9Class *clazz;
				clazz = J9OBJECT_CLAZZ(vmThread, lockObject);
				/* PR 80305 : Do not write back to the castClassCache as this code may be running while the GC is unloading the class */
				if ( instanceOfOrCheckCastNoCacheUpdate(clazz, aosClazz) ) {
					lockOwnerObject =
							J9VMJAVAUTILCONCURRENTLOCKSABSTRACTOWNABLESYNCHRONIZER_EXCLUSIVEOWNERTHREAD(vmThread, lockObject);
				}
			}
		}
		_OutputStream.writeCharacters("3XMTHREADBLOCK     Parked on: " );

	} else {
		// If not blocked, waiting or parked, don't write anything out.
		return;
	}
	// We have the lockObject and lockOwner, and have decided if we are
	// parked or blocked. Write the rest of the output here to ensure
	// consistency.
	if ( lockObject ) {
		writeObject(lockObject);
	} else {
		_OutputStream.writeCharacters("<unknown>");
	}
	_OutputStream.writeCharacters(" Owned by: ");
	if ( lockOwnerThread != NULL ) {
		_OutputStream.writeCharacters("\"");
		writeThreadName(lockOwnerThread);
		_OutputStream.writeCharacters("\" (J9VMThread:");
		_OutputStream.writePointer(lockOwnerThread);
		_OutputStream.writeCharacters(", java/lang/Thread:");
		_OutputStream.writePointer(lockOwnerThread->threadObject);
		_OutputStream.writeCharacters(")");
	} else if ( lockOwnerObject != NULL ) {
		// The owning thread has terminated. We need to report this
		// as the parked thread is deadlocked. We can't get the thread
		// name from the lockOwner thread, it's null, but it will be
		// available inside the java/lang/Thread object on the heap.
		j9object_t nameObject = NULL;
		char *threadName = NULL;
		nameObject = J9VMJAVALANGTHREAD_NAME(vmThread, lockOwnerObject);
		threadName = getVMThreadNameFromString(vmThread, nameObject);
		if( threadName != NULL ) {
			// Port access so we can free threadName.
			PORT_ACCESS_FROM_JAVAVM(vmThread->javaVM);
			_OutputStream.writeCharacters("\"");
			_OutputStream.writeCharacters(threadName);
			_OutputStream.writeCharacters("\"");
			j9mem_free_memory(threadName);
		} else {
			_OutputStream.writeCharacters("<unknown>");
		}
		_OutputStream.writeCharacters(" (J9VMThread:");
		_OutputStream.writeCharacters("<null>");
		_OutputStream.writeCharacters(", java/lang/Thread:");
		_OutputStream.writePointer(lockOwnerObject);
		_OutputStream.writeCharacters(")");
	} else {
		if( vmstate == J9VMTHREAD_STATE_PARKED || vmstate == J9VMTHREAD_STATE_PARKED_TIMED ) {
			// No owning thread recorded.
			_OutputStream.writeCharacters("<unknown>");
		} else {
			// Should only occur for WAITING threads.
			// (For BLOCKED threads this would indicate a broken VM)
			_OutputStream.writeCharacters("<unowned>");
		}
	}
	_OutputStream.writeCharacters("\n");
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeFrame() method implementation                                         */
/*                                                                                                */
/**************************************************************************************************/
UDATA
JavaCoreDumpWriter::writeFrame(J9StackWalkState* state)
{
	J9Method* method = state->method;
	UDATA* depth = (UDATA*)state->userData2;
	J9ObjectMonitorInfo * monitorInfo  = (J9ObjectMonitorInfo *) state->userData3;
	IDATA *monitorCount = (IDATA*)(&state->userData4);

	/* Reset to show we dumped frames */
	if ((*depth) == 0) {
		/* first time through */
		_OutputStream.writeCharacters("3XMTHREADINFO3           Java callstack:\n");
	}

	if( ++(*depth) > _MaximumJavaStackDepth ) {
		_OutputStream.writeCharacters("4XESTACKERR                  Java callstack truncated at ");
		_OutputStream.writeInteger(_MaximumJavaStackDepth, "%zu");
		_OutputStream.writeCharacters(" methods\n");
		return J9_STACKWALK_STOP_ITERATING;
	}

	if (method == NULL) {
		_OutputStream.writeCharacters("4XESTACKTRACE                at (Missing Method)\n");
		return J9_STACKWALK_STOP_ITERATING;
	}

	J9Class*     methodClass = J9_CLASS_FROM_METHOD(method);
	J9UTF8*      className   = J9ROMCLASS_CLASSNAME(methodClass->romClass);
	J9ROMMethod* romMethod   = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	J9UTF8*      methodName  = J9ROMMETHOD_NAME(romMethod);

	_OutputStream.writeCharacters("4XESTACKTRACE                at ");
	_OutputStream.writeCharacters(className);
	_OutputStream.writeCharacters(".");
	_OutputStream.writeCharacters(methodName);

	if (romMethod->modifiers & J9AccNative) {
		_OutputStream.writeCharacters("(Native Method)\n");
		return J9_STACKWALK_KEEP_ITERATING;
	}

	UDATA offsetPC = state->bytecodePCOffset;
	bool compiledMethod = false;

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	J9JITConfig*         jitConfig = _VirtualMachine->jitConfig;
	J9JITExceptionTable* metaData  = state->jitInfo;
	void*                stackMap  = NULL;

	if (jitConfig && metaData) {
		stackMap = jitConfig->jitGetInlinerMapFromPC(_VirtualMachine, metaData, (UDATA)state->pc);
		if (stackMap) {
			compiledMethod = true;
		}
	}
#endif

#ifdef J9VM_OPT_DEBUG_INFO_SERVER
	/* Write source file and line number info, if available and we can take locks. */
	if (!avoidLocks()) {
		J9UTF8* sourceFile = getSourceFileNameForROMClass(_VirtualMachine, methodClass->classLoader, methodClass->romClass);
		if (sourceFile) {
			_OutputStream.writeCharacters("(");
			_OutputStream.writeCharacters(sourceFile);

			UDATA lineNumber = getLineNumberForROMClass(_VirtualMachine, method, offsetPC);

			if (lineNumber != (UDATA)-1) {
				_OutputStream.writeCharacters(":");
				_OutputStream.writeInteger(lineNumber, "%zu");
			}

			if (compiledMethod) {
				_OutputStream.writeCharacters("(Compiled Code)");
			}

			_OutputStream.writeCharacters(")\n");

			/* Use a while loop as there may be more than one lock taken in a stack frame. */
			while((*monitorCount) && ((UDATA)monitorInfo->depth == state->framesWalked)) {
				_OutputStream.writeCharacters("5XESTACKTRACE                   (entered lock: ");
				writeObject(monitorInfo->object);
				_OutputStream.writeCharacters(", entry count: ");
				_OutputStream.writeInteger(monitorInfo->count, "%zu");
				_OutputStream.writeCharacters(")\n");
				monitorInfo++;
				/* Store the updated progress back in userData for the next callback */
				state->userData3 = monitorInfo;
				(*monitorCount)--;
			}

			return J9_STACKWALK_KEEP_ITERATING;
		}
	}
#endif

	/* avoidLocks() is true or have no source file or line number info available, write PC. */
	/* (If better information was available we will have returned above.) */
	_OutputStream.writeCharacters("(Bytecode PC:");
	_OutputStream.writeInteger(offsetPC, "%zu");

	if (compiledMethod) {
		_OutputStream.writeCharacters("(Compiled Code)");
	}

	_OutputStream.writeCharacters(")\n");

	/* Use a while loop as there may be more than one lock taken in a stack frame. */
	while((*monitorCount) && ((UDATA)monitorInfo->depth == state->framesWalked)) {
		_OutputStream.writeCharacters("5XESTACKTRACE                   (entered lock: ");
		writeObject(monitorInfo->object);
		_OutputStream.writeCharacters(", entry count: ");
		_OutputStream.writeInteger(monitorInfo->count, "%zu");
		_OutputStream.writeCharacters(")\n");
		monitorInfo++;
		/* Store the updated progress back in userData for the next callback */
		state->userData3 = monitorInfo;
		(*monitorCount)--;
	}

	return J9_STACKWALK_KEEP_ITERATING;
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeExceptionFrame() method implementation                                */
/*                                                                                                */
/**************************************************************************************************/
UDATA
JavaCoreDumpWriter::writeExceptionFrame(
	void*        userData,
	J9ROMClass*  romClass,
	J9ROMMethod* romMethod,
	J9UTF8*      sourceFile,
	UDATA        lineNumber
)
{
	if (((J9StackWalkState*)userData)->userData2) {
		/* first time through */
		_OutputStream.writeCharacters("3XMTHREADINFO3           Java callstack:\n");
		((J9StackWalkState*)userData)->userData2 = (void*) 0;
	}

	if (romMethod == NULL) {
		_OutputStream.writeCharacters("4XESTACKTRACE                at (Missing Method)\n");
		return TRUE;
	}

	J9UTF8* className  = J9ROMCLASS_CLASSNAME(romClass);
	J9UTF8* methodName = J9ROMMETHOD_NAME(romMethod);

	_OutputStream.writeCharacters("4XESTACKTRACE                at ");
	_OutputStream.writeCharacters(className);
	_OutputStream.writeCharacters(".");
	_OutputStream.writeCharacters(methodName);

	if (romMethod->modifiers & J9AccNative) {
		_OutputStream.writeCharacters("(Native Method)\n");
		return TRUE;
	}

	if (sourceFile) {
		_OutputStream.writeCharacters("(");
		_OutputStream.writeCharacters(sourceFile);

		if (lineNumber != (UDATA)-1) {
			_OutputStream.writeCharacters(":");
			_OutputStream.writeInteger(lineNumber, "%zu");
		}

		_OutputStream.writeCharacters(")\n");
		return TRUE;
	}

	_OutputStream.writeCharacters("(No Source)\n");
	return TRUE;
}


/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeThreadTime() method implementation                                    */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeThreadTime(const char * timerName, I_64 nanoTime)
{
	_OutputStream.writeCharacters(timerName);
	_OutputStream.writeCharacters(": ");
	// Special case 0 to 0.0 for readability.
	if( nanoTime == 0) {
		_OutputStream.writeCharacters("0.0");
	} else {
		_OutputStream.writeInteger64((U_64)nanoTime / 1000000000, "%llu");
		_OutputStream.writeCharacters(".");
		_OutputStream.writeInteger64((U_64)nanoTime % 1000000000, "%0.9llu");
	}
	_OutputStream.writeCharacters(" secs");
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeLoader() method implementation                                        */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeLoader(J9ClassLoader* classLoader)
{
	/* Determine and write the status of the given loader */
	j9object_t object = getClassLoaderObject(classLoader);
	j9object_t parent = object ? J9VMJAVALANGCLASSLOADER_PARENT_VM(_VirtualMachine, object) : NULL;
	j9object_t appLdr = getClassLoaderObject(_VirtualMachine->applicationClassLoader);
	j9object_t extLdr = appLdr ? J9VMJAVALANGCLASSLOADER_PARENT_VM(_VirtualMachine, appLdr) : NULL;

	bool unload = (_Context->eventFlags & J9RAS_DUMP_ON_CLASS_UNLOAD) != 0;

	bool isSystem = (classLoader == _VirtualMachine->systemClassLoader);
	bool isApp    = (appLdr ? classLoader == J9VMJAVALANGCLASSLOADER_VMREF_VM(_VirtualMachine, appLdr) : false);
	bool isExt    = (extLdr ? classLoader == J9VMJAVALANGCLASSLOADER_VMREF_VM(_VirtualMachine, extLdr) : false);
	bool isAnon   = (classLoader == _VirtualMachine->anonClassLoader);

	char flags[9];
	flags[0] = isSystem ? 'p' : '-';
	flags[1] = isExt    ? 'x' : '-';
	flags[2] = false    ? 'h' : '-';
	flags[3] = false    ? 'm' : '-';
	flags[4] = parent   ? '-' : 's';
	flags[5] = false    ? '-' : 't';
	flags[6] = isApp    ? 'a' : '-';
	flags[7] = false    ? 'd' : '-';
	flags[8] = '\0';

	_OutputStream.writeCharacters("2CLTEXTCLLOADER\t\t");
	_OutputStream.writeCharacters(flags);

	/* Determine and write the description of the given loader */
	if (isSystem) {
		_OutputStream.writeCharacters(" Loader *System*(");
		_OutputStream.writePointer(object);
		_OutputStream.writeCharacters(")\n");

	} else if (unload && !isExt && !isApp) {
		_OutputStream.writeCharacters(" Loader [locked](");
		_OutputStream.writePointer(object);
		_OutputStream.writeCharacters(")\n");

	} else if (object == NULL) {
		_OutputStream.writeCharacters(" Loader [missing](");
		_OutputStream.writePointer(object);
		_OutputStream.writeCharacters(")\n");

	} else {
		_OutputStream.writeCharacters(" Loader ");
		_OutputStream.writeCharacters(J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ_VM(_VirtualMachine, object)->romClass));
		_OutputStream.writeCharacters("(");
		_OutputStream.writePointer(object);
		_OutputStream.writeCharacters(")");

		if (parent) {
			_OutputStream.writeCharacters(", Parent ");
			_OutputStream.writeCharacters(J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ_VM(_VirtualMachine, parent)->romClass));
			_OutputStream.writeCharacters("(");
		} else {
			_OutputStream.writeCharacters(", Parent *none*(");
		}

		_OutputStream.writePointer(parent);
		_OutputStream.writeCharacters(")\n");
	}

	/* Determine and write the number of loaded libraries */
	_OutputStream.writeCharacters("3CLNMBRLOADEDLIB\t\tNumber of loaded libraries ");

	if (classLoader->sharedLibraries == NULL) {
		_OutputStream.writeInteger(0, "%zu");
	} else {
		_OutputStream.writeInteger(pool_numElements(classLoader->sharedLibraries), "%zu");
	}

	_OutputStream.writeCharacters("\n");

	if (avoidLocks()) {
		_OutputStream.writeCharacters("3CLNMBRLOADEDCL\t\t\tNumber of loaded classes ");
		_OutputStream.writeInteger(hashTableGetCount(classLoader->classHashTable), "%zu");
		_OutputStream.writeCharacters("\n");
		return;
	}

	J9ClassWalkState classWalkState;
	UDATA            count = 0;
	J9Class*         clazz = _VirtualMachine->internalVMFunctions->allClassesStartDo(&classWalkState, _VirtualMachine, classLoader);
#if defined(J9VM_OPT_SHARED_CLASSES)
	UDATA            sharedCount = 0;
	void *sharedROMBoundsStart, *sharedROMBoundsEnd;

	if (_VirtualMachine->sharedClassConfig && (classLoader->flags & J9CLASSLOADER_SHARED_CLASSES_ENABLED)) {
		sharedROMBoundsStart = _VirtualMachine->sharedClassConfig->cacheDescriptorList->romclassStartAddress;
		sharedROMBoundsEnd = _VirtualMachine->sharedClassConfig->cacheDescriptorList->metadataStartAddress;
	} else {
		sharedROMBoundsStart = sharedROMBoundsEnd = NULL;
	}
#endif

	while (clazz) {
		if ((clazz->classLoader == classLoader) || isAnon) {
			count++;
		}
#if defined(J9VM_OPT_SHARED_CLASSES)
		if (sharedROMBoundsStart && (clazz->romClass >= sharedROMBoundsStart) && (clazz->romClass < sharedROMBoundsEnd)) {
			sharedCount++;
		}
#endif
		clazz = _VirtualMachine->internalVMFunctions->allClassesNextDo(&classWalkState);
	}

	_VirtualMachine->internalVMFunctions->allClassesEndDo(&classWalkState);

	/* Determine and write the number of loaded classes */
	_OutputStream.writeCharacters("3CLNMBRLOADEDCL\t\t\tNumber of loaded classes ");
	_OutputStream.writeInteger(count, "%zu");
	_OutputStream.writeCharacters("\n");

#if defined(J9VM_OPT_SHARED_CLASSES)
	if (sharedROMBoundsStart) {
		/* Determine and write the number of loaded classes */
		_OutputStream.writeCharacters("3CLNMBRSHAREDCL\t\t\tNumber of shared classes ");
		_OutputStream.writeInteger(sharedCount, "%zu");
		_OutputStream.writeCharacters("\n");
	}
#endif
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeLibraries() method implementation                                     */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeLibraries(J9ClassLoader* classLoader)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);
	char* executableName = NULL;
	/* If there are no libraries, there is nothing to do */
	if (classLoader->sharedLibraries == NULL) {
		return;
	}

	/* Determine the status of the given loader */
	j9object_t object = getClassLoaderObject(classLoader);
	j9object_t appLdr = getClassLoaderObject(_VirtualMachine->applicationClassLoader);
	j9object_t extLdr = appLdr ? J9VMJAVALANGCLASSLOADER_PARENT_VM(_VirtualMachine, appLdr) : NULL;

	bool unload = (_Context->eventFlags & J9RAS_DUMP_ON_CLASS_UNLOAD) != 0;

	bool isSystem = (classLoader == _VirtualMachine->systemClassLoader);
	bool isApp = (appLdr ? classLoader == J9VMJAVALANGCLASSLOADER_VMREF_VM(_VirtualMachine, appLdr) : false);
	bool isExt = (extLdr ? classLoader == J9VMJAVALANGCLASSLOADER_VMREF_VM(_VirtualMachine, extLdr) : false);

	/* Decode and write the status */
	_OutputStream.writeCharacters("2CLTEXTCLLIB    \t");

	if (isSystem) {
		_OutputStream.writeCharacters("Loader *System*(");
		_OutputStream.writePointer(object);
		_OutputStream.writeCharacters(")\n");

	} else if (unload && !isExt && !isApp) {
		_OutputStream.writeCharacters("Loader [locked](");
		_OutputStream.writePointer(object);
		_OutputStream.writeCharacters(")\n");

	} else if (object == NULL) {
		_OutputStream.writeCharacters("Loader [missing](");
		_OutputStream.writePointer(object);
		_OutputStream.writeCharacters(")\n");

	} else {
		_OutputStream.writeCharacters("Loader ");
		_OutputStream.writeCharacters(J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ_VM(_VirtualMachine, object)->romClass));
		_OutputStream.writeCharacters("(");
		_OutputStream.writePointer(object);
		_OutputStream.writeCharacters(")\n");
	}

	/* Write the libraries */
	pool_state       sharedLibrariesWalkState;
	J9NativeLibrary* sharedLibrary = (J9NativeLibrary*)pool_startDo(classLoader->sharedLibraries, &sharedLibrariesWalkState);

	while (sharedLibrary) {
		if (J9NATIVELIB_LINK_MODE_STATIC == sharedLibrary->linkMode) {
			/* Attempt obtaining executable name when first statically linked library is encountered. */
			if (NULL == executableName) {
				/* Executable name launching the jvm is required, in case of statically linked libraries. */
				if (-1 == j9sysinfo_get_executable_name(NULL, &executableName)) {
					/* If the executable name was not found, indicate this. */
					executableName = (char*) "[executable name unavailable]";
				}
				/* Don't reclaim executable name returned by j9sysinfo_get_executable_name; system-owned. */
			}
			_OutputStream.writeCharacters("3CLTEXTSLIB   \t\t\t");
			_OutputStream.writeCharacters(executableName);
			_OutputStream.writeCharacters(" (");
			_OutputStream.writeCharacters(sharedLibrary->logicalName);
			_OutputStream.writeCharacters(")");
		} else {
			_OutputStream.writeCharacters("3CLTEXTLIB   \t\t\t");
			_OutputStream.writeCharacters(sharedLibrary->name);
		}
		_OutputStream.writeCharacters("\n");

		sharedLibrary = (J9NativeLibrary*)pool_nextDo(&sharedLibrariesWalkState);
	}
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::writeClasses() method implementation                                       */
/*                                                                                                */
/**************************************************************************************************/
void
JavaCoreDumpWriter::writeClasses(J9ClassLoader* classLoader)
{
	/* Determine the status of the given loader */
	j9object_t object = getClassLoaderObject(classLoader);
	j9object_t appLdr = getClassLoaderObject(_VirtualMachine->applicationClassLoader);
	j9object_t extLdr = appLdr ? J9VMJAVALANGCLASSLOADER_PARENT_VM(_VirtualMachine, appLdr) : NULL;

	bool unload = (_Context->eventFlags & J9RAS_DUMP_ON_CLASS_UNLOAD) != 0;

	bool isSystem = (classLoader == _VirtualMachine->systemClassLoader);
	bool isApp = (appLdr ? classLoader == J9VMJAVALANGCLASSLOADER_VMREF_VM(_VirtualMachine, appLdr) : false);
	bool isExt = (extLdr ? classLoader == J9VMJAVALANGCLASSLOADER_VMREF_VM(_VirtualMachine, extLdr) : false);
	bool isAnon = (classLoader == _VirtualMachine->anonClassLoader);
	/* Decode and write the status */
	_OutputStream.writeCharacters("2CLTEXTCLLOAD  \t\t");

	if (isSystem) {
		_OutputStream.writeCharacters("Loader *System*(");
		_OutputStream.writePointer(object);
		_OutputStream.writeCharacters(")\n");

	} else if (unload && !isExt && !isApp) {
		_OutputStream.writeCharacters("Loader [locked](");
		_OutputStream.writePointer(object);
		_OutputStream.writeCharacters(")\n");

	} else if (object == NULL) {
		_OutputStream.writeCharacters("Loader [missing](");
		_OutputStream.writePointer(object);
		_OutputStream.writeCharacters(")\n");

	} else {
		_OutputStream.writeCharacters("Loader ");
		_OutputStream.writeCharacters(J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ_VM(_VirtualMachine, object)->romClass));
		_OutputStream.writeCharacters("(");
		_OutputStream.writePointer(object);
		_OutputStream.writeCharacters(")\n");
	}

	if (avoidLocks()) {
		return;
	}

	/* Write the classes */
	J9ClassWalkState classWalkState;
	J9Class*         clazz = _VirtualMachine->internalVMFunctions->allClassesStartDo(&classWalkState, _VirtualMachine, classLoader);
#if defined(J9VM_OPT_SHARED_CLASSES)
	void *sharedROMBoundsStart, *sharedROMBoundsEnd;

	if (_VirtualMachine->sharedClassConfig && (classLoader->flags & J9CLASSLOADER_SHARED_CLASSES_ENABLED)) {
		sharedROMBoundsStart = _VirtualMachine->sharedClassConfig->cacheDescriptorList->romclassStartAddress;
		sharedROMBoundsEnd = _VirtualMachine->sharedClassConfig->cacheDescriptorList->metadataStartAddress;
	} else {
		sharedROMBoundsStart = sharedROMBoundsEnd = NULL;
	}
#endif

	while (clazz) {
		/* Ignore classes which do not belong to the current loader */
		if ((clazz->classLoader == classLoader) || isAnon) {
			/* Handle arrays and normal classes separately */
			if (J9ROMCLASS_IS_ARRAY(clazz->romClass)) {
				/* Write the prefix */
				_OutputStream.writeCharacters("3CLTEXTCLASS   \t\t\t");

				/* Write out the arity */
				J9ArrayClass* array = (J9ArrayClass*)clazz;
				if (array->arity > 255) {
					/* damaged or in-flight class, bail out of this classloader */
					_OutputStream.writeCharacters("[unknown]\n");
					break;
				}
				for (UDATA n = array->arity; n > 1; n--) {
					_OutputStream.writeCharacters("[");
				}

				/* Write out the class name */
				J9Class*    leafClass = array->leafComponentType;
				J9ROMClass* leafType  = leafClass->romClass;

				_OutputStream.writeCharacters(J9ROMCLASS_CLASSNAME(leafClass->arrayClass->romClass));

				if (!J9ROMCLASS_IS_PRIMITIVE_TYPE(leafType)) {
					_OutputStream.writeCharacters(J9ROMCLASS_CLASSNAME(leafType));
					_OutputStream.writeCharacters(";");
				}
				_OutputStream.writeCharacters("(");
				_OutputStream.writePointer(clazz);
#if defined(J9VM_OPT_SHARED_CLASSES)
				if (sharedROMBoundsStart && (clazz->romClass >= sharedROMBoundsStart) && (clazz->romClass < sharedROMBoundsEnd)) {
					_OutputStream.writeCharacters(" shared");
				}
#endif
				_OutputStream.writeCharacters(")\n");

			} else {
				/* It's a normal class */
				_OutputStream.writeCharacters("3CLTEXTCLASS   \t\t\t");
				_OutputStream.writeCharacters(J9ROMCLASS_CLASSNAME(clazz->romClass));
				_OutputStream.writeCharacters("(");
				_OutputStream.writePointer(clazz);
#if defined(J9VM_OPT_SHARED_CLASSES)
				if (sharedROMBoundsStart && (clazz->romClass >= sharedROMBoundsStart) && (clazz->romClass < sharedROMBoundsEnd)) {
					_OutputStream.writeCharacters(" shared");
				}
#endif
				_OutputStream.writeCharacters(")\n");
			}
		}

		clazz = _VirtualMachine->internalVMFunctions->allClassesNextDo(&classWalkState);
	}

	_VirtualMachine->internalVMFunctions->allClassesEndDo(&classWalkState);
}

/**************************************************************************************************/
/*                                                                                                */
/* JavaCoreDumpWriter::getClassLoaderObject() method implementation                               */
/*                                                                                                */
/**************************************************************************************************/
j9object_t
JavaCoreDumpWriter::getClassLoaderObject(J9ClassLoader* loader)
{
	return (j9object_t)((loader != NULL) ? TMP_J9CLASSLOADER_CLASSLOADEROBJECT(loader) : NULL);
}

/**
 * Obtain J9ObjectMonitorInfo's for the given thread to obtain lock context information.
 * Calls getOwnedObjectMonitors and then totals the monitors up to give a total entry
 * count rather than per thread entry counts.
 * @param vmThread[in] the thread to query (not the current thread)
 * @param monitorInfos[out] pointer to the J9ObjectMonitorInfo array to store the results in.
 * @return number of records found
 */
IDATA
JavaCoreDumpWriter::getOwnedObjectMonitors(J9VMThread* vmThread, J9ObjectMonitorInfo* monitorInfos)
{
	IDATA monitorCount = 0;
	monitorCount = _VirtualMachine->internalVMFunctions->getOwnedObjectMonitors(_Context->onThread, vmThread, monitorInfos, _MaximumMonitorInfosPerThread);

	/* Walk the returned monitor info and sum up the entry counts to make them more useful in the stack
	 * trace. (The array is walked backwards so the totals start at 1 at the bottom of the stack!)
	 * (monitorCount is set to 0 if handlerGetOwnedObjectMonitors is called.)
	 */
	for( IDATA i = monitorCount - 2; i >= 0; i-- ) {
		IDATA j = i+1;
		while(  j < monitorCount &&  monitorInfos[i].object != monitorInfos[j].object ) {
			j++;
		}
		if( j < monitorCount ) {
			monitorInfos[i].count += monitorInfos[j].count;
		}
	}

	return monitorCount;
}

/**
 * Write details from the java/lang/Thread instance associated with this thread.
 * Called under signal protection as this reaches into the java heap.
 * @param vmThread[in] the thread to query (not the current thread)
 */
void
JavaCoreDumpWriter::writeJavaLangThreadInfo (J9VMThread* vmThread)
{
	I_64 threadID = J9VMJAVALANGTHREAD_TID(vmThread, vmThread->threadObject);

	_OutputStream.writeCharacters("3XMJAVALTHREAD            (java/lang/Thread getId:");
	_OutputStream.writeInteger64(threadID);
	_OutputStream.writeCharacters(", isDaemon:");
	_OutputStream.writeCharacters(J9VMJAVALANGTHREAD_ISDAEMON(vmThread, vmThread->threadObject)?"true":"false");
	_OutputStream.writeCharacters(")\n");

}

void
JavaCoreDumpWriter::writeCPUinfo(void)
{
	PORT_ACCESS_FROM_PORT(_PortLibrary);

	UDATA bound = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_BOUND);
	UDATA target = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET);

	_OutputStream.writeCharacters(
			"NULL           \n");
	_OutputStream.writeCharacters(
			"1CICPUINFO     CPU Information\n"
			"NULL           ------------------------------------------------------------------------\n"
			"2CIPHYSCPU     Physical CPUs: ");
	_OutputStream.writeInteger(j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_PHYSICAL), "%i\n");
	_OutputStream.writeCharacters(
			"2CIONLNCPU     Online CPUs: ");
	_OutputStream.writeInteger(j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE), "%i\n");
	_OutputStream.writeCharacters(
			"2CIBOUNDCPU    Bound CPUs: ");
	_OutputStream.writeInteger(bound, "%i\n");
	_OutputStream.writeCharacters(
			"2CIACTIVECPU   Active CPUs: ");
	if (bound != target) {
		_OutputStream.writeInteger(target, "%i\n");
	} else {
		/* target is not being overridden by active CPUs, so print 0 */
		_OutputStream.writeCharacters("0\n");
	}
	_OutputStream.writeCharacters(
			"2CITARGETCPU   Target CPUs: ");
	_OutputStream.writeInteger(target, "%i\n");

	return;
}

#if defined(LINUX)
void
JavaCoreDumpWriter::writeCgroupMetrics(void)
{
	OMRPORT_ACCESS_FROM_J9PORT(_PortLibrary);
	BOOLEAN isCgroupSystemAvailable = omrsysinfo_cgroup_is_system_available();
	if (isCgroupSystemAvailable) {
		const OMRCgroupEntry *entryHead = omrsysinfo_get_cgroup_subsystem_list();
		OMRCgroupEntry *cgEntry = (OMRCgroupEntry *)entryHead;
		int32_t rc = 0;
		if (NULL != cgEntry) {
			_OutputStream.writeCharacters("NULL \n");
			_OutputStream.writeCharacters("1CICGRPINFO    Cgroup Information \n");
			_OutputStream.writeCharacters("NULL           ------------------------------------------------------------------------\n");
			do {
				_OutputStream.writeCharacters("2CICGRPINFO    subsystem : ");
				_OutputStream.writeCharacters(cgEntry->subsystem);
				_OutputStream.writeCharacters("\n");
				_OutputStream.writeCharacters("2CICGRPINFO    cgroup name : ");
				_OutputStream.writeCharacters(cgEntry->cgroup);
				_OutputStream.writeCharacters("\n");
				OMRCgroupMetricIteratorState cgroupState = {0};
				rc = omrsysinfo_cgroup_subsystem_iterator_init(cgEntry->flag, &cgroupState);
				if (0 == rc) {
					if (0 != cgroupState.numElements) {
						OMRCgroupMetricElement metricElement = {0};
						while (0 != omrsysinfo_cgroup_subsystem_iterator_hasNext(&cgroupState)) {
							const char *metricKey = NULL;
							rc = omrsysinfo_cgroup_subsystem_iterator_metricKey(&cgroupState, &metricKey);
							if (0 == rc) {
								rc = omrsysinfo_cgroup_subsystem_iterator_next(&cgroupState, &metricElement);
								if (rc == 0) {
									_OutputStream.writeCharacters("3CICGRPINFO        ");
									_OutputStream.writeCharacters(metricKey);
									_OutputStream.writeCharacters(" : ");
									_OutputStream.writeCharacters(metricElement.value);
									if (NULL != metricElement.units) {
										_OutputStream.writeCharacters(" ");
										_OutputStream.writeCharacters(metricElement.units);
									}
									_OutputStream.writeCharacters("\n");
								} else {
									_OutputStream.writeCharacters("3CICGRPINFO        ");
									_OutputStream.writeCharacters(metricKey);
									_OutputStream.writeCharacters(" : Unavailable\n");
								}
							}
						}
					}
					omrsysinfo_cgroup_subsystem_iterator_destroy(&cgroupState);
				}
				cgEntry = cgEntry->next;
			} while (cgEntry != entryHead);
		}
	}
}
#endif

/**************************************************************************************************/
/*                                                                                                */
/* Callback function implementations                                                              */
/*                                                                                                */
/**************************************************************************************************/
void
writeLoaderCallBack(void* classLoader, void* userData)
{
	((JavaCoreDumpWriter*)(userData))->writeLoader((J9ClassLoader*)classLoader);
}

void
writeLibrariesCallBack(void* classLoader, void* userData)
{
	((JavaCoreDumpWriter*)(userData))->writeLibraries((J9ClassLoader*)classLoader);
}

void
writeClassesCallBack(void* classLoader, void* userData)
{
	((JavaCoreDumpWriter*)(userData))->writeClasses((J9ClassLoader*)classLoader);
}

UDATA
writeFrameCallBack(J9VMThread* vmThread, J9StackWalkState* state)
{
	return ((JavaCoreDumpWriter*)(state->userData1))->writeFrame(state);
}

UDATA
writeExceptionFrameCallBack(J9VMThread* vmThread, void* userData, J9ROMClass* romClass, J9ROMMethod* romMethod, J9UTF8* sourceFile, UDATA lineNumber, J9ClassLoader* classLoader)
{
	JavaCoreDumpWriter *jcdw = (JavaCoreDumpWriter *)((J9StackWalkState*)userData)->userData1;
	return jcdw->writeExceptionFrame(userData, romClass, romMethod, sourceFile, lineNumber);
}

/**************************************************************************************************/
/*                                                                                                */
/* GC iterator call back functions                                                                   */
/*                                                                                                */
/**************************************************************************************************/
static jvmtiIterationControl
heapIteratorCallback(J9JavaVM* virtualMachine, J9MM_IterateHeapDescriptor* heapDescriptor, void* userData)
{
	JavaCoreDumpWriter* jcw = (JavaCoreDumpWriter*)userData;

	virtualMachine->memoryManagerFunctions->j9mm_iterate_spaces(virtualMachine, virtualMachine->portLibrary, heapDescriptor, 0, spaceIteratorCallback, jcw);

	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
spaceIteratorCallback(J9JavaVM* virtualMachine, J9MM_IterateSpaceDescriptor* spaceDescriptor, void* userData)
{
	UDATA sizeTotal = 0;
	UDATA sizeTarget = 0;
	UDATA allocTotal = 0;
	UDATA freeTotal = 0;
#if defined (J9VM_GC_VLHGC)
	UDATA regionEnd;
	regioniterationblock regionTotals;
#endif /* J9VM_GC_VLHGC */
	JavaCoreDumpWriter* jcw = (JavaCoreDumpWriter*)userData;


	jcw->_OutputStream.writeCharacters("NULL           ");
#if defined(J9VM_ENV_DATA64)
	jcw->_OutputStream.writeCharacters(
		"id                 start              end                size               space");
#else
	jcw->_OutputStream.writeCharacters(
		"id         start      end        size       space");
#endif

	/* Balanced (VLHGC) has a lot more regions than the normal GC modes so we collapse down to spaces */
	if (J9_GC_POLICY_BALANCED == ((OMR_VM *)virtualMachine->omrVM)->gcPolicy) {
		jcw->_OutputStream.writeCharacters("\n");
#if defined(J9VM_GC_VLHGC)
		/* For VLH add up the total size of all the regions and print a summary. */
		regionTotals._newIteration = true;
		virtualMachine->memoryManagerFunctions->j9mm_iterate_regions(virtualMachine, virtualMachine->portLibrary, spaceDescriptor, j9mm_iterator_flag_regions_read_only, regionIteratorCallback, &regionTotals);
#endif /* J9VM_GC_VLHGC */
	} else {
		/* For non-VLH print out each region individually on it's own line. */
		jcw->_OutputStream.writeCharacters("/region\n");
		jcw->_SpaceDescriptorName = spaceDescriptor->name;
		/* Write out the details of the containing space first. */
		jcw->_OutputStream.writeCharacters("1STHEAPSPACE   ");
		jcw->_OutputStream.writePointer((void *)spaceDescriptor->id);
		jcw->_OutputStream.writeCharacters(" ");
		jcw->_OutputStream.writeVPrintf("%*c--%*c",  sizeof(void *), ' ',  sizeof(void *), ' ');
		jcw->_OutputStream.writeCharacters(" ");
		jcw->_OutputStream.writeVPrintf("%*c--%*c",  sizeof(void *), ' ',  sizeof(void *), ' ');
		jcw->_OutputStream.writeCharacters(" ");
		jcw->_OutputStream.writeVPrintf("%*c--%*c",  sizeof(void *), ' ',  sizeof(void *), ' ');
		jcw->_OutputStream.writeCharacters(" ");
		jcw->_OutputStream.writeCharacters(spaceDescriptor->name);
		jcw->_OutputStream.writeCharacters(" \n");
		virtualMachine->memoryManagerFunctions->j9mm_iterate_regions(virtualMachine, virtualMachine->portLibrary, spaceDescriptor, j9mm_iterator_flag_regions_read_only, regionIteratorCallback, jcw);
	}

	if (J9_GC_POLICY_BALANCED == ((OMR_VM *)virtualMachine->omrVM)->gcPolicy) {
#if defined(J9VM_GC_VLHGC)
		jcw->_OutputStream.writeCharacters("1STHEAPSPACE   ");
		jcw->_OutputStream.writePointer((void *)spaceDescriptor->id);
		jcw->_OutputStream.writeCharacters(" ");
		jcw->_OutputStream.writePointer((void *)regionTotals._regionStart);
		jcw->_OutputStream.writeCharacters(" ");
		regionEnd = (UDATA)regionTotals._regionStart + (UDATA)regionTotals._regionSize;
		jcw->_OutputStream.writePointer((const void*)regionEnd);
		jcw->_OutputStream.writeCharacters(" ");
		jcw->_OutputStream.writeVPrintf(FORMAT_SIZE_HEX, sizeof(void *) * 2, regionTotals._regionSize);
		jcw->_OutputStream.writeCharacters(" ");
		jcw->_OutputStream.writeCharacters(spaceDescriptor->name);
		jcw->_OutputStream.writeCharacters(" \n");
#endif /* J9VM_GC_VLHGC */
	}

	sizeTotal = virtualMachine->memoryManagerFunctions->j9gc_heap_total_memory(virtualMachine);
	sizeTarget = virtualMachine->memoryManagerFunctions->j9gc_get_softmx(virtualMachine);
	freeTotal = virtualMachine->memoryManagerFunctions->j9gc_heap_free_memory(virtualMachine);
	allocTotal = sizeTotal - freeTotal;

	int decimalLength = sizeof(void*) == 4 ? 10 : 20;

	jcw->_OutputStream.writeCharacters("NULL\n");
	jcw->_OutputStream.writeCharacters("1STHEAPTOTAL   ");
	jcw->_OutputStream.writeCharacters("Total memory:        ");
	jcw->_OutputStream.writeVPrintf(FORMAT_SIZE_DECIMAL, decimalLength, sizeTotal);
	jcw->_OutputStream.writeCharacters(" (");
	jcw->_OutputStream.writeVPrintf(FORMAT_SIZE_HEX, sizeof(void *) * 2, sizeTotal);
	jcw->_OutputStream.writeCharacters(")\n");
	if (sizeTarget != 0) {
		jcw->_OutputStream.writeCharacters("1STHEAPTARGET  ");
		jcw->_OutputStream.writeCharacters("Target memory:       ");
		jcw->_OutputStream.writeVPrintf(FORMAT_SIZE_DECIMAL, decimalLength, sizeTarget);
		jcw->_OutputStream.writeCharacters(" (");
		jcw->_OutputStream.writeVPrintf(FORMAT_SIZE_HEX, sizeof(void *) * 2, sizeTarget);
		jcw->_OutputStream.writeCharacters(")\n");
	}
	jcw->_OutputStream.writeCharacters("1STHEAPINUSE   ");
	jcw->_OutputStream.writeCharacters("Total memory in use: ");
	jcw->_OutputStream.writeVPrintf(FORMAT_SIZE_DECIMAL, decimalLength, allocTotal);
	jcw->_OutputStream.writeCharacters(" (");
	jcw->_OutputStream.writeVPrintf(FORMAT_SIZE_HEX, sizeof(void *) * 2, allocTotal);
	jcw->_OutputStream.writeCharacters(")\n");
	jcw->_OutputStream.writeCharacters("1STHEAPFREE    ");
	jcw->_OutputStream.writeCharacters("Total memory free:   ");
	jcw->_OutputStream.writeVPrintf(FORMAT_SIZE_DECIMAL, decimalLength, freeTotal);
	jcw->_OutputStream.writeCharacters(" (");
	jcw->_OutputStream.writeVPrintf(FORMAT_SIZE_HEX, sizeof(void *) * 2, freeTotal);
	jcw->_OutputStream.writeCharacters(")\n");
	jcw->_OutputStream.writeCharacters("NULL\n");

	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
regionIteratorCallback(J9JavaVM* virtualMachine, J9MM_IterateRegionDescriptor* regionDescriptor, void* userData)
{
	if (J9_GC_POLICY_BALANCED == ((OMR_VM *)virtualMachine->omrVM)->gcPolicy) {
		/*
		 * For balanced (VLH) GC there are too many regions to print them individually.
		 * We add up the sizes and print a summary after scanning the regions.
		 */
#if defined(J9VM_GC_VLHGC)
		regioniterationblock* regionTotals = (regioniterationblock*)userData;

		if (true == regionTotals->_newIteration) {
			regionTotals->_newIteration = false;
			regionTotals->_regionSize = 0;
			regionTotals->_regionStart = regionDescriptor->regionStart;
		}
		regionTotals->_regionSize += (UDATA)regionDescriptor->regionSize;
#endif /* J9VM_GC_VLHGC */
	} else {
		/*
		 * For normal GC modes we can print a line per region as
		 * we iterate over the regions.
		 */
		UDATA regionEnd;
		JavaCoreDumpWriter* jcw = (JavaCoreDumpWriter*)userData;
		jcw->_OutputStream.writeCharacters("1STHEAPREGION  ");
		jcw->_OutputStream.writePointer((void *)regionDescriptor->id);
		jcw->_OutputStream.writeCharacters(" ");
		jcw->_OutputStream.writePointer((void *)regionDescriptor->regionStart);
		jcw->_OutputStream.writeCharacters(" ");
		regionEnd = (UDATA)regionDescriptor->regionStart + (UDATA)regionDescriptor->regionSize;
		jcw->_OutputStream.writePointer((const void*)regionEnd);
		jcw->_OutputStream.writeCharacters(" ");
		jcw->_OutputStream.writeVPrintf(FORMAT_SIZE_HEX, sizeof(void *) * 2, regionDescriptor->regionSize);
		jcw->_OutputStream.writeCharacters(" ");
		jcw->_OutputStream.writeCharacters(jcw->_SpaceDescriptorName);
		jcw->_OutputStream.writeCharacters("/");
		jcw->_OutputStream.writeCharacters(regionDescriptor->name);
		jcw->_OutputStream.writeCharacters(" \n");
	}

	return JVMTI_ITERATION_CONTINUE;
}

extern "C" {
UDATA
protectedWriteSection(struct J9PortLibrary *portLibrary, void *arg)
{
	sectionClosure *closure = (sectionClosure *)arg;
	closure->invoke();

	return 0;
}

UDATA
handlerWriteSection(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	JavaCoreDumpWriter* jcw = (JavaCoreDumpWriter*)userData;

	jcw->_OutputStream.writeCharacters("1INTERNAL      In-flight data encountered. Output may be missing or incomplete.\n");
//  jcw->_OutputStream.writeCharacters("NULL           \n");
//  jcw->writeGPCategory(gpInfo, "2INTERNAL                ", J9PORT_SIG_SIGNAL);
//
//  jcw->_OutputStream.writeCharacters("NULL           \n");
//  jcw->writeGPCategory(gpInfo, "2INTERNAL                ", J9PORT_SIG_MODULE);
//
//  jcw->_OutputStream.writeCharacters("NULL\n");

	return J9PORT_SIG_EXCEPTION_RETURN;
}

UDATA
protectedStartDoWithSignal(struct J9PortLibrary *portLibrary, void *args)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct walkClosure *closure = (struct walkClosure *)args;
	return (UDATA)j9introspect_threads_startDo_with_signal(closure->heap, (J9ThreadWalkState*)closure->state, closure->gpInfo);
}

UDATA
protectedStartDo(struct J9PortLibrary *portLibrary, void *args)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct walkClosure *closure = (struct walkClosure *)args;
	return (UDATA)j9introspect_threads_startDo(closure->heap, (J9ThreadWalkState*)closure->state);
}

UDATA
protectedNextDo(struct J9PortLibrary *portLibrary, void *args)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct walkClosure *closure = (struct walkClosure *)args;
	return (UDATA)j9introspect_threads_nextDo((J9ThreadWalkState*)closure->state);
}

UDATA
protectedGetVMThreadName(struct J9PortLibrary *portLibrary, void *args)
{
	void **parameters = (void**)args;

	return (UDATA)tryGetOMRVMThreadName(((J9VMThread*)parameters[1])->omrVMThread);
}

UDATA
protectedGetVMThreadObjectState(struct J9PortLibrary *portLibrary, void *args)
{
	void **parameters = (void**)args;

	return (UDATA)getVMThreadObjectState((J9VMThread*)parameters[0], (j9object_t*)parameters[1], (J9VMThread**)parameters[3], (UDATA*)parameters[4]);
}

UDATA
protectedGetVMThreadRawState(struct J9PortLibrary *portLibrary, void *args)
{
	void **parameters = (void**)args;

	return (UDATA)getVMThreadRawState((J9VMThread*)parameters[0], (j9object_t*)parameters[1], (omrthread_monitor_t*)parameters[2], (J9VMThread**)parameters[3], (UDATA*)parameters[4]);
}
UDATA
protectedWalkJavaStack(struct J9PortLibrary *portLibrary, void *args)
{
	struct walkClosure *closure = (struct walkClosure *)args;
	return (UDATA)closure->jcw->_VirtualMachine->walkStackFrames((J9VMThread*)closure->jcw->_Context->onThread, (J9StackWalkState*)closure->state);
}

UDATA
protectedIterateStackTrace(struct J9PortLibrary *portLibrary, void *args)
{
	struct walkClosure *closure = (struct walkClosure *)args;
	void **parameters = (void**) closure->state;
	/* Key:
	 * parameters[0] = vmThread;
	 * parameters[1] = throwable;
	 * parameters[2] = stackWalkState;
	 */
	J9VMThread* vmThread = (J9VMThread*) parameters[0];
	closure->jcw->_VirtualMachine->internalVMFunctions->iterateStackTrace(vmThread, (j9object_t*) parameters[1],
										writeExceptionFrameCallBack, (J9StackWalkState*)parameters[2],
										FALSE);

	return 0;
}

UDATA
protectedWriteGCHistoryLines(struct J9PortLibrary *portLibrary, void *args)
{
	struct walkClosure *closure = (struct walkClosure *)args;
	void **parameters = (void**) closure->state;
	closure->jcw->writeGCHistoryLines((UtThreadData**) parameters[0], (UtTracePointIterator*) parameters[1], (char*) parameters[2]);
	return 0;
}

UDATA
protectedWriteThreadBlockers(struct J9PortLibrary *portLibrary, void *args)
{
	struct walkClosure *closure = (struct walkClosure *)args;
	void **parameters = (void**) closure->state;
	J9VMThread* vmThread = (J9VMThread*)parameters[0];
	UDATA vmstate = *((UDATA*)parameters[1]);
	j9object_t lockObject = (j9object_t)parameters[2];
	J9VMThread* lockOwnerThread = (J9VMThread*)parameters[3];
	closure->jcw->writeThreadBlockers(vmThread, vmstate, lockObject, lockOwnerThread);
	return 0;
}

/**
 * Wrapper function for getOwnedObjectMonitors to unpack arguments
 * when called via _PortLibrary->sig_protect().
 * If the call to getOwnedObjectMonitors fails handlerGetOwnedObjectMonitors
 * will be called and return J9PORT_SIG_EXCEPTION_RETURN instead.
 * @param portLibrary[in] pointer to the port library
 * @param args[in,out] pointer to the arguments to unpack for getOwnedObjectMonitors
 * @return 0
 */
UDATA
protectedGetOwnedObjectMonitors(struct J9PortLibrary *portLibrary, void *args)
{
	struct walkClosure *closure = (struct walkClosure *)args;
	void **parameters = (void**) closure->state;
	J9VMThread* vmThread = (J9VMThread*)parameters[0];
	J9ObjectMonitorInfo* monitorInfos = (J9ObjectMonitorInfo*)parameters[1];
	IDATA *monitorCount = (IDATA*)parameters[2];
	(*monitorCount) = closure->jcw->getOwnedObjectMonitors(vmThread, monitorInfos);
	return 0;
}

/**
 * Wrapper function for writeJavaLangThreadInfo to unpack arguments
 * when called via _PortLibrary->sig_protect().
 * If the call to writeJavaLangThreadInfo fails handlerWriteJavaLangThreadInfo
 * will be called and return J9PORT_SIG_EXCEPTION_RETURN instead.
 * @param portLibrary[in] pointer to the port library
 * @param args[in,out] pointer to the arguments to unpack for writeJavaLangThreadInfo
 * @return 0
 */
UDATA
protectedWriteJavaLangThreadInfo(struct J9PortLibrary *portLibrary, void *args)
{
	struct walkClosure *closure = (struct walkClosure *)args;
	void **parameters = (void**) closure->state;
	J9VMThread* vmThread = (J9VMThread*)parameters[0];
	closure->jcw->writeJavaLangThreadInfo(vmThread);
	return 0;
}

/**
 * Wrapper function for writeThreadsWithNativeStacks
  * If the call to writeThreadsWithNativeStacks fails handlerWriteStacks
 * will be called and return J9PORT_SIG_EXCEPTION_RETURN instead.
 * @param portLibrary[in] pointer to the port library
 * @param args[in,out] pointer to the arguments to unpack for writeThreadsWithNativeStacks
 * @return 0
 */
UDATA
protectedWriteThreadsWithNativeStacks(struct J9PortLibrary *portLibrary, void *args)
{
	struct walkClosure *closure = (struct walkClosure *)args;
	closure->jcw->writeThreadsWithNativeStacks();
	return 0;
}

/**
 * Wrapper function for writeThreadsJavaOnly
  * If the call to writeThreadsJavaOnly fails handlerWriteStacks
 * will be called and return J9PORT_SIG_EXCEPTION_RETURN instead.
 * @param portLibrary[in] pointer to the port library
 * @param args[in,out] pointer to the arguments to unpack for writeThreadsJavaOnly
 * @return 0
 */
UDATA
protectedWriteThreadsJavaOnly(struct J9PortLibrary *portLibrary, void *args)
{
	struct walkClosure *closure = (struct walkClosure *)args;
	closure->jcw->writeThreadsJavaOnly();
	return 0;
}

/**
 * Wrapper function for writeThreadsUsageSummary
 * If the call to writeThreadsUsageSummary fails handlerGetThreadsUsageInfo
 * will be called and return J9PORT_SIG_EXCEPTION_RETURN instead.
 * @param portLibrary[in] pointer to the port library
 * @param args[in,out] pointer to the arguments to unpack for writeThreadsUsageSummary
 * @return 0
 */
UDATA
protectedWriteThreadsUsageSummary(struct J9PortLibrary *portLibrary, void *args)
{
	struct walkClosure *closure = (struct walkClosure *)args;
	closure->jcw->writeThreadsUsageSummary();
	return 0;
}

UDATA
handlerJavaThreadWalk(struct J9PortLibrary *portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	JavaCoreDumpWriter* jcw = (JavaCoreDumpWriter*)userData;

	jcw->_OutputStream.writeCharacters("1INTERNAL                    Unable to walk in-flight data on call stack\n");

	return J9PORT_SIG_EXCEPTION_RETURN;
}

UDATA
handlerNativeThreadWalk(struct J9PortLibrary *portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	/* we suppress this error as it should be reported at the end of the threads section anyway */
	return J9PORT_SIG_EXCEPTION_RETURN;
}

UDATA
handlerGetVMThreadName(struct J9PortLibrary *portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	const char **namePtr = (const char**)userData;
	*namePtr = "<name unavailable>";

	return J9PORT_SIG_EXCEPTION_RETURN;
}

UDATA
handlerGetVMThreadObjectState(struct J9PortLibrary *portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	/* suppress this error as all we can do is skip the data we would have provided */
	return J9PORT_SIG_EXCEPTION_RETURN;
}

UDATA
handlerGetVMThreadRawState(struct J9PortLibrary *portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	/* suppress this error as all we can do is skip the data we would have provided */
	return J9PORT_SIG_EXCEPTION_RETURN;
}

UDATA
handlerWriteThreadBlockers(struct J9PortLibrary *portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	JavaCoreDumpWriter* jcw = (JavaCoreDumpWriter*)userData;

	jcw->_OutputStream.writeCharacters("1INTERNAL                    Unable to obtain blocking thread information\n");

	return J9PORT_SIG_EXCEPTION_RETURN;
}

UDATA
handlerGetThreadsUsageInfo(struct J9PortLibrary *portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	omrthread_get_jvm_cpu_usage_info_error_recovery();

	JavaCoreDumpWriter* jcw = (JavaCoreDumpWriter*)userData;

	jcw->_OutputStream.writeCharacters("1INTERNAL                    Unable to obtain JVM threads CPU usage summary information\n");

	return J9PORT_SIG_EXCEPTION_RETURN;
}

/**
 * Handler function for writeJavaLangThreadInfo when called via _PortLibrary->sig_protect().
 * If the call fails this function will write out a message indicating java/lang/Thread
 * info is unavailable for the specified thread.
 * @param portLibrary[in] pointer to the port library
 * @param gpType[in] failure type
 * @param gpInfo[in] failure info
 * @param userData[in] pointer to "this", the JavaCoreDumpWriter that we are running in.
 * @return 0
 */
UDATA
handlerWriteJavaLangThreadInfo(struct J9PortLibrary *portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	JavaCoreDumpWriter* jcw = (JavaCoreDumpWriter*)userData;

	jcw->_OutputStream.writeCharacters("1INTERNAL                    Unable to obtain java/lang/Thread information\n");

	return J9PORT_SIG_EXCEPTION_RETURN;
}

/**
 * Handler function for getOwnedObjectMonitors when called via _PortLibrary->sig_protect().
 * If the call fails this function will write out a message indicating lock context
 * info is unavailable for the specified thread.
 * @param portLibrary[in] pointer to the port library
 * @param gpType[in] failure type
 * @param gpInfo[in] failure info
 * @param userData[in] pointer to "this", the JavaCoreDumpWriter that we are running in.
 * @return 0
 */
UDATA
handlerGetOwnedObjectMonitors(struct J9PortLibrary *portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	JavaCoreDumpWriter* jcw = (JavaCoreDumpWriter*)userData;

	jcw->_OutputStream.writeCharacters("1INTERNAL                    Unable to obtain lock context information\n");

	return J9PORT_SIG_EXCEPTION_RETURN;
}

UDATA
handlerIterateStackTrace(struct J9PortLibrary *portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	JavaCoreDumpWriter* jcw = (JavaCoreDumpWriter*)userData;

	jcw->_OutputStream.writeCharacters("1INTERNAL                    Unable to write in-flight data on call stack\n");

	return J9PORT_SIG_EXCEPTION_RETURN;
}

/**
 * Handler function for writeThreadsWithNativeStacks and writeThreadsJavaOnly
 * when called via _PortLibrary->sig_protect().
 * If the call fails this function will write out a message indicating the thread
 * walk could not be completed and thread may be missing from the output.
 * @param portLibrary[in] pointer to the port library
 * @param gpType[in] failure type
 * @param gpInfo[in] failure info
 * @param userData[in] pointer to "this", the JavaCoreDumpWriter that we are running in.
 * @return 0
 */
UDATA
handlerWriteStacks(struct J9PortLibrary *portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	JavaCoreDumpWriter* jcw = (JavaCoreDumpWriter*)userData;

	jcw->_OutputStream.writeCharacters("NULL\n");
	if( jcw->_ThreadsWalkStarted ==  true ) {
		jcw->_OutputStream.writeCharacters("1INTERNAL     Unable to walk threads. Some or all threads may have been omitted.\n");
	} else {
		jcw->_OutputStream.writeCharacters("1INTERNAL     Unable to collect native thread information.\n");
	}

	return J9PORT_SIG_EXCEPTION_RETURN;
}

} // extern C
/**************************************************************************************************/
/*                                                                                                */
/* Functions used by hash table implementations                                                   */
/*                                                                                                */
/**************************************************************************************************/
static UDATA
lockHashFunction(void* key, void* user)
{
	return ((UDATA)((JavaCoreDumpWriter::DeadLockGraphNode*)key)->thread) / sizeof(UDATA);
}

static UDATA
lockHashEqualFunction(void* left, void* right, void* user)
{
	return ((JavaCoreDumpWriter::DeadLockGraphNode*)left)->thread == ((JavaCoreDumpWriter::DeadLockGraphNode*)right)->thread;
}

/* Primary entry point */
extern "C" void
runJavadump(char *label, J9RASdumpContext *context, J9RASdumpAgent *agent)
{
	/* Create a java core writer object which will write the dump file */
	JavaCoreDumpWriter(label, context, agent);
}

//============================================================================================

static IDATA
vmthread_comparator(struct J9AVLTree *tree, struct J9AVLTreeNode *insertNode, struct J9AVLTreeNode *walkNode)
{
	UDATA insert_tid = 0, walk_tid = 0;
	vmthread_avl_node *insert_thread = (vmthread_avl_node *)insertNode;
	vmthread_avl_node *walk_thread = (vmthread_avl_node *)walkNode;

	if (insert_thread == NULL || walkNode == NULL) {
		/* mustn't return a match for null nodes */
		return -1;
	}

	if (insert_thread->vmthread->osThread) {
		insert_tid = omrthread_get_osId(insert_thread->vmthread->osThread);
		if (insert_tid == 0) {
			insert_tid = (UDATA) (((U_8*)insert_thread->vmthread->osThread) + sizeof(J9AbstractThread));
		}
	}

	if (walk_thread->vmthread->osThread) {
		walk_tid = omrthread_get_osId(walk_thread->vmthread->osThread);
		if (walk_tid == 0) {
			walk_tid = (UDATA) (((U_8*)walk_thread->vmthread->osThread) + sizeof(J9AbstractThread));
		}
	}

	return insert_tid - walk_tid;
}

static IDATA
vmthread_locator(struct J9AVLTree *tree, UDATA tid , struct J9AVLTreeNode *walkNode)
{
	UDATA walk_tid = 0;
	vmthread_avl_node *walk_thread = (vmthread_avl_node *)walkNode;

	if (walk_thread->vmthread->osThread) {
		walk_tid = omrthread_get_osId(walk_thread->vmthread->osThread);
		if (walk_tid == 0) {
			walk_tid = (UDATA) (((U_8*)walk_thread->vmthread->osThread) + sizeof(J9AbstractThread));
		}
	}

	return tid - walk_tid;
}

/*
 * Walks the J9MonitorTableListEntry list counting the monitors in each monitorTable
 *
 * @param   vm  the J9JavaVM
 *
 * @return  the total number of monitors found
 */
static UDATA
getObjectMonitorCount(J9JavaVM *vm)
{
	J9MonitorTableListEntry *entry = vm->monitorTableList;
	UDATA count = 0;

	while (NULL != entry) {
		if (NULL != entry->monitorTable) {
			count = count + hashTableGetCount(entry->monitorTable);
		}
		entry = entry->next;
	}

	return count;
}

/*
 * Calculate number of allocated VMThreads
 *
 * @param   vm  the J9JavaVM
 *
 * @return  the total number of allocated VMThreads
 */
static UDATA
getAllocatedVMThreadCount(J9JavaVM *vm)
{
	J9VMThread *vmThread;
	UDATA count = 0;

	/* Number of threads in mainThread (linked list) */
	vmThread = J9_LINKED_LIST_START_DO(vm->mainThread);
	while (NULL != vmThread) {
		count += 1;
		vmThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, vmThread);
	}

	/* Number of threads in deadThreadList (linked list) */
	vmThread = J9_LINKED_LIST_START_DO(vm->deadThreadList);
	while (NULL != vmThread) {
		count += 1;
		vmThread = J9_LINKED_LIST_NEXT_DO(vm->deadThreadList, vmThread);
	}

	return count;
}

/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

#ifndef RASTRACE_INTERNAL_H
#define RASTRACE_INTERNAL_H

/* @ddr_namespace: map_to_type=RastraceInternalConstants */

#include "rastrace_external.h"
#include "traceformat.h"
#include "trcqueue.h"
#include "ute_core.h"
#include "ute_dataformat.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define OS_THREAD_FROM_UT_THREAD(thr)		((omrthread_t)(*(thr))->synonym1)
#define OMR_VM_THREAD_FROM_UT_THREAD(thr)	((OMR_VMThread*)(*(thr))->synonym2)
#define UT_THREAD_FROM_ENV(env) \
	( env ? UT_THREAD_FROM_OMRVM_THREAD((OMR_VMThread*)env) : twThreadSelf() )

/*
 * =============================================================================
 *  Constants
 * =============================================================================
 */
#define UT_FASTPATH                   17
#define UT_TRC_SPECIAL_MASK              0x3ff
#define UT_TRACE_WRITE_PRIORITY       8
#define UT_TRACE_INTERNAL             0
#define UT_TRACE_EXTERNAL             1
#define UT_STRUCT_ALIGN               4
#define UT_STACK_SIZE                 0
#define UT_DEBUG                      "UTE_DEBUG"
#define UT_EXCEPTION_THREAD_NAME      "Exception trace pseudo-thread"
#define UT_DEFAULT_PROPERTIES         "IBMTRACE.properties"
#define UT_CONTROL_FILE               "utetcf"
#define UT_FORMAT_FILE                "TraceFormat.dat"
#define UT_PROPERTIES_COMMENT         "//"
#define UT_TPID                       "TPID"
#define UT_TPNID                      "TPNID"
#define UT_TRACE_APPLID_NAME          "UTAP"
#define UT_TRACE_METHOD_NAME          "UTME"
#define UT_TRIGGER_METHOD_RULE_NAME   "UTTM"
#define UT_TRACE_GETMICROS            "getMicros"
#define UT_MAX_TIMERS                 2

#define UT_LEVELMASK                  0xbf

#define UT_STRING_VALUE               15

#define UT_SUSPEND_SNAP               1
#define UT_SUSPEND_INITIALIZE         2
#define UT_SUSPEND_TERMINATE          4
#define UT_SUSPEND_USER               8
#define UT_SUSPEND_LOCKED             16

#define UT_CONTEXT_INITIAL            0
#define UT_CONTEXT_COMPONENT          1
#define UT_CONTEXT_CLASS              2
#define UT_CONTEXT_PROCESS            3
#define UT_CONTEXT_FINAL              4

#define UT_NORMAL_BUFFER              0
#define UT_EXCEPTION_BUFFER           1

#define UT_PURGE_BUFFER_TIMEOUT       1000
#define UT_TRC_BUFFER_FULL            0x00000001
#define UT_TRC_BUFFER_PURGE           0x00000002
#define UT_TRC_BUFFER_SNAP            0x00000004
#define UT_TRC_BUFFER_FLUSH           0x00000008
#define UT_TRC_BUFFER_ACTIVE          0x80000000
#define UT_TRC_BUFFER_EXTERNAL        0x40000000
#define UT_TRC_BUFFER_NEW             0x20000000
#define UT_TRC_BUFFER_WRITTEN         0x00010000
#define UT_TRC_BUFFER_WRITE           0x0000FFFF
#define UT_TCFS                       2


#define J9RAS_APPLICATION_TRACE_BUFFER 1024

/*
 * =============================================================================
 * Event types to match events type constants in com.ibm.jvm.Trace, used
 * to index into UT_FORMAT_TYPES when creating formatting strings for AppTrace.
 * =============================================================================
 */
#define UT_EVENT_TYPE                 0
#define UT_EXCEPTION_TYPE             1
#define UT_ENTRY_TYPE                 2
#define UT_ENTRY_EXCPT_TYPE           3
#define UT_EXIT_TYPE                  4
#define UT_EXIT_EXCPT_TYPE            5
#define UT_MEM_TYPE                   6
#define UT_MEM_EXCPT_TYPE             7
#define UT_DEBUG_TYPE                 8
#define UT_DEBUG_EXCPT_TYPE           9
#define UT_PERF_TYPE                  10
#define UT_PERF_EXCPT_TYPE            11
#define UT_MAX_TYPES                  11

#define UT_FORMAT_TYPES               "-*>><<       "


/*
 * =============================================================================
 * Constants for trace point actions.
 * =============================================================================
 */
#define UT_MINIMAL                    1
#define UT_MAXIMAL                    2
#define UT_COUNT                      4
#define UT_PRINT                      8
#define UT_PLATFORM                   16
#define UT_EXCEPTION                  32
#define UT_EXTERNAL                   64
#define UT_TRIGGER                    128
#define UT_NONE                       0
/* Note that another flag, UT_SPECIAL_ASSERTION, is defined in
 * ut_module.h and occupies the top byte, not the bottom.
 */


#define UT_POINTER_SPEC        "0x%zx"
#define UT_MAX_OPTS 55

/*
 * =============================================================================
 * Trace record subscription (UtSubscription) states
 * =============================================================================
 */
 #define UT_SUBSCRIPTION_ALIVE 0
 #define UT_SUBSCRIPTION_KILLED 1
 #define UT_SUBSCRIPTION_DEAD 2

/* assert() and abort() are a bit useless on Windows - they
 * just print a message. Force a GPF instead.
 */
#if defined (WIN32)

#include <stdio.h>
#include <stdlib.h>

#define UT_ASSERT(expr) do { \
		if (! (expr)) { \
			fprintf(stderr,"UT_ASSERT FAILED: %s at %s:%d\n",#expr,__FILE__,__LINE__); \
			*((int*)0) = 42; \
		} \
	} while (0)

#else

#include <assert.h>
#define UT_ASSERT(expr) assert(expr)

#endif

#define DBG_ASSERT(expr) \
	if (UT_GLOBAL(traceDebug) > 0) { \
		UT_ASSERT((expr)); \
	}

#define UT_GLOBAL(x) (utGlobal->x)

#define UT_DBG_PRINT( data )\
	twFprintf data;

#define UT_DBGOUT(level, data) \
	if (UT_GLOBAL(traceDebug) >= level) { \
		UT_DBG_PRINT(data); \
	} else;

#define UT_DBGOUT_CHECKED(level, data) \
	if (NULL != utGlobal && UT_GLOBAL(traceDebug) >= level) { \
		UT_DBG_PRINT(data); \
	} else;

/*
 * =============================================================================
 *  Macros for atomic operations
 * =============================================================================
 */

#define UT_ATOMIC_AND(target, value) \
		do { \
			uint32_t oldValue; \
			uint32_t newValue; \
			do { \
				oldValue = *(target); \
				newValue = oldValue & (value); \
			} while(!twCompareAndSwap32((target), oldValue, newValue)); \
		} while(0)

#define UT_ATOMIC_OR(target, value) \
		do {  \
			uint32_t oldValue; \
			uint32_t newValue; \
			do { \
				oldValue = *(target); \
				newValue = oldValue | (value); \
			} while(!twCompareAndSwap32((target), oldValue, newValue)); \
		} while(0)

#define UT_ATOMIC_INC(target) \
		do {  \
			uint32_t oldValue; \
			uint32_t newValue; \
			do { \
				oldValue = *(target); \
				newValue = oldValue + 1; \
			} while(!twCompareAndSwap32((target), oldValue, newValue)); \
		} while(0)

#define UT_ATOMIC_ADD(target, value) \
		do {  \
			uint32_t oldValue; \
			uint32_t newValue; \
			do { \
				oldValue = *(target); \
				newValue = oldValue + (value); \
			} while(!twCompareAndSwap32((target), oldValue, newValue)); \
		} while(0)

#define UT_ATOMIC_DEC(target) \
		do {  \
			uint32_t oldValue; \
			uint32_t newValue; \
			do { \
				oldValue = *(target); \
				newValue = oldValue - 1; \
			} while(!twCompareAndSwap32((target), oldValue, newValue)); \
		} while(0)

#define UT_ATOMIC_SUB(target, value) \
		do {  \
			uint32_t oldValue; \
			uint32_t newValue; \
			do { \
				oldValue = *(target); \
				newValue = oldValue - (value); \
			} while(!twCompareAndSwap32((target), oldValue, newValue)); \
		} while(0)

/*
 *  Forward declaration of UtGlobalData
 */
typedef struct  UtGlobalData UtGlobalData;

/*
 * =============================================================================
 *  Trace configuration buffer (UTCF)
 * =============================================================================
 */
#define UT_TRACE_CONFIG_NAME "UTCF"
typedef struct  UtTraceCfg {
	UtDataHeader       header;
	struct UtTraceCfg  *next;             /* Next trace config command        */
	char               command[1];       /* Start of variable length section */
} UtTraceCfg;

typedef struct UtDeferredConfigInfo {
	char *componentName;
	int32_t all;
	int32_t firstTracePoint;
	int32_t lastTracePoint;
	unsigned char value;
	int level;
	char *groupName;
	struct UtDeferredConfigInfo *next;
	int32_t setActive;
} UtDeferredConfigInfo;

/* The following structures will form the core of the new trace control mechanisms. */
#define UT_TRACE_COMPONENT_DATA "UTCD"
typedef struct UtComponentData {
	UtDataHeader           header;
	char                   *componentName;
	char                   *qualifiedComponentName;
	UtModuleInfo           *moduleInfo;
	int                    tracepointCount;
	int                    numFormats;
	char                   **tracepointFormattingStrings;
	uint64_t                 *tracepointcounters;
	int                    alreadyfailedtoloaddetails;
	char                   *formatStringsFileName;
	struct UtComponentData *prev;
	struct UtComponentData *next;
} UtComponentData;

#define UT_TRACE_COMPONENT_LIST "UTCL"
typedef struct UtComponentList{
	UtDataHeader          header;
	UtComponentData       *head;
	UtDeferredConfigInfo  *deferredConfigInfoHead;
} UtComponentList;

omr_error_t initializeComponentData(UtComponentData **componentDataPtr, UtModuleInfo *moduleInfo, const char *componentName);
void freeComponentData(UtComponentData *componentDataPtr);
omr_error_t initializeComponentList(UtComponentList **componentListPtr);
omr_error_t freeComponentList(UtComponentList *componentList);
omr_error_t addComponentToList(UtComponentData *componentData, UtComponentList *componentList);
omr_error_t removeModuleFromList(UtModuleInfo* module, UtComponentList *componentList);
UtComponentData * getComponentData(const char *componentName, UtComponentList *componentList);
omr_error_t setTracePointsTo(const char *componentName, UtComponentList *componentList, int32_t all, int32_t first, int32_t last, unsigned char value, int level, const char *groupName, BOOLEAN suppressMessages, int32_t setActive);
omr_error_t setTracePointsToParsed(const char *componentName, UtComponentList *componentList, int32_t all, int32_t first, int32_t last, unsigned char value, int level, const char *groupName, BOOLEAN suppressMessages, int32_t setActive);
char * getFormatString(const char *componentName, int32_t tracepoint);
char * getTracePointName(const char *componentName, UtComponentList *componentList, int32_t tracepoint);
omr_error_t setTracePointGroupTo(const char *groupName, UtComponentData *componentData, unsigned char value, BOOLEAN suppressMessages, int32_t setActive);
omr_error_t setTracePointsByLevelTo(UtComponentData *componentData, int level, unsigned char value, int32_t setActive);
omr_error_t processComponentDefferedConfig(UtComponentData *componentData, UtComponentList *componentList);
uint64_t incrementTraceCounter(UtModuleInfo *moduleInfo, UtComponentList *componentList, int32_t tracepoint);
omr_error_t addTraceConfig(UtThreadData **thr, const char *cmd);
omr_error_t addTraceConfigKeyValuePair(UtThreadData **thr, const char *cmdKey, const char *cmdValue);


/*
 * =============================================================================
 *  Trace listener (UTTL)
 * =============================================================================
 */
#define UT_TRACE_LISTENER_NAME "UTTL"
typedef struct  UtTraceListener UtTraceListener;
struct  UtTraceListener {
	UtDataHeader header;
	UtTraceListener *next; /* Next listener */
	UtListenerWrapper listener; /* Listening function */
	void *userData;
};

/*
 * =============================================================================
 * TraceBuffer (UTTB)
 * =============================================================================
 */
#define UT_TRACE_BUFFER_NAME "UTTB"
/* If you update this structure, please update dbgext_uttracebuffer in dbgTrc.c
 * to cope with the changes.
 */
typedef struct  UtTraceBuffer {
	UtDataHeader      header;             /* Eyecatcher, version etc          */
	struct UtTraceBuffer    *next;        /* Next thread/freebuffer           */
	struct UtTraceBuffer    *writeNext;   /* Next buffer on write queue       */
	struct UtTraceBuffer    *globalNext;  /* Next buffer on global queue      */
	volatile int32_t   flags;                /* Flags                            */
	volatile int32_t   lostCount;            /* Lost entry count                 */
	int32_t            bufferType;           /* Buffer type                      */
	UtThreadData    **thr;                /* The thread that last owned this  */
	qMessage          queueData;          /* Internal queue info              */
										  /* This section written to disk     */
	UtTraceRecord     record;             /* Disk record                      */
} UtTraceBuffer;

/*
 * =============================================================================
 *  RAS thread data (UTTD)
 * =============================================================================
 */

#define UT_THREAD_DATA_NAME "UTTD"
/* Structure definition moved to ute.h so that UtThreadData can be allocated and partially
 * initialized outside of UTE when needed
 */

/*
 * =============================================================================
 *  RAS event semaphore - platform dependent portion
 * =============================================================================
 */
#ifndef UT_EVENT_SEM
#define UT_EVENT_SEM omrthread_monitor_t
#endif
typedef struct  UtEventSem_pd {
    UT_EVENT_SEM  sem;
    volatile int flags;
} UtEventSem_pd;
#define UT_SEM_WAITING 1
#define UT_SEM_POSTED  2

/* =============================================================================
 *  RAS event semaphore
 * =============================================================================
 */

typedef struct  UtEventSem {
	UtDataHeader  header;
	UtEventSem_pd pfmInfo;
} UtEventSem;

/*
 * =============================================================================
 *  UT global data (UTGD)
 * =============================================================================
 */

#define UT_GLOBAL_DATA_NAME "UTGD"
/* If you change this structure, please update dbgext_utglobaldata in dbgtrc.c
 * to reflect your changes.
 */
struct  UtGlobalData {
UtDataHeader       header;
const OMR_VM	*vm;                   /* Client identifier               */
J9PortLibrary    *portLibrary;           /* Port Library                    */
uint64_t             startPlatform;          /* Platform timer                  */
uint64_t             startSystem;            /* Time relative 1/1/1970          */
int32_t             snapSequence;           /* Snap sequence number            */
int32_t             bufferSize;             /* Trace buffer size               */
int32_t             traceWrap;              /* Size limit for external trace   */
int32_t             traceGenerations;       /* Number of trace files           */
int32_t             nextGeneration;         /* Next generation of file         */
int32_t             exceptTraceWrap;        /* Limit for exception trace file  */
uint32_t             lostRecords;            /* Lost record counter             */
int32_t             platformTraceStarted;   /* Platform trace active flag      */
int32_t             traceDebug;             /* Trace debug level               */
int32_t             initialSuspendResume;   /* Initial thread suspend count    */
uint32_t             traceSuspend;           /* Global suspend flag             */
uint32_t             traceDisable;           /* Global disable flag             */
volatile uint32_t    traceSnap;              /* Snap in progress                */
int32_t            traceActive;            /* Trace has been activated       ????? - unread, might be useful in dumps */
int32_t            traceInitialized;       /* Trace is initialized            */
int32_t            traceWriteStarted;      /* Trace write thread started      */
int32_t            traceEnabled;           /* Trace enabled at startup        */
int32_t            dynamicBuffers;         /* Dynamic buffering requested     */
int32_t            externalTrace;          /* Trace is being written to disk  */
int32_t            extExceptTrace;         /* Exception trace to disk         */
int32_t            indentPrint;            /* Indent print trace              */
unsigned char      traceDests;             /* Trace destinations              */
omrthread_monitor_t traceLock;              /* Trace monitor pointer           */
int32_t            traceCount;             /* trace counters enabled          */
char              *properties;             /* System properties               */
char              *serviceInfo;            /* Service information             */
char             **ignore;                 /* List of commands to ignore      */
char              *propertyFilePath;       /* Default property file path      */
char              *traceFilename;          /* Trace file name                 */
char              *generationChar;         /* Trace generation character      */
char              *exceptFilename;         /* Exception trace file name       */
char              *traceFormatSpec;        /* Printf template filespec        */
UtThreadData      *exceptionContext;       /* UtThreadData for last excptn    */
UtThreadData      *lastPrint;              /* UtThreadData for last print     */
UtTraceListener   *traceListeners;         /* List of external listeners      */
UtTraceBuffer     *traceGlobal;            /* Queue of all trace buffers      */
UtTraceBuffer     *freeQueue;              /* Free buffer queue               */
qQueue             outputQueue;            /* Buffer queue for external trace */
UtTraceBuffer     *exceptionTrcBuf;        /* Exception trace buffers         */
UtTraceCfg        *config;                 /* Trace selection cmds link/list  */
UtTraceFileHdr    *traceHeader;            /* Trace file header               */
UtComponentList   *componentList;          /* registered or configured component */
UtComponentList   *unloadedComponentList;  /* unloaded component */
volatile uint32_t    threadCount;            /* Number of threads being traced  */
int32_t            traceFinalized;         /* Trace has been finalized        */
intptr_t              snapFile;               /* File descriptor for current snap file */
volatile UtSubscription    *subscribers;            /* List of external trace subscribers */
omrthread_monitor_t subscribersLock;			/* Enforces atomicity of updates to the list of external trace subscribers */
int32_t            traceInCore;            /* If true then we don't queue buffers */
volatile uint32_t    allocatedTraceBuffers;  /* The number of allocated trace buffers ????*/
omrthread_monitor_t threadLock;             /* lock for thread stop */
omrthread_monitor_t freeQueueLock;          /* lock for free queue */
UtSubscription    *tracePointSubscribers;  /* Linked list of tracepoint subscribers */
struct RasTriggerTpidRange *triggerOnTpids;              /* Trace point ranges to fire trigger actions on */
omrthread_monitor_t         triggerOnTpidsWriteMutex;     /* Write access spin lock. */
intptr_t                      triggerOnTpidsReferenceCount; /* Reference count */
struct  RasTriggerGroup    *triggerOnGroups;             /* Trace groups to fire trigger actions on */
omrthread_monitor_t         triggerOnGroupsWriteMutex;    /* Write access spin lock. */
intptr_t                      triggerOnGroupsReferenceCount;/* Reference count */
unsigned int               sleepTimeMillis;              /* Sleep time for the "sleep" trigger action */
int                        fatalassert;                  /* Whether assertion type trace points are fatal or not. */
OMRTraceLanguageInterface  languageIntf;				 /* Language interface */
};

/*
 * =============================================================================
 *   Types used by the native trace engine's tracepoint formatter.
 * =============================================================================
 */

struct UtTracePointIterator {
	UtTraceBuffer *buffer;
	int32_t recordLength;
	uint64_t end;
	uint64_t start;
	uint32_t dataLength;
	uint32_t currentPos;
	uint64_t startPlatform;
	uint64_t startSystem;
	uint64_t endPlatform;
	uint64_t endSystem;
	uint64_t timeConversion;
	uint64_t currentUpperTimeWord;
	int32_t isBigEndian;
	int32_t isCircularBuffer;
	int32_t iteratorHasWrapped;
	char *tempBuffForWrappedTP;
	int32_t processingIncompleteDueToPartialTracePoint;
	uint32_t longTracePointLength;
	uint32_t numberOfBytesInPlatformUDATA;
	uint32_t numberOfBytesInPlatformPtr;
	uint32_t numberOfBytesInPlatformShort;
	J9PortLibrary *portLib;
	FormatStringCallback getFormatStringFn;
};

struct UtTraceFileIterator {
	UtTraceFileHdr* header;
	UtTraceSection* traceSection;
	UtServiceSection* serviceSection;
	UtStartupSection* startupSection;
	UtActiveSection* activeSection;
	UtProcSection* procSection;
	FormatStringCallback getFormatStringFn;
	OMRPortLibrary *portLib;
	intptr_t traceFileHandle;
	intptr_t currentPosition;
};

/*
 * ======================================================================
 *  Trace write thread state data type
 * ======================================================================
 */
typedef struct TraceWorkerData {
	intptr_t        trcFile;
	int64_t         trcSize;
	int64_t         maxTrc;
	intptr_t        exceptFile;
	int64_t         exceptSize;
	int64_t         maxExcept;
} TraceWorkerData;

/*
 * =============================================================================
 *  Internal function prototypes
 * =============================================================================
 */

int subscriptionHandler(void *arg);
void enlistRecordSubscriber(UtSubscription *subscription);
void delistRecordSubscriber(UtSubscription *subscription);
void deleteRecordSubscriber(UtSubscription *subscription);
BOOLEAN findRecordSubscriber(UtSubscription *subscription);
omr_error_t destroyRecordSubscriber(UtThreadData **thr, UtSubscription *subscription);

omr_error_t setupTraceWorkerThread(UtThreadData **thr);
omr_error_t writeBuffer(UtSubscription *subscription);
void cleanupSnapDumpThread(UtSubscription *subscription);
omr_error_t writeSnapBuffer(UtSubscription *subscription);
UtTraceBuffer * queueWrite(UtTraceBuffer *trcBuf, int flags);
omr_error_t initEvent(UtEventSem **sem, char *name);
void destroyEvent(UtEventSem *sem);
void postEvent(UtEventSem *sem);
void postEventAll(UtEventSem *sem);
void waitEvent(UtEventSem *sem);
omr_error_t  setTraceState(const char *cmd, BOOLEAN atRuntime);
intptr_t  openSnap(char *label);
void freeBuffers(qMessage *msg);
char * getNextBracketedParm(const char *from, omr_error_t *rc, int32_t *done, BOOLEAN atRuntime);
void listCounters(void);
void initHeader(UtDataHeader * header, char * name, uintptr_t size);
int32_t getTraceLock(UtThreadData **thr);
int32_t freeTraceLock(UtThreadData **thr);
omr_error_t processEarlyOptions(const char **opts);
omr_error_t processOptions(UtThreadData **thr, const char **opts, BOOLEAN atRuntime);
omr_error_t expandString(char *returnBuffer, const char *original, BOOLEAN atRuntime);
int hexStringLength(const char *str);
int initModuleBlocks(UtThreadData **thr);
int decimalString2Int(const char *decString, int32_t signedAllowed, omr_error_t *rc, BOOLEAN atRuntime);
void getTimestamp(int64_t time, uint32_t* pHours, uint32_t* pMinutes, uint32_t* pSeconds, uint32_t* pMillis);
void incrementRecursionCounter(UtThreadData *thr);
void decrementRecursionCounter(UtThreadData *thr);
omr_error_t initTraceHeader(void);
const char * getPositionalParm(int pos, const char *string, int *size);
int getParmNumber(const char *string);

/** Unpacked trace wrappers. **/
void  twFprintf(const char * fmtTemplate, ...);
omr_error_t  twE2A(char * str);
int32_t  twCompareAndSwap32(volatile uint32_t *target, uint32_t oldV, uint32_t newV);
int32_t  twCompareAndSwapPtr(volatile uintptr_t *target, uintptr_t oldV, uintptr_t newV);
omr_error_t  twThreadAttach(UtThreadData **thr, char *name);
omr_error_t  twThreadDetach(UtThreadData **thr);
UtThreadData ** twThreadSelf(void);

omr_error_t moduleLoaded(UtThreadData **thr, UtModuleInfo *modInfo);
omr_error_t moduleUnLoading(UtThreadData **thr, UtModuleInfo *modInfo);
omr_error_t getComponentGroup(char *name, char *group, int32_t *count, int32_t **tracePts);
omr_error_t registerRecordSubscriber(UtThreadData **thr, char *description, utsSubscriberCallback func, utsSubscriberAlarmCallback alarm, void *userData, UtTraceBuffer *start, UtTraceBuffer *stop, UtSubscription **, int32_t);
omr_error_t deregisterRecordSubscriber(UtThreadData **thr, UtSubscription *subscriptionID);
void internalTrace(UtThreadData **thr, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, ...);
/**************************************************************************
 * name        - reportCommandLineError
 * description - Report an error in a command line option and put the given
 * 			   - string in as detail in an NLS enabled message.
 * parameters  - detailStr, args for formatting into detailStr.
 *************************************************************************/
void reportCommandLineError(BOOLEAN atRuntime, const char* detailStr, ...);
void freeTriggerOptions(J9PortLibrary *portLibrary);
omr_error_t setSleepTime(UtThreadData **thr, const char * str, BOOLEAN atRuntime);
omr_error_t setTriggerActions(UtThreadData **thr, const char * value, BOOLEAN atRuntime);
void clearAllTriggerActions();

/** Functions exposed to the outside world via the server interface and used in rastrace code
 *  outside of main.c
 *  All functions on the server interface (and only functions on the server interface) start
 *  with trc **/
UtTracePointIterator * trcGetTracePointIteratorForBuffer(UtThreadData **thr, const char *bufferName);
omr_error_t trcFreeTracePointIterator(UtThreadData **thr, UtTracePointIterator *iter);
int32_t trcSuspend(UtThreadData **thr, int32_t type);
int32_t trcResume(UtThreadData **thr, int32_t type);
void trcTraceVNoThread(UtModuleInfo *modInfo, uint32_t traceId, const char *spec, va_list varArgs);
omr_error_t trcRegisterRecordSubscriber(UtThreadData **thr, const char *description, utsSubscriberCallback func, utsSubscriberAlarmCallback alarm, void *userData, UtTraceBuffer *start, UtTraceBuffer *stop, UtSubscription **, int32_t);
omr_error_t trcDeregisterRecordSubscriber(UtThreadData **thr, UtSubscription *subscriptionID);

/** Functions exposed to the outside world via the module interface and used outside main.c
 *  All functions on the module interface (and only functions on the module interface) start
 *  with j9 **/
void omrTrace (void *env, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, ...);
void omrTraceMem (void *env, UtModuleInfo *modInfo, uint32_t traceId, uintptr_t length, void *memptr);
void omrTraceState (void *env, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, ...);

/*
 * =============================================================================
 *  Externs
 * =============================================================================
 */

extern UtGlobalData      *utGlobal;
extern omrthread_tls_key_t j9uteTLSKey;
extern omrthread_tls_key_t j9rasTLSKey;

typedef struct J9rasTLS {
    char *appTrace;
    void *external;
} J9rasTLS;


#ifdef  __cplusplus
}
#endif

#endif /* !RASTRACE_INTERNAL_H */


/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#ifndef ibmjvmti_h
#define ibmjvmti_h

/* @ddr_namespace: default */
/* 
 *-----------------------------------------------------------------------------
 * This file defines structures, constants, enums and other
 * definitions which can be used with IBM Corporation's
 * JVMTI extensions. These extensions are available through
 * the JVMTI extension mechanism.
 * See GetExtensionEvents(), GetExtensionFunctions() and
 * SetExtensionEventCallback()
 *-----------------------------------------------------------------------------
 */


#include "jvmti.h"

/* 
 *-----------------------------------------------------------------------------
 * Extended JVMTI constants
 *-----------------------------------------------------------------------------
 */

#define COM_IBM_COMPILING_START "com.ibm.CompilingStart"
#define COM_IBM_COMPILING_END "com.ibm.CompilingEnd"

#define COM_IBM_INSTRUMENTABLE_OBJECT_ALLOC "com.ibm.InstrumentableObjectAlloc"

#define COM_IBM_SET_VM_TRACE "com.ibm.SetVmTrace"
#define COM_IBM_SET_VM_DUMP "com.ibm.SetVmDump"
#define COM_IBM_RESET_VM_DUMP "com.ibm.ResetVmDump"
#define COM_IBM_TRIGGER_VM_DUMP "com.ibm.TriggerVmDump"
#define COM_IBM_VM_DUMP_START "com.ibm.VmDumpStart"
#define COM_IBM_VM_DUMP_END "com.ibm.VmDumpEnd"
#define COM_IBM_QUERY_VM_DUMP "com.ibm.QueryVmDump"

#define COM_IBM_QUERY_VM_LOG_OPTIONS "com.ibm.QueryVmLogOptions"
#define COM_IBM_SET_VM_LOG_OPTIONS "com.ibm.SetVmLogOptions"

#define COM_IBM_SET_VM_JLM "com.ibm.SetVmJlm"
#define COM_IBM_SET_VM_JLM_DUMP "com.ibm.SetVmJlmDump"

#define COM_IBM_GET_OS_THREAD_ID "com.ibm.GetOSThreadID"

#define COM_IBM_GET_STACK_TRACE_EXTENDED "com.ibm.GetStackTraceExtended"
#define COM_IBM_GET_ALL_STACK_TRACES_EXTENDED "com.ibm.GetAllStackTracesExtended"
#define COM_IBM_GET_THREAD_LIST_STACK_TRACES_EXTENDED "com.ibm.GetThreadListStackTracesExtended"

#define COM_IBM_GARBAGE_COLLECTION_CYCLE_START "com.ibm.GarbageCollectionCycleStart"
#define COM_IBM_GARBAGE_COLLECTION_CYCLE_FINISH "com.ibm.GarbageCollectionCycleFinish"

#define COM_IBM_GET_HEAP_FREE_MEMORY "com.ibm.GetHeapFreeMemory"
#define COM_IBM_GET_HEAP_TOTAL_MEMORY "com.ibm.GetHeapTotalMemory"

#define COM_IBM_ITERATE_SHARED_CACHES "com.ibm.IterateSharedCaches"
#define COM_IBM_DESTROY_SHARED_CACHE "com.ibm.DestroySharedCache"

#define COM_IBM_REMOVE_ALL_TAGS   "com.ibm.RemoveAllTags"

#define COM_IBM_REGISTER_TRACE_SUBSCRIBER "com.ibm.RegisterTraceSubscriber"
#define COM_IBM_DEREGISTER_TRACE_SUBSCRIBER "com.ibm.DeregisterTraceSubscriber"
#define COM_IBM_FLUSH_TRACE_DATA "com.ibm.FlushTraceData"
#define COM_IBM_GET_TRACE_METADATA "com.ibm.GetTraceMetadata"

#define COM_IBM_GET_METHOD_AND_CLASS_NAMES "com.ibm.GetMethodAndClassNames"

#define COM_IBM_JLM_DUMP_STATS "com.ibm.JlmDumpStats"

#define COM_IBM_GET_MEMORY_CATEGORIES "com.ibm.GetMemoryCategories"

#define COM_IBM_REGISTER_VERBOSEGC_SUBSCRIBER "com.ibm.RegisterVerboseGCSubscriber"
#define COM_IBM_DEREGISTER_VERBOSEGC_SUBSCRIBER "com.ibm.DeregisterVerboseGCSubscriber"

#define COM_IBM_GET_J9VMTHREAD "com.ibm.GetJ9vmThread"
#define COM_IBM_GET_J9METHOD "com.ibm.GetJ9method"

#define COM_IBM_REGISTER_TRACEPOINT_SUBSCRIBER "com.ibm.RegisterTracePointSubscriber"
#define COM_IBM_DEREGISTER_TRACEPOINT_SUBSCRIBER "com.ibm.DeregisterTracePointSubscriber"

#define COM_IBM_SHARED_CACHE_MODLEVEL_JAVA5 1
#define COM_IBM_SHARED_CACHE_MODLEVEL_JAVA6 2
#define COM_IBM_SHARED_CACHE_MODLEVEL_JAVA7 3
#define COM_IBM_SHARED_CACHE_MODLEVEL_JAVA8 4
#define COM_IBM_SHARED_CACHE_MODLEVEL_JAVA9 5
/* 
 * No macro is defined for shared cache modLevel starting from Java 10. The value of modLevel equals to the java version number
 * on which the shared cache is created.
 */

#define COM_IBM_SHARED_CACHE_ADDRMODE_32 32
#define COM_IBM_SHARED_CACHE_ADDRMODE_64 64

/*
 * Constants for flags field of COM_IBM_ITERATE_SHARED_CACHES.
 * Users should always pass one of the following values to the flags field of  COM_IBM_ITERATE_SHARED_CACHES.
 */
#define COM_IBM_ITERATE_SHARED_CACHES_NO_FLAGS 0

/*
 *-----------------------------------------------------------------------------
 * Extended cacheType constants for COM_IBM_DESTROY_SHARED_CACHE
 *-----------------------------------------------------------------------------
 */
#define COM_IBM_SHARED_CACHE_PERSISTENCE_DEFAULT 0
#define COM_IBM_SHARED_CACHE_PERSISTENT 1
#define COM_IBM_SHARED_CACHE_NONPERSISTENT 2
#define COM_IBM_SHARED_CACHE_SNAPSHOT 5

/*
 *-----------------------------------------------------------------------------
 * Return type for COM_IBM_DESTROY_SHARED_CACHE
 *-----------------------------------------------------------------------------
 */
/* When no cache exists or successfully destroyed all caches. */
#define COM_IBM_DESTROYED_ALL_CACHE					0
/* When failed to destroy any cache. */
#define COM_IBM_DESTROYED_NONE						-1
/* When failed to destroy cache of current generation. */
#define COM_IBM_DESTROY_FAILED_CURRENT_GEN_CACHEE	-2
/* When failed to destroy one or more older generation cache and either current generation cache does not exists or is successfully destroyed */
#define COM_IBM_DESTROY_FAILED_OLDER_GEN_CACHE		-3

/*
 *-----------------------------------------------------------------------------
 * Extended JVMTI enumerations
 *-----------------------------------------------------------------------------
 */
enum {
	COM_IBM_METHOD_ENTRY_EXTENDED_INTERPRETED = 0,
	COM_IBM_METHOD_ENTRY_EXTENDED_COMPILED = 1,
	COM_IBM_METHOD_ENTRY_EXTENDED_NATIVE = 2,
	COM_IBM_METHOD_ENTRY_EXTENDED_PARTIAL_IN_LINE = 3,
	COM_IBM_METHOD_ENTRY_EXTENDED_IN_LINE = 4
};

enum {
	COM_IBM_JLM_START = 0,
	COM_IBM_JLM_START_TIME_STAMP = 1,
	COM_IBM_JLM_STOP = 2,
	COM_IBM_JLM_STOP_TIME_STAMP = 3
};

enum {
	COM_IBM_STACK_FRAME_EXTENDED_NOT_JITTED = 0,
	COM_IBM_STACK_FRAME_EXTENDED_JITTED     = 1
};

/**
 * Bits used to select the type of data to be returned by the extended stack trace calls 
 */
enum {
	COM_IBM_GET_STACK_TRACE_PRUNE_UNREPORTED_METHODS	= 1,	/** Prunes methods for which method enter was not reported */
	COM_IBM_GET_STACK_TRACE_ENTRY_LOCAL_STORAGE			= 2,	/** Returns ELS pointers */
	COM_IBM_GET_STACK_TRACE_EXTRA_FRAME_INFO			= 4		/** Returns jitted vs non-jitted data */
};


/**
 * JlmDumpStats format specifiers
 */ 

enum {
	COM_IBM_JLM_DUMP_FORMAT_OBJECT_ID = 0,
	COM_IBM_JLM_DUMP_FORMAT_TAGS      = 1
};


#define JVMTI_MONITOR_JAVA		0x01
#define JVMTI_MONITOR_RAW		0x02


/*
 *-----------------------------------------------------------------------------
 * Extended JVMTI function types
 *-----------------------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------
 * Extended JVMTI structure types
 *-----------------------------------------------------------------------------
 */

typedef struct jlm_dump {
	char * begin;
	char * end;
} jlm_dump;

/*  JLM dump format
 *
 *  All entries are in  packed big endian format
 *	u1	monitor type
 *
 *   	1	Java monitor
 *   		jobjectID	object
 *   		JNIEnv *	owner thread
 *  		u4    		entry count
 *  		u4    		# of threads waiting to enter
 *  		[JNIEnv *]*	threads waiting to enter
 *  		u4   		# of threads waiting to be notified
 *  		[JNIEnv *]*	threads waiting to be notified
 *
 *  	2	raw monitor
 *  		char *    	name
 *  		RawMonitor	raw monitor
 *  		JNIEnv * 	owner thread
 *  		u4    		entry count
 *  		u4   		# of threads waiting to enter
 *  		[JNIEnv *]*	threads waiting to enter
 *  		u4  		# of threads waiting to be notified
 *  		[JNIEnv *]*	threads waiting to be notified
 */

/**
 * struct jvmtiFrameInfoExtended
 * Has two additional fields, compared to struct jvmtiFrameInfo:
 * machinepc and type
 */  
typedef struct jvmtiFrameInfoExtended {
	jmethodID method;
	jlocation location;
	jlocation machinepc;
	jint      type;                 /*!< frame type can be not jitted or jitted */
	void    * nativeFrameAddress;   /*!< address of the native frame */
} jvmtiFrameInfoExtended;

/**
 * struct jvmtiStackInfoExtended
 * frame buffer is a pointer to jvmtiFrameInfoExtended,
 * whereas frame buffer is pointer to jvmtiFrameInfo 
 * in struct jvmtiStackInfo
 */  
typedef struct jvmtiStackInfoExtended {
	jthread thread;
	jint state;
	jvmtiFrameInfoExtended* frame_buffer;
	jint frame_count;
} jvmtiStackInfoExtended;

/**
 * Version information for the COM_IBM_ITERATE_SHARED_CACHES API. Users
 * should pass this value as the "version" parameter. If the jvmtiSharedCacheInfo
 * structure is expanded, IBM will increment this define.
 */ 
#define COM_IBM_ITERATE_SHARED_CACHES_VERSION_1 1
#define COM_IBM_ITERATE_SHARED_CACHES_VERSION_2 2
#define COM_IBM_ITERATE_SHARED_CACHES_VERSION_3 3
#define COM_IBM_ITERATE_SHARED_CACHES_VERSION_4 4

/**
 * The following 5 macros can be used to get address mode and compressedRefs mode from jvmtiSharedCacheInfo.addrMode when COM_IBM_ITERATE_SHARED_CACHES_VERSION_3 or later is specified.
 */
#define COM_IBM_ITERATE_SHARED_CACHES_UNKNOWN_COMPRESSED_POINTERS_MODE 0x00000
#define COM_IBM_ITERATE_SHARED_CACHES_COMPRESSED_POINTERS_MODE	0x10000
#define COM_IBM_ITERATE_SHARED_CACHES_NON_COMPRESSED_POINTERS_MODE 0x20000
#define COM_IBM_ITERATE_SHARED_CACHES_GET_ADDR_MODE(value) (value & 0xFFFF)
#define COM_IBM_ITERATE_SHARED_CACHES_GET_CMPRSSREF_MODE(value) (value & (COM_IBM_ITERATE_SHARED_CACHES_COMPRESSED_POINTERS_MODE | COM_IBM_ITERATE_SHARED_CACHES_NON_COMPRESSED_POINTERS_MODE))

/**
 * struct jvmtiSharedCacheInfo
 * name - the name of the cache
 * isCompatible - is the cache compatible with this JVM
 * isPersistent - true if the cache is persistent cache, false if the cache is non-persistent or cache snapshot
 * os_shmid - Operating System specific shared memory id
 * os_semid - Operating System specific semaphore id
 * modLevel - the modification level
 * addrMode - the address mode of the JVM. This field includes additional information on whether it is a 64-bit compressedRefs cache when COM_IBM_ITERATE_SHARED_CACHES_VERSION_3 or later is specified
 * isCorrupt - true when the cache is found to be corrupt
 * cacheSize - the size of the cache
 * freeBytes - the size of free space in the cache
 * lastDetach - time from which last detach has happened
 * cacheType - the type of the cache. This is the new field included when COM_IBM_ITERATE_SHARED_CACHES_VERSION_2 or later is specified
 * softMaxBytes - the soft limit for the available space in the cache. This is the new field included when COM_IBM_ITERATE_SHARED_CACHES_VERSION_4 or later is specified
 * 
 * If IBM adds new information to this structure, it will be added
 * to the end to preserve backwards compatibility, and
 * COM_IBM_ITERATE_SHARED_CACHES_VERSION will be incremented.
 */
typedef struct jvmtiSharedCacheInfo {
	const char *name;
	jboolean isCompatible;
	jboolean isPersistent;
	jint os_shmid;
	jint os_semid;
	jint modLevel;
	jint addrMode;
	jboolean isCorrupt;
	jlong cacheSize;
	jlong freeBytes;
	jlong lastDetach;
	jint cacheType;
	jlong softMaxBytes;
} jvmtiSharedCacheInfo;

/**
 * Signature of callback function which must be provided to COM_IBM_ITERATE_SHARED_CACHES.
 * 
 * When your callback is called you will be provided with:
 * env - the jvmtiEnv you supplied when calling COM_IBM_ITERATE_SHARED_CACHES
 * cache_info - a structure containing information about a shared cache
 * user_data - the user_data you provided to COM_IBM_ITERATE_SHARED_CACHES
 * 
 * Return - JNI_OK on success, JNI_ERR on failure (no more callbacks will be sent).
 */
typedef jint (JNICALL *jvmtiIterateSharedCachesCallback)(
		jvmtiEnv *env, jvmtiSharedCacheInfo *cache_info, void *user_data);

/**
 * struct jvmtiObjectRenameInfo
 * oldAutoTag is the previous automatic tag.
 * newAutoTag is the new automatic tag. 
 * The value 0 means that the object is deleted.
 */  
typedef struct jvmtiObjectRenameInfo {
    jlong oldAutoTag;
    jlong newAutoTag;
} jvmtiObjectRenameInfo;

/*
 * Trace subscriber callback function. This function will be passed records containing trace
 * data as they are processed. This provides a mechanism for a user defined transport and is
 * unrelated to actual formatting of the trace data.
 * env - the jvmti env supplied when the subscriber was registered
 * record   - the record
 * length - the size of the record
 * userData - user data provided at registration
 * return:
 *    JVMTI_ERROR_NONE - success
 *    any other value - the subscriber is deregistered and will not receive any more data
 *    
 */
typedef jvmtiError (*jvmtiTraceSubscriber)(jvmtiEnv *env, void *record, jlong length, void *userData);

/*
 * Alarm callback function that's executed if the subscriber returns an error or misbehaves.
 * env - the jvmti env supplied when the subscriber was registered
 * subscriptionID - the ID of the subscription that raised the alarm
 * userData - user data provided at registration
 */
typedef void (*jvmtiTraceAlarm)(jvmtiEnv *env, void *subscriptionID, void *userData);


	
/*
 * Return data for the jvmtiGetMethodClassPackageNames extension. Keeps track of class, 
 * method and package name pointers copied into the user supplied buffer. 
 */
typedef	struct jvmtiExtensionRamMethodData
{
	jchar * className;           /* ptr into ramMethodStrings, a null terminated class name string */
	jchar * methodName;          /* ptr into ramMethodStrings, a null terminated method name string */
	jvmtiError reasonCode;       /* error code indicating why this record has not been returned back 
									(out of memory, invalid method or none) */
} jvmtiExtensionRamMethodData;


/**
 * Version information for the GetMemoryCategories API. Users
 * should pass this value as the "version" parameter. If the jvmtiMemoryCategory
 * structure is expanded, IBM will increment this define.
 */
#define COM_IBM_GET_MEMORY_CATEGORIES_VERSION_1 1

/*
 * Return data for the GetMemoryCategories API
 */
typedef struct jvmtiMemoryCategory {
	/* Category name */
	const char * name;

	/* Bytes allocated under this category */
	jlong liveBytesShallow;

	/* Bytes allocated under this category and all child categories */
	jlong liveBytesDeep;

	/* Number of allocations under this category */
	jlong liveAllocationsShallow;

	/* Number of allocations under this category and all child categories */
	jlong liveAllocationsDeep;

	/* Pointer to the first child category (NULL if this node has no children) */
	struct jvmtiMemoryCategory * firstChild;

	/* Pointer to the next sibling category (NULL if this node has no next sibling)*/
	struct jvmtiMemoryCategory * nextSibling;

	/* Pointer to the parent category. (NULL if this node is a root) */
	struct jvmtiMemoryCategory * parent;
} jvmtiMemoryCategory;

/*
 * Verbose GC subscriber callback function. This function will be passed records containing verbose GC
 * data as it is processed. The data is in ASCII XML. The data is valid for the duration of the callback.
 * If the subscriber wishes to save the data, it must copy it elsewhere.
 *
 * Note that the callback is called while the VM is stopped, thus the callback must not use JNI functions 
 * and must not use JVMTI functions except those which specifically allow such use. 
 *
 * @param env[in] the jvmti env supplied when the subscriber was registered
 * @param record[in] the XML data
 * @param length[in] the size of the record, in bytes. Does not include any NUL terminator, if any.
 * @param userData[in] user data provided at registration
 * @return JVMTI_ERROR_NONE on success
 * If an error is returned, the alarm callback is called, the subscriber is deregistered, and no more data is sent
 */
typedef jvmtiError (*jvmtiVerboseGCSubscriber)(jvmtiEnv *env, const char *record, jlong length, void *userData);

/*
 * Alarm callback function that's executed if the subscriber returns an error or misbehaves.
 * @param env[in] the jvmti env supplied when the subscriber was registered
 * @param subscriptionID[in] the ID of the subscription that raised the alarm
 * @param userData[in] user data provided at registration
 */
typedef void (*jvmtiVerboseGCAlarm)(jvmtiEnv *env, void *subscriptionID, void *userData);


#endif     /* ibmjvmti_h */

/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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


#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "omrcfg.h"
#include "j9user.h"
#include "j9.h"
#include "cfr.h"
#include "j9port.h"
#include "j9protos.h"
#include "j9consts.h"
#include "jni.h"
#include "ibmjvmti.h"
#include "j9cp.h"
#include "rommeth.h"
#include "stackwalk.h"
#include "jlm.h"
#include "util_internal.h"
#include "jvmtiInternal.h"


/*
 * Monitor name defines for the JLM Report
 * 
 * Example object monitor name: "[003257F8] java/lang/Object@1342BD58(Object)"
 *
 */
#define OBJ_MON_NAME_FORMAT "[%p] %.*s@%p (%s)"
#define OBJ_MON_NAME_POINTER_SIZE  (sizeof(void*) * 2)
#define	OBJ_MON_NAME_CLASS_NAME_SIZE	(128)
#define OBJ_MON_NAME_OBJ_TYPE_SIZE (10)
/* big enough to contain OBJ_MON_NAME_FORMAT, plus 6 for a bit of leeway */
#define OBJ_MON_NAME_BUF_MINIMUM_SIZE  (1 + OBJ_MON_NAME_POINTER_SIZE + 1 + 1 + \
							   	   	   OBJ_MON_NAME_CLASS_NAME_SIZE + 1 + OBJ_MON_NAME_POINTER_SIZE + 1 + \
							   	   	   	  1 + OBJ_MON_NAME_OBJ_TYPE_SIZE + 1 + 1 + 6)

#define OBJ_MON_NAME_BUF_SIZE  OBJ_MON_NAME_BUF_MINIMUM_SIZE

/* ENDIAN_HELPERS */

#ifndef SWAP_2BYTES
#define SWAP_2BYTES(v)  (\
						(((v) >> (8*1)) & 0x00FF) | \
						(((v) << (8*1)) & 0xFF00)   \
						)
#endif

#ifndef SWAP_4BYTES
#define SWAP_4BYTES(v) (\
					   (((v) >> (8*3)) & 0x000000FF) | \
					   (((v) >> (8*1)) & 0x0000FF00) | \
					   (((v) << (8*1)) & 0x00FF0000) | \
					   (((v) << (8*3)) & 0xFF000000)   \
					   )
#endif

#ifndef SWAP_8BYTES
#define SWAP_8BYTES(v) (\
						(((v) >> (8*7)) & 0x000000FF) | \
						(((v) >> (8*5)) & 0x0000FF00) | \
						(((v) >> (8*3)) & 0x00FF0000) | \
						(((v) >> (8*1)) & 0xFF000000) | \
						(((v) & 0xFF000000) << (8*1)) | \
						(((v) & 0x00FF0000) << (8*3)) | \
						(((v) & 0x0000FF00) << (8*5)) | \
						(((v) & 0x000000FF) << (8*7)) \
					   )
#endif

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define FORCE_BIG_ENDIAN_2BYTES(val) SWAP_2BYTES((val))
#define FORCE_BIG_ENDIAN_4BYTES(val) SWAP_4BYTES((val))
#define FORCE_BIG_ENDIAN_8BYTES(val) SWAP_8BYTES((U_64)(val))
#else
#define FORCE_BIG_ENDIAN_2BYTES(val) (val)
#define FORCE_BIG_ENDIAN_4BYTES(val) (val)
#define FORCE_BIG_ENDIAN_8BYTES(val) (val)
#endif



/* JPROFBUF_HELPERS */

#define WRITE_1BYTE(val)  *((U_8*) dump) = (val);                        dump++;
#define WRITE_2BYTES(val) *((U_16*)dump) = FORCE_BIG_ENDIAN_2BYTES(val); dump += sizeof(U_16);
#define WRITE_4BYTES(val) *((U_32*)dump) = FORCE_BIG_ENDIAN_4BYTES((U_32)val); dump += sizeof(U_32);
#define WRITE_8BYTES(val) *((U_64*)dump) = FORCE_BIG_ENDIAN_8BYTES(val); dump += sizeof(U_64);


/* Dump Format defs */
/* 1 byte raw/Java + 1 held + 4 enter + 4 slow + 4 recursive + 4 spin2 + 4 yield + 8 hold time = 30  */
#define JLM_DUMP_COUNT_FIELD_SIZE 30
/* 2 integer fields */ 
#define JLM_DUMP_FORMAT_SIZE       8
/* version */
#define JLM_DUMP_VERSION           1


static void GetMonitorName (J9VMThread *vmThread, J9ThreadAbstractMonitor *monitor, char *nameBuf);


jint 
request_MonitorJlmDump(jvmtiEnv * env, J9VMJlmDump * jlmd, jint dump_format)
{
#if	defined(OMR_THR_JLM)
	J9VMThread * vmThread;
	J9ThreadAbstractMonitor * monitor;
	J9ThreadMonitorTracing  * tracing;
	omrthread_monitor_walk_state_t walkState;
	unsigned char held;
	char * dump;
	J9JavaVM * jvm = JAVAVM_FROM_ENV(env);
	char monitor_name[OBJ_MON_NAME_BUF_SIZE];
	J9MemoryManagerFunctions * memoryManagerFunctions = jvm->memoryManagerFunctions;
	J9ThreadMonitorTracing *lnrl_lock = NULL;
	pool_state j9gc_LWNRLock_walk_state = { 0 };

	monitor = NULL;
	vmThread = jvm->internalVMFunctions->currentVMThread(jvm);

	/* Assumes acquireExclusiveVMAccess & release by caller */
	/* Assumes caller has allocated the memory for the dump and is pointed to by jlmd->begin */

	dump = jlmd->begin;
	held = 1;

	/* Write the header fields if not the original format */
	if (dump_format != COM_IBM_JLM_DUMP_FORMAT_OBJECT_ID) {
		WRITE_4BYTES(JLM_DUMP_VERSION);
		WRITE_4BYTES(dump_format);
	}

	monitor = NULL;
	/* walk all the monitors to copy data into monitor_dump data */
	omrthread_monitor_init_walk(&walkState);
	while (NULL != (monitor = (J9ThreadAbstractMonitor *) omrthread_monitor_walk_no_locking(&walkState))) {
		if (monitor->tracing) {
			tracing = monitor->tracing;

			if ( monitor->flags & J9THREAD_MONITOR_OBJECT ) {
				WRITE_1BYTE(JVMTI_MONITOR_JAVA);
			} else {
				WRITE_1BYTE(JVMTI_MONITOR_RAW);
			}

			WRITE_1BYTE((unsigned char)held);
			WRITE_4BYTES(tracing->enter_count);
			WRITE_4BYTES(tracing->slow_count);
			WRITE_4BYTES(tracing->recursive_count);
			WRITE_4BYTES(tracing->spin2_count);
			WRITE_4BYTES(tracing->yield_count);
#if defined(OMR_THR_JLM_HOLD_TIMES)
			WRITE_8BYTES(tracing->holdtime_sum);
#else
			WRITE_8BYTES(0);
#endif

			/* If format with tags is required, write the tag (8 bytes),
			   otherwise write 0 in the objectid field - 4 or 8 bytes */
			if (dump_format == COM_IBM_JLM_DUMP_FORMAT_TAGS) {
				jlong tag = 0;
					if (monitor->flags & J9THREAD_MONITOR_OBJECT) {
					j9object_t object = J9MONITORTABLE_OBJECT_LOAD(vmThread, &monitor->userData);
						/* Similar to jvmtiGetTag code, but with object being of object_t type */
						if (object != NULL) {							
						J9JVMTIObjectTag   entry;
						J9JVMTIObjectTag * objectTag;

						entry.ref = object;

						/* No need to check if entry.ref != NULL, since we checked object above */

						/* Ensure exclusive access to tag table */
						omrthread_monitor_enter(((J9JVMTIEnv *)env)->mutex);

						objectTag = hashTableFind(((J9JVMTIEnv *)env)->objectTagTable, &entry);
						if (objectTag) {
							tag = objectTag->tag;
						}
						omrthread_monitor_exit(((J9JVMTIEnv *)env)->mutex);
					}
				}

				WRITE_8BYTES(tag);

			} else {
				/* The next field has a pointer size */
				if (sizeof(void *) == 4) {
					WRITE_4BYTES(0);
				} else {
					WRITE_8BYTES(0);
				}
			}

			GetMonitorName(vmThread, monitor, monitor_name);
			strcpy(dump, monitor_name);

			dump += strlen((const char *)dump) + 1;
		}
	}

	/*
	 * Write lock's name and statistics for each LightweightNonreentrantLock
	 * @note omrgc_walkLWNRLockTracePool locks the pool and unlocks it after iterating all elements.
	 */
	while (NULL != (lnrl_lock = memoryManagerFunctions->omrgc_walkLWNRLockTracePool(jvm->omrVM, &j9gc_LWNRLock_walk_state))) {
		WRITE_1BYTE(JVMTI_MONITOR_RAW);
		WRITE_1BYTE((unsigned char)held);
		WRITE_4BYTES(lnrl_lock->enter_count);
		WRITE_4BYTES(lnrl_lock->slow_count);
		WRITE_4BYTES(lnrl_lock->recursive_count);
		WRITE_4BYTES(lnrl_lock->spin2_count);
		WRITE_4BYTES(lnrl_lock->yield_count);
#if defined(OMR_THR_JLM_HOLD_TIMES)
		WRITE_8BYTES(lnrl_lock->holdtime_sum);
#else
		WRITE_8BYTES(0);
#endif /* defined(OMR_THR_JLM_HOLD_TIMES) */

		if (dump_format == COM_IBM_JLM_DUMP_FORMAT_TAGS) {
			WRITE_8BYTES(0);
		} else {
			/* The next field has a pointer size */
			if (sizeof(void *) == 8) {
				WRITE_8BYTES(0);
			} else {
				WRITE_4BYTES(0);
			}
		}

		strcpy(dump, lnrl_lock->monitor_name);
		dump += strlen(lnrl_lock->monitor_name) + 1;
	}
	return (jint) JLM_SUCCESS;
#else
	return (jint) JLM_NOT_AVAILABLE;
#endif /* defined(OMR_THR_JLM) */
}


jint
JlmStart(J9VMThread* vmThread)
{
#if	defined(OMR_THR_JLM)
	return (omrthread_jlm_init(J9THREAD_LIB_FLAG_JLM_ENABLED) == 0) ? (jint) JLM_SUCCESS : (jint) JLM_NOT_AVAILABLE;
#else
	return (jint) JLM_NOT_AVAILABLE;
#endif
}


jint 
JlmStop(void)
{
#if	defined(OMR_THR_JLM)
	omrthread_lib_clear_flags(J9THREAD_LIB_FLAG_JLM_ENABLED_ALL);
	return (jint) JLM_SUCCESS;
#else
	return (jint) JLM_NOT_AVAILABLE;
#endif
}


jint 
JlmStartTimeStamps(void)
{
#if	defined(OMR_THR_JLM_HOLD_TIMES)
	return (omrthread_jlm_init(J9THREAD_LIB_FLAG_JLM_ENABLED | J9THREAD_LIB_FLAG_JLM_TIME_STAMPS_ENABLED) == 0) ? (jint) JLM_SUCCESS : (jint) JLM_NOT_AVAILABLE;
#else
	return (jint) JLM_NOT_AVAILABLE;
#endif
}


jint 
JlmStopTimeStamps(void)
{
#if defined(OMR_THR_JLM_HOLD_TIMES)
	omrthread_lib_clear_flags(J9THREAD_LIB_FLAG_JLM_TIME_STAMPS_ENABLED);
	return (jint) JLM_SUCCESS;
#else
	return (jint) JLM_NOT_AVAILABLE;
#endif
}



static void 
GetMonitorName(J9VMThread *vmThread, J9ThreadAbstractMonitor *monitor, char *nameBuf)
{
	PORT_ACCESS_FROM_VMC(vmThread);

	if (monitor->flags & J9THREAD_MONITOR_OBJECT ) {
		j9object_t object = J9MONITORTABLE_OBJECT_LOAD(vmThread, &monitor->userData);
		J9Class *clazz;
		char* objType;
		J9ROMClass *romClass;
		J9UTF8* className;
		char* name;
		UDATA length;
		BOOLEAN free = FALSE;
		
		if (J9VM_IS_INITIALIZED_HEAPCLASS(vmThread, object)) {
			clazz = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, object);
			objType = "Class";
		} else {
			clazz = J9OBJECT_CLAZZ(vmThread, object);
			objType = "Object";
		}
		romClass = clazz->romClass;
		
		/* Default case: non-array class */
		className = J9ROMCLASS_CLASSNAME(romClass);
		name = (char *) J9UTF8_DATA(className);
		length = J9UTF8_LENGTH(className);

		if (J9ROMCLASS_IS_ARRAY(romClass)) {
			J9ArrayClass* arrayClass = (J9ArrayClass*) clazz;
			J9Class * leafType = arrayClass->leafComponentType;
			UDATA arity = arrayClass->arity;
			length = arity;

			if (J9ROMCLASS_IS_PRIMITIVE_TYPE(leafType->romClass)) {
				length += 1; /* +1 for primitive sig char */
			} else {
				className = J9ROMCLASS_CLASSNAME(leafType->romClass);
				length += J9UTF8_LENGTH(className) + 2; /* +2 for the L and ; */
			}
			name = j9mem_allocate_memory(sizeof(char) * length + 1, OMRMEM_CATEGORY_VM);
			/* If allocate fails, fall back to old behaviour and show the array as '[30B7BE78] [L@B0089D08 (Object)' */
			if (name){
				memset(name, '[', arity);
				if (J9ROMCLASS_IS_PRIMITIVE_TYPE(leafType->romClass)) {
					name[arity] = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(leafType->arrayClass->romClass))[1];
				} else {
					name[arity] = 'L';
					memcpy(name + arity + 1, J9UTF8_DATA(className), J9UTF8_LENGTH(className));
					name[length - 1] = ';';
				}
				name[length] = '\0';
				free = TRUE;
			}
		}


		 j9str_printf(PORTLIB, nameBuf, OBJ_MON_NAME_BUF_SIZE,
				OBJ_MON_NAME_FORMAT,
				monitor,
				(length < OBJ_MON_NAME_CLASS_NAME_SIZE) ? length : OBJ_MON_NAME_CLASS_NAME_SIZE, 
				name, 
				object, objType);



		if (free){
			j9mem_free_memory(name);
		}
	} else {
		j9str_printf(PORTLIB, nameBuf, OBJ_MON_NAME_BUF_SIZE, "[%p] %s", monitor, omrthread_monitor_get_name((omrthread_monitor_t) monitor));
	}
}


jint request_MonitorJlmDumpSize(J9JavaVM * jvm, UDATA * dump_size, jint dump_format)
{
#if	defined(OMR_THR_JLM)
{
	J9VMThread * vmThread;
	J9ThreadAbstractMonitor * monitor;
	omrthread_monitor_walk_state_t walkState;
	char monitor_name[OBJ_MON_NAME_BUF_SIZE];
	jint rc = (jint) JLM_SUCCESS;
	int objIDfieldSize;
	J9MemoryManagerFunctions * memoryManagerFunctions = jvm->memoryManagerFunctions;
	J9ThreadMonitorTracing *lnrl_lock = NULL;
	pool_state j9gc_LWNRLock_walk_state = { 0 };

	if (! (omrthread_lib_get_flags() & J9THREAD_LIB_FLAG_JLM_HAS_BEEN_ENABLED)) {
		/* must not be called if JLM has never been enabled */
		return (jint) JLM_NOT_AVAILABLE;
	}

	monitor = NULL;
	vmThread = jvm->internalVMFunctions->currentVMThread(jvm);

	/* Count the header fields if not the original format */
	if (dump_format != COM_IBM_JLM_DUMP_FORMAT_OBJECT_ID) {
		*dump_size    = JLM_DUMP_FORMAT_SIZE;
		objIDfieldSize = 8;
	} else {
		*dump_size = 0;
		objIDfieldSize = sizeof(void *);
	}

    /* Assumes acquireExclusiveVMAccess & release by caller */
	/* walk all the monitors to count them */
	omrthread_monitor_init_walk(&walkState);
	while ( NULL != (monitor = (J9ThreadAbstractMonitor *) omrthread_monitor_walk_no_locking(&walkState)) ) {
		if (monitor->tracing) {
				GetMonitorName(vmThread, monitor, monitor_name);
				*dump_size += JLM_DUMP_COUNT_FIELD_SIZE + objIDfieldSize + strlen(monitor_name)+1;
		}
	}

	/*
	 * calculate the dump size for GC's LightweightNonReentrantLocks
	 * @note omrgc_walkLWNRLockTracePool locks the pool and unlocks it after iterating all elements.
	 */
	while (NULL != (lnrl_lock = memoryManagerFunctions->omrgc_walkLWNRLockTracePool(jvm->omrVM, &j9gc_LWNRLock_walk_state))) {
		*dump_size += JLM_DUMP_COUNT_FIELD_SIZE + objIDfieldSize + strlen(lnrl_lock->monitor_name) + 1;
	}

	return rc;
}
#else
        return (jint) JLM_NOT_AVAILABLE;
#endif /* defined(OMR_THR_JLM) */
}

/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "jni.h"

#include "j9dbgext.h"
#include "j9protos.h"
#include "j9port.h"
#include "jextractnatives_internal.h"
#include "j9version.h"

#include <stdarg.h>
#include <stdlib.h>

#define CACHE_SIZE 1024

typedef struct {
	UDATA address;
	U_8 data[4096];
} dbgCacheElement;

static JNIEnv *globalEnv;
static jobject globalDumpObj;
static jmethodID globalGetMemMid;
static jmethodID globalFindPatternMid;
static dbgCacheElement cache[CACHE_SIZE];

static jint cacheIDs (JNIEnv* env, jobject dumpObj);
static jboolean validateDump (JNIEnv *env);
static jint callFindPattern (U_8* pattern, jint patternLength, jint patternAlignment, jlong startSearchFrom, jlong* resultP);
static void callGetMemoryBytes(UDATA address, void *structure, UDATA size, UDATA *bytesRead);
static void readCachedMemory(UDATA address, void *structure, UDATA size, UDATA *bytesRead);
static void flushCache(void);


void 
dbgReadMemory (UDATA address, void *structure, UDATA size, UDATA *bytesRead)
{
	if (address == 0) {
		memset(structure, 0, size);
		*bytesRead = 0;
		return;
	}

	readCachedMemory(address, structure, size, bytesRead);
	if (*bytesRead != size) {
		callGetMemoryBytes(address, structure, size, bytesRead);
	}

	return;
}


UDATA 
dbgGetExpression (const char* args)
{
#ifdef WIN64
	return (UDATA)_strtoui64(args, NULL, 16);
#else
	return (UDATA)strtoul(args, NULL, 16);
#endif
}


/*
 * See dbgFindPatternInRange
 */
void*
dbgFindPattern(U_8* pattern, UDATA patternLength, UDATA patternAlignment, U_8* startSearchFrom, UDATA* bytesSearched) 
{
	jlong result;

	*bytesSearched = 0;

	if (callFindPattern(pattern, (jint)patternLength, (jint)patternAlignment, (jlong)(UDATA)startSearchFrom, &result)) {
		return NULL;
	}

	*bytesSearched = (UDATA)-1;
	if (result == (jlong)-1) {
		return NULL;
	} else {
		return (void*)(UDATA)result;
	}
}



/*
 * Find the J9RAS structure and validate that it is correct.
 * This prevents jextract from one build or platform being used with a 
 * dump produced by a different build or platform.
 *
 * Return JNI_TRUE if the dump matches jextract.
 * Return JNI_FALSE and set a Java exception if the dump does not match.
 */
static jboolean
validateDump(JNIEnv *env)
{
	jlong startFrom = 0;
	jlong eyecatcher;
	char errBuf[256];

	PORT_ACCESS_FROM_VMC((J9VMThread*)env);

	jclass errorClazz = (*env)->FindClass(env, "java/lang/Error");
	if (errorClazz == NULL) {
		return JNI_FALSE;
	}

#if !defined (J9VM_RAS_EYECATCHERS)
	(*env)->ThrowNew(env, errorClazz, "RAS is not enabled on this platform");
	return JNI_FALSE;
#else


	for(;;) {
		J9RAS *ras = NULL;
		const char* rasString = "J9VMRAS";

		if (callFindPattern((U_8*)rasString, sizeof(rasString), 8, startFrom, &eyecatcher)) {
			(*env)->ThrowNew(env, errorClazz, "An error occurred while searching for the J9VMRAS eyecatcher");
			return JNI_FALSE;
		}

		if (eyecatcher == (jlong)-1) {
			j9str_printf(PORTLIB, 
					errBuf, sizeof(errBuf), 
					"JVM anchor block (J9VMRAS) not found in dump. Dump may be truncated, corrupted or contains a partially initialized JVM.");
			(*env)->ThrowNew(env, errorClazz, errBuf);
			return JNI_FALSE;
		}

#if !defined (J9VM_ENV_DATA64)
		if ( (U_64)eyecatcher > (U_64)0xFFFFFFFF ) {
			j9str_printf(PORTLIB, 
				errBuf, sizeof(errBuf), 
				"J9RAS is out of range for a 32-bit pointer (0x%16.16llx). This version of jextract is incompatible with this dump.", 
				eyecatcher);
			(*env)->ThrowNew(env, errorClazz, errBuf);
			return JNI_FALSE;
		}
#endif
		/* Allocate this, since on 64bit platforms we want to know now that we can't allocate
		 * the scratch space. This allows us to exit early with a simple error message */
		ras = dbgMallocAndRead(sizeof(J9RAS), (void *)(UDATA)eyecatcher);
		if (ras != NULL) {
			if (ras->bitpattern1 == 0xaa55aa55 && ras->bitpattern2 == 0xaa55aa55) {
				if (ras->version != J9RASVersion) {
					j9str_printf(PORTLIB, 
						errBuf, sizeof(errBuf), 
						"J9RAS.version is incorrect (found %u, expecting %u). This version of jextract is incompatible with this dump.", 
						ras->version,
						J9RASVersion);
					(*env)->ThrowNew(env, errorClazz, errBuf);
					return JNI_FALSE;
				}
				if (ras->length != sizeof(J9RAS)) {
					j9str_printf(PORTLIB, 
						errBuf, sizeof(errBuf), 
						"J9RAS.length is incorrect (found %u, expecting %u). This version of jextract is incompatible with this dump.", 
						ras->length,
						sizeof(J9RAS));
					(*env)->ThrowNew(env, errorClazz, errBuf);
					return JNI_FALSE;
				}
				if (ras->buildID != J9UniqueBuildID) {
					j9str_printf(PORTLIB, 
						errBuf, sizeof(errBuf), 
						"J9RAS.buildID is incorrect (found %llx, expecting %llx). This version of jextract is incompatible with this dump.", 
						ras->buildID,
						(U_64)J9UniqueBuildID);
					(*env)->ThrowNew(env, errorClazz, errBuf);
					return JNI_FALSE;
				}

				/* cache the value here so that dbgSniffForJavaVM doesn't need to duplicate this work */
				dbgSetVM((J9JavaVM*)ras->vm);
				return JNI_TRUE;
			}
			dbgFree(ras);
		} else {
			/* on 64bit platforms, the code that tries to allocate the scratch space J9DBGEXT_SCRATCH_SIZE 
			 * scratch space will have issued it's own informative error already. */
			j9str_printf(PORTLIB, 
				errBuf, sizeof(errBuf), 
				"Cannot allocate %zu bytes of memory for initial RAS eyecatcher, cannot continue processing this dump.", 
				sizeof(J9RAS));
			(*env)->ThrowNew(env, errorClazz, errBuf);
			return JNI_FALSE;
		}

		/* this isn't it -- look for the next occurrence */
		startFrom = eyecatcher + 8;
	}
#endif
}


/*
 * See dbgFindPatternInRange
 */
static jint
callFindPattern(U_8* pattern, jint patternLength, jint patternAlignment, jlong startSearchFrom, jlong* resultP)
{
	jbyteArray patternArray;
	jlong result;

	if (!globalDumpObj || !globalFindPatternMid) {
		return -1;
	}

	patternArray = (*globalEnv)->NewByteArray(globalEnv, (jsize)patternLength);
	if (patternArray == NULL) {
		(*globalEnv)->ExceptionDescribe(globalEnv);
		return -1;
	}

	(*globalEnv)->SetByteArrayRegion(globalEnv, patternArray, 0, (jsize)patternLength, (jbyte*)pattern);
	if ((*globalEnv)->ExceptionCheck(globalEnv)) {
		(*globalEnv)->DeleteLocalRef(globalEnv, patternArray);
		(*globalEnv)->ExceptionDescribe(globalEnv);
		return -1;
	}

	result = (*globalEnv)->CallLongMethod(globalEnv, 
		globalDumpObj, 
		globalFindPatternMid, 
		patternArray, 
		(jint)patternAlignment, 
		(jlong)startSearchFrom);

	(*globalEnv)->DeleteLocalRef(globalEnv, patternArray);

	if ((*globalEnv)->ExceptionCheck(globalEnv)) {
		(*globalEnv)->ExceptionDescribe(globalEnv);
		return -1;
	}

	*resultP = result;
	return 0;
}



static jint
cacheIDs(JNIEnv* env, jobject dumpObj)
{
	jclass cls;
	
	globalEnv = env;
	globalDumpObj = dumpObj;

	if (!dumpObj) {
		return -1;
	}

	cls = (*env)->GetObjectClass(env, dumpObj);
	if (!cls) {
		return -1;
	}

	globalGetMemMid = (*env)->GetMethodID(env, cls,"getMemoryBytes","(JI)[B");
	if (!globalGetMemMid) {
		return -1;
	}

	globalFindPatternMid = (*env)->GetMethodID(env, cls,"findPattern","([BIJ)J");
	if (!globalFindPatternMid) {
		return -1;
	}

	return 0;
}


void JNICALL Java_com_ibm_jvm_j9_dump_extract_Main_doCommand(JNIEnv *env, jobject obj, jobject dumpObj, jstring commandObject)
{
	const char *command = (const char *) (*env)->GetStringUTFChars(env, commandObject, 0);
	PORT_ACCESS_FROM_VMC((J9VMThread*)env);

	if (command == NULL) {
		return;
	}
	
	if (cacheIDs(env, dumpObj)) {
		return;
	}

	/* hook the debug extension's malloc and free up to ours, so that it can benefit from -memorycheck */
	OMRPORT_FROM_J9PORT(dbgGetPortLibrary())->mem_allocate_memory = OMRPORT_FROM_J9PORT(PORTLIB)->mem_allocate_memory;
	OMRPORT_FROM_J9PORT(dbgGetPortLibrary())->mem_free_memory = OMRPORT_FROM_J9PORT(PORTLIB)->mem_free_memory;
	OMRPORT_FROM_J9PORT(dbgGetPortLibrary())->port_control = OMRPORT_FROM_J9PORT(PORTLIB)->port_control;

	run_command(command);
	
	(*env)->ReleaseStringUTFChars(env, commandObject, command);

	return;
}

/**
 * Gets the environment pointer from the J9RAS structure.
 */
jlong JNICALL Java_com_ibm_jvm_j9_dump_extract_Main_getEnvironmentPointer(JNIEnv * env, jobject obj, jobject dumpObj)
{
	J9JavaVM* vmPtr = NULL;
	J9JavaVM* localVMPtr = NULL;
	J9RAS* localRAS = NULL;
	jlong toReturn = 0;

	if (cacheIDs(env, dumpObj)) {
		goto end;
	}

	if (!validateDump(env)) {
		goto end;
	}

	vmPtr = dbgSniffForJavaVM();
	if (!vmPtr) {
		goto end;
	}

	localVMPtr = dbgMallocAndRead(sizeof(J9JavaVM), (void *)(UDATA)vmPtr);
	if (!localVMPtr) {
		goto end;
	}

	localRAS = dbgMallocAndRead(sizeof(J9RAS), (void *)(UDATA)localVMPtr->j9ras);
	if (!localRAS) {
		goto end;
	}

#if defined (J9VM_ENV_DATA64)
	toReturn = (jlong)(IDATA)localRAS->environment;
#else
	toReturn = (jlong)(IDATA)localRAS->environment & J9CONST64(0xFFFFFFFF);
#endif

end:
	flushCache();
	dbgFreeAll();

	return toReturn;
}

I_32
dbg_j9port_create_library(J9PortLibrary *portLib, J9PortLibraryVersion *version, UDATA size)
{
	PORT_ACCESS_FROM_ENV(globalEnv);

	return PORTLIB->port_create_library(portLib, version, size);
}

/*
 * Need this to get it to link. This is how the debug extension code write messages
 * to the platform debugger - or rather to stdout here when we are not running under the
 * platform debugger.
 */
void 
dbgWriteString (const char* message)
{
	PORT_ACCESS_FROM_VMC((J9VMThread*)globalEnv);

	j9tty_printf(PORTLIB, "%s", message);
}

static void 
callGetMemoryBytes(UDATA address, void *structure, UDATA size, UDATA *bytesRead)
{
	jbyteArray data;
	jlong ja = address;
	jint js = (jsize)size;
	
	*bytesRead = 0;
	memset(structure, 0, size);

	/* ensure that size can be represented as a jsize */
	if ( (js < 0) || ((UDATA)js != size) ) {
		return;
	}
	
	if (!globalDumpObj || !globalGetMemMid) {
		return;
	}

	/* we need to allocate another 1-3 local refs so make sure we can get them to satisfy -Xcheck:jni */
	(*globalEnv)->EnsureLocalCapacity(globalEnv, 3);
	if ((*globalEnv)->ExceptionCheck(globalEnv)) {
		/* if we fail to allocate local ref storage, just fail since this definitely won't work */
		(*globalEnv)->ExceptionClear(globalEnv);
		return;
	}
	data = (jbyteArray)((*globalEnv)->CallObjectMethod(globalEnv, globalDumpObj, globalGetMemMid, ja, js));
	if ((*globalEnv)->ExceptionCheck(globalEnv)) {
		/* CMVC 110117:  ExceptionDescribe causes an uncaught event so manually call printStackTrace() */
		jthrowable exception = (*globalEnv)->ExceptionOccurred(globalEnv);
		jclass exceptionClass = NULL;
		jmethodID printStackTraceID = NULL;
		
		(*globalEnv)->ExceptionClear(globalEnv);
		/* note that the error cases where we are missing an exception, a class or printStackTrace, we have no good solution */
		exceptionClass = (*globalEnv)->GetObjectClass(globalEnv, exception);
		printStackTraceID = (*globalEnv)->GetMethodID(globalEnv, exceptionClass, "printStackTrace", "()V");
		(*globalEnv)->CallVoidMethod(globalEnv, exception, printStackTraceID);
		/* if we throw while trying to print a stack trace, all bets are off in terms of how to handle that exception so just clear it */
		(*globalEnv)->ExceptionClear(globalEnv);
		(*globalEnv)->DeleteLocalRef(globalEnv, exception);
		(*globalEnv)->DeleteLocalRef(globalEnv, exceptionClass);
		return;
	}

	if (data) {
		jsize jbytesRead = (*globalEnv)->GetArrayLength(globalEnv, data); 
		if (jbytesRead > js) {
			/* throw an exception here? */
		} else {
		   (*globalEnv)->GetByteArrayRegion(globalEnv, data, 0, jbytesRead, structure);
		}
		(*globalEnv)->DeleteLocalRef(globalEnv, data);
		*bytesRead = (UDATA)jbytesRead;
	}

	return;
}

/**
 * Flush all elements from the small object memory cache.
 */
static void
flushCache(void)
{
	int i=0;

	for (i=0; i < sizeof(cache)/sizeof(cache[0]); i++) {
		/* invalidate this element */
		cache[i].address = 0;
	}
}

/**
 * This function implements a very simple caching scheme to accelerate the reading of small objects.
 * A more sophisticated scheme is implemented in the Java code. This cache allows us to bypass
 * the relatively expensive call-in to Java for most objects. During a normal jextract run we expect 
 * the cache to have a hit rate of over 90%.
 */
static void 
readCachedMemory(UDATA address, void *structure, UDATA size, UDATA *bytesRead)
{
	static UDATA hits, total;

	dbgCacheElement* thisElement = NULL;
	UDATA lineStart = address & ~(UDATA)(sizeof(thisElement->data) - 1);
	UDATA endAddress = address + size;

#if 0
	if ( ((++total) % (16 * 1024)) == 0) {
		dbgPrint("Cache hit rate: %.2f\n", (float)hits / (float)(total));
	}
#endif

	*bytesRead = 0;

	if (( (lineStart + sizeof(thisElement->data)) >= endAddress ) &&
		(endAddress > address)) { /* check for arithmetic overflow */
		UDATA cacheBytesRead;

		thisElement = &cache[ (lineStart / sizeof(thisElement->data)) % CACHE_SIZE ];

		/* is the data cached at this slot? */
		if (thisElement->address == lineStart) {
			memcpy(structure, thisElement->data + (address - lineStart), size);
			*bytesRead = size;
			hits++;
			return;
		}

		/* it wasn't -- cache it now */
		callGetMemoryBytes(lineStart, thisElement->data, sizeof(thisElement->data), &cacheBytesRead);
		if (cacheBytesRead == sizeof(thisElement->data)) {
			thisElement->address = lineStart;
			memcpy(structure, thisElement->data + (address - lineStart), size);
			*bytesRead = size;
		} else {
			/* invalidate this element */
			thisElement->address = 0;
		}
	}
}


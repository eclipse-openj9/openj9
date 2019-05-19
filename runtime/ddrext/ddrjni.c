/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

#include <stdio.h>
#include <stdlib.h>

#include "ddr.h"
#include "ddrjni.h"
#include "ddrutils.h"
#include "ut_ddrext.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniMemory.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniNatives.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniProcess.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniReader.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniRegisters.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniSearchableMemory.h"




#ifdef __cplusplus
extern "C" {
#endif

/**
 * In the event that dbgReadMemory fails, throw an exception
 * notifying the caller that we couldn't read the requested memory
 * @param env JNI environment
 * @param address requested address that could not be read
 */
static void
throwMemoryFaultException(JNIEnv * env, jlong address)
{
	jclass cls = NULL;
	jmethodID cid = NULL;
	jobject exception = NULL;
	jint throwResult = 0;
	/* These functions are spec'd to throw a MemoryFault exception */
	cls = (*env)->FindClass(env, "com/ibm/j9ddr/corereaders/memory/MemoryFault");
	Assert_ddrext_true(NULL != cls);

	if(NULL != cls){
		/* We throw the address we faulted on */
		cid = (*env)->GetMethodID(env, cls, "<init>", "(J)V");
		Assert_ddrext_true(NULL != cid);
		if(NULL != cid){
			exception = (*env)->NewObject(env, cls, cid, address);
			Assert_ddrext_true(NULL != exception);
			if(NULL != exception){
				throwResult = (*env)->Throw(env, exception);
				(*env)->DeleteLocalRef(env,exception);
			}
		}
		(*env)->DeleteLocalRef(env,cls);
	}
}

/**
---------------------------------Start--------------------------------------
-----------------com_ibm_j9ddr_corereaders_debugger_JniMemory---------------
----------------------------------------------------------------------------
**/

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniMemory
 * Method:    getByteAt
 * Signature: (J)B
 */
JNIEXPORT jbyte JNICALL Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getByteAt(JNIEnv * env, jobject self, jlong address)
{
	UDATA bytesRead;
	jbyte result;
	dbgReadMemory((UDATA) address, &result, sizeof(jbyte), &bytesRead);
	if(sizeof(jbyte) != bytesRead){
		throwMemoryFaultException(env,address);
		return 0;
	}
	return result;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniMemory
 * Method:    getByteOrder
 * Signature: ()Ljava/nio/ByteOrder;
 */
JNIEXPORT jobject JNICALL Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getByteOrder(JNIEnv * env, jobject elf)
{
	jclass byteOrderClass;
	jfieldID endienField;
	int endian = 1;

	byteOrderClass = (*env)->FindClass(env, "java/nio/ByteOrder");
	if (byteOrderClass == NULL) {
		dbgWriteString("failed to find class java/nio/ByteOrder\n");
		return NULL;
	}

	if(*(char *)&endian == 1) {
		endienField = (*env)->GetStaticFieldID(env, byteOrderClass, "LITTLE_ENDIAN", "Ljava/nio/ByteOrder;");
	} else {
		endienField = (*env)->GetStaticFieldID(env, byteOrderClass, "BIG_ENDIAN", "Ljava/nio/ByteOrder;");
	}
	if (endienField == NULL) {
		dbgWriteString("failed to find static field id of endian type in class java/nio/ByteOrder\n");
		return NULL;
	}
	return (*env)->GetStaticObjectField(env, byteOrderClass, endienField);
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniMemory
 * Method:    getBytesAt
 * Signature: (J[B)I
 */
JNIEXPORT jint JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getBytesAt__J_3B
(JNIEnv * env, jobject self, jlong address, jbyteArray buffer) {
	UDATA bytesRead;
	jbyte* readBuffer;
	jsize arrayLen = (*env)->GetArrayLength(env, buffer);

	if (arrayLen == 0) { /* nothing to do */
		return 0;
	}

	readBuffer = (*env)->GetByteArrayElements(env, buffer, NULL);
	dbgReadMemory((UDATA) address, readBuffer, arrayLen, &bytesRead);
	if(bytesRead == 0){
		throwMemoryFaultException(env,address);
		return 0;
	}

	/**
	 * copy back and free the readBuffer buffer
	 */
	(*env)->ReleaseByteArrayElements(env, buffer,  readBuffer, 0);
	/* TODO: bytesRead? */
	return (jint) bytesRead;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniMemory
 * Method:    getBytesAt
 * Signature: (J[BII)I
 */
JNIEXPORT jint JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getBytesAt__J_3BII(JNIEnv * env, jobject self, jlong address, jbyteArray buffer, jint offset, jint length)
{
	UDATA bytesRead;
	jbyte* readBuffer;
	jsize arrayLen = (*env)->GetArrayLength(env, buffer);

	if (length == 0) { /* nothing to do */
		return 0;
	}

	readBuffer = (*env)->GetByteArrayElements(env, buffer, NULL);
	dbgReadMemory((UDATA) address, readBuffer + offset, length, &bytesRead);
	if(bytesRead == 0){
		throwMemoryFaultException(env,address);
		return 0;
	}

	/**
	 * copy back and free the readBuffer buffer
	 */
	(*env)->ReleaseByteArrayElements(env, buffer,  readBuffer,	0);
	return (jint) bytesRead;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniMemory
 * Method:    getIntAt
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getIntAt(JNIEnv * env, jobject self, jlong address)
{
	UDATA bytesRead;
	jint result;
	dbgReadMemory((UDATA) address, &result, sizeof(jint), &bytesRead);
	if(sizeof(jint) != bytesRead){
		throwMemoryFaultException(env,address);
		return 0;
	}
	return result;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniMemory
 * Method:    getLongAt
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getLongAt(JNIEnv * env, jobject self, jlong address)
{
	UDATA bytesRead;
	jlong result;
	dbgReadMemory((UDATA) address, &result, sizeof(jlong), &bytesRead);
	if(sizeof(jlong) != bytesRead){
		throwMemoryFaultException(env,address);
		return 0;
	}
	return result;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniMemory
 * Method:    getShortAt
 * Signature: (J)S
 */
JNIEXPORT jshort JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getShortAt(JNIEnv * env, jobject self, jlong address)
{
	UDATA bytesRead;
	jshort result;
	dbgReadMemory((UDATA) address, &result, sizeof(jshort), &bytesRead);
	if(sizeof(jshort) != bytesRead){
		throwMemoryFaultException(env,address);
		return 0;
	}
	return result;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniMemory
 * Method:    isExecutable
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_isExecutable(JNIEnv * env, jobject self, jlong address)
{

	return JNI_FALSE;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniMemory
 * Method:    isReadOnly
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_isReadOnly(JNIEnv * env, jobject self, jlong address)
{
	return JNI_FALSE;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniMemory
 * Method:    isShared
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_isShared(JNIEnv * env, jobject self, jlong address)
{
	return JNI_FALSE;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniMemory
 * Method:    readAddress
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_readAddress(JNIEnv * env, jobject self, jlong address)
{
	UDATA bytesRead;
	jlong result;
	dbgReadMemory((UDATA) address, &result, sizeof(jlong), &bytesRead);
	if(sizeof(jlong) != bytesRead){
		throwMemoryFaultException(env,address);
		return 0;
	}
	return result;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniMemory
 * Method:    getValidRegionVirtual
 * Signature: (JJ)Lcom/ibm/j9ddr/corereaders/memory/MemoryRange;
 */
JNIEXPORT jobject JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getValidRegionVirtual(JNIEnv * env, jclass self, jlong baseAddress, jlong searchSize)
{
	return NULL;
}

/*
---------------------------------End--------------------------------------
-----------------com_ibm_j9ddr_corereaders_debugger_JniMemory-------------
--------------------------------------------------------------------------
*/

/*
---------------------------------Start------------------------------------
-----------------com_ibm_j9ddr_corereaders_debugger_JniNatives------------
--------------------------------------------------------------------------
*/

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniNatives
 * Method:    getPlatform
 * Signature: ()Lcom/ibm/j9ddr/corereaders/Platform;
 */
JNIEXPORT jobject JNICALL Java_com_ibm_j9ddr_corereaders_debugger_JniNatives_getPlatform(JNIEnv * env, jclass self)
{
	jclass platformClass;
	jfieldID platformField;

#if defined(WIN32) || defined(WIN64)
	char* platform = "WINDOWS";
#endif
#if defined(J9ZOS390)
	char* platform = "ZOS";
#endif
#if defined(LINUX)
	char* platform = "LINUX";
#endif
#if defined(AIXPPC)
	char* platform = "AIX";
#endif
#if defined(OSX)
	char* platform = "OSX";
#endif


	platformClass = (*env)->FindClass(env, "com/ibm/j9ddr/corereaders/Platform");
	if (platformClass == NULL) {
		dbgWriteString("failed to find class com/ibm/j9ddr/corereaders/Platform\n");
		return NULL;
	}

	platformField = (*env)->GetStaticFieldID(env, platformClass, platform, "Lcom/ibm/j9ddr/corereaders/Platform;");

	if (platformField == NULL) {
		dbgWriteString("failed to find static field id of platform type in class com/ibm/j9ddr/corereaders/Platform\n");
		return NULL;
	}
	return (*env)->GetStaticObjectField(env, platformClass, platformField);
}

/*
---------------------------------End--------------------------------------
-----------------com_ibm_j9ddr_corereaders_debugger_JniNatives------------
--------------------------------------------------------------------------
*/


/*
---------------------------------Start------------------------------------
-----------------com_ibm_j9ddr_corereaders_debugger_JniProcess------------
--------------------------------------------------------------------------
*/
/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniProcess
 * Method:    bytesPerPointer
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniProcess_bytesPerPointer(JNIEnv * env, jobject self)
{
	return sizeof(void*);
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniProcess
 * Method:    getSignalNumber
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniProcess_getSignalNumber(JNIEnv * env, jobject self)
{
	return 0;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniProcess
 * Method:    getProcessId
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniProcess_getProcessId(JNIEnv * env, jobject self)
{
	return 0;
}

/** 
 * \brief	Get an array of live thread IDs
 * \ingroup ddrext
 * 
 * @param[in] env 
 * @param[in] self	receiver 
 * @return	Long Array of thread IDs
 * 
 */
JNIEXPORT jlongArray JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniProcess_getThreadIds(JNIEnv * env, jobject self)
{
	jint rc;
	UDATA i;
	UDATA * threadIdList = NULL; 
	UDATA threadCount = 0;
	jlongArray tidArray = NULL;
	jlong element;

	rc = dbgGetThreadList(&threadCount, &threadIdList);
	if (rc != 0) {
		goto done;
	}

	if (threadCount == 0 || threadIdList == NULL) {
		goto done;
	}

	tidArray = (*env)->NewLongArray(env, (jsize) threadCount);
	if (tidArray == NULL) {
		goto done;
	}

	for (i = 0; i < threadCount; i++) {
		element = threadIdList[i];
		(*env)->SetLongArrayRegion(env, tidArray, (jsize) i, 1, &element);
	}

done:	
	if (threadIdList) {
		free(threadIdList);
	}

	return tidArray;
}

JNIEXPORT jint JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniProcess_getTargetArchitecture(JNIEnv * env, jobject self)
{
	int targetArchitecture;

	targetArchitecture = dbgGetTargetArchitecture();
	if (targetArchitecture < 0) {
		return -1;
	}

	return targetArchitecture;
}


/*
---------------------------------End--------------------------------------
-----------------com_ibm_j9ddr_corereaders_debugger_JniProcess------------
--------------------------------------------------------------------------
*/


/*
---------------------------------Start------------------------------------
-----------------com_ibm_j9ddr_corereaders_debugger_JniReader-------------
--------------------------------------------------------------------------
*/
/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniReader
 * Method:    getDumpFormat
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniReader_getDumpFormat(JNIEnv * env, jobject self)
{
	return NULL;
}
/*
---------------------------------End--------------------------------------
-----------------com_ibm_j9ddr_corereaders_debugger_JniReader-------------
--------------------------------------------------------------------------
*/


/*
---------------------------------Start------------------------------------
-----------------com_ibm_j9ddr_corereaders_debugger_JniRegisters----------
--------------------------------------------------------------------------
*/
/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniRegisters
 * Method:    getNumberRegisters
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniRegisters_getNumberRegisters(JNIEnv * env, jclass self, jlong tid)
{
	return 0;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniRegisters
 * Method:    getRegisterName
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniRegisters_getRegisterName(JNIEnv * env, jclass self, jlong index)
{
	return NULL;
}

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniRegisters
 * Method:    getRegisterValue
 * Signature: (JJ)Ljava/lang/Number;
 */
JNIEXPORT jobject JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniRegisters_getRegisterValue(JNIEnv * env, jclass self, jlong tid, jlong index)
{
	return NULL;
}

/** 
 * \brief	Retrieve the current register set for the top most frame of given thread  
 * \ingroup 
 * 
 * @param[in] env 
 * @param[in] obj	receiver
 * @param[in] tid	thread id to retrieve registers for
 * @return	true on success
 */
JNIEXPORT jboolean JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniRegisters_fetchRegisters(JNIEnv * env, jobject obj, jlong tid)
{
	static ddrRegister * registerList = NULL;    /* Warning: This code is NOT reentrant, DDR operates as a single thread */ 
	UDATA registerCount = 0;
	jmethodID mid;
	jclass cls;
	UDATA i;
	int rc;

	cls = (*env)->GetObjectClass(env, obj);
	if (cls == NULL) {
		dbgWriteString("failed to find class com/ibm/j9ddr/corereaders/debugger/JniRegisters\n");
		return JNI_FALSE;
	}

	mid = (*env)->GetMethodID(env, cls, "setRegister", "(Ljava/lang/String;J)V");
	if (mid == NULL) {
		dbgWriteString("failed to find setRegister method in com/ibm/j9ddr/corereaders/debugger/JniRegisters\n");
		return JNI_FALSE;
	}

	rc = dbgGetRegisters((UDATA) tid, &registerCount, &registerList);
	if (rc != 0) {
		dbgWrite("DDREXT: Failed to get registers, rc = %d\n", rc);
		return JNI_FALSE;
	}
	
	
	if (registerCount == 0 || registerList == NULL) {
		return JNI_FALSE;
	}


	for (i = 0; i < registerCount; i++) {
		jstring regName;

		/*printf("Calling setRegister with %s %llx\n", registerList[i].name, registerList[i].value);*/

		regName = (*env)->NewStringUTF(env, registerList[i].name);
		if ((*env)->ExceptionCheck(env)) {
			dbgWriteString("exception in NewStringUTF\n");
			return JNI_FALSE;
		}
		if (regName == NULL) {
			return JNI_FALSE;
		}

		(*env)->CallVoidMethod(env, obj, mid, regName, registerList[i].value);
		if ((*env)->ExceptionCheck(env)) {
			dbgWriteString("exception in setRegister\n");
			return JNI_FALSE;
		}
	}

	return JNI_TRUE;
}



/*
---------------------------------End--------------------------------------
-----------------com_ibm_j9ddr_corereaders_debugger_JniRegisters----------
--------------------------------------------------------------------------
*/


/*
---------------------------------Start-------------------------------------
-----------------com_ibm_j9ddr_corereaders_debugger_JniSearchableMemory----
---------------------------------------------------------------------------
*/

/*
 * Class:     com_ibm_j9ddr_corereaders_debugger_JniSearchableMemory
 * Method:    findPattern
 * Signature: ([BIJ)J
 */
JNIEXPORT jlong JNICALL
Java_com_ibm_j9ddr_corereaders_debugger_JniSearchableMemory_findPattern(JNIEnv * env, jobject self, jbyteArray whatBytes, jint alignment, jlong startFrom)
{

	jsize arrayLen;
	jbyte* searchBytes;

	U_8* pattern;
	UDATA patternLength;
	UDATA patternAlignment;
	U_8* startSearchFrom;
	UDATA bytesSearched;
	void * rc;

	arrayLen = (*env)->GetArrayLength(env, whatBytes);
	searchBytes = (*env)->GetByteArrayElements(env, whatBytes, NULL);

	if(searchBytes == NULL) {
		dbgWriteString("findPattern: failed to load pattern\n");
		return 0;
	}

	pattern = (U_8*)searchBytes;
	patternLength = (UDATA) arrayLen;
	patternAlignment = (UDATA) alignment;
	startSearchFrom = (U_8*) (UDATA) startFrom;

	rc = dbgFindPatternInRange(pattern, patternLength, patternAlignment, startSearchFrom, (UDATA) -1 - (UDATA) startSearchFrom, &bytesSearched);
	(*env)->ReleaseByteArrayElements(env, whatBytes,  searchBytes, JNI_ABORT);
	if (rc == NULL) {
		return -1; // The various Java implementers of IMemory.findPattern return -1 for not found, so should we
	} else {
		return (jlong) (UDATA) rc;
	}
}

/*
---------------------------------End--------------------------------------
-----------------com_ibm_j9ddr_corereaders_debugger_JniSearchableMemory---
--------------------------------------------------------------------------
*/

#ifdef __cplusplus
}
#endif

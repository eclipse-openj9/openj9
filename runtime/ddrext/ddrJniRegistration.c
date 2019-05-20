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

#include "j9.h"
#include <jni.h>
#include "ddrJniRegistration.h"
#include "ut_ddrext.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniMemory.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniNatives.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniProcess.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniReader.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniRegisters.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniSearchableMemory.h"
#include "com_ibm_j9ddr_corereaders_debugger_JniOutputStream.h"

/**
 * define array of structures that contain names, descriptors and function pointers
 * of the native methods
 */
static JNINativeMethod jniMemoryMethods[] = {
	{ "getByteAt", 		"(J)B",		(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getByteAt },
	{ "getBytesAt", 	"(J[B)I",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getBytesAt__J_3B },
	{ "getBytesAt", 	"(J[BII)I",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getBytesAt__J_3BII },
	{ "getIntAt", 		"(J)I", (void*) Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getIntAt },
	{ "getLongAt", 		"(J)J", (void*) Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getLongAt },
	{ "getShortAt", 	"(J)S",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getShortAt },
	{ "isExecutable", 	"(J)Z", (void*) Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_isExecutable },
	{ "isReadOnly", 	"(J)Z", (void*) Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_isReadOnly },
	{ "isShared", 		"(J)Z", (void*) Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_isShared },
	{ "readAddress", 	"(J)J", (void*) Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_readAddress },
	{ "getByteOrder", 	"()Ljava/nio/ByteOrder;",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getByteOrder },
	{ "getValidRegionVirtual", "(JJ)Lcom/ibm/j9ddr/corereaders/memory/MemoryRange;", (void*) Java_com_ibm_j9ddr_corereaders_debugger_JniMemory_getValidRegionVirtual },
};

static JNINativeMethod jniNativesMethods[] = {
	{ "getPlatform",	"()Lcom/ibm/j9ddr/corereaders/Platform;", (void*) Java_com_ibm_j9ddr_corereaders_debugger_JniNatives_getPlatform},
};

static JNINativeMethod jniProcessMethods[] = {
	{ "bytesPerPointer", "()I",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniProcess_bytesPerPointer},
	{ "getSignalNumber", "()I",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniProcess_getSignalNumber},
	{ "getProcessId",    "()J",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniProcess_getProcessId},
	{ "getThreadIds",    "()[J",(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniProcess_getThreadIds},
	{ "getTargetArchitecture", "()I",(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniProcess_getTargetArchitecture},
};

static JNINativeMethod jniReaderMethods[] = {
	{ "getDumpFormat",	"()Ljava/lang/String;",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniReader_getDumpFormat},
};

static JNINativeMethod jniRegistersMethods[] = {
	{ "getNumberRegisters", "(J)J",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniRegisters_getNumberRegisters},
	{ "getRegisterName",	"(J)Ljava/lang/String;", (void*) Java_com_ibm_j9ddr_corereaders_debugger_JniRegisters_getRegisterName},
	{ "getRegisterValue",	"(JJ)Ljava/lang/Number;",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniRegisters_getRegisterValue},
	{ "fetchRegisters",		"(J)Z",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniRegisters_fetchRegisters},
};

static JNINativeMethod jniSearchableMemoryMethods[] = {
	{"findPattern",	"([BIJ)J",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniSearchableMemory_findPattern},
};

static JNINativeMethod jniOutputStreamMemoryMethods[] = {
	{ "write",	"(I)V",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniOutputStream_write__I},
	{ "write",	"([BII)V",	(void*) Java_com_ibm_j9ddr_corereaders_debugger_JniOutputStream_write___3BII},
};

/**
 * Helper method, takes class name and array of methods, and registers
 * native methods with the class.
 * @param env java env
 * @param class Java class name.
 * @param methods array of native methods
 * @param nMethods size of array
 */
static BOOLEAN
registerDDRNatives(JNIEnv *env, char* class, JNINativeMethod *methods, jint nMethods)
{
	jclass cls;
	jint r = -1;
	cls = (*env)->FindClass(env, class);
	if (cls == NULL || (*env)->ExceptionCheck(env)) {
		return FALSE;
	}

	r = (*env)->RegisterNatives(env, cls, methods, nMethods);
	if (r != 0 || (*env)->ExceptionCheck(env)) {
		return FALSE;
	}
	return TRUE;
}

/**
 * Register all DDR native methods.
 */
BOOLEAN
register_ddr_natives(JNIEnv * env)
{
	UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM(((J9VMThread*)env)->javaVM));
	if (registerDDRNatives(env,	"com/ibm/j9ddr/corereaders/debugger/JniMemory",	jniMemoryMethods, 12) == FALSE) { 
		return FALSE; 
	}
	
	if (registerDDRNatives(env, "com/ibm/j9ddr/corereaders/debugger/JniNatives", jniNativesMethods, 1) == FALSE) {
		return FALSE; 
	}
	
	if (registerDDRNatives(env,	"com/ibm/j9ddr/corereaders/debugger/JniProcess", jniProcessMethods, 5) == FALSE) { 
		return FALSE; 
	}
	
	if (registerDDRNatives(env,	"com/ibm/j9ddr/corereaders/debugger/JniReader", jniReaderMethods, 1) == FALSE) { 
		return FALSE; 
	}
	
	if (registerDDRNatives(env,	"com/ibm/j9ddr/corereaders/debugger/JniRegisters", jniRegistersMethods, 4) == FALSE) { 
		return FALSE; 
	}
	
	if (registerDDRNatives(env,	"com/ibm/j9ddr/corereaders/debugger/JniSearchableMemory", jniSearchableMemoryMethods, 1) == FALSE) { 
		return FALSE; 
	}
	
	if (registerDDRNatives(env,	"com/ibm/j9ddr/corereaders/debugger/JniOutputStream", jniOutputStreamMemoryMethods, 2) == FALSE) { 
		return FALSE; 
	}
	
	return TRUE;
}

/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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
#include <assert.h>
#include <jni.h>

#include "bcverify_api.h"
#include "j9.h"
#include "j9cfg.h"
#include "rommeth.h"
#include "ut_j9scar.h"

/* Define for debug
#define DEBUG_BCV
*/

#if JAVA_SPEC_VERSION >= 16
JNIEXPORT void JNICALL
JVM_DefineArchivedModules(JNIEnv *env, jobject obj1, jobject obj2)
{
	assert(!"JVM_DefineArchivedModules unimplemented");
}
#endif /* JAVA_SPEC_VERSION >= 16 */

#if JAVA_SPEC_VERSION >= 11
JNIEXPORT void JNICALL
JVM_InitializeFromArchive(JNIEnv *env, jclass clz)
{
	/* A no-op implementation is ok. */
}
#endif /* JAVA_SPEC_VERSION >= 11 */

#if JAVA_SPEC_VERSION >= 14
typedef struct GetStackTraceElementUserData {
	J9ROMClass *romClass;
	J9ROMMethod *romMethod;
	UDATA bytecodeOffset;
} GetStackTraceElementUserData;

static UDATA
getStackTraceElementIterator(J9VMThread *vmThread, void *voidUserData, UDATA bytecodeOffset, J9ROMClass *romClass, J9ROMMethod *romMethod, J9UTF8 *fileName, UDATA lineNumber, J9ClassLoader *classLoader, J9Class* ramClass)
{
	GetStackTraceElementUserData *userData = voidUserData;

	/* We are done, only first stack frame is needed. */
	userData->romClass = romClass;
	userData->romMethod = romMethod;
	userData->bytecodeOffset = bytecodeOffset;

	return FALSE;
}

#if defined(DEBUG_BCV)
static void cfdumpBytecodePrintFunction(void *userData, char *format, ...)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary*)userData);
	va_list args;
	char outputBuffer[512] = {0};

	va_start(args, format);
	j9str_vprintf(outputBuffer, 512, format, args);
	va_end(args);
	j9tty_printf(PORTLIB, "%s", outputBuffer);
}
#endif /* defined(DEBUG_BCV) */

JNIEXPORT jstring JNICALL
JVM_GetExtendedNPEMessage(JNIEnv *env, jthrowable throwableObj)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JavaVM *vm = vmThread->javaVM;
	jobject msgObjectRef = NULL;
	
	Trc_SC_GetExtendedNPEMessage_Entry(vmThread, throwableObj);
	if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_SHOW_EXTENDED_NPEMSG)) {
		J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;
		char *npeMsg = NULL;
		GetStackTraceElementUserData userData = {0};
		
		Trc_SC_GetExtendedNPEMessage_Entry2(vmThread, throwableObj);
		vmFuncs->internalEnterVMFromJNI(vmThread);
		userData.bytecodeOffset = UDATA_MAX;
		vmFuncs->iterateStackTrace(vmThread, (j9object_t*)throwableObj, getStackTraceElementIterator, &userData, FALSE);
		if ((NULL != userData.romClass)
			&& (NULL != userData.romMethod)
			&& (UDATA_MAX != userData.bytecodeOffset)
		) {
#if defined(DEBUG_BCV)
			{
				PORT_ACCESS_FROM_VMC(vmThread);
				U_8 *bytecodes = J9_BYTECODE_START_FROM_ROM_METHOD(userData.romMethod);
				U_32 flags = 0;

#if defined(J9VM_ENV_LITTLE_ENDIAN)
				flags |= BCT_LittleEndianOutput;
#else /* defined(J9VM_ENV_LITTLE_ENDIAN) */
				flags |= BCT_BigEndianOutput;
#endif /* defined(J9VM_ENV_LITTLE_ENDIAN) */
				j9bcutil_dumpBytecodes(PORTLIB, userData.romClass, bytecodes, 0, userData.bytecodeOffset, flags, (void *)cfdumpBytecodePrintFunction, PORTLIB, "");
			}
#endif /* defined(DEBUG_BCV) */
			npeMsg = vmFuncs->getCompleteNPEMessage(vmThread, J9_BYTECODE_START_FROM_ROM_METHOD(userData.romMethod) + userData.bytecodeOffset, userData.romClass, npeMsg);
			if (NULL != npeMsg) {
				PORT_ACCESS_FROM_VMC(vmThread);
				j9object_t msgObject = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, (U_8 *)npeMsg, strlen(npeMsg), 0);
				if (NULL != msgObject) {
					msgObjectRef = vmFuncs->j9jni_createLocalRef(env, msgObject);
				}
				j9mem_free_memory(npeMsg);
			}
		} else {
			Trc_SC_GetExtendedNPEMessage_Null_NPE_MSG(vmThread, userData.romClass, userData.romMethod, userData.bytecodeOffset);
		}
		vmFuncs->internalExitVMToJNI(vmThread);
	}
	Trc_SC_GetExtendedNPEMessage_Exit(vmThread, msgObjectRef);
	
	return msgObjectRef;
}
#endif /* JAVA_SPEC_VERSION >= 14 */

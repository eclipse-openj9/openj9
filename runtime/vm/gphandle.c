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

/* Turn off FPO optimisation on Windows 32-bit to improve native stack traces */
#if defined(WIN32)
#pragma optimize("y",off)
#endif

#include <stdlib.h>
#include <string.h>

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9port.h"
#include "j9dump.h"
#include "j9consts.h"
#include "gp.h"
#include "rommeth.h"
#include "j9cp.h"
#include "vmhook_internal.h"
#include "fltconst.h"
#include "vm_internal.h"
#include "ut_j9vm.h"
#include "j9vmnls.h"
#include "j9version.h"

#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
#include "leconditionexceptionsup.h"
#include "jni.h"
#include "atoe.h"

#define K8ZOS_CONTINUE_SEARCH 1
#define K8ZOS_RESUME_EXECUTION 2
#define K8ZOS_GO_DOWN 3

static UDATA prepareToResumeBackInJava(struct J9PortLibrary* portLibrary, J9VMThread *vmThread, void *sigInfo);
static UDATA handleJNIZOSLECondition(struct J9PortLibrary* portLibrary, J9VMThread *vmThread, void* gpInfo);
static UDATA throwConditionException(J9VMThread *vmThread, struct ConditionExceptionConstructorArgs *ceArgs);
#endif

#if defined(J9VM_PORT_OMRSIG_SUPPORT)
#include "omrsig.h"
#endif /* defined(J9VM_PORT_OMRSIG_SUPPORT) */

#define J9DO_NOT_WRITE_DUMP "J9NO_DUMPS"
#define INFO_COLS 4

#define MEMORY_SEGMENT_LIST_DO(segmentList, imageSegment) {\
	J9MemorySegment *imageSegment, *i2; \
	imageSegment = segmentList->nextSegment; \
	while(imageSegment) { \
		i2 = (J9MemorySegment *)(imageSegment->nextSegment);
#define END_MEMORY_SEGMENT_LIST_DO(imageSegment) \
		imageSegment = i2; }}

typedef struct J9CrashData {
	J9JavaVM* javaVM;
	J9VMThread* vmThread;
	U_32 gpType;
	void* gpInfo;
	char *consoleOutputBuf;
	size_t sizeofConsoleOutputBuf;
#ifdef J9VM_RAS_EYECATCHERS
J9RASCrashInfo *rasCrashInfo;
#endif
} J9CrashData;

typedef struct J9WriteGPInfoData {
	char *s;
	UDATA length;
	void* gpInfo;
	U_32 category;
} J9WriteGPInfoData;

typedef struct J9RecursiveCrashData {
	char *protectedFunctionName;
	U_32 info;
} J9RecursiveCrashData;

typedef struct vmDetails {
	UDATA cpus;
	U_64 mem;
	const char *osBuf;
	const char *cpuBuf;
	const char *versionBuf;
} vmDetails;

typedef struct J9SignalHandlerParams {
	J9JavaVM *vm;
	J9VMThread *thread;
} J9SignalHandlerParams;

static void generateSystemDump(struct J9PortLibrary* portLibrary, void* gpInfo);
static UDATA executeAbortHook (struct J9PortLibrary* portLibrary, void* userData);
static UDATA writeCrashDataToConsole (struct J9PortLibrary* portLibrary, void* userData);
static const char* getSignalDescription (struct J9PortLibrary* portLibrary, U_32 gpType);
static UDATA recursiveCrashHandler (struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void *recursiveCrashHandlerData);
static void dumpVmDetailString(struct J9PortLibrary *portLibrary, J9JavaVM* vm);
static void getVmDetailsFromPortLib(struct J9PortLibrary *portLibrary, struct vmDetails *details);
static UDATA writeVMInfo(J9JavaVM* vm, char* s, UDATA length);
static UDATA writeGPInfo(struct J9PortLibrary* portLibrary, void *writeGPInfoCrashData);
static UDATA reportThreadCrash(struct J9PortLibrary* portLibrary, void* userData);
static UDATA generateDiagnosticFiles(struct J9PortLibrary* portLibrary, void* userData);
UDATA vmSignalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData);

#if defined(J9VM_INTERP_USE_UNSAFE_HELPER)
static UDATA isCallerClassJavaNio(J9VMThread *currentThread, J9StackWalkState *walkState);
static UDATA sigBusHandler(J9VMThread *vmThread, void* gpInfo);
#endif /* defined(J9VM_INTERP_USE_UNSAFE_HELPER) */

#ifdef J9VM_RAS_EYECATCHERS
static UDATA setupRasCrashInfo(struct J9PortLibrary* portLibrary, void* userData);
#endif

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
static UDATA writeJITInfo (J9VMThread* vmThread, char* s, UDATA length, void* gpInfo);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */

#if defined(J9VM_RAS_DUMP_AGENTS)
static void printBacktrace(struct J9JavaVM *vm, void* gpInfo);
#endif /* J9VM_RAS_DUMP_AGENTS */

#if defined(J9VM_ARCH_X86) && defined(J9VM_ENV_DATA64)
#define UNSAFE_TARGET_REGISTER J9PORT_SIG_GPR_AMD64_RDI
#elif defined(J9VM_ARCH_X86) && !defined(J9VM_ENV_DATA64)
#define UNSAFE_TARGET_REGISTER J9PORT_SIG_GPR_X86_EAX
#elif defined(AIXPPC) || (defined(LINUXPPC64) && defined(J9VM_ENV_LITTLE_ENDIAN))
/* The target register is GPR3 */
#define UNSAFE_TARGET_REGISTER 3
#elif defined(S390) && defined(LINUX)
/* The target register is GPR2 */
#define UNSAFE_TARGET_REGISTER 2
#elif defined(J9ZOS390)
/* The target register is GPR1 */
#define UNSAFE_TARGET_REGISTER 1
#elif defined(J9VM_ARCH_ARM) && defined(LINUX)
/* The target register is GPR0 */
#define UNSAFE_TARGET_REGISTER 0
#elif defined(J9VM_ARCH_AARCH64)
/* The target register is GPR0 */
#define UNSAFE_TARGET_REGISTER 0
#endif /* defined(J9VM_ARCH_X86) && defined(J9VM_ENV_DATA64) */


static void
getVmDetailsFromPortLib(struct J9PortLibrary *portLibrary, struct vmDetails *details)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	details->osBuf = j9sysinfo_get_OS_type();
	details->versionBuf = j9sysinfo_get_OS_version();
	details->cpuBuf = j9sysinfo_get_CPU_architecture();
	details->cpus = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
	details->mem = j9sysinfo_get_physical_memory();
}

static void 
dumpVmDetailString(struct J9PortLibrary *portLibrary, J9JavaVM* vm)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct vmDetails details;

#if defined(J9VM_RAS_EYECATCHERS)

	if (NULL != vm->j9ras) {
		details.osBuf = (char*)vm->j9ras->osname;
		details.versionBuf = (char*)vm->j9ras->osversion;
		details.cpuBuf = (char*)vm->j9ras->osarch;
		details.cpus = vm->j9ras->cpus;
		details.mem = vm->j9ras->memory;
	} else
#endif	
	{
		getVmDetailsFromPortLib(portLibrary, &details);
	}

	j9tty_printf(portLibrary, "Target=%u_%02u_" EsBuildVersionString " (%s %s)\n", EsVersionMajor, EsVersionMinor,
			details.osBuf ? details.osBuf : "unknown", details.versionBuf ? details.versionBuf : "unknown");
	j9tty_printf(portLibrary, "CPU=%s (%d logical CPUs) (0x%llx RAM)\n",
			details.cpuBuf ? details.cpuBuf : "unknown", details.cpus, details.mem);

}

static UDATA
reportThreadCrash(struct J9PortLibrary* portLibrary, void* userData)
{
	J9CrashData* data = userData;
	J9JavaVM* vm = data->javaVM;
	J9VMThread *crashingThread = data->vmThread;

	TRIGGER_J9HOOK_VM_THREAD_CRASH(vm->hookInterface, crashingThread);
	return 0;
}

#if defined(J9VM_INTERP_USE_UNSAFE_HELPER)
static UDATA
isCallerClassJavaNio(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	UDATA rc = J9_STACKWALK_KEEP_ITERATING;

	if (NULL != walkState->method) {
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(walkState->method)->romClass);
		UDATA length = J9UTF8_LENGTH(className);
		U_8 *data = J9UTF8_DATA(className);

		/* Ignore any methods in Unsafe */
		if (J9UTF8_LITERAL_EQUALS(data, length, "sun/misc/Unsafe") || J9UTF8_LITERAL_EQUALS(data, length, "jdk/internal/misc/Unsafe")) {
			goto done;
		}
		if (length >= 9) {
			if (0 == memcmp(data, "java/nio/", 9)) {
				walkState->userData1 = (void *)TRUE;
			}
		}
		rc = J9_STACKWALK_STOP_ITERATING;
	}
done:
	return rc;
}

static UDATA
sigBusHandler(J9VMThread *vmThread, void* gpInfo)
{
	UDATA rc = J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
	J9JavaVM* vm = vmThread->javaVM;
	J9StackWalkState walkState;

	PORT_ACCESS_FROM_VMC(vmThread);
	walkState.skipCount = 0;
	walkState.walkThread = vmThread;
	walkState.flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_ITERATE_FRAMES;
	walkState.frameWalkFunction = isCallerClassJavaNio;
	walkState.userData1 = (void *) FALSE;
	vm->walkStackFrames(vmThread, &walkState);

	if ((void *)TRUE == walkState.userData1) {
		U_32 gpType = 0;
		UDATA* rspPtr = NULL;
		UDATA* targetRegPtr = NULL;
		const char* infoName = NULL;
		void* infoValue = NULL;
#if defined(AIXPPC) || (defined(LINUXPPC64) && defined(J9VM_ENV_LITTLE_ENDIAN))
		/* GPR1 is the stack pointer */
		gpType = j9sig_info(gpInfo, J9PORT_SIG_GPR, 1, &infoName, &infoValue);
#elif defined(S390) && defined(LINUX)
		/* GPR15 is the stack pointer */
		gpType = j9sig_info(gpInfo, J9PORT_SIG_GPR, 15, &infoName, &infoValue);
#elif defined(J9ZOS390)
		/* GPR4 is the stack pointer */
		gpType = j9sig_info(gpInfo, J9PORT_SIG_GPR, 4, &infoName, &infoValue);
#else
		gpType = j9sig_info(gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_SP, &infoName, &infoValue);
#endif /* defined(AIXPPC) || (defined(LINUXPPC64) && defined(J9VM_ENV_LITTLE_ENDIAN)) */
		if (gpType != J9PORT_SIG_VALUE_ADDRESS) {
			goto done;
		}
		rspPtr = (UDATA*) infoValue;

		gpType = j9sig_info(gpInfo, J9PORT_SIG_GPR, UNSAFE_TARGET_REGISTER, &infoName, &infoValue);
		if (gpType != J9PORT_SIG_VALUE_ADDRESS) {
			goto done;
		}
		targetRegPtr = (UDATA*) infoValue;
		/* change the target register to a writable address and clear J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS. */
#if defined(J9ZOS390)
		/* Stack register is biased. It is 2048 bytes before the top of stack. It is safer to add the bias back. */
		*targetRegPtr = *rspPtr + 2048 - 16;
#else
		*targetRegPtr = *rspPtr - 16;
#endif /* defined(J9ZOS390) */
		vmThread->privateFlags2 &= ~J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		rc = J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
	}

done:
	return rc;
}
#endif /* defined(J9VM_INTERP_USE_UNSAFE_HELPER) */

#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)

/*
 * Returns non-zero if it failed to throw the ConditionException, in which case the caller
 * should check for a pending Java exception.
 *
 * @param vmthread
 * @param ceArgs	contains all the parameters required by com.ibm.le.conditionhandling.ConditionHandler()
 * 					some of these need to be cast/converted to types understood by the JNI.
*/
static UDATA
throwConditionException(J9VMThread *vmThread, struct ConditionExceptionConstructorArgs *ceArgs)
{
	JNIEnv *env = (JNIEnv *) vmThread;
	jclass cls;
	jmethodID cid;
	jobject ceObject;
	jint throwResult;

	jstring failingRoutineJString;
	jstring facilityIDJString;
	jbyteArray rawTokenBytesJBArray;
	char *failingRoutine = "";

	UDATA rc = 1;

	if ((*env)->ExceptionCheck(env)) {
		goto cleanup;
	}

	rawTokenBytesJBArray = (*env)->NewByteArray(env, ceArgs->lenBytesRawTokenBytes);
	if (NULL == rawTokenBytesJBArray) {
		goto cleanup;
	}
	(*env)->SetByteArrayRegion(env, rawTokenBytesJBArray, 0, ceArgs->lenBytesRawTokenBytes, (jbyte *)ceArgs->rawTokenBytes);
	if ((*env)->ExceptionCheck(env)) {
		goto cleanup;
	}

	if (NULL != ceArgs->failingRoutine) {
		failingRoutine = ceArgs->failingRoutine;
	}
	failingRoutineJString = (*env)->NewStringUTF(env, ceArgs->failingRoutine);
	if (NULL == failingRoutineJString) {
		goto cleanup;
	}

	facilityIDJString = (*env)->NewStringUTF(env, ceArgs->facilityID);
	if (NULL == facilityIDJString) {
		goto cleanup;
	}

	cls = (*env)->FindClass(env, "com/ibm/le/conditionhandling/ConditionException");
	if (NULL == cls) {
		goto cleanup;
	}

	cid = (*env)->GetMethodID(env, cls, "<init>", "(Ljava/lang/String;J[BLjava/lang/String;IIIIIJ)V");
	if (NULL == cid) {
		goto cleanup;
	}

	ceObject = (*env)->NewObject(env, cls, cid, failingRoutineJString, ceArgs->offset, rawTokenBytesJBArray,
							facilityIDJString, ceArgs->c1, ceArgs->c2, ceArgs->caze,
							ceArgs->severity, ceArgs->control, ceArgs->iSInfo);
	if (NULL == ceObject) {
		goto cleanup;
	}

	throwResult = (*env)->Throw(env, ceObject);
	if (0 != throwResult) {
		goto cleanup;
	}

	/* success */
	rc = 0;

cleanup:

	if (NULL != failingRoutineJString) {
		(*env)->DeleteLocalRef(env,failingRoutineJString);
	}
	if (NULL != facilityIDJString) {
		(*env)->DeleteLocalRef(env,facilityIDJString);
	}
	if (NULL != rawTokenBytesJBArray) {
		(*env)->DeleteLocalRef(env,rawTokenBytesJBArray);
	}
	if (NULL != cls) {
		(*env)->DeleteLocalRef(env,cls);
	}
	if (NULL != ceObject) {
		(*env)->DeleteLocalRef(env,ceObject);
	}

	return rc;
}

/*
 * Prepares to resume execution
 *
 * Return K8ZOS_RESUME_EXECUTION if you want to resume execution, K8ZOS_GO_DOWN if there was a failure
 */
static UDATA
prepareToResumeBackInJava(struct J9PortLibrary* portLibrary, J9VMThread *vmThread, void *sigInfo)
{

	void *infoValue;
	const char* infoName;
	U_32 infoType;
	long builderDSA;
	UDATA *builderGPRs;
	U_64 *builderFPRs;
	UDATA i;
	long targetAddress;
	PORT_ACCESS_FROM_PORT(portLibrary);

	if ((vmThread->omrVMThread->vmState) == J9VMSTATE_JNI_FROM_JIT) {
		builderGPRs = vmThread->entryLocalStorage->jitGlobalStorageBase;
		builderFPRs = (U_64 *)vmThread->entryLocalStorage->jitFPRegisterStorageBase;
	} else if ((vmThread->omrVMThread->vmState) == J9VMSTATE_JNI){
		builderGPRs = vmThread->entryLocalStorage->ceehdlrGPRBase;
		builderFPRs = (U_64 *)vmThread->entryLocalStorage->ceehdlrFPRBase;
	} else {
		Assert_VM_unreachable();
	}

	builderDSA = builderGPRs[4];
	targetAddress = getReturnAddressOfJNINative((void *)builderDSA);

	if (-1 == targetAddress) {
		return K8ZOS_GO_DOWN;
	}

	/* gprs 1-3 need to be set to zero so that the return value of the native isn't mistaken for a valid JNI ref */
	for (i = 1 ; i <=3 ; i ++) {
		infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, i, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return K8ZOS_GO_DOWN;
		}
		*(UDATA *)infoValue = 0;
	}

	/* low gprs */
	for (i = 4 ; i <=15 ; i++) {
		infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, i, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return K8ZOS_GO_DOWN;
		}
		*(UDATA *)infoValue = builderGPRs[i];
	}

#if !defined(J9VM_ENV_DATA_64)
	/* high gprs */
	for (i = 16 ; i <=31 ; i ++) {
		infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, i, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
			return K8ZOS_GO_DOWN;
		}
		*(UDATA *)infoValue = builderGPRs[i];
	}
#endif

	/* FPRs */
	for (i = 0 ; i <=15 ; i++) {
		infoType = j9sig_info(sigInfo, J9PORT_SIG_FPR, i, &infoName, &infoValue);
		if (infoType != J9PORT_SIG_VALUE_64) {
			return K8ZOS_GO_DOWN;
		}
		*(U_64 *)infoValue = builderFPRs[i];
	}

	infoType = j9sig_info(sigInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_S390_FPC, &infoName, &infoValue);
	if (infoType != J9PORT_SIG_VALUE_32) {
		return K8ZOS_GO_DOWN;
	}
	*(U_32 *)infoValue = *((U_32 *)vmThread->entryLocalStorage->ceehdlrFPCLocation);

	infoType = j9sig_info(sigInfo, J9PORT_SIG_GPR, 7, &infoName, &infoValue);
	if (infoType != J9PORT_SIG_VALUE_ADDRESS) {
		return K8ZOS_GO_DOWN;
	}
	*(UDATA *)infoValue = targetAddress;

	return K8ZOS_RESUME_EXECUTION;
}

/*
 * If the condition corresponding to the gpInfo token
 * is between CEE 3208-3234 inclusive and condition occurred while executing a JNI native
 * create a com.ibm.le.conditionhandling.ConditionException, throw it,
 * and K8ZOS_RESUME_EXECUTION indicating to the caller that the signal handler
 * should return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION
 *
 * If the condition is sev 0 or 1, return K8ZOS_CONTINUE_SEARCH so that the condition is percolated.
 *
 * If a problem occurs, return K8ZOS_GO_DOWN, indicating to the caller that it should proceed to generate
 * diagnostics and shut down.
 *
 * @param portLibrary
 * @param thread
 * @param goInfo
 *
 * @return
 * 		K8ZOS_RESUME_EXECUTION if the signal handler should continue execution
 *	 	K8ZOS_CONTINUE_SEARCH if you want the signal handler to continue searching,
 * 		K8ZOS_GO_DOWN if you want to follow the crash route.
 *
 */
static UDATA
handleJNIZOSLECondition(struct J9PortLibrary* portLibrary, J9VMThread *vmThread, void* gpInfo) {

	J9JavaVM *vm = vmThread->javaVM;
	J9VMEntryLocalStorage* els = vmThread->entryLocalStorage;
	void *infoValue;
	const char *infoName;
	U_32 infoType;
	U_32 infoKind;
	U_16 severity;
	char *facilityID;
	PORT_ACCESS_FROM_PORT(portLibrary);

	if ( (vmThread->omrVMThread->vmState & J9VMSTATE_MAJOR) == J9VMSTATE_JNI) {

		infoType = j9sig_info(gpInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_ZOS_CONDITION_SEVERITY, &infoName, &infoValue);
		if (J9PORT_SIG_VALUE_16 == infoType) {
			severity = *(U_16 *)infoValue;
		} else {
			return K8ZOS_GO_DOWN;
		}

		/* Check to see if the condition is in the range CEE 3208-3234 inclusive */
		infoType = j9sig_info(gpInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_ZOS_CONDITION_FACILITY_ID, &infoName, &infoValue);
		if (J9PORT_SIG_VALUE_STRING == infoType) {
			facilityID = (char *)infoValue;
		} else {
			return K8ZOS_GO_DOWN;
		}

		if ( (0 == strncmp("CEE",facilityID,3)) && (3 == severity) ) {
			U_16 messageNumber;

			infoType = j9sig_info(gpInfo, J9PORT_SIG_SIGNAL, J9PORT_SIG_SIGNAL_ZOS_CONDITION_MESSAGE_NUMBER, &infoName, &infoValue);
			if (J9PORT_SIG_VALUE_16 != infoType) {
				return K8ZOS_GO_DOWN;
			}
			messageNumber = *(U_16 *)infoValue;

			if ( ((3208 <= messageNumber) &&  (messageNumber <= 3234)) ) {
				struct ConditionExceptionConstructorArgs ceArgs;
				UDATA prepJmpRC = 0;
				UDATA throwRC = 0;

				/* convert the condition to an exception */
				getConditionExceptionConstructorArgs(portLibrary, gpInfo, &ceArgs);
				throwRC = throwConditionException(vmThread, &ceArgs);

				/* free memory allocated by e2a_func() */
				if (NULL != ceArgs.failingRoutine) {
					free(ceArgs.failingRoutine);
				}
				if (0 != throwRC) {
					return K8ZOS_GO_DOWN;
				}
				prepJmpRC = prepareToResumeBackInJava(portLibrary, vmThread, gpInfo);

				if (K8ZOS_RESUME_EXECUTION == prepJmpRC) {

					/* record that we converted a LE Condition to a Java Exception */
					vmThread->javaVM->leConditionConvertedToJavaException = 1;
					vmThread->leConditionConvertedToJavaException = 1;

					return K8ZOS_RESUME_EXECUTION;
				}
			}
		}
	}

	return K8ZOS_GO_DOWN;
}


#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT  */

 
#if defined(J9VM_OPT_SWITCH_STACKS_FOR_SIGNAL_HANDLER)

UDATA swapStacksAndRunHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, J9VMThread* userData);
  
 /**
  * @internal
  *
  * The generic VM signal handler. 
 * Assumes the void* parameter is a J9JavaVM.
 */
UDATA
structuredSignalHandlerVM(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	void *handlerParam = NULL;
	J9JavaVM *javaVM = userData;
	J9VMThread* vmThread = currentVMThread(javaVM);
 	/* We may not always be executing on a Java thread! */
 	if (NULL == vmThread) {
 		handlerParam = javaVM;
 	} else {
 		handlerParam = vmThread;
 	}
 	return structuredSignalHandler(portLibrary, gpType, gpInfo, handlerParam);
}

 /**
  * @internal
  *
  * The generic VM signal handler.
  * Assumes the void* parameter is a J9VMThread.
  */
 UDATA
 structuredSignalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData)
 {
	 J9VMThread *vmThread = userData;

	/* See if it *is* actually a thread and not a VM pointer! */
	if ((NULL != vmThread) && ( ((J9JavaVM*) vmThread) != vmThread->javaVM)) {
		/* Figure out the bounds of the Java stack */
		J9JavaStack* javaStack = vmThread->stackObject;
		UDATA* lowestSlot =  J9_LOWEST_STACK_SLOT(vmThread);
		UDATA* highestSlot = javaStack->end;
		UDATA* localAddress = (UDATA*)&highestSlot;

		/* if our local variables are located in the Java stack we know that an ESP flip is required */
		if ((localAddress >= lowestSlot) && (localAddress < highestSlot)) {
			return swapStacksAndRunHandler(portLibrary, gpType, gpInfo, vmThread);
		}
	}

	return vmSignalHandler(portLibrary, gpType, gpInfo, vmThread);
 }
#else
#define vmSignalHandler structuredSignalHandler
#define vmSignalHandlerVM structuredSignalHandlerVM
#endif

#if defined(WIN64)

 /* return non-zero if we should defer to a try/except handler */
 static UDATA
 checkForTryExceptHandler(struct J9PortLibrary* portLibrary, void* userData)
 {

	PORT_ACCESS_FROM_PORT(portLibrary);
	J9CrashData* data = userData;
	void* gpInfo = data->gpInfo;
	const char* name;
	U_32 *value;
	U_32 infoKind;

	infoKind = j9sig_info(gpInfo, J9PORT_SIG_OTHER, J9PORT_SIG_WINDOWS_DEFER_TRY_EXCEPT_HANDLER, &name, &value);

	if (infoKind != J9PORT_SIG_VALUE_32) {
		/* the call failed, do not defer to any handlers */
		return 0;
	} 
	
	if ( 0 != *value) {
		/* there is a try/except handler */
		return 1;
	}

	return 0;

 }

#endif /* WIN64 */

/**
 * @internal
 *
 * The generic VM signal handler. 
  * Assumes the void* parameter is a J9JavaVM.
  */
 UDATA
 vmSignalHandlerVM(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData)
 {
 	void *handlerParam = NULL;
 	J9JavaVM *javaVM = userData;
 	J9VMThread* vmThread = currentVMThread(javaVM);
 	/* We may not always be executing on a Java thread! */
 	if (NULL == vmThread) {
 		handlerParam = javaVM;
 	} else {
 		handlerParam = vmThread;
 	}
 	return vmSignalHandler(portLibrary, gpType, gpInfo, handlerParam);
 }


/**
 * @internal
 *
 * The generic VM signal handler. 
 */
UDATA
vmSignalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
 	J9VMThread *vmThread = userData;
 	J9JavaVM *vm = NULL;

	J9CrashData crashData;
	UDATA result;
	PORT_ACCESS_FROM_PORT(portLibrary);
	char consoleOutputBuf[3560]; /* Reduced from 4096 so that stack frame fits in one page */
	J9RecursiveCrashData recursiveCrashData;
#ifdef J9VM_RAS_EYECATCHERS
	J9RASCrashInfo rasCrashInfo;
#endif

	/* We no longer have J9VMToken, but we must still use the second slot to 
	 * figure out whether we were passed a VM or a Thread in this case, because the stack swap code
	 * calls this handler with a thread, but in some cases it may be called with a VM only (e.g. Signal Handler thread).
	 */
	
	/* Case 1: It's not actually a thread, but a VM pointer! */
	if (((J9JavaVM*) vmThread) == vmThread->javaVM) {
		vm = (J9JavaVM*) vmThread;
		vmThread = NULL;
	/* Case 2: It's a proper VM Thread, get the VM pointer. */
	} else {
		vm = vmThread->javaVM;
	}
		
	memset(&recursiveCrashData, 0, sizeof(J9RecursiveCrashData));

#if defined(J9VM_INTERP_NATIVE_SUPPORT) && !defined(J9ZTPF)
	/* give the JIT a chance to recover from the exception */
	if (NULL != vmThread) {
		J9JITConfig *jitConfig = vm->jitConfig;

		if (jitConfig != NULL) {
			if (jitConfig->jitSignalHandler != NULL) {
				if (jitConfig->jitSignalHandler(vmThread, gpType, gpInfo) == J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION) {
					return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
				}
			}
		}
	}
#endif /* defined(J9VM_INTERP_NATIVE_SUPPORT) && !defined(J9ZTPF) */

#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
	if (J9_SIG_ZOS_CEEHDLR == (vm->sigFlags & J9_SIG_ZOS_CEEHDLR)) {
		if (NULL != vmThread) {
			UDATA handleJNIRC = 0;

			if (J9_SIG_PERCOLATE_CONDITIONS == (vm->sigFlags & J9_SIG_PERCOLATE_CONDITIONS)) {
				if (0 != vmThread->percolatedConditionAndTriggeredDiagnostics) {
					/* we've already generated RAS diagnostics for this, percolate the condition */
					return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
				} 

				/* fall through to generating RAS diagnostics */

			} else {

				handleJNIRC = handleJNIZOSLECondition(portLibrary, vmThread, gpInfo);

				if (K8ZOS_RESUME_EXECUTION == handleJNIRC) {
					return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
				} else if (K8ZOS_CONTINUE_SEARCH == handleJNIRC) {
					return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
				} else {
					/* fall through, continue down normal crash path */
				}
			}
		}
	}
#endif

#if defined(J9VM_INTERP_USE_UNSAFE_HELPER)
	if ((J9PORT_SIG_FLAG_SIGBUS == (gpType & J9PORT_SIG_FLAG_SIGALLSYNC))
#if defined(J9ZOS390)
		|| (J9PORT_SIG_FLAG_SIGABEND == (gpType & J9PORT_SIG_FLAG_SIGALLSYNC))
#endif /* defined(J9ZOS390) */
	) {
		J9VMThread *currentThread = ((NULL == vmThread) ? vm->internalVMFunctions->currentVMThread(vm): vmThread);
		
		if (NULL != currentThread) {
			if (J9_ARE_ALL_BITS_SET(currentThread->privateFlags2, J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS)) {
				if (J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION == sigBusHandler(currentThread, gpInfo)) {
					return J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION;
				}
			}
		}
	}
#endif /* defined(J9VM_INTERP_USE_UNSAFE_HELPER) */
#if defined(J9ZOS390)
	if (J9PORT_SIG_FLAG_SIGABEND == (gpType & J9PORT_SIG_FLAG_SIGALLSYNC)) {
		/* percolate the condition */
		return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
	}
#endif /* defined(J9ZOS390) */

	crashData.javaVM = vm;
	crashData.vmThread = vmThread;
	crashData.gpType = gpType;
	crashData.gpInfo = gpInfo;
	crashData.consoleOutputBuf = consoleOutputBuf;
	crashData.sizeofConsoleOutputBuf = sizeof(consoleOutputBuf);

#if defined(WIN64)

	result = 0;
	recursiveCrashData.protectedFunctionName = "checkForTryExceptHandler";
	j9sig_protect(
			checkForTryExceptHandler, &crashData,
			recursiveCrashHandler, &recursiveCrashData,
			J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGALLSYNC,
			&result);

	if (0 != result) {
		return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
	}

#endif

#ifdef J9VM_RAS_EYECATCHERS
	crashData.rasCrashInfo = &rasCrashInfo;

	recursiveCrashData.protectedFunctionName = "setupRasCrashInfo";
	j9sig_protect(
			setupRasCrashInfo, &crashData,
			recursiveCrashHandler, &recursiveCrashData,
			J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGALLSYNC,
			&result);
#endif

	recursiveCrashData.protectedFunctionName = "writeCrashDataToConsole";
	j9sig_protect(
		writeCrashDataToConsole, &crashData,
		recursiveCrashHandler, &recursiveCrashData,
		J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGALLSYNC,
		&result);

	recursiveCrashData.protectedFunctionName = "generateDiagnosticFiles";
	j9sig_protect(
		generateDiagnosticFiles, &crashData,
		recursiveCrashHandler, &recursiveCrashData,
		J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGALLSYNC,
		&result);

	recursiveCrashData.protectedFunctionName = "reportThreadCrash";
	j9sig_protect(
		reportThreadCrash, &crashData,
		recursiveCrashHandler, &recursiveCrashData,
		J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGALLSYNC,
		&result);

	recursiveCrashData.protectedFunctionName = "executeAbortHook";
	j9sig_protect(
		executeAbortHook, &crashData, 
		recursiveCrashHandler, &recursiveCrashData,
		J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGALLSYNC,
		&result);


#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
	if (J9PORT_SIG_OPTIONS_ZOS_USE_CEEHDLR == (J9PORT_SIG_OPTIONS_ZOS_USE_CEEHDLR & j9sig_get_options())) {
		
		if (J9_SIG_PERCOLATE_CONDITIONS == (vm->sigFlags & J9_SIG_PERCOLATE_CONDITIONS)) {

			/* record that we already triggered diagnostics, so that any le condition handlers further down the call chain do not */
			vmThread->percolatedConditionAndTriggeredDiagnostics = 1;
			vmThread->privateFlags |= J9_PRIVATE_FLAGS_STACKS_OUT_OF_SYNC;
			return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
			
		} else {
			_INT4 code = PORT_ABEND_CODE;
			_INT4 reason = 0; /* arbitrarily chosen reason code*/
			_INT4 cleanup = 0; /* 0 - issue the abend without clean-up */
		
			CEE3AB2(&code, &reason, &cleanup);
		}
	}
#endif

#if defined(J9ZOS390)
	if (J9_SIG_POSIX_COOPERATIVE_SHUTDOWN == (J9_SIG_POSIX_COOPERATIVE_SHUTDOWN & vm->sigFlags)) {
		return J9PORT_SIG_EXCEPTION_COOPERATIVE_SHUTDOWN;
	}
#endif

	j9exit_shutdown_and_exit(-1);

	/* even if we don't have signal support, we still need a minimal handler so that we can pass it in to j9sig_protect */
	return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
}


static void
generateSystemDump(struct J9PortLibrary* portLibrary, void* gpInfo)
{
	char dumpName[EsMaxPath];
	UDATA dumpCreateReturnCode;
	IDATA getEnvForDumpReturnCode;
	PORT_ACCESS_FROM_PORT(portLibrary);

	getEnvForDumpReturnCode = j9sysinfo_get_env(J9DO_NOT_WRITE_DUMP, NULL, 0);
	if (getEnvForDumpReturnCode != 0) {
		/* failed to find the env var, so write the dump */
		*dumpName = '\0';
#ifdef LINUX
		/* Ensure that userdata argument is NULL on Linux. */
		dumpCreateReturnCode = j9dump_create(dumpName, NULL, NULL);
#else
		dumpCreateReturnCode = j9dump_create(dumpName, NULL, gpInfo);
#endif

		if (dumpCreateReturnCode == 0) {
			j9tty_printf(PORTLIB, "\nGenerated system dump: %s\n", dumpName);
		} else {
			/* failed, error message in dumpName */
			j9tty_err_printf(PORTLIB, "\nError: %s\n", dumpName);
		}
	} else {
		j9tty_err_printf(PORTLIB, "\nSystem dump not written since \""J9DO_NOT_WRITE_DUMP"\" was defined in the environment\n");
	}
}

static const char*
getSignalDescription(struct J9PortLibrary* portLibrary, U_32 gpType)
{
	switch (gpType & J9PORT_SIG_FLAG_SIGALLSYNC) {
	case J9PORT_SIG_FLAG_SIGSEGV:
		return "Segmentation error";
	case J9PORT_SIG_FLAG_SIGBUS:
		return "Bus error";
	case J9PORT_SIG_FLAG_SIGILL:
		return "Illegal instruction";
	case J9PORT_SIG_FLAG_SIGFPE:
		return "Floating point error";
	case J9PORT_SIG_FLAG_SIGTRAP:
		return "Unhandled trap";
#if defined(J9ZOS390)
	case J9PORT_SIG_FLAG_SIGABEND:
		return "Abend error";
#endif /* defined(J9ZOS390) */
	default:
		return "Unknown error";
	}
}

static UDATA
writeGPInfo(struct J9PortLibrary* portLibrary, void *writeGPInfoCrashData)
{

	J9WriteGPInfoData *crashData = writeGPInfoCrashData;
	PORT_ACCESS_FROM_PORT(portLibrary);
	char *cursor = crashData->s;
	UDATA length = crashData->length;
	void *gpInfo = crashData->gpInfo;
	U_32 category = crashData->category;
	UDATA numBytesWritten = 0;
	U_32 info;
	U_32 infoCount;
	UDATA n;

	infoCount = j9sig_info_count(gpInfo, category);
	for (info = 0; info < infoCount; info++) {
		char c;
		U_32 infoKind;
		void *value;
		const char* name;

		infoKind = j9sig_info(gpInfo, category, info, &name, &value);
		if (((info%INFO_COLS)==INFO_COLS-1) || (info==infoCount-1)  || (infoKind == J9PORT_SIG_VALUE_STRING)  || (infoKind == J9PORT_SIG_VALUE_FLOAT_64)) {
			c = '\n';
		} else {
			c = ' ';
		}

		switch (infoKind) {
		case J9PORT_SIG_VALUE_16:
				n = j9str_printf(PORTLIB,  cursor, length, "%s=%04X%c", name, *(U_16 *)value, c);
			break;
		case J9PORT_SIG_VALUE_32:
				n = j9str_printf(PORTLIB, cursor , length, "%s=%08.8x%c", name, *(U_32 *)value, c);
			break;
		case J9PORT_SIG_VALUE_64:
				n = j9str_printf(PORTLIB, cursor, length,"%s=%016.16llx%c", name, *(U_64 *)value, c);
			break;
		case J9PORT_SIG_VALUE_128:
			{
				const U_128 * const v = (U_128 *) value;
				const U_64 h = v->high64;
				const U_64 l = v->low64;

				n = j9str_printf(PORTLIB, cursor, length,"%s=%016.16llx%016.16llx%c", name, h, l, c);
			}
			break;
		case J9PORT_SIG_VALUE_STRING:
				n = j9str_printf(PORTLIB,  cursor, length, "%s=%s%c", name, (char *)value, c);
			break;
		case J9PORT_SIG_VALUE_ADDRESS:
				n = j9str_printf(PORTLIB,  cursor, length, "%s=%p%c", name, *(void**)value, c);
			break;
		case J9PORT_SIG_VALUE_FLOAT_64:
			/* make sure when casting to a float that we get least significant 32-bits. */
				n = j9str_printf(PORTLIB, cursor, length,"%s %016.16llx (f: %f, d: %e)%c", name, *(U_64 *)value, (float)LOW_U32_FROM_DBL_PTR(value), *(double *)value, c);
			break;
		case J9PORT_SIG_VALUE_UNDEFINED:
		default:
				n = j9str_printf(PORTLIB,  cursor, length, "%s=<UNDEFINED>%c", name, c);
			break;
		}

		if (n > length) {
			length = 0;
		} else {
			length -= n;
			cursor += n;
		}
		numBytesWritten += n;
	}

	return numBytesWritten;
}

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
static UDATA
writeJITInfo(J9VMThread* vmThread, char* s, UDATA length, void* gpInfo)
{
	J9JavaVM* vm = vmThread->javaVM;
	UDATA n;
	J9JITConfig *jitConfig = vm->jitConfig;
	J9JITExceptionTable *jitExceptionTable;
	UDATA pc;
	PORT_ACCESS_FROM_VMC(vmThread);
	U_32 infoKind;
	void *value;
	const char* name;
	UDATA numBytesWritten = 0;

	if (jitConfig == NULL) {
		return numBytesWritten;
	}

	if((vmThread->omrVMThread->vmState & J9VMSTATE_MAJOR) == J9VMSTATE_JIT_CODEGEN) {
		if(vmThread->jitMethodToBeCompiled) {
			J9Method *ramMethod = vmThread->jitMethodToBeCompiled;
			J9Class *clazz = J9_CLASS_FROM_METHOD(ramMethod);
			J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
			J9UTF8 *methName = J9ROMMETHOD_GET_NAME(clazz->romClass, romMethod);
			J9UTF8 *methSig = J9ROMMETHOD_GET_SIGNATURE(clazz->romClass, romMethod);
			J9UTF8 *className = J9ROMCLASS_CLASSNAME(clazz->romClass);

			n = j9str_printf(PORTLIB, s, length, "\nMethod_being_compiled=%.*s.%.*s%.*s\n",
					(UDATA)J9UTF8_LENGTH(className), J9UTF8_DATA(className),
					(UDATA)J9UTF8_LENGTH(methName), J9UTF8_DATA(methName),
					(UDATA)J9UTF8_LENGTH(methSig), J9UTF8_DATA(methSig));
		} else {
			n = j9str_printf(PORTLIB, s, length, "\nMethod_being_compiled=<unknown>\n");
		}
		numBytesWritten += n;
		return numBytesWritten;
	}

	/*
	 * Determine if the program counter is in a compiled java method.
	 */
	infoKind = j9sig_info(gpInfo, J9PORT_SIG_CONTROL, J9PORT_SIG_CONTROL_PC, &name, &value);
	if (infoKind == J9PORT_SIG_VALUE_ADDRESS) {
		pc = *(UDATA*)value;

		if (jitConfig->jitGetExceptionTableFromPC == NULL) {
			return numBytesWritten;
		}
		jitExceptionTable = jitConfig->jitGetExceptionTableFromPC(vmThread, pc);

		if (jitExceptionTable) {
			J9Method *ramMethod = jitExceptionTable->ramMethod;
			J9Class *clazz = J9_CLASS_FROM_METHOD(ramMethod);
			J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
			J9ROMClass *romClass = clazz->romClass;
			J9UTF8 *methName = J9ROMMETHOD_GET_NAME(romClass, romMethod);
			J9UTF8 *methSig = J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod);
			J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);

			n = j9str_printf(PORTLIB, s, length, "\nCompiled_method=%.*s.%.*s%.*s\n",
					(UDATA)J9UTF8_LENGTH(className), J9UTF8_DATA(className),
					(UDATA)J9UTF8_LENGTH(methName), J9UTF8_DATA(methName),
					(UDATA)J9UTF8_LENGTH(methSig), J9UTF8_DATA(methSig));
			numBytesWritten += n;
		} else {
			/* scan code segments to see if we're in a segment but somehow not in a method */
			MEMORY_SEGMENT_LIST_DO(jitConfig->codeCacheList, seg);
			if((pc >= (UDATA) seg->heapBase) && (pc < (UDATA) seg->heapTop)) {
				n = j9str_printf(PORTLIB, s, length, "\nCompiled_method=unknown (In JIT code segment %p but no method found)\n", seg);
				numBytesWritten += n;
				return numBytesWritten;
			}
			END_MEMORY_SEGMENT_LIST_DO(seg);
		}
	}
	return numBytesWritten;
}
#endif /* INTERP_NATIVE_SUPPORT */

static UDATA
writeVMInfo(J9JavaVM* vm, char* s, UDATA length)
{
	J9VMInitArgs *j9vm_args = vm->vmArgsArray;
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA numBytesWritten = 0;

	if (j9vm_args) {
		U_32 numOptions = j9vm_args->actualVMArgs->nOptions;
		U_32 optionCount;
		UDATA n;

		/* show number of options */
		n = j9str_printf(PORTLIB, s, length, "\nJavaVMInitArgs.nOptions=%i:\n", numOptions);  
		length -= n;
		s += n;
		numBytesWritten += n;

		for (optionCount = 0; optionCount < numOptions; optionCount++) {
			n = j9str_printf(PORTLIB, s, length, "    %s", j9vm_args->actualVMArgs->options[optionCount].optionString);
			length -= n;
			s += n;
			numBytesWritten += n;

			/* append option with only non-zero extra info */
			if (j9vm_args->actualVMArgs->options[optionCount].extraInfo != 0) {
				n = j9str_printf(PORTLIB, s, length, " (extra info: %p)\n", j9vm_args->actualVMArgs->options[optionCount].extraInfo);
			} else {
				n = j9str_printf(PORTLIB, s, length, "\n");
			}
			numBytesWritten += n;

			length -= n;
			s += n;
		}
	}
	return numBytesWritten;
}

static UDATA
generateDiagnosticFiles(struct J9PortLibrary* portLibrary, void* userData)
{

	UDATA gpHaveRASdump = 0;
	J9CrashData* data = userData;
	J9JavaVM* vm = data->javaVM;
	J9VMThread *vmThread = data->vmThread;
	void* gpInfo = data->gpInfo;

#ifdef J9VM_RAS_DUMP_AGENTS
	gpHaveRASdump = ( vm->j9rasDumpFunctions && vm->j9rasDumpFunctions->reserved != 0 );
#endif

	/* Generate primary crash dump (but only if RASdump is not activated) */
	if (!gpHaveRASdump) {
		generateSystemDump(portLibrary, gpInfo);
	}

#ifdef J9VM_RAS_DUMP_AGENTS
	if (vmThread) {
		vmThread->gpInfo = gpInfo;
		printBacktrace(vm, gpInfo);
	}

	/* Trigger dump only if RASdump is activated */
	if (gpHaveRASdump) {
		J9DMP_TRIGGER( vm, vmThread, J9RAS_DUMP_ON_GP_FAULT );
	}
#else
	gpThreadDump(vm, vmThread);
#endif

	return 0;

}

#ifdef J9VM_RAS_EYECATCHERS
static UDATA
setupRasCrashInfo(struct J9PortLibrary* portLibrary, void* userData)
{
	J9CrashData* data = userData;
	J9JavaVM* vm = data->javaVM;
	J9VMThread *vmThread = data->vmThread;
	J9RASCrashInfo *rasCrashInfo = data->rasCrashInfo;

	rasCrashInfo->failingThread = vmThread;
	rasCrashInfo->failingThreadID = ((J9AbstractThread*)omrthread_self())->tid;
	rasCrashInfo->gpInfo = data->consoleOutputBuf;
	vm->j9ras->crashInfo = rasCrashInfo;

	return 0;
}
#endif

static UDATA 
writeCrashDataToConsole(struct J9PortLibrary* portLibrary, void* userData)
{
	J9CrashData* data = userData;
	J9JavaVM* vm = data->javaVM;
	J9VMThread *vmThread = data->vmThread;
	void* gpInfo = data->gpInfo;
	U_32 gpType = data->gpType;
	char *consoleOutputBuf = data->consoleOutputBuf;
	size_t sizeofConsoleOutputBuf = data->sizeofConsoleOutputBuf;
	UDATA length = sizeofConsoleOutputBuf;
	const char* description;
	char *bufStart, *s;
	UDATA gpHaveRASdump = 0;
	UDATA buffTrimAmount = 0;
	J9WriteGPInfoData writeGPInfoCrashData;
	J9RecursiveCrashData recursiveCrashData;
	U_32 category;
	PORT_ACCESS_FROM_PORT(portLibrary);

	s = consoleOutputBuf;
	*s = '\0';
	bufStart = s;

#ifdef J9VM_RAS_DUMP_AGENTS
	gpHaveRASdump = ( vm->j9rasDumpFunctions && vm->j9rasDumpFunctions->reserved != 0 );
#endif

	description = getSignalDescription(portLibrary, gpType);
	j9tty_printf(portLibrary, "Unhandled exception\nType=%s vmState=0x%08.8x\n", description, vmThread ? vmThread->omrVMThread->vmState : -1);

	/* the usable length of the buffer might have been decreased for 8-alignment and to make space for eyecatchers but by how much? */
	buffTrimAmount = bufStart - consoleOutputBuf;

	writeGPInfoCrashData.gpInfo = gpInfo;
	recursiveCrashData.protectedFunctionName = "writeGPInfo";

	for (category = 0; category < J9PORT_SIG_NUM_CATEGORIES; category++) {

		UDATA result;
		
		writeGPInfoCrashData.s = s;
		writeGPInfoCrashData.length = length;
		writeGPInfoCrashData.category = category;

		recursiveCrashData.info = category;

		j9sig_protect(
			writeGPInfo, &writeGPInfoCrashData,
			recursiveCrashHandler, &recursiveCrashData,
			J9PORT_SIG_FLAG_MAY_RETURN | J9PORT_SIG_FLAG_SIGALLSYNC,
			&result);

		if (0 < result) {
			/* make absolutely sure that the buffer is NULL terminated */
			consoleOutputBuf[sizeofConsoleOutputBuf - 1] = '\0';
			/* dump out what we've gotten so far */
			j9tty_printf(portLibrary, "%s", s);

			length = sizeofConsoleOutputBuf - buffTrimAmount - strlen(bufStart);
			s = consoleOutputBuf + sizeofConsoleOutputBuf - length;
		}
	}

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (vmThread) {
		if(writeJITInfo(vmThread, s, length, gpInfo) > 0) {

			/* make absolutely sure that the buffer is NUL terminated */
			consoleOutputBuf[sizeofConsoleOutputBuf-1]='\0';
			/* dump out what we've gotten so far */
			j9tty_printf(portLibrary, "%s", s);

			length = sizeofConsoleOutputBuf - buffTrimAmount - strlen(bufStart);
			s = consoleOutputBuf + sizeofConsoleOutputBuf - length;
		}
	}
#endif  /* J9VM_INTERP_NATIVE_SUPPORT */

	dumpVmDetailString(portLibrary, vm);

	/* get the vm options (but only if RASdump not activated) */
	if (!gpHaveRASdump) {
		if (writeVMInfo(vm, s, length) > 0) {

			/* make absolutely sure that the buffer is NUL terminated */
			consoleOutputBuf[sizeofConsoleOutputBuf - 1] = '\0';
			/* dump out what we've gotten so far */
			j9tty_printf(portLibrary, "%s", s);

			length = sizeofConsoleOutputBuf - buffTrimAmount - strlen(bufStart);
			s = consoleOutputBuf + sizeofConsoleOutputBuf - length;
		}
	}

	return 0;
}

static UDATA
recursiveCrashHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* recursiveCrashHandlerData)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	J9RecursiveCrashData *data = recursiveCrashHandlerData;

	/* a second crash has occurred while we were attempting to dump crash data */
	j9tty_printf(portLibrary, "\nUnhandled exception in signal handler. Protected function: %s (0x%X)\n\n",
			data->protectedFunctionName != NULL ? (char *)data->protectedFunctionName : "unknown", data->info);

	return J9PORT_SIG_EXCEPTION_RETURN;
}

static UDATA 
executeAbortHook(struct J9PortLibrary* portLibrary, void* userData)
{
	J9CrashData* data = userData;
	J9JavaVM* vm = data->javaVM;

	if (vm->abortHook) {
		vm->abortHook();
	}

	return 0;
}

#ifdef J9VM_RAS_DUMP_AGENTS
static void
printBacktrace(struct J9JavaVM *vm, void* gpInfo)
{
	J9PlatformThread thread;
	J9PlatformStackFrame *frame;

	PORT_ACCESS_FROM_JAVAVM(vm);

	memset(&thread, 0, sizeof(thread));

	j9tty_printf(PORTLIB, "----------- Stack Backtrace -----------\n");
	j9introspect_backtrace_thread(&thread, NULL, gpInfo);
	j9introspect_backtrace_symbols(&thread, NULL);

	frame = thread.callstack;
	while (frame) {
		J9PlatformStackFrame *tmp = frame;

		if (frame->symbol) {
			j9tty_printf(PORTLIB, "%s\n", frame->symbol);
			j9mem_free_memory(frame->symbol);
		} else {
			j9tty_printf(PORTLIB, "0x%p\n", (void*)frame->instruction_pointer);
		}

		frame = frame->parent_frame;
		j9mem_free_memory(tmp);
	}

	j9tty_printf(PORTLIB, "---------------------------------------\n");
}
#endif /* ifdef J9VM_RAS_DUMP_AGENTS */

#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
void
javaAndCStacksMustBeInSync(J9VMThread *vmThread, BOOLEAN fromJIT)
{
#define J9_GPHANDLE_ZOS_CEE3AB2_ILLEGAL_RESUME_DETECTED_BACK_INTO_JIT 0x10
#define J9_GPHANDLE_ZOS_CEE3AB2_ILLEGAL_RESUME_DETECTED 0x11

	_INT4 code = PORT_ABEND_CODE;
	_INT4 reason = 0;
	_INT4 cleanup = 0; /* 0 - issue the abend without clean-up */

	PORT_ACCESS_FROM_ENV(vmThread);

	if (fromJIT) {
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VM_ILLEGAL_RESUME_DETECTED_JIT);
		reason = J9_GPHANDLE_ZOS_CEE3AB2_ILLEGAL_RESUME_DETECTED_BACK_INTO_JIT;	/* arbitrarily chosen */
	} else {
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VM_ILLEGAL_RESUME_DETECTED);
		reason = J9_GPHANDLE_ZOS_CEE3AB2_ILLEGAL_RESUME_DETECTED; /* arbitrarily chosen */
	}

	j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VM_TERMINATING_PROCESS_USING_CEEAB2, code, reason, cleanup);
	CEE3AB2(&code, &reason, &cleanup); /* terminate the process, no chance for anyone to resume */
	
	/* can't get here */
}
#endif

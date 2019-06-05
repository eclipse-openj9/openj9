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

#include "jvmtiHelpers.h"
#include "jvmti_internal.h"

#if JAVA_SPEC_VERSION >= 9
#define JVMTI_9_ENTRY(name) name
#else /* JAVA_SPEC_VERSION >= 9 */
#define JVMTI_9_ENTRY(name) NULL
#endif /* JAVA_SPEC_VERSION >= 9 */

#if JAVA_SPEC_VERSION >= 11
#define JVMTI_11_ENTRY(name) name
#else /* JAVA_SPEC_VERSION >= 11 */
#define JVMTI_11_ENTRY(name) NULL
#endif /* JAVA_SPEC_VERSION >= 11*/

jvmtiNativeInterface jvmtiFunctionTable = {
	NULL,
	jvmtiSetEventNotificationMode,
	JVMTI_9_ENTRY(jvmtiGetAllModules),
	jvmtiGetAllThreads,
	jvmtiSuspendThread,
	jvmtiResumeThread,
	jvmtiStopThread,
	jvmtiInterruptThread,
	jvmtiGetThreadInfo,
	jvmtiGetOwnedMonitorInfo,
	jvmtiGetCurrentContendedMonitor,
	jvmtiRunAgentThread,
	jvmtiGetTopThreadGroups,
	jvmtiGetThreadGroupInfo,
	jvmtiGetThreadGroupChildren,
	jvmtiGetFrameCount,
	jvmtiGetThreadState,
	jvmtiGetCurrentThread,
	jvmtiGetFrameLocation,
	jvmtiNotifyFramePop,
	jvmtiGetLocalObject,
	jvmtiGetLocalInt,
	jvmtiGetLocalLong,
	jvmtiGetLocalFloat,
	jvmtiGetLocalDouble,
	jvmtiSetLocalObject,
	jvmtiSetLocalInt,
	jvmtiSetLocalLong,
	jvmtiSetLocalFloat,
	jvmtiSetLocalDouble,
	jvmtiCreateRawMonitor,
	jvmtiDestroyRawMonitor,
	jvmtiRawMonitorEnter,
	jvmtiRawMonitorExit,
	jvmtiRawMonitorWait,
	jvmtiRawMonitorNotify,
	jvmtiRawMonitorNotifyAll,
	jvmtiSetBreakpoint,
	jvmtiClearBreakpoint,
	JVMTI_9_ENTRY(jvmtiGetNamedModule),
	jvmtiSetFieldAccessWatch,
	jvmtiClearFieldAccessWatch,
	jvmtiSetFieldModificationWatch,
	jvmtiClearFieldModificationWatch,
	jvmtiIsModifiableClass,
	jvmtiAllocate,
	jvmtiDeallocate,
	jvmtiGetClassSignature,
	jvmtiGetClassStatus,
	jvmtiGetSourceFileName,
	jvmtiGetClassModifiers,
	jvmtiGetClassMethods,
	jvmtiGetClassFields,
	jvmtiGetImplementedInterfaces,
	jvmtiIsInterface,
	jvmtiIsArrayClass,
	jvmtiGetClassLoader,
	jvmtiGetObjectHashCode,
	jvmtiGetObjectMonitorUsage,
	jvmtiGetFieldName,
	jvmtiGetFieldDeclaringClass,
	jvmtiGetFieldModifiers,
	jvmtiIsFieldSynthetic,
	jvmtiGetMethodName,
	jvmtiGetMethodDeclaringClass,
	jvmtiGetMethodModifiers,
	NULL,
	jvmtiGetMaxLocals,
	jvmtiGetArgumentsSize,
	jvmtiGetLineNumberTable,
	jvmtiGetMethodLocation,
	jvmtiGetLocalVariableTable,
	jvmtiSetNativeMethodPrefix,
	jvmtiSetNativeMethodPrefixes,
	jvmtiGetBytecodes,
	jvmtiIsMethodNative,
	jvmtiIsMethodSynthetic,
	jvmtiGetLoadedClasses,
	jvmtiGetClassLoaderClasses,
	jvmtiPopFrame,
	jvmtiForceEarlyReturnObject,
	jvmtiForceEarlyReturnInt,
	jvmtiForceEarlyReturnLong,
	jvmtiForceEarlyReturnFloat,
	jvmtiForceEarlyReturnDouble,
	jvmtiForceEarlyReturnVoid,
	jvmtiRedefineClasses,
	jvmtiGetVersionNumber,
	jvmtiGetCapabilities,
	jvmtiGetSourceDebugExtension,
	jvmtiIsMethodObsolete,
	jvmtiSuspendThreadList,
	jvmtiResumeThreadList,
	JVMTI_9_ENTRY(jvmtiAddModuleReads),
	JVMTI_9_ENTRY(jvmtiAddModuleExports),
	JVMTI_9_ENTRY(jvmtiAddModuleOpens),
	JVMTI_9_ENTRY(jvmtiAddModuleUses),
	JVMTI_9_ENTRY(jvmtiAddModuleProvides),
	JVMTI_9_ENTRY(jvmtiIsModifiableModule),
	jvmtiGetAllStackTraces,
	jvmtiGetThreadListStackTraces,
	jvmtiGetThreadLocalStorage,
	jvmtiSetThreadLocalStorage,
	jvmtiGetStackTrace,
	NULL,
	jvmtiGetTag,
	jvmtiSetTag,
	jvmtiForceGarbageCollection,
	jvmtiIterateOverObjectsReachableFromObject,
	jvmtiIterateOverReachableObjects,
	jvmtiIterateOverHeap,
	jvmtiIterateOverInstancesOfClass,
	NULL,
	jvmtiGetObjectsWithTags,
	jvmtiFollowReferences,
	jvmtiIterateThroughHeap,
	NULL,
	NULL,
	NULL,
	jvmtiSetJNIFunctionTable,
	jvmtiGetJNIFunctionTable,
	jvmtiSetEventCallbacks,
	jvmtiGenerateEvents,
	jvmtiGetExtensionFunctions,
	jvmtiGetExtensionEvents,
	jvmtiSetExtensionEventCallback,
	jvmtiDisposeEnvironment,
	jvmtiGetErrorName,
	jvmtiGetJLocationFormat,
	jvmtiGetSystemProperties,
	jvmtiGetSystemProperty,
	jvmtiSetSystemProperty,
	jvmtiGetPhase,
	jvmtiGetCurrentThreadCpuTimerInfo,
	jvmtiGetCurrentThreadCpuTime,
	jvmtiGetThreadCpuTimerInfo,
	jvmtiGetThreadCpuTime,
	jvmtiGetTimerInfo,
	jvmtiGetTime,
	jvmtiGetPotentialCapabilities,
	NULL,
	jvmtiAddCapabilities,
	jvmtiRelinquishCapabilities,
	jvmtiGetAvailableProcessors,
	jvmtiGetClassVersionNumbers,
	jvmtiGetConstantPool,
	jvmtiGetEnvironmentLocalStorage,
	jvmtiSetEnvironmentLocalStorage,
	jvmtiAddToBootstrapClassLoaderSearch,
	jvmtiSetVerboseFlag,
	jvmtiAddToSystemClassLoaderSearch,
	jvmtiRetransformClasses,
	jvmtiGetOwnedMonitorStackDepthInfo,
	jvmtiGetObjectSize,
	jvmtiGetLocalInstance,
	JVMTI_11_ENTRY(jvmtiSetHeapSamplingInterval),
};



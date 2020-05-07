################################################################################
# Copyright (c) 2019, 2020 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
################################################################################

# Wrapper areround omr_add_exports which strips windows name mangling (except on 32bit windows)
function(jvm_add_exports tgt)
	set(filtered_exports)
	if(OMR_OS_WINDOWS AND OMR_ENV_DATA32)
		# we keep mangled names on 32 bit windows
		set(filtered_exports ${ARGN})
	else()
		# for each symbol name of the form '_foo@1234' replace with 'foo'
		foreach(symbol IN LISTS ARGN)
			string(REGEX REPLACE "^_([a-zA-Z0-9_]+)@[0-9]+$" "\\1" stripped_sym ${symbol})
			list(APPEND filtered_exports "${stripped_sym}")
		endforeach()
	endif()
	omr_add_exports(${tgt} ${filtered_exports})
endfunction()

jvm_add_exports(jvm
	JNI_CreateJavaVM
	JNI_GetCreatedJavaVMs
	JNI_GetDefaultJavaVMInitArgs
	_JVM_Accept@12
	_JVM_ActiveProcessorCount@0
	_JVM_AllocateNewArray@16
	_JVM_AllocateNewObject@16
	_JVM_Available@8
	_JVM_ClassDepth@8
	_JVM_ClassLoaderDepth@4
	_JVM_Close@4
	_JVM_Connect@12
	_JVM_ConstantPoolGetClassAt@16
	_JVM_ConstantPoolGetClassAtIfLoaded@16
	_JVM_ConstantPoolGetDoubleAt@16
	_JVM_ConstantPoolGetFieldAt@16
	_JVM_ConstantPoolGetFieldAtIfLoaded@16
	_JVM_ConstantPoolGetFloatAt@16
	_JVM_ConstantPoolGetIntAt@16
	_JVM_ConstantPoolGetLongAt@16
	_JVM_ConstantPoolGetMemberRefInfoAt@16
	_JVM_ConstantPoolGetMethodAt@16
	_JVM_ConstantPoolGetMethodAtIfLoaded@16
	_JVM_ConstantPoolGetSize@12
	_JVM_ConstantPoolGetStringAt@16
	_JVM_ConstantPoolGetUTF8At@16
	_JVM_CurrentClassLoader@4
	_JVM_CurrentLoadedClass@4
	_JVM_CurrentTimeMillis@8
	_JVM_CX8Field@28
	_JVM_DefineClassWithSource@28
	_JVM_DumpThreads@12
	_JVM_ExpandFdTable@4
	_JVM_FindLibraryEntry@8
	_JVM_FindSignal@4
	_JVM_FreeMemory@0
	_JVM_GC@0
	_JVM_GCNoCompact@0
	_JVM_GetAllThreads@8
	_JVM_GetClassAccessFlags@8
	_JVM_GetClassAnnotations@8
	_JVM_GetClassConstantPool@8
	_JVM_GetClassContext@4
	_JVM_GetClassLoader@8
	_JVM_GetClassSignature@8
	_JVM_GetEnclosingMethodInfo@8
	_JVM_GetInterfaceVersion@0
	_JVM_GetLastErrorString@8
	_JVM_GetManagement@4
	_JVM_GetPortLibrary@0
	_JVM_GetSystemPackage@8
	_JVM_GetSystemPackages@4
	_JVM_GetThreadInterruptEvent@0
	_JVM_Halt@4
	_JVM_InitializeSocketLibrary@0
	_JVM_InvokeMethod@16
	_JVM_IsNaN@8
	_JVM_LatestUserDefinedLoader@4
	_JVM_Listen@8
	_JVM_LoadLibrary@4
	_JVM_LoadSystemLibrary@4
	_JVM_Lseek@16
	_JVM_MaxMemory@0
	_JVM_MaxObjectInspectionAge@0
	_JVM_MonitorNotify@8
	_JVM_MonitorNotifyAll@8
	_JVM_MonitorWait@16
	_JVM_NanoTime@8
	_JVM_NativePath@4
	_JVM_NewInstanceFromConstructor@12
	_JVM_OnExit@4
	_JVM_Open@12
	_JVM_RaiseSignal@4
	_JVM_RawMonitorCreate@0
	_JVM_RawMonitorDestroy@4
	_JVM_RawMonitorEnter@4
	_JVM_RawMonitorExit@4
	_JVM_Read@12
	_JVM_Recv@16
	_JVM_RecvFrom@24
	_JVM_RegisterSignal@8
	_JVM_RegisterUnsafeMethods@8
	_JVM_Send@16
	_JVM_SendTo@24
	_JVM_SetLength@12
	_JVM_Sleep@16
	_JVM_Socket@12
	_JVM_SocketAvailable@8
	_JVM_SocketClose@4
	_JVM_Startup@8
	_JVM_SupportsCX8@0
	_JVM_Sync@4
	_JVM_Timeout@8
	_JVM_TotalMemory@0
	_JVM_TraceInstructions@4
	_JVM_TraceMethodCalls@4
	_JVM_UcsOpen@12
	_JVM_ZipHook@12
	_JVM_Write@12
	_JVM_RawAllocate@8
	_JVM_RawRealloc@12
	_JVM_RawCalloc@12
	_JVM_RawAllocateInCategory@12
	_JVM_RawReallocInCategory@16
	_JVM_RawCallocInCategory@16
	_JVM_RawFree@4
	jio_fprintf
	jio_snprintf
	jio_vfprintf
	jio_vsnprintf
	post_block
	pre_block
	# Additions for Java 7
	_JVM_GetStackAccessControlContext@8
	_JVM_GetInheritedAccessControlContext@8
	_JVM_GetArrayLength@8
	_JVM_GetArrayElement@12
	_JVM_GetStackTraceElement@12
	_JVM_GetStackTraceDepth@8
	_JVM_FillInStackTrace@8
	_JVM_StartThread@8
	_JVM_StopThread@12
	_JVM_IsThreadAlive@8
	_JVM_SuspendThread@8
	_JVM_ResumeThread@8
	_JVM_SetThreadPriority@12
	_JVM_Yield@8
	_JVM_CurrentThread@8
	_JVM_CountStackFrames@8
	_JVM_Interrupt@8
	_JVM_IsInterrupted@12
	_JVM_HoldsLock@12
	_JVM_InitProperties@8
	_JVM_ArrayCopy@28
	_JVM_DoPrivileged@20
	_JVM_IHashCode@8
	_JVM_Clone@8
	_JVM_CompileClass@12
	_JVM_CompileClasses@12
	_JVM_CompilerCommand@12
	_JVM_EnableCompiler@8
	_JVM_DisableCompiler@8
	_JVM_IsSupportedJNIVersion@4
	_JVM_UnloadLibrary@4
	_JVM_FindLoadedClass@12
	_JVM_ResolveClass@8
	_JVM_AssertionStatusDirectives@8
	_JVM_FindPrimitiveClass@8
	_JVM_FindClassFromClassLoader@20
	JVM_FindClassFromBootLoader
	_JVM_GetClassInterfaces@8
	_JVM_IsInterface@8
	_JVM_GetClassSigners@8
	_JVM_SetClassSigners@12
	_JVM_IsArrayClass@8
	_JVM_IsPrimitiveClass@8
	_JVM_GetComponentType@8
	_JVM_GetClassModifiers@8
	_JVM_GetClassDeclaredFields@12
	_JVM_GetClassDeclaredMethods@12
	_JVM_GetClassDeclaredConstructors@12
	_JVM_GetProtectionDomain@8
	_JVM_SetProtectionDomain@12
	_JVM_GetDeclaredClasses@8
	_JVM_GetDeclaringClass@8
	_JVM_DesiredAssertionStatus@12
	_JVM_InternString@8
	_JVM_NewMultiArray@12
	_JVM_NewArray@12
	_JVM_SetPrimitiveArrayElement@24
	_JVM_SetArrayElement@16
	_JVM_GetPrimitiveArrayElement@16
	_JVM_GetSockOpt@20
	_JVM_ExtendBootClassPath@8
	_JVM_Bind@12
	_JVM_DTraceActivate@20
	_JVM_DTraceDispose@12
	_JVM_DTraceGetVersion@4
	_JVM_DTraceIsProbeEnabled@8
	_JVM_DTraceIsSupported@4
	_JVM_DefineClass@24
	_JVM_DefineClassWithSourceCond@32
	_JVM_EnqueueOperation@20
	_JVM_Exit@4
	_JVM_GetCPFieldNameUTF@12
	_JVM_GetClassConstructor@16
	_JVM_GetClassConstructors@12
	_JVM_GetClassField@16
	_JVM_GetClassFields@12
	_JVM_GetClassMethod@20
	_JVM_GetClassMethods@12
	_JVM_GetField@12
	_JVM_GetFieldAnnotations@8
	_JVM_GetMethodAnnotations@8
	_JVM_GetMethodDefaultAnnotationValue@8
	_JVM_GetMethodParameterAnnotations@8
	_JVM_GetPrimitiveField@16
	_JVM_InitializeCompiler@8
	_JVM_IsSilentCompiler@8
	_JVM_LoadClass0@16
	_JVM_NewInstance@8
	_JVM_PrintStackTrace@12
	_JVM_SetField@16
	_JVM_SetPrimitiveField@24
	_JVM_SetNativeThreadName@12

	# Additions used on linux-x86
	_JVM_SetSockOpt@20
	_JVM_SocketShutdown@8
	_JVM_GetSockName@12
	_JVM_GetHostName@8

	# Additions to support the JDWP agent
	JVM_InitAgentProperties

	# Additions to support Java 7 verification
	_JVM_GetMethodIxLocalsCount@12
	_JVM_GetCPMethodNameUTF@12
	_JVM_GetMethodIxExceptionTableEntry@20
	_JVM_GetMethodIxExceptionTableLength@12
	_JVM_GetMethodIxMaxStack@12
	_JVM_GetMethodIxExceptionIndexes@16
	_JVM_GetCPFieldSignatureUTF@12
	_JVM_GetClassMethodsCount@8
	_JVM_GetClassFieldsCount@8
	_JVM_GetClassCPTypes@12
	_JVM_GetClassCPEntriesCount@8
	_JVM_GetCPMethodSignatureUTF@12
	_JVM_GetCPFieldModifiers@16
	_JVM_GetCPMethodModifiers@16
	_JVM_IsSameClassPackage@12
	_JVM_GetCPMethodClassNameUTF@12
	_JVM_GetCPFieldClassNameUTF@12
	_JVM_GetCPClassNameUTF@12
	_JVM_GetMethodIxArgsSize@12
	_JVM_GetMethodIxModifiers@12
	_JVM_IsConstructorIx@12
	_JVM_GetMethodIxByteCodeLength@12
	_JVM_GetMethodIxByteCode@16
	_JVM_GetFieldIxModifiers@12
	_JVM_FindClassFromClass@16
	_JVM_GetClassNameUTF@8
	_JVM_GetMethodIxNameUTF@12
	_JVM_GetMethodIxSignatureUTF@12
	_JVM_GetMethodIxExceptionsCount@12
	_JVM_ReleaseUTF@4

	# Additions for Java 8
	_JVM_GetClassTypeAnnotations@8
	_JVM_GetFieldTypeAnnotations@8
	_JVM_GetMethodParameters@8
	_JVM_GetMethodTypeAnnotations@8
	_JVM_IsVMGeneratedMethodIx@12
	JVM_GetTemporaryDirectory
	_JVM_CopySwapMemory@44
	JVM_BeforeHalt
)

if(JAVA_SPEC_VERSION LESS 11)
	# i.e. JAVA_SPEC_VERSION < 11
	jvm_add_exports(jvm _JVM_GetCallerClass@4)
else()
	jvm_add_exports(jvm
		_JVM_GetCallerClass@8
		# Additions for Java 9 (Modularity)
		JVM_DefineModule
		JVM_AddModuleExports
		JVM_AddModuleExportsToAll
		JVM_AddReadsModule
		JVM_CanReadModule
		JVM_AddModulePackage
		JVM_AddModuleExportsToAllUnnamed
		JVM_SetBootLoaderUnnamedModule
		JVM_GetModuleByPackageName

		# Additions for Java 9 RAW
		JVM_GetSimpleBinaryName
		JVM_SetMethodInfo
		JVM_ConstantPoolGetNameAndTypeRefIndexAt
		JVM_MoreStackWalk
		JVM_ConstantPoolGetClassRefIndexAt
		JVM_GetVmArguments
		JVM_FillStackFrames
		JVM_FindClassFromCaller
		JVM_ConstantPoolGetNameAndTypeRefInfoAt
		JVM_ConstantPoolGetTagAt
		JVM_CallStackWalk
		JVM_ToStackTraceElement
		JVM_GetStackTraceElements
		JVM_InitStackTraceElementArray
		JVM_InitStackTraceElement
		JVM_GetAndClearReferencePendingList
		JVM_HasReferencePendingList
		JVM_WaitForReferencePendingList

		# Additions for Java 9 (General)
		_JVM_GetNanoTimeAdjustment@16

		# Additions for Java 11 (General)
		JVM_GetNestHost
		JVM_GetNestMembers
		JVM_AreNestMates
		JVM_InitClassName
		JVM_InitializeFromArchive
	)
endif()

if(NOT JAVA_SPEC_VERSION LESS 14)
	jvm_add_exports(jvm
		# Additions for Java 14 (General)
		JVM_GetExtendedNPEMessage
	)
endif()

if(NOT JAVA_SPEC_VERSION LESS 15)
	jvm_add_exports(jvm
		# Additions for Java 15 (General)
		JVM_GetRandomSeedForCDSDump
	)
endif()

if(J9VM_OPT_JITSERVER)
	jvm_add_exports(jvm
		JITServer_CreateServer
	)
endif()

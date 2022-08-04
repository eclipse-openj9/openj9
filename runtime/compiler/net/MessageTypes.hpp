/*******************************************************************************
 * Copyright (c) 2020, 2022 IBM Corp. and others
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

#ifndef MESSAGE_TYPES_HPP
#define MESSAGE_TYPES_HPP

#include <cstdint> // for uint16_t
#include "j9cfg.h"

namespace JITServer
   {
enum MessageType : uint16_t
   {
   compilationCode, // Send the compiled code back to the client
   compilationFailure,
   AOTCache_serializedAOTMethod,// Final response to a compilation request that was an AOT cache hit
   mirrorResolvedJ9Method,
   get_params_to_construct_TR_j9method,
   getUnloadedClassRangesAndCHTable,
   compilationRequest, // type used when client sends remote compilation requests
   compilationInterrupted, // type used when client informs the server to abort the remote compilation
   clientSessionTerminate, // type used when client process is about to terminate
   connectionTerminate, // type used when client informs the server to close the connection
   compilationThreadCrashed,
   jitDumpPrintIL,

   // For TR_ResolvedJ9JITServerMethod methods
   ResolvedMethod_setRecognizedMethodInfo,
   ResolvedMethod_startAddressForInterpreterOfJittedMethod,
   ResolvedMethod_staticAttributes,
   ResolvedMethod_getClassFromConstantPool,
   ResolvedMethod_getDeclaringClassFromFieldOrStatic,
   ResolvedMethod_classOfStatic,
   ResolvedMethod_startAddressForJNIMethod,
   ResolvedMethod_fieldAttributes,
   ResolvedMethod_getResolvedStaticMethodAndMirror,
   ResolvedMethod_getResolvedSpecialMethodAndMirror,
   ResolvedMethod_startAddressForJittedMethod,
   ResolvedMethod_localName,
   ResolvedMethod_getResolvedPossiblyPrivateVirtualMethodAndMirror,
   ResolvedMethod_getResolvedInterfaceMethod_2, // arity 2
   ResolvedMethod_getResolvedInterfaceMethodAndMirror_3, // arity 3
   ResolvedMethod_getResolvedInterfaceMethodOffset,
   ResolvedMethod_getUnresolvedStaticMethodInCP,
   ResolvedMethod_isSubjectToPhaseChange,
   ResolvedMethod_getUnresolvedSpecialMethodInCP,
   ResolvedMethod_getUnresolvedFieldInCP,
   ResolvedMethod_getRemoteROMClassAndMethods,
   ResolvedMethod_getResolvedHandleMethod,
   ResolvedMethod_isUnresolvedMethodTypeTableEntry,
   ResolvedMethod_methodTypeTableEntryAddress,
   ResolvedMethod_isUnresolvedCallSiteTableEntry,
   ResolvedMethod_callSiteTableEntryAddress,
   ResolvedMethod_getResolvedDynamicMethod,
   ResolvedMethod_shouldFailSetRecognizedMethodInfoBecauseOfHCR,
   ResolvedMethod_isSameMethod,
   ResolvedMethod_isInlineable,
   ResolvedMethod_setWarmCallGraphTooBig,
   ResolvedMethod_setVirtualMethodIsOverridden,
   ResolvedMethod_methodIsNotzAAPEligible,
   ResolvedMethod_setClassForNewInstance,
   ResolvedMethod_getResolvedImproperInterfaceMethodAndMirror,
   ResolvedMethod_isUnresolvedString,
   ResolvedMethod_stringConstant,
   ResolvedMethod_getResolvedVirtualMethod,
   ResolvedMethod_getMultipleResolvedMethods,
#if defined(J9VM_OPT_METHOD_HANDLE)
   ResolvedMethod_varHandleMethodTypeTableEntryAddress,
   ResolvedMethod_isUnresolvedVarHandleMethodTypeTableEntry,
#endif /* defined(J9VM_OPT_METHOD_HANDLE) */
   ResolvedMethod_getConstantDynamicTypeFromCP,
   ResolvedMethod_isUnresolvedConstantDynamic,
   ResolvedMethod_dynamicConstant,
   ResolvedMethod_definingClassFromCPFieldRef,
   ResolvedMethod_getResolvedImplementorMethods,
   ResolvedMethod_isFieldFlattened,

   ResolvedRelocatableMethod_createResolvedRelocatableJ9Method,
   ResolvedRelocatableMethod_fieldAttributes,
   ResolvedRelocatableMethod_staticAttributes,
   ResolvedRelocatableMethod_getFieldType,

   // For TR_J9ServerVM methods
   VM_isClassLibraryClass,
   VM_isClassLibraryMethod,
   VM_isClassArray,
   VM_transformJlrMethodInvoke,
   VM_getStaticReferenceFieldAtAddress,
   VM_getSystemClassFromClassName,
   VM_isMethodTracingEnabled,
   VM_getClassClassPointer,
   VM_setJ2IThunk,
   VM_getClassOfMethod,
   VM_getClassFromSignature,
   VM_jitFieldsOrStaticsAreSame,
   VM_classHasBeenExtended,
   VM_compiledAsDLTBefore,
   VM_isThunkArchetype,
   VM_printTruncatedSignature,
   VM_getStaticHookAddress,
   VM_isClassInitialized,
   VM_getOSRFrameSizeInBytes,
   VM_getInitialLockword,
   VM_JavaStringObject,
   VM_getMethods,
   VM_getObjectClass,
   VM_getObjectClassAt,
   VM_getObjectClassFromKnownObjectIndex,
   VM_stackWalkerMaySkipFrames,
   VM_getStringUTF8Length,
   VM_classInitIsFinished,
   VM_getClassFromNewArrayType,
   VM_getArrayClassFromComponentClass,
   VM_matchRAMclassFromROMclass,
   VM_getReferenceFieldAtAddress,
   VM_getVolatileReferenceFieldAt,
   VM_getInt32FieldAt,
   VM_getInt64FieldAt,
   VM_setInt64FieldAt,
   VM_compareAndSwapInt64FieldAt,
   VM_getArrayLengthInElements,
   VM_getClassFromJavaLangClass,
   VM_getOffsetOfClassFromJavaLangClassField,
   VM_getIdentityHashSaltPolicy,
   VM_getOffsetOfJLThreadJ9Thread,
   VM_getVFTEntry,
   VM_scanReferenceSlotsInClassForOffset,
   VM_findFirstHotFieldTenuredClassOffset,
   VM_getResolvedVirtualMethod,
   VM_getInstanceFieldOffset,
   VM_getJavaLangClassHashCode,
   VM_getClassDepthAndFlagsValue,
   VM_getMethodFromName,
   VM_getMethodFromClass,
   VM_isClassVisible,
   VM_markClassForTenuredAlignment,
   VM_reportHotField,
   VM_getReferenceSlotsInClass,
   VM_getMethodSize,
   VM_addressOfFirstClassStatic,
   VM_getStaticFieldAddress,
   VM_getInterpreterVTableSlot,
   VM_revertToInterpreted,
   VM_getLocationOfClassLoaderObjectPointer,
   VM_getClassFromMethodBlock,
   VM_fetchMethodExtendedFlagsPointer,
   VM_stringEquals,
   VM_getStringHashCode,
   VM_getLineNumberForMethodAndByteCodeIndex,
   VM_getObjectNewInstanceImplMethod,
   VM_getBytecodePC,
   VM_setInvokeExactJ2IThunk,
   VM_createMethodHandleArchetypeSpecimen,
   VM_instanceOfOrCheckCast,
   VM_getResolvedMethodsAndMirror,
   VM_getVMInfo,
   VM_dereferenceStaticAddress,
   VM_getClassFromCP,
   VM_getROMMethodFromRAMMethod,
   VM_getReferenceFieldAt,
   VM_getJ2IThunk,
   VM_needsInvokeExactJ2IThunk,
   VM_instanceOfOrCheckCastNoCacheUpdate,
   VM_getCellSizeForSizeClass,
   VM_getObjectSizeClass,
   VM_stackWalkerMaySkipFramesSVM,
   VM_getFields,
   VM_increaseOSRGlobalBufferSize,
   VM_methodOfDirectOrVirtualHandle,
   VM_targetMethodFromMemberName,
   VM_targetMethodFromMethodHandle,
   VM_getKnotIndexOfInvokeCacheArrayAppendixElement,
   VM_targetMethodFromInvokeCacheArrayMemberNameObj,
   VM_refineInvokeCacheElementSymRefWithKnownObjectIndex,
   VM_isLambdaFormGeneratedMethod,
   VM_vTableOrITableIndexFromMemberName,
   VM_getMemberNameFieldKnotIndexFromMethodHandleKnotIndex,
   VM_isMethodHandleExpectedType,
   VM_isStable,
   VM_delegatingMethodHandleTarget,
   VM_getVMTargetOffset,
   VM_getVMIndexOffset,

   // For static TR::CompilationInfo methods
   CompInfo_isCompiled,
   CompInfo_getPCIfCompiled,
   CompInfo_getInvocationCount,
   CompInfo_setInvocationCount,
   CompInfo_getJ9MethodExtra,
   CompInfo_isJNINative,
   CompInfo_isJSR292,
   CompInfo_getMethodBytecodeSize,
   CompInfo_setJ9MethodExtra,
   CompInfo_setInvocationCountAtomic,
   CompInfo_getJ9MethodStartPC,

   // For J9::ClassEnv Methods
   ClassEnv_classFlagsValue,
   ClassEnv_superClassesOf,
   ClassEnv_indexedSuperClassOf,
   ClassEnv_iTableOf,
   ClassEnv_iTableNext,
   ClassEnv_iTableRomClass,
   ClassEnv_getITable,
   ClassEnv_enumerateFields,
   ClassEnv_isClassRefPrimitiveValueType,
   ClassEnv_flattenedArrayElementSize,
   ClassEnv_getDefaultValueSlotAddress,

   // For TR_J9SharedCache
   SharedCache_getClassChainOffsetIdentifyingLoader,
   SharedCache_rememberClass,
   SharedCache_addHint,
   SharedCache_storeSharedData,

   // For runFEMacro
   runFEMacro_invokeCollectHandleNumArgsToCollect,
   runFEMacro_invokeExplicitCastHandleConvertArgs,
   runFEMacro_targetTypeL,
   runFEMacro_invokeILGenMacrosInvokeExactAndFixup,
   runFEMacro_invokeArgumentMoverHandlePermuteArgs,
   runFEMacro_invokePermuteHandlePermuteArgs,
   runFEMacro_invokeGuardWithTestHandleNumGuardArgs,
   runFEMacro_invokeInsertHandle,
   runFEMacro_invokeSpreadHandleArrayArg,
   runFEMacro_invokeSpreadHandle,
   runFEMacro_invokeFoldHandle,
   runFEMacro_invokeFoldHandle2,
   runFEMacro_invokeFinallyHandle,
   runFEMacro_invokeFilterArgumentsHandle,
   runFEMacro_invokeFilterArgumentsHandle2,
   runFEMacro_invokeCatchHandle,
   runFEMacro_invokeILGenMacrosParameterCount,
   runFEMacro_invokeILGenMacrosArrayLength,
   runFEMacro_invokeILGenMacrosGetField,
   runFEMacro_invokeFilterArgumentsWithCombinerHandleNumSuffixArgs,
   runFEMacro_invokeFilterArgumentsWithCombinerHandleFilterPosition,
   runFEMacro_invokeFilterArgumentsWithCombinerHandleArgumentIndices,
   runFEMacro_invokeCollectHandleAllocateArray,

   // for JITServerPersistentCHTable
   CHTable_clearReservable,

   // for JITServerIProfiler
   IProfiler_profilingSample,
   IProfiler_searchForMethodSample,
   IProfiler_getMaxCallCount,
   IProfiler_setCallCount,

   Recompilation_getJittedBodyInfoFromPC,

   // for KnownObjectTable
   KnownObjectTable_getOrCreateIndex,
   KnownObjectTable_getOrCreateIndexAt,
   KnownObjectTable_getPointer,
   KnownObjectTable_getExistingIndexAt,
   // for KnownObjectTable outside J9::KnownObjectTable class
   KnownObjectTable_symbolReferenceTableCreateKnownObject,
   KnownObjectTable_mutableCallSiteEpoch,
   KnownObjectTable_dereferenceKnownObjectField,
   KnownObjectTable_dereferenceKnownObjectField2,
   KnownObjectTable_createSymRefWithKnownObject,
   KnownObjectTable_getReferenceField,
   KnownObjectTable_getKnownObjectTableDumpInfo,

   AOTCache_getROMClassBatch,

   MessageType_MAXTYPE
   };
   extern const char *messageNames[];
   }; // namespace JITServer

#endif // MESSAGE_TYPES_HPP

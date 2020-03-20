/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

namespace JITServer
   {
enum MessageType : uint16_t
   {
   compilationCode, // Send the compiled code back to the client
   compilationFailure,
   mirrorResolvedJ9Method,
   get_params_to_construct_TR_j9method,
   getUnloadedClassRanges,
   compilationRequest, // type used when client sends remote compilation requests
   compilationInterrupted, // type used when client informs the server to abort the remote compilation
   clientSessionTerminate, // type used when client process is about to terminate
   connectionTerminate, // type used when client informs the server to close the connection

   // For TR_ResolvedJ9JITServerMethod methods
   ResolvedMethod_isJNINative,
   ResolvedMethod_isInterpreted,
   ResolvedMethod_setRecognizedMethodInfo,
   ResolvedMethod_startAddressForInterpreterOfJittedMethod,
   ResolvedMethod_jniNativeMethodProperties,
   ResolvedMethod_staticAttributes,
   ResolvedMethod_getClassFromConstantPool,
   ResolvedMethod_getDeclaringClassFromFieldOrStatic,
   ResolvedMethod_classOfStatic,
   ResolvedMethod_startAddressForJNIMethod,
   ResolvedMethod_fieldAttributes,
   ResolvedMethod_getResolvedStaticMethodAndMirror,
   ResolvedMethod_getResolvedSpecialMethodAndMirror,
   ResolvedMethod_classCPIndexOfMethod,
   ResolvedMethod_startAddressForJittedMethod,
   ResolvedMethod_localName,
   ResolvedMethod_getResolvedPossiblyPrivateVirtualMethodAndMirror,
   ResolvedMethod_virtualMethodIsOverridden,
   ResolvedMethod_getResolvedInterfaceMethod_2, // arity 2
   ResolvedMethod_getResolvedInterfaceMethodAndMirror_3, // arity 3
   ResolvedMethod_getResolvedInterfaceMethodOffset,
   ResolvedMethod_getUnresolvedStaticMethodInCP,
   ResolvedMethod_isSubjectToPhaseChange,
   ResolvedMethod_getUnresolvedSpecialMethodInCP,
   ResolvedMethod_getUnresolvedFieldInCP,
   ResolvedMethod_getRemoteROMString,
   ResolvedMethod_fieldOrStaticName,
   ResolvedMethod_getRemoteROMClassAndMethods,
   ResolvedMethod_getResolvedHandleMethod,
   ResolvedMethod_isUnresolvedMethodTypeTableEntry,
   ResolvedMethod_methodTypeTableEntryAddress,
   ResolvedMethod_isUnresolvedCallSiteTableEntry,
   ResolvedMethod_callSiteTableEntryAddress,
   ResolvedMethod_getResolvedDynamicMethod,
   ResolvedMethod_shouldFailSetRecognizedMethodInfoBecauseOfHCR,
   ResolvedMethod_isSameMethod,
   ResolvedMethod_isBigDecimalMethod,
   ResolvedMethod_isBigDecimalConvertersMethod,
   ResolvedMethod_isInlineable,
   ResolvedMethod_setWarmCallGraphTooBig,
   ResolvedMethod_setVirtualMethodIsOverridden,
   ResolvedMethod_addressContainingIsOverriddenBit,
   ResolvedMethod_methodIsNotzAAPEligible,
   ResolvedMethod_setClassForNewInstance,
   ResolvedMethod_getJittedBodyInfo,
   ResolvedMethod_getResolvedImproperInterfaceMethodAndMirror,
   ResolvedMethod_isUnresolvedString,
   ResolvedMethod_stringConstant,
   ResolvedMethod_getResolvedVirtualMethod,
   ResolvedMethod_getMultipleResolvedMethods,
   ResolvedMethod_varHandleMethodTypeTableEntryAddress,
   ResolvedMethod_isUnresolvedVarHandleMethodTypeTableEntry,
   ResolvedMethod_getConstantDynamicTypeFromCP,
   ResolvedMethod_isUnresolvedConstantDynamic,
   ResolvedMethod_dynamicConstant,
   ResolvedMethod_definingClassFromCPFieldRef,

   ResolvedRelocatableMethod_createResolvedRelocatableJ9Method,
   ResolvedRelocatableMethod_storeValidationRecordIfNecessary,
   ResolvedRelocatableMethod_fieldAttributes,
   ResolvedRelocatableMethod_staticAttributes,
   ResolvedRelocatableMethod_getFieldType,

   // For TR_J9ServerVM methods
   VM_isClassLibraryClass,
   VM_isClassLibraryMethod,
   VM_getSuperClass,
   VM_isInstanceOf,
   VM_isClassArray,
   VM_transformJlrMethodInvoke,
   VM_getStaticReferenceFieldAtAddress,
   VM_getSystemClassFromClassName,
   VM_isMethodTracingEnabled,
   VM_getClassClassPointer,
   VM_setJ2IThunk,
   VM_getClassOfMethod,
   VM_getBaseComponentClass,
   VM_getLeafComponentClassFromArrayClass,
   VM_isClassLoadedBySystemClassLoader,
   VM_getClassFromSignature,
   VM_jitFieldsAreSame,
   VM_jitStaticsAreSame,
   VM_getComponentClassFromArrayClass,
   VM_classHasBeenReplaced,
   VM_classHasBeenExtended,
   VM_compiledAsDLTBefore,
   VM_isThunkArchetype,
   VM_printTruncatedSignature,
   VM_getStaticHookAddress,
   VM_isClassInitialized,
   VM_getOSRFrameSizeInBytes,
   VM_getInitialLockword,
   VM_isString1,
   VM_getMethods,
   VM_isPrimitiveArray,
   VM_getAllocationSize,
   VM_getObjectClass,
   VM_stackWalkerMaySkipFrames,
   VM_hasFinalFieldsInClass,
   VM_getClassNameSignatureFromMethod,
   VM_getHostClass,
   VM_getStringUTF8Length,
   VM_classInitIsFinished,
   VM_getClassFromNewArrayType,
   VM_isCloneable,
   VM_canAllocateInlineClass,
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
   VM_sameClassLoaders,
   VM_isUnloadAssumptionRequired,
   VM_getInstanceFieldOffset,
   VM_getJavaLangClassHashCode,
   VM_hasFinalizer,
   VM_getClassDepthAndFlagsValue,
   VM_getMethodFromName,
   VM_getMethodFromClass,
   VM_isClassVisible,
   VM_markClassForTenuredAlignment,
   VM_getReferenceSlotsInClass,
   VM_getMethodSize,
   VM_addressOfFirstClassStatic,
   VM_getStaticFieldAddress,
   VM_getInterpreterVTableSlot,
   VM_revertToInterpreted,
   VM_getLocationOfClassLoaderObjectPointer,
   VM_isOwnableSyncClass,
   VM_getClassFromMethodBlock,
   VM_fetchMethodExtendedFlagsPointer,
   VM_stringEquals,
   VM_getStringHashCode,
   VM_getLineNumberForMethodAndByteCodeIndex,
   VM_getObjectNewInstanceImplMethod,
   VM_getBytecodePC,
   VM_getClassFromStatic,
   VM_setInvokeExactJ2IThunk,
   VM_createMethodHandleArchetypeSpecimen,
   VM_instanceOfOrCheckCast,
   VM_getResolvedMethodsAndMirror,
   VM_getVMInfo,
   VM_isAnonymousClass,
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

   // For static TR::CompilationInfo methods
   CompInfo_isCompiled,
   CompInfo_getInvocationCount,
   CompInfo_setInvocationCount,
   CompInfo_getJ9MethodExtra,
   CompInfo_isJNINative,
   CompInfo_isJSR292,
   CompInfo_getMethodBytecodeSize,
   CompInfo_setJ9MethodExtra,
   CompInfo_setInvocationCountAtomic,
   CompInfo_isClassSpecial,
   CompInfo_getJ9MethodStartPC,

   // For J9::ClassEnv Methods
   ClassEnv_classFlagsValue,
   ClassEnv_classDepthOf,
   ClassEnv_classInstanceSize,
   ClassEnv_superClassesOf,
   ClassEnv_indexedSuperClassOf,
   ClassEnv_iTableOf,
   ClassEnv_iTableNext,
   ClassEnv_iTableRomClass,
   ClassEnv_getITable,
   ClassEnv_classHasIllegalStaticFinalFieldModification,
   ClassEnv_getROMClassRefName,

   // For TR_J9SharedCache
   SharedCache_getClassChainOffsetInSharedCache,
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
   runFEMacro_invokeDirectHandleDirectCall,
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
   CHTable_getAllClassInfo,
   CHTable_getClassInfoUpdates,
   CHTable_commit,
   CHTable_clearReservable,

   // for JITServerIProfiler
   IProfiler_profilingSample,
   IProfiler_searchForMethodSample,
   IProfiler_getMaxCallCount,
   IProfiler_setCallCount,

   Recompilation_getExistingMethodInfo,
   Recompilation_getJittedBodyInfoFromPC,

   ClassInfo_getRemoteROMString,

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
   KnownObjectTable_invokeDirectHandleDirectCall,
   KnownObjectTable_getKnownObjectTableDumpInfo,
   MessageType_MAXTYPE
   };

const int MessageType_ARRAYSIZE = MessageType_MAXTYPE;
   
static const char *messageNames[MessageType_ARRAYSIZE] =
   {
   "compilationCode", // 0
   "compilationFailure", // 1
   "mirrorResolvedJ9Method", // 2
   "get_params_to_construct_TR_j9method", // 3
   "getUnloadedClassRanges", // 4
   "compilationRequest", // 5
   "compilationInterrupted", // 6
   "clientSessionTerminate", // 7
   "connectionTerminate", // 8
   "ResolvedMethod_isJNINative", // 9
   "ResolvedMethod_isInterpreted", // 10
   "ResolvedMethod_setRecognizedMethodInfo", // 11
   "ResolvedMethod_startAddressForInterpreterOfJittedMethod", // 12
   "ResolvedMethod_jniNativeMethodProperties", // 13
   "ResolvedMethod_staticAttributes", // 14
   "ResolvedMethod_getClassFromConstantPool", // 15
   "ResolvedMethod_getDeclaringClassFromFieldOrStatic", // 16
   "ResolvedMethod_classOfStatic", // 17
   "ResolvedMethod_startAddressForJNIMethod", // 18
   "ResolvedMethod_fieldAttributes", // 19
   "ResolvedMethod_getResolvedStaticMethodAndMirror", // 20
   "ResolvedMethod_getResolvedSpecialMethodAndMirror", // 21
   "ResolvedMethod_classCPIndexOfMethod", // 22
   "ResolvedMethod_startAddressForJittedMethod", // 23
   "ResolvedMethod_localName", // 24
   "ResolvedMethod_getResolvedPossiblyPrivateVirtualMethodAndMirror", // 25
   "ResolvedMethod_virtualMethodIsOverridden", // 26
   "ResolvedMethod_getResolvedInterfaceMethod_2", // 27
   "ResolvedMethod_getResolvedInterfaceMethodAndMirror_3", // 28
   "ResolvedMethod_getResolvedInterfaceMethodOffset", // 29
   "ResolvedMethod_getUnresolvedStaticMethodInCP", // 30
   "ResolvedMethod_isSubjectToPhaseChange", // 31
   "ResolvedMethod_getUnresolvedSpecialMethodInCP", // 32
   "ResolvedMethod_getUnresolvedFieldInCP", // 33
   "ResolvedMethod_getRemoteROMString", // 34
   "ResolvedMethod_fieldOrStaticName", // 35
   "ResolvedMethod_getRemoteROMClassAndMethods", // 36
   "ResolvedMethod_getResolvedHandleMethod", // 37
   "ResolvedMethod_isUnresolvedMethodTypeTableEntry", // 38
   "ResolvedMethod_methodTypeTableEntryAddress", // 39
   "ResolvedMethod_isUnresolvedCallSiteTableEntry", // 40
   "ResolvedMethod_callSiteTableEntryAddress", // 41
   "ResolvedMethod_getResolvedDynamicMethod", // 42
   "ResolvedMethod_shouldFailSetRecognizedMethodInfoBecauseOfHCR", // 43
   "ResolvedMethod_isSameMethod", // 44
   "ResolvedMethod_isBigDecimalMethod", // 45
   "ResolvedMethod_isBigDecimalConvertersMethod", // 46
   "ResolvedMethod_isInlineable", // 47
   "ResolvedMethod_setWarmCallGraphTooBig", // 48
   "ResolvedMethod_setVirtualMethodIsOverridden", // 49
   "ResolvedMethod_addressContainingIsOverriddenBit", // 50
   "ResolvedMethod_methodIsNotzAAPEligible", // 51
   "ResolvedMethod_setClassForNewInstance", // 52
   "ResolvedMethod_getJittedBodyInfo", // 53
   "ResolvedMethod_getResolvedImproperInterfaceMethodAndMirror", // 54
   "ResolvedMethod_isUnresolvedString", // 55
   "ResolvedMethod_stringConstant", // 56
   "ResolvedMethod_getResolvedVirtualMethod", // 57
   "ResolvedMethod_getMultipleResolvedMethods", // 58
   "ResolvedMethod_varHandleMethodTypeTableEntryAddress", // 59
   "ResolvedMethod_isUnresolvedVarHandleMethodTypeTableEntry", // 60
   "ResolvedMethod_getConstantDynamicTypeFromCP", // 61
   "ResolvedMethod_isUnresolvedConstantDynamic", // 62
   "ResolvedMethod_dynamicConstant", // 63
   "ResolvedMethod_definingClassFromCPFieldRef", // 64
   "ResolvedRelocatableMethod_createResolvedRelocatableJ9Method", // 65
   "ResolvedRelocatableMethod_storeValidationRecordIfNecessary", // 66
   "ResolvedRelocatableMethod_fieldAttributes", // 67
   "ResolvedRelocatableMethod_staticAttributes", // 68
   "ResolvedRelocatableMethod_getFieldType", // 69
   "VM_isClassLibraryClass", // 70
   "VM_isClassLibraryMethod", // 71
   "VM_getSuperClass", // 72
   "VM_isInstanceOf", // 73
   "VM_isClassArray", // 74
   "VM_transformJlrMethodInvoke", // 75
   "VM_getStaticReferenceFieldAtAddress", // 76
   "VM_getSystemClassFromClassName", // 77
   "VM_isMethodTracingEnabled", // 78
   "VM_getClassClassPointer", // 79
   "VM_setJ2IThunk", // 80
   "VM_getClassOfMethod", // 81
   "VM_getBaseComponentClass", // 82
   "VM_getLeafComponentClassFromArrayClass", // 83
   "VM_isClassLoadedBySystemClassLoader", // 84
   "VM_getClassFromSignature", // 85
   "VM_jitFieldsAreSame", // 86
   "VM_jitStaticsAreSame", // 87
   "VM_getComponentClassFromArrayClass", // 88
   "VM_classHasBeenReplaced", // 89
   "VM_classHasBeenExtended", // 90
   "VM_compiledAsDLTBefore", // 91
   "VM_isThunkArchetype", // 92
   "VM_printTruncatedSignature", // 93
   "VM_getStaticHookAddress", // 94
   "VM_isClassInitialized", // 95
   "VM_getOSRFrameSizeInBytes", // 96
   "VM_getInitialLockword", // 97
   "VM_isString1", // 98
   "VM_getMethods", // 99
   "VM_isPrimitiveArray", // 100
   "VM_getAllocationSize", // 101
   "VM_getObjectClass", // 102
   "VM_stackWalkerMaySkipFrames", // 103
   "VM_hasFinalFieldsInClass", // 104
   "VM_getClassNameSignatureFromMethod", // 105
   "VM_getHostClass", // 106
   "VM_getStringUTF8Length", // 107
   "VM_classInitIsFinished", // 108
   "VM_getClassFromNewArrayType", // 109
   "VM_isCloneable", // 110
   "VM_canAllocateInlineClass", // 111
   "VM_getArrayClassFromComponentClass", // 112
   "VM_matchRAMclassFromROMclass", // 113
   "VM_getReferenceFieldAtAddress", // 114
   "VM_getVolatileReferenceFieldAt", // 115
   "VM_getInt32FieldAt", // 116
   "VM_getInt64FieldAt", // 117
   "VM_setInt64FieldAt", // 118
   "VM_compareAndSwapInt64FieldAt", // 119
   "VM_getArrayLengthInElements", // 120
   "VM_getClassFromJavaLangClass", // 121
   "VM_getOffsetOfClassFromJavaLangClassField", // 122
   "VM_getIdentityHashSaltPolicy", // 123
   "VM_getOffsetOfJLThreadJ9Thread", // 124
   "VM_getVFTEntry", // 125
   "VM_scanReferenceSlotsInClassForOffset", // 126
   "VM_findFirstHotFieldTenuredClassOffset", // 127
   "VM_getResolvedVirtualMethod", // 128
   "VM_sameClassLoaders", // 129
   "VM_isUnloadAssumptionRequired", // 130
   "VM_getInstanceFieldOffset", // 131
   "VM_getJavaLangClassHashCode", // 132
   "VM_hasFinalizer", // 133
   "VM_getClassDepthAndFlagsValue", // 134
   "VM_getMethodFromName", // 135
   "VM_getMethodFromClass", // 136
   "VM_isClassVisible", // 137
   "VM_markClassForTenuredAlignment", // 138
   "VM_getReferenceSlotsInClass", // 139
   "VM_getMethodSize", // 140
   "VM_addressOfFirstClassStatic", // 141
   "VM_getStaticFieldAddress", // 142
   "VM_getInterpreterVTableSlot", // 143
   "VM_revertToInterpreted", // 144
   "VM_getLocationOfClassLoaderObjectPointer", // 145
   "VM_isOwnableSyncClass", // 146
   "VM_getClassFromMethodBlock", // 147
   "VM_fetchMethodExtendedFlagsPointer", // 148
   "VM_stringEquals", // 149
   "VM_getStringHashCode", // 150
   "VM_getLineNumberForMethodAndByteCodeIndex", // 151
   "VM_getObjectNewInstanceImplMethod", // 152
   "VM_getBytecodePC", // 153
   "VM_getClassFromStatic", // 154
   "VM_setInvokeExactJ2IThunk", // 155
   "VM_createMethodHandleArchetypeSpecimen", // 156
   "VM_instanceOfOrCheckCast", // 157
   "VM_getResolvedMethodsAndMirror", // 158
   "VM_getVMInfo", // 159
   "VM_isAnonymousClass", // 160
   "VM_dereferenceStaticAddress", // 161
   "VM_getClassFromCP", // 162
   "VM_getROMMethodFromRAMMethod", // 163
   "VM_getReferenceFieldAt", // 164
   "VM_getJ2IThunk", // 165
   "VM_needsInvokeExactJ2IThunk", // 166
   "VM_instanceOfOrCheckCastNoCacheUpdate", // 167
   "VM_getCellSizeForSizeClass", // 168
   "VM_getObjectSizeClass", // 169
   "VM_stackWalkerMaySkipFramesSVM", // 170
   "CompInfo_isCompiled", // 171
   "CompInfo_getInvocationCount", // 172
   "CompInfo_setInvocationCount", // 173
   "CompInfo_getJ9MethodExtra", // 174
   "CompInfo_isJNINative", // 175
   "CompInfo_isJSR292", // 176
   "CompInfo_getMethodBytecodeSize", // 177
   "CompInfo_setJ9MethodExtra", // 178
   "CompInfo_setInvocationCountAtomic", // 179
   "CompInfo_isClassSpecial", // 180
   "CompInfo_getJ9MethodStartPC", // 181
   "ClassEnv_classFlagsValue", // 182
   "ClassEnv_classDepthOf", // 183
   "ClassEnv_classInstanceSize", // 184
   "ClassEnv_superClassesOf", // 185
   "ClassEnv_indexedSuperClassOf", // 186
   "ClassEnv_iTableOf", // 187
   "ClassEnv_iTableNext", // 188
   "ClassEnv_iTableRomClass", // 189
   "ClassEnv_getITable", // 190
   "ClassEnv_classHasIllegalStaticFinalFieldModification", // 191
   "ClassEnv_getROMClassRefName", // 192
   "SharedCache_getClassChainOffsetInSharedCache", // 193
   "SharedCache_rememberClass", // 194
   "SharedCache_addHint", // 195
   "SharedCache_storeSharedData", // 196
   "runFEMacro_invokeCollectHandleNumArgsToCollect", // 197
   "runFEMacro_invokeExplicitCastHandleConvertArgs", // 198
   "runFEMacro_targetTypeL", // 199
   "runFEMacro_invokeILGenMacrosInvokeExactAndFixup", // 200
   "runFEMacro_invokeArgumentMoverHandlePermuteArgs", // 201
   "runFEMacro_invokePermuteHandlePermuteArgs", // 202
   "runFEMacro_invokeGuardWithTestHandleNumGuardArgs", // 203
   "runFEMacro_invokeInsertHandle", // 204
   "runFEMacro_invokeDirectHandleDirectCall", // 205
   "runFEMacro_invokeSpreadHandleArrayArg", // 206
   "runFEMacro_invokeSpreadHandle", // 207
   "runFEMacro_invokeFoldHandle", // 208
   "runFEMacro_invokeFoldHandle2", // 209
   "runFEMacro_invokeFinallyHandle", // 210
   "runFEMacro_invokeFilterArgumentsHandle", // 211
   "runFEMacro_invokeFilterArgumentsHandle2", // 212
   "runFEMacro_invokeCatchHandle", // 213
   "runFEMacro_invokeILGenMacrosParameterCount", // 214
   "runFEMacro_invokeILGenMacrosArrayLength", // 215
   "runFEMacro_invokeILGenMacrosGetField", // 216
   "runFEMacro_invokeFilterArgumentsWithCombinerHandleNumSuffixArgs", // 217
   "runFEMacro_invokeFilterArgumentsWithCombinerHandleFilterPosition", // 218
   "runFEMacro_invokeFilterArgumentsWithCombinerHandleArgumentIndices", // 219
   "runFEMacro_invokeCollectHandleAllocateArray", // 220
   "CHTable_getAllClassInfo", // 221
   "CHTable_getClassInfoUpdates", // 222
   "CHTable_commit", // 223
   "CHTable_clearReservable", // 224
   "IProfiler_profilingSample", // 225
   "IProfiler_searchForMethodSample", // 226
   "IProfiler_getMaxCallCount", // 227
   "IProfiler_setCallCount", // 228
   "Recompilation_getExistingMethodInfo", // 229
   "Recompilation_getJittedBodyInfoFromPC", // 230
   "ClassInfo_getRemoteROMString", // 231
   "KnownObjectTable_getOrCreateIndex", // 232
   "KnownObjectTable_getOrCreateIndexAt", // 233
   "KnownObjectTable_getPointer", // 234
   "KnownObjectTable_getExistingIndexAt", // 235
   "KnownObjectTable_symbolReferenceTableCreateKnownObject", // 236
   "KnownObjectTable_mutableCallSiteEpoch", // 237
   "KnownObjectTable_dereferenceKnownObjectField", // 238
   "KnownObjectTable_dereferenceKnownObjectField2", // 239
   "KnownObjectTable_createSymRefWithKnownObject", // 240
   "KnownObjectTable_getReferenceField", // 241
   "KnownObjectTable_invokeDirectHandleDirectCall", // 242
   "KnownObjectTable_getKnownObjectTableDumpInfo" // 243
   };
   }; // namespace JITServer
#endif // MESSAGE_TYPES_HPP

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
   compilationCode = 0, // Send the compiled code back to the client
   compilationFailure = 1,
   mirrorResolvedJ9Method = 2,
   get_params_to_construct_TR_j9method = 3,
   getUnloadedClassRanges = 4,
   compilationRequest = 5, // type used when client sends remote compilation requests
   compilationInterrupted = 6, // type used when client informs the server to abort the remote compilation
   clientSessionTerminate = 7, // type used when client process is about to terminate
   connectionTerminate = 8, // type used when client informs the server to close the connection

   // For TR_ResolvedJ9JITServerMethod methods
   ResolvedMethod_isJNINative = 100,
   ResolvedMethod_isInterpreted = 101,
   ResolvedMethod_setRecognizedMethodInfo = 102,
   ResolvedMethod_startAddressForInterpreterOfJittedMethod = 103,
   ResolvedMethod_jniNativeMethodProperties = 104,
   ResolvedMethod_staticAttributes = 105,
   ResolvedMethod_getClassFromConstantPool = 106,
   ResolvedMethod_getDeclaringClassFromFieldOrStatic = 107,
   ResolvedMethod_classOfStatic = 108,
   ResolvedMethod_startAddressForJNIMethod = 109,
   ResolvedMethod_fieldAttributes = 110,
   ResolvedMethod_getResolvedStaticMethodAndMirror = 111,
   ResolvedMethod_getResolvedSpecialMethodAndMirror = 112,
   ResolvedMethod_classCPIndexOfMethod = 113,
   ResolvedMethod_startAddressForJittedMethod = 114,
   ResolvedMethod_localName = 115,
   ResolvedMethod_getResolvedPossiblyPrivateVirtualMethodAndMirror = 116,
   ResolvedMethod_virtualMethodIsOverridden = 117,
   ResolvedMethod_getResolvedInterfaceMethod_2 = 118, // arity 2
   ResolvedMethod_getResolvedInterfaceMethodAndMirror_3 = 119, // arity 3
   ResolvedMethod_getResolvedInterfaceMethodOffset = 120,
   ResolvedMethod_getUnresolvedStaticMethodInCP = 121,
   ResolvedMethod_isSubjectToPhaseChange = 122,
   ResolvedMethod_getUnresolvedSpecialMethodInCP = 123,
   ResolvedMethod_getUnresolvedFieldInCP = 124,
   ResolvedMethod_getRemoteROMString = 125,
   ResolvedMethod_fieldOrStaticName = 126,
   ResolvedMethod_getRemoteROMClassAndMethods = 127,
   ResolvedMethod_getResolvedHandleMethod = 128,
   ResolvedMethod_isUnresolvedMethodTypeTableEntry = 129,
   ResolvedMethod_methodTypeTableEntryAddress = 130,
   ResolvedMethod_isUnresolvedCallSiteTableEntry = 131,
   ResolvedMethod_callSiteTableEntryAddress = 132,
   ResolvedMethod_getResolvedDynamicMethod = 133,
   ResolvedMethod_shouldFailSetRecognizedMethodInfoBecauseOfHCR = 134,
   ResolvedMethod_isSameMethod = 135,
   ResolvedMethod_isBigDecimalMethod = 136,
   ResolvedMethod_isBigDecimalConvertersMethod = 137,
   ResolvedMethod_isInlineable = 139,
   ResolvedMethod_setWarmCallGraphTooBig = 141,
   ResolvedMethod_setVirtualMethodIsOverridden = 142,
   ResolvedMethod_addressContainingIsOverriddenBit = 143,
   ResolvedMethod_methodIsNotzAAPEligible = 144,
   ResolvedMethod_setClassForNewInstance = 145,
   ResolvedMethod_getJittedBodyInfo = 146,
   ResolvedMethod_getResolvedImproperInterfaceMethodAndMirror = 147,
   ResolvedMethod_isUnresolvedString = 148,
   ResolvedMethod_stringConstant = 149,
   ResolvedMethod_getResolvedVirtualMethod = 150,
   ResolvedMethod_getMultipleResolvedMethods = 151,
   ResolvedMethod_varHandleMethodTypeTableEntryAddress = 152,
   ResolvedMethod_isUnresolvedVarHandleMethodTypeTableEntry = 153,
   ResolvedMethod_getConstantDynamicTypeFromCP = 154,
   ResolvedMethod_isUnresolvedConstantDynamic = 155,
   ResolvedMethod_dynamicConstant = 156,
   ResolvedMethod_definingClassFromCPFieldRef = 157,

   ResolvedRelocatableMethod_createResolvedRelocatableJ9Method = 160,
   ResolvedRelocatableMethod_storeValidationRecordIfNecessary = 161,
   ResolvedRelocatableMethod_fieldAttributes = 162,
   ResolvedRelocatableMethod_staticAttributes = 163,
   ResolvedRelocatableMethod_getFieldType = 164,

   // For TR_J9ServerVM methods
   VM_isClassLibraryClass = 200,
   VM_isClassLibraryMethod = 201,
   VM_getSuperClass = 202,
   VM_isInstanceOf = 203,
   VM_isClassArray = 205,
   VM_transformJlrMethodInvoke = 206,
   VM_getStaticReferenceFieldAtAddress = 207,
   VM_getSystemClassFromClassName = 208,
   VM_isMethodTracingEnabled = 209,
   VM_getClassClassPointer = 211,
   VM_setJ2IThunk = 212,
   VM_getClassOfMethod = 213,
   VM_getBaseComponentClass = 214,
   VM_getLeafComponentClassFromArrayClass = 215,
   VM_isClassLoadedBySystemClassLoader = 216,
   VM_getClassFromSignature = 217,
   VM_jitFieldsAreSame = 218,
   VM_jitStaticsAreSame = 219,
   VM_getComponentClassFromArrayClass = 220,
   VM_classHasBeenReplaced = 221,
   VM_classHasBeenExtended = 222,
   VM_compiledAsDLTBefore = 223,
   VM_isThunkArchetype = 226,
   VM_printTruncatedSignature = 227,
   VM_getStaticHookAddress = 228,
   VM_isClassInitialized = 229,
   VM_getOSRFrameSizeInBytes = 230,
   VM_getInitialLockword = 231,
   VM_isString1 = 232,
   VM_getMethods = 233,
   VM_isPrimitiveArray = 236,
   VM_getAllocationSize = 237,
   VM_getObjectClass = 238,
   VM_stackWalkerMaySkipFrames = 239,
   VM_hasFinalFieldsInClass = 240,
   VM_getClassNameSignatureFromMethod = 241,
   VM_getHostClass = 242,
   VM_getStringUTF8Length = 243,
   VM_classInitIsFinished = 244,
   VM_getClassFromNewArrayType = 245,
   VM_isCloneable = 246,
   VM_canAllocateInlineClass = 247,
   VM_getArrayClassFromComponentClass = 248,
   VM_matchRAMclassFromROMclass = 249,
   VM_getReferenceFieldAtAddress = 250,
   VM_getVolatileReferenceFieldAt = 251,
   VM_getInt32FieldAt = 252,
   VM_getInt64FieldAt = 253,
   VM_setInt64FieldAt = 254,
   VM_compareAndSwapInt64FieldAt = 255,
   VM_getArrayLengthInElements = 256,
   VM_getClassFromJavaLangClass = 257,
   VM_getOffsetOfClassFromJavaLangClassField = 258,
   VM_getIdentityHashSaltPolicy = 261,
   VM_getOffsetOfJLThreadJ9Thread = 262,
   VM_getVFTEntry = 263,
   VM_scanReferenceSlotsInClassForOffset = 264,
   VM_findFirstHotFieldTenuredClassOffset = 265,
   VM_getResolvedVirtualMethod = 266,
   VM_sameClassLoaders = 267,
   VM_isUnloadAssumptionRequired = 268,
   VM_getInstanceFieldOffset = 270,
   VM_getJavaLangClassHashCode = 271,
   VM_hasFinalizer = 272,
   VM_getClassDepthAndFlagsValue = 273,
   VM_getMethodFromName = 274,
   VM_getMethodFromClass = 275,
   VM_isClassVisible = 276,
   VM_markClassForTenuredAlignment = 277,
   VM_getReferenceSlotsInClass = 278,
   VM_getMethodSize = 279,
   VM_addressOfFirstClassStatic = 280,
   VM_getStaticFieldAddress = 281,
   VM_getInterpreterVTableSlot = 282,
   VM_revertToInterpreted = 283,
   VM_getLocationOfClassLoaderObjectPointer = 284,
   VM_isOwnableSyncClass = 285,
   VM_getClassFromMethodBlock = 286,
   VM_fetchMethodExtendedFlagsPointer = 287,
   VM_stringEquals = 288,
   VM_getStringHashCode = 289,
   VM_getLineNumberForMethodAndByteCodeIndex = 290,
   VM_getObjectNewInstanceImplMethod = 291,
   VM_getBytecodePC = 292,
   VM_getClassFromStatic = 293,
   VM_setInvokeExactJ2IThunk = 294,
   VM_createMethodHandleArchetypeSpecimen = 295,
   VM_instanceOfOrCheckCast = 297,
   VM_getResolvedMethodsAndMirror = 300,
   VM_getVMInfo = 301,
   VM_isAnonymousClass = 302,
   VM_dereferenceStaticAddress = 303,
   VM_getClassFromCP = 304,
   VM_getROMMethodFromRAMMethod = 305,
   VM_getReferenceFieldAt = 306,
   VM_getJ2IThunk = 307,
   VM_needsInvokeExactJ2IThunk = 308,
   VM_instanceOfOrCheckCastNoCacheUpdate = 309,
   VM_getCellSizeForSizeClass = 310,
   VM_getObjectSizeClass = 311,
   VM_stackWalkerMaySkipFramesSVM = 312,

   // For static TR::CompilationInfo methods
   CompInfo_isCompiled = 400,
   CompInfo_getInvocationCount = 401,
   CompInfo_setInvocationCount = 402,
   CompInfo_getJ9MethodExtra = 403,
   CompInfo_isJNINative = 404,
   CompInfo_isJSR292 = 405,
   CompInfo_getMethodBytecodeSize = 406,
   CompInfo_setJ9MethodExtra = 407,
   CompInfo_setInvocationCountAtomic = 408,
   CompInfo_isClassSpecial = 409,
   CompInfo_getJ9MethodStartPC = 410,

   // For J9::ClassEnv Methods
   ClassEnv_classFlagsValue = 500,
   ClassEnv_classDepthOf = 501,
   ClassEnv_classInstanceSize = 502,
   ClassEnv_superClassesOf = 504,
   ClassEnv_indexedSuperClassOf = 505,
   ClassEnv_iTableOf = 506,
   ClassEnv_iTableNext = 507,
   ClassEnv_iTableRomClass = 508,
   ClassEnv_getITable = 509,
   ClassEnv_classHasIllegalStaticFinalFieldModification = 510,
   ClassEnv_getROMClassRefName = 511,

   // For TR_J9SharedCache
   SharedCache_getClassChainOffsetInSharedCache = 600,
   SharedCache_rememberClass = 601,
   SharedCache_addHint = 602,
   SharedCache_storeSharedData = 603,

   // For runFEMacro
   runFEMacro_invokeCollectHandleNumArgsToCollect = 700,
   runFEMacro_invokeExplicitCastHandleConvertArgs = 701,
   runFEMacro_targetTypeL = 702,
   runFEMacro_invokeILGenMacrosInvokeExactAndFixup = 703,
   runFEMacro_invokeArgumentMoverHandlePermuteArgs = 704,
   runFEMacro_invokePermuteHandlePermuteArgs = 705,
   runFEMacro_invokeGuardWithTestHandleNumGuardArgs = 706,
   runFEMacro_invokeInsertHandle = 707,
   runFEMacro_invokeDirectHandleDirectCall = 708,
   runFEMacro_invokeSpreadHandleArrayArg = 709,
   runFEMacro_invokeSpreadHandle = 710,
   runFEMacro_invokeFoldHandle = 711,
   runFEMacro_invokeFoldHandle2 = 712,
   runFEMacro_invokeFinallyHandle = 713,
   runFEMacro_invokeFilterArgumentsHandle = 714,
   runFEMacro_invokeFilterArgumentsHandle2 = 715,
   runFEMacro_invokeCatchHandle = 716,
   runFEMacro_invokeILGenMacrosParameterCount = 717,
   runFEMacro_invokeILGenMacrosArrayLength = 718,
   runFEMacro_invokeILGenMacrosGetField = 719,
   runFEMacro_invokeFilterArgumentsWithCombinerHandleNumSuffixArgs = 720,
   runFEMacro_invokeFilterArgumentsWithCombinerHandleFilterPosition = 721,
   runFEMacro_invokeFilterArgumentsWithCombinerHandleArgumentIndices = 722,
   runFEMacro_invokeCollectHandleAllocateArray = 723,

   // for JITServerPersistentCHTable
   CHTable_getAllClassInfo = 800,
   CHTable_getClassInfoUpdates = 801,
   CHTable_commit = 802,
   CHTable_clearReservable = 803,

   // for JITServerIProfiler
   IProfiler_profilingSample = 900,
   IProfiler_searchForMethodSample = 901,
   IProfiler_getMaxCallCount = 902,
   IProfiler_setCallCount = 903,

   Recompilation_getExistingMethodInfo = 1000,
   Recompilation_getJittedBodyInfoFromPC = 1001,

   ClassInfo_getRemoteROMString = 1100,

   // for KnownObjectTable
   KnownObjectTable_getOrCreateIndex = 1200,
   KnownObjectTable_getOrCreateIndexAt = 1201,
   KnownObjectTable_getPointer = 1202,
   KnownObjectTable_getExistingIndexAt = 1203,
   // for KnownObjectTable outside J9::KnownObjectTable class
   KnownObjectTable_symbolReferenceTableCreateKnownObject = 1204,
   KnownObjectTable_mutableCallSiteEpoch = 1205,
   KnownObjectTable_dereferenceKnownObjectField = 1206,
   KnownObjectTable_dereferenceKnownObjectField2 = 1207,
   KnownObjectTable_createSymRefWithKnownObject = 1208,
   KnownObjectTable_getReferenceField = 1209,
   KnownObjectTable_invokeDirectHandleDirectCall = 1210,
   KnownObjectTable_getKnownObjectTableDumpInfo = 1211,
   MessageType_MAXTYPE
   };

   const int MessageType_ARRAYSIZE = MessageType_MAXTYPE;
   }; // namespace JITServer
#endif // MESSAGE_TYPES_HPP

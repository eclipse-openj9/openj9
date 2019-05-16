/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef J9RUNTIME_INCL
#define J9RUNTIME_INCL

#include "runtime/Runtime.hpp"
#include "codegen/PreprologueConst.hpp"


#if defined(TR_HOST_S390)
void restoreJitEntryPoint(uint8_t* intEP, uint8_t* jitEP);
void saveJitEntryPoint(uint8_t* intEP, uint8_t* jitEP);
#endif



#if defined(TR_HOST_X86)

inline uint16_t jitEntryOffset(void *startPC)
   {
#if defined(TR_HOST_64BIT)
   return TR_LinkageInfo::get(startPC)->getReservedWord();
#else
   return 0;
#endif
   }

inline uint16_t jitEntryJmpInstruction(void *startPC, int32_t startPCToTarget)
   {
   const uint8_t TWO_BYTE_JUMP_INSTRUCTION = 0xEB;
   int32_t displacement = startPCToTarget - (jitEntryOffset(startPC) + 2);
   return (displacement << 8) | TWO_BYTE_JUMP_INSTRUCTION;
   }

void saveFirstTwoBytes(void *startPC, int32_t startPCToSaveArea);
void replaceFirstTwoBytesWithShortJump(void *startPC, int32_t startPCToTarget);
void replaceFirstTwoBytesWithData(void *startPC, int32_t startPCToData);
#endif



#if defined(TR_HOST_POWER)

#define  OFFSET_REVERT_INTP_PRESERVED_FSD                (-4)
#define  OFFSET_REVERT_INTP_FIXED_PORTION                (-12-2*sizeof(intptrj_t))

#define  OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC        (-(16+sizeof(intptrj_t)))
#define  OFFSET_SAMPLING_BRANCH_FROM_STARTPC             (-(12+sizeof(intptrj_t)))
#define  OFFSET_SAMPLING_METHODINFO_FROM_STARTPC         (-(8+sizeof(intptrj_t)))
#define  OFFSET_SAMPLING_PRESERVED_FROM_STARTPC          (-8)

inline uint32_t getJitEntryOffset(TR_LinkageInfo *linkageInfo)
   {
   return linkageInfo->getReservedWord() & 0x0ffff;
   }
#endif



#if defined(TR_HOST_ARM)
#define  OFFSET_REVERT_INTP_FIXED_PORTION                (-12-2*sizeof(intptrj_t))
#define  OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC        (-(16+sizeof(intptrj_t)))
#define  OFFSET_SAMPLING_BRANCH_FROM_STARTPC             (-(12+sizeof(intptrj_t)))
#define  OFFSET_METHODINFO_FROM_STARTPC                  (-(8+sizeof(intptrj_t)))
#define  OFFSET_SAMPLING_PRESERVED_FROM_STARTPC          (-8)
#define  START_PC_TO_METHOD_INFO_ADDRESS                  -8 // offset from startpc to jitted body info
#define  OFFSET_COUNTING_BRANCH_FROM_JITENTRY             36
#endif

/* Functions used by AOT runtime to fixup recompilation info for AOT */
#if defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64)
uint32_t *getLinkageInfo(void * startPC);
uint32_t isRecompMethBody(void *li);
void fixPersistentMethodInfo(void *table);
void fixupMethodInfoAddressInCodeCache(void *startPC, void *bodyInfo);
#endif


typedef struct TR_InlinedSiteLinkedListEntry
   {
   TR_ExternalRelocationTargetKind reloType;
   uint8_t *location;
   uint8_t *destination;
   uint8_t *guard;
   TR_InlinedSiteLinkedListEntry *next;
   } TR_InlinedSiteLinkedListEntry;


typedef struct TR_InlinedSiteHastTableEntry
   {
   TR_InlinedSiteLinkedListEntry *first;
   TR_InlinedSiteLinkedListEntry *last;
   } TR_InlinedSiteHastTableEntry;


typedef enum
   {
   inlinedMethodIsStatic = 1,
   inlinedMethodIsSpecial = 2,
   inlinedMethodIsVirtual = 3
   } TR_InlinedMethodKind;


typedef enum
   {
   needsFullSizeRuntimeAssumption = 1
   } TR_HCRAssumptionFlags;


typedef enum
   {
   noPerfAssumptions = 0,
   tooManyFailedValidations = 1,
   tooManyFailedInlinedMethodRelos = 2,
   tooManyFailedInlinedAllocRelos = 3
   } TR_FailedPerfAssumptionCode;



/* TR_AOTMethodHeader Versions:
*     1.0    Java6 GA
*     1.1    Java6 SR1
*     2.0    Java7
*     2.1    Java7
*     3.0    Java 8
*/

#define TR_AOTMethodHeader_MajorVersion   3
#define TR_AOTMethodHeader_MinorVersion   0

typedef struct TR_AOTMethodHeader {
   uint16_t  minorVersion;
   uint16_t  majorVersion;
   uint32_t  offsetToRelocationDataItems;
   uint32_t  offsetToExceptionTable;
   uint32_t  offsetToPersistentInfo;
   uintptr_t compileMethodCodeStartPC;
   uintptr_t compileMethodCodeSize;
   uintptr_t compileMethodDataStartPC;
   uintptr_t compileMethodDataSize;
   uintptr_t compileFirstClassLocation;
   uint32_t flags;
   } TR_AOTMethodHeader;


/* AOT Method flags */
#define TR_AOTMethodHeader_NeedsRecursiveMethodTrampolineReservation 0x00000001
#define TR_AOTMethodHeader_IsNotCapableOfMethodEnterTracing          0x00000002
#define TR_AOTMethodHeader_IsNotCapableOfMethodExitTracing           0x00000004
#define TR_AOTMethodHeader_UsesEnableStringCompressionFolding        0x00000008
#define TR_AOTMethodHeader_StringCompressionEnabled                  0x00000010
#define TR_AOTMethodHeader_UsesSymbolValidationManager               0x00000020
#define TR_AOTMethodHeader_TMDisabled                                0x00000040



typedef struct TR_AOTInliningStats
   {
   int32_t numFailedValidations;
   int32_t numSucceededValidations;
   int32_t numMethodFromDiffClassLoader;
   int32_t numMethodInSameClass;
   int32_t numMethodNotInSameClass;
   int32_t numMethodResolvedAtCompile;
   int32_t numMethodNotResolvedAtCompile;
   int32_t numMethodROMMethodNotInSC;
   } TR_AOTInliningStats;


typedef struct TR_AOTStats
   {
   int32_t numCHEntriesAlreadyStoredInLocalList;
   int32_t numStaticEntriesAlreadyStoredInLocalList;
   int32_t numNewCHEntriesInLocalList;
   int32_t numNewStaticEntriesInLocalList;
   int32_t numNewCHEntriesInSharedClass;
   int32_t numEntriesFoundInLocalChain;
   int32_t numEntriesFoundAndValidatedInSharedClass;
   int32_t numClassChainNotInSharedClass;
   int32_t numCHInSharedCacheButFailValiation;
   int32_t numInstanceFieldInfoNotUsed;
   int32_t numStaticFieldInfoNotUsed;
   int32_t numDefiningClassNotFound;
   int32_t numInstanceFieldInfoUsed;
   int32_t numStaticFieldInfoUsed;
   int32_t numCannotGenerateHashForStore; // Shouldn't happen
   int32_t numRuntimeChainNotFound;
   int32_t numRuntimeStaticFieldUnresolvedCP;
   int32_t numRuntimeInstanceFieldUnresolvedCP;
   int32_t numRuntimeUnresolvedStaticFieldFromCP;
   int32_t numRuntimeUnresolvedInstanceFieldFromCP;
   int32_t numRuntimeResolvedStaticFieldButFailValidation;
   int32_t numRuntimeResolvedInstanceFieldButFailValidation;
   int32_t numRuntimeStaticFieldReloOK;
   int32_t numRuntimeInstanceFieldReloOK;

   int32_t numInlinedMethodOverridden;
   int32_t numInlinedMethodNotResolved;
   int32_t numInlinedMethodClassNotMatch;
   int32_t numInlinedMethodCPNotResolved;
   int32_t numInlinedMethodRelocated;
   int32_t numInlinedMethodValidationFailed;

   TR_AOTInliningStats staticMethods;
   TR_AOTInliningStats specialMethods;
   TR_AOTInliningStats virtualMethods;
   TR_AOTInliningStats interfaceMethods;
   TR_AOTInliningStats abstractMethods;

   TR_AOTInliningStats profiledInlinedMethods;
   TR_AOTInliningStats profiledClassGuards;
   TR_AOTInliningStats profiledMethodGuards;

   int32_t numDataAddressRelosSucceed;
   int32_t numDataAddressRelosFailed;

   int32_t numCheckcastNodesIlgenTime;
   int32_t numInstanceofNodesIlgenTime;
   int32_t numCheckcastNodesCodegenTime;
   int32_t numInstanceofNodesCodegenTime;

   int32_t numRuntimeClassAddressUnresolvedCP;
   int32_t numRuntimeClassAddressFromCP;
   int32_t numRuntimeClassAddressButFailValidation;
   int32_t numRuntimeClassAddressReloOK;

   int32_t numRuntimeClassAddressRelocationCount;
   int32_t numRuntimeClassAddressReloUnresolvedCP;
   int32_t numRuntimeClassAddressReloUnresolvedClass;

   int32_t numVMCheckCastEvaluator;
   int32_t numVMInstanceOfEvaluator;
   int32_t numVMIfInstanceOfEvaluator;
   int32_t numCheckCastVMHelperInstructions;
   int32_t numInstanceOfVMHelperInstructions;
   int32_t numIfInstanceOfVMHelperInstructions;

   int32_t numClassValidations;
   int32_t numClassValidationsFailed;
   int32_t numWellKnownClassesValidationsFailed;

   TR_FailedPerfAssumptionCode failedPerfAssumptionCode;

   uint32_t numRelocationsFailedByType[TR_NumExternalRelocationKinds];

   } TR_AOTStats;


#endif

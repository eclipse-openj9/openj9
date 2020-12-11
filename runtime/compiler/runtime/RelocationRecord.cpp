/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#include <stdint.h>
#include "jilconsts.h"
#include "jitprotos.h"
#include "jvminit.h"
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9cp.h"
#include "j9protos.h"
#include "rommeth.h"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/PicHelpers.hpp"
#include "codegen/Relocation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CHTable.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/jittypes.h"
#include "env/VMAccessCriticalSection.hpp"
#include "exceptions/AOTFailure.hpp"
#include "il/StaticSymbol.hpp"
#include "infra/SimpleRegex.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/RelocationRecord.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRuntimeLogger.hpp"
#include "runtime/RelocationTarget.hpp"
#include  "runtime/SymbolValidationManager.hpp"
#include "env/VMJ9.h"
#include "control/rossa.h"
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "control/MethodToBeCompiled.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */

// TODO: move this someplace common for RuntimeAssumptions.cpp and here
#if defined(__IBMCPP__) && !defined(AIXPPC) && !defined(LINUXPPC)
#define ASM_CALL __cdecl
#else
#define ASM_CALL
#endif
#if defined(TR_HOST_S390) || defined(TR_HOST_X86) // gotten from RuntimeAssumptions.cpp, should common these up
extern "C" void _patchVirtualGuard(uint8_t *locationAddr, uint8_t *destinationAddr, int32_t smpFlag);
#else
extern "C" void ASM_CALL _patchVirtualGuard(uint8_t*, uint8_t*, uint32_t);
#endif

// START OF BINARY TEMPLATES

// These *BinaryTemplate structs describe the shape of the binary relocation records.
struct TR_RelocationRecordBinaryTemplate
   {
   uint8_t type(TR_RelocationTarget *reloTarget);

   uint16_t _size;
   uint8_t _type;
   uint8_t _flags;

#if defined(TR_HOST_64BIT)
   uint32_t _extra;
#endif
   };

// Generating 32-bit code on a 64-bit machine or vice-versa won't work because the alignment won't
// be right.  Relying on inheritance in the structures here means padding is automatically inserted
// at each inheritance boundary: this inserted padding won't match across 32-bit and 64-bit platforms.
// Making as many fields as possible UDATA should minimize the differences and gives the most freedom
// in the hierarchy of binary relocation record structures, but the header definitely has an inheritance
// boundary at offset 4B.

// This struct must be identical to TR_RelocationRecordBinaryTemplate
// in at least the first three (_size, _type, and _flags) fields
struct TR_RelocationRecordHelperAddressBinaryTemplate
   {
   uint16_t _size;
   uint8_t _type;
   uint8_t _flags;
   uint32_t _helperID;
   };
static_assert(offsetof(TR_RelocationRecordBinaryTemplate, _size)
              == offsetof(TR_RelocationRecordHelperAddressBinaryTemplate, _size),
              "TR_RelocationRecordHelperAddressBinaryTemplate::_size not the same offset as TR_RelocationRecordBinaryTemplate::_size");
static_assert(offsetof(TR_RelocationRecordBinaryTemplate, _type)
              == offsetof(TR_RelocationRecordHelperAddressBinaryTemplate, _type),
              "TR_RelocationRecordHelperAddressBinaryTemplate::_type not the same offset as TR_RelocationRecordBinaryTemplate::_type");
static_assert(offsetof(TR_RelocationRecordBinaryTemplate, _flags)
              == offsetof(TR_RelocationRecordHelperAddressBinaryTemplate, _flags),
              "TR_RelocationRecordHelperAddressBinaryTemplate::_flags not the same offset as TR_RelocationRecordBinaryTemplate::_flags");

// This struct must be identical to TR_RelocationRecordBinaryTemplate
// in at least the first three (_size, _type, and _flags) fields
struct TR_RelocationRecordPicTrampolineBinaryTemplate
   {
   uint16_t _size;
   uint8_t _type;
   uint8_t _flags;
   uint32_t _numTrampolines;
   };
static_assert(offsetof(TR_RelocationRecordBinaryTemplate, _size)
              == offsetof(TR_RelocationRecordPicTrampolineBinaryTemplate, _size),
              "TR_RelocationRecordPicTrampolineBinaryTemplate::_size not the offset same as TR_RelocationRecordBinaryTemplate::_size");
static_assert(offsetof(TR_RelocationRecordBinaryTemplate, _type)
              == offsetof(TR_RelocationRecordPicTrampolineBinaryTemplate, _type),
              "TR_RelocationRecordPicTrampolineBinaryTemplate::_type not the offset same as TR_RelocationRecordBinaryTemplate::_type");
static_assert(offsetof(TR_RelocationRecordBinaryTemplate, _flags)
              == offsetof(TR_RelocationRecordPicTrampolineBinaryTemplate, _flags),
              "TR_RelocationRecordPicTrampolineBinaryTemplate::_flags not the offset same as TR_RelocationRecordBinaryTemplate::_flags");

struct TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   UDATA _inlinedSiteIndex;
   };

struct TR_RelocationRecordValidateArbitraryClassBinaryTemplate : public TR_RelocationRecordBinaryTemplate //public TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate
   {
   UDATA _loaderClassChainOffset;
   UDATA _classChainOffsetForClassBeingValidated;
   };


struct TR_RelocationRecordWithOffsetBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   UDATA _offset;
   };

struct TR_RelocationRecordConstantPoolBinaryTemplate : public TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate
   {
   UDATA _constantPool;
   };

struct TR_RelocationRecordConstantPoolWithIndexBinaryTemplate : public TR_RelocationRecordConstantPoolBinaryTemplate
   {
   UDATA _index;
   };

struct TR_RelocationRecordJ2IVirtualThunkPointerBinaryTemplate : public TR_RelocationRecordConstantPoolBinaryTemplate
   {
   IDATA _offsetToJ2IVirtualThunkPointer;
   };

struct TR_RelocationRecordInlinedAllocationBinaryTemplate : public TR_RelocationRecordConstantPoolWithIndexBinaryTemplate
   {
   UDATA _branchOffset;
   };

struct TR_RelocationRecordVerifyClassObjectForAllocBinaryTemplate : public TR_RelocationRecordInlinedAllocationBinaryTemplate
   {
   UDATA _allocationSize;
   };

struct TR_RelocationRecordRomClassFromCPBinaryTemplate : public TR_RelocationRecordConstantPoolWithIndexBinaryTemplate
   {
   UDATA  _romClassOffsetInSharedCache;
   };

typedef TR_RelocationRecordRomClassFromCPBinaryTemplate TR_RelocationRecordInlinedMethodBinaryTemplate;

struct TR_RelocationRecordNopGuardBinaryTemplate : public TR_RelocationRecordInlinedMethodBinaryTemplate
   {
   UDATA _destinationAddress;
   };

struct TR_RelocationRecordProfiledInlinedMethodBinaryTemplate : TR_RelocationRecordInlinedMethodBinaryTemplate
   {
   UDATA _classChainIdentifyingLoaderOffsetInSharedCache;

   // Class chain of the defining class of the inlined method
   UDATA _classChainForInlinedMethod;

   // The inlined j9method's index into its defining class' array of j9methods
   UDATA _methodIndex;
   };

struct TR_RelocationRecordMethodTracingCheckBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   UDATA _destinationAddress;
   };

struct TR_RelocationRecordValidateClassBinaryTemplate : public TR_RelocationRecordConstantPoolWithIndexBinaryTemplate
   {
   UDATA _classChainOffsetInSharedCache;
   };

typedef TR_RelocationRecordRomClassFromCPBinaryTemplate TR_RelocationRecordValidateStaticFieldBinaryTemplate;

struct TR_RelocationRecordDataAddressBinaryTemplate : public TR_RelocationRecordConstantPoolWithIndexBinaryTemplate
   {
   UDATA _offset;
   };

struct TR_RelocationRecordPointerBinaryTemplate : TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate
   {
   UDATA _classChainIdentifyingLoaderOffsetInSharedCache;
   UDATA _classChainForInlinedMethod;
   };

struct TR_RelocationRecordMethodPointerBinaryTemplate : TR_RelocationRecordPointerBinaryTemplate
   {
   UDATA _vTableSlot;
   };
struct TR_RelocationRecordEmitClassBinaryTemplate : public TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate
   {
   int32_t _bcIndex;
#if defined(TR_HOST_64BIT)
   uint32_t _extra;
#endif
   };

struct TR_RelocationRecordDebugCounterBinaryTemplate : public TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate
   {
   int8_t _fidelity;
   int32_t _bcIndex;
   int32_t _delta;
   int32_t _staticDelta;
   UDATA _offsetOfNameString;
   };

typedef TR_RelocationRecordBinaryTemplate TR_RelocationRecordClassUnloadAssumptionBinaryTemplate;

struct TR_RelocationRecordValidateClassByNameBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _classID;
   uint16_t _beholderID;
   UDATA _classChainOffsetInSCC;
   };

struct TR_RelocationRecordValidateProfiledClassBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _classID;
   UDATA _classChainOffsetInSCC;
   UDATA _classChainOffsetForCLInScc;
   };

struct TR_RelocationRecordValidateClassFromCPBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _classID;
   uint16_t _beholderID;
   uint32_t _cpIndex;
   };

struct TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint8_t _isStatic;
   uint16_t _classID;
   uint16_t _beholderID;
   uint32_t _cpIndex;
   };

struct TR_RelocationRecordValidateArrayFromCompBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _arrayClassID;
   uint16_t _componentClassID;
   };

struct TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _superClassID;
   uint16_t _childClassID;
   };

struct TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint8_t _objectTypeIsFixed;
   uint8_t _castTypeIsFixed;
   uint8_t _isInstanceOf;
   uint16_t _classOneID;
   uint16_t _classTwoID;
   };

struct TR_RelocationRecordValidateSystemClassByNameBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _systemClassID;
   UDATA _classChainOffsetInSCC;
   };

struct TR_RelocationRecordValidateClassChainBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _classID;
   UDATA _classChainOffsetInSCC;
   };

struct TR_RelocationRecordValidateMethodFromClassBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _beholderID;
   uint32_t _index;
   };

struct TR_RelocationRecordValidateMethodFromCPBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _beholderID;
   uint16_t _cpIndex; // constrained to 16 bits by the class file format
   };

struct TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _beholderID;

   // Value is (offset | ignoreRtResolve); offset must be even
   uint16_t _virtualCallOffsetAndIgnoreRtResolve;
   };

struct TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _beholderID;
   uint16_t _lookupID;
   uint16_t _cpIndex; // constrained to 16 bits by the class file format
   };

struct TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _lookupClassID;
   uint16_t _beholderID;
   UDATA _romMethodOffsetInSCC;
   };

struct TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _thisClassID;
   int32_t _cpIndexOrVftSlot;
   uint16_t _callerMethodID;
   uint16_t _useGetResolvedInterfaceMethod;
   };

struct TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _thisClassID;
   uint16_t _callerMethodID;
   uint16_t _cpIndex; // constrained to 16 bits by the class file format
   };

struct TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _thisClassID;
   uint16_t _callerMethodID;
   int32_t _vftSlot;
   };

struct TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _methodClassID;
   uint8_t _skipFrames;
   };

struct TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _classID;
   uint8_t _isInitialized;
   };

struct TR_RelocationRecordSymbolFromManagerBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _symbolID;
   uint16_t _symbolType;
   };

struct TR_RelocationRecordMethodCallAddressBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   UDATA _methodAddress;
   };

struct TR_RelocationRecordResolvedTrampolinesBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _symbolID;
   };

struct TR_RelocationRecordBlockFrequencyBinaryTemplate: public TR_RelocationRecordBinaryTemplate
   {
   uintptr_t _frequencyOffset;
   };

// END OF BINARY TEMPLATES

uint8_t
TR_RelocationRecordBinaryTemplate::type(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b(&_type);
   }

// TR_RelocationRecordGroup

void
TR_RelocationRecordGroup::setSize(TR_RelocationTarget *reloTarget, uintptr_t size)
   {
   reloTarget->storePointer((uint8_t *)size, (uint8_t *) &_group);
   }

uintptr_t
TR_RelocationRecordGroup::size(TR_RelocationTarget *reloTarget)
   {
   return (uintptr_t)reloTarget->loadPointer((uint8_t *) _group);
   }

TR_RelocationRecordBinaryTemplate *
TR_RelocationRecordGroup::firstRecord(
   TR_RelocationRuntime *reloRuntime,
   TR_RelocationTarget *reloTarget)
   {
   // The first word is the size of the group (itself pointer-sized).
   // When using the SVM, the second is a pointer-sized SCC offset to the
   // well-known classes' class chain offsets.
   bool useSVM = reloRuntime->comp()->getOption(TR_UseSymbolValidationManager);
   int offs = 1 + int(useSVM);
   return (TR_RelocationRecordBinaryTemplate *) (((uintptr_t *)_group)+offs);
   }

TR_RelocationRecordBinaryTemplate *
TR_RelocationRecordGroup::pastLastRecord(TR_RelocationTarget *reloTarget)
   {
   return (TR_RelocationRecordBinaryTemplate *) ((uint8_t *)_group + size(reloTarget));
   }

const uintptr_t *
TR_RelocationRecordGroup::wellKnownClassChainOffsets(
   TR_RelocationRuntime *reloRuntime,
   TR_RelocationTarget *reloTarget)
   {
   if (!TR::comp()->getOption(TR_UseSymbolValidationManager))
      return NULL;

   // The first word is the size of the group (itself pointer-sized). Skip it
   // to reach the SCC offset of the well-known classes' class chain offsets.
   uintptr_t offset = *((uintptr_t *)_group + 1);
   void *classChains =
      reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(offset);

   return reinterpret_cast<const uintptr_t*>(classChains);
   }

int32_t
TR_RelocationRecordGroup::applyRelocations(TR_RelocationRuntime *reloRuntime,
                                           TR_RelocationTarget *reloTarget,
                                           uint8_t *reloOrigin)
   {
   const uintptr_t *wkClassChainOffsets =
      wellKnownClassChainOffsets(reloRuntime, reloTarget);
   TR_AOTStats *aotStats = reloRuntime->aotStats();

   if (wkClassChainOffsets != NULL)
      {
      TR::SymbolValidationManager *svm =
         reloRuntime->comp()->getSymbolValidationManager();

      if (!svm->validateWellKnownClasses(wkClassChainOffsets))
         {
         if (aotStats)
            aotStats->numWellKnownClassesValidationsFailed++;
         return compilationAotClassReloFailure;
         }
      }

   TR_RelocationRecordBinaryTemplate *recordPointer = firstRecord(reloRuntime, reloTarget);
   TR_RelocationRecordBinaryTemplate *endOfRecords = pastLastRecord(reloTarget);

   while (recordPointer < endOfRecords)
      {
      TR_RelocationRecord storage;
      // Create a specific type of relocation record based on the information
      // in the binary record pointed to by `recordPointer`
      TR_RelocationRecord *reloRecord = TR_RelocationRecord::create(&storage, reloRuntime, reloTarget, recordPointer);
      int32_t rc = handleRelocation(reloRuntime, reloTarget, reloRecord, reloOrigin);
      if (rc != 0)
         {
         uint8_t reloType = recordPointer->type(reloTarget);
         aotStats->numRelocationsFailedByType[reloType]++;
         return rc;
         }

      recordPointer = reloRecord->nextBinaryRecord(reloTarget);
      }

   return 0;
   }


int32_t
TR_RelocationRecordGroup::handleRelocation(TR_RelocationRuntime *reloRuntime,
                                           TR_RelocationTarget *reloTarget,
                                           TR_RelocationRecord *reloRecord,
                                           uint8_t *reloOrigin)
   {
   if (reloRuntime->reloLogger()->logEnabled())
      reloRecord->print(reloRuntime);

   switch (reloRecord->action(reloRuntime))
      {
      case TR_RelocationRecordAction::apply:
         {
         reloRecord->preparePrivateData(reloRuntime, reloTarget);
         return reloRecord->applyRelocationAtAllOffsets(reloRuntime, reloTarget, reloOrigin);
         }
      case TR_RelocationRecordAction::ignore:
         {
         RELO_LOG(reloRuntime->reloLogger(), 6, "\tignore!\n");
         return 0;
         }
      case TR_RelocationRecordAction::failCompilation:
         {
         RELO_LOG(reloRuntime->reloLogger(), 6, "\tINTERNAL ERROR!\n");
         return compilationAotClassReloFailure;
         }
      default:
         {
         TR_ASSERT_FATAL(false, "Unknown relocation action %d\n", reloRecord->action(reloRuntime));
         }
      }

   return compilationAotClassReloFailure;
   }

TR_RelocationRecord *
TR_RelocationRecord::create(TR_RelocationRecord *storage, TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_RelocationRecordBinaryTemplate *record)
   {
   return create(storage, reloRuntime, record->type(reloTarget), record);
   }

TR_RelocationRecord *
TR_RelocationRecord::create(TR_RelocationRecord *storage, TR_RelocationRuntime *reloRuntime, uint8_t reloType, TR_RelocationRecordBinaryTemplate *record)
   {
   TR_RelocationRecord *reloRecord = NULL;
   switch (reloType)
      {
      case TR_HelperAddress:
         reloRecord = new (storage) TR_RelocationRecordHelperAddress(reloRuntime, record);
         break;
      case TR_ConstantPool:
         reloRecord = new (storage) TR_RelocationRecordConstantPool(reloRuntime, record);
         break;
      case TR_BlockFrequency:
         reloRecord = new (storage) TR_RelocationRecordBlockFrequency(reloRuntime, record);
         break;
      case TR_RecompQueuedFlag:
         reloRecord = new  (storage) TR_RelocationRecordRecompQueuedFlag(reloRuntime, record);
         break;
      case TR_BodyInfoAddress:
         reloRecord = new (storage) TR_RelocationRecordBodyInfo(reloRuntime, record);
         break;
      case TR_VerifyRefArrayForAlloc:
         reloRecord = new (storage) TR_RelocationRecordVerifyRefArrayForAlloc(reloRuntime, record);
         break;
      case TR_VerifyClassObjectForAlloc:
         reloRecord = new (storage) TR_RelocationRecordVerifyClassObjectForAlloc(reloRuntime, record);
         break;
      case TR_InlinedStaticMethodWithNopGuard:
         reloRecord = new (storage) TR_RelocationRecordInlinedStaticMethodWithNopGuard(reloRuntime, record);
         break;
      case TR_InlinedStaticMethod:
         reloRecord = new (storage) TR_RelocationRecordInlinedStaticMethod(reloRuntime, record);
         break;
      case TR_InlinedSpecialMethodWithNopGuard:
         reloRecord = new (storage) TR_RelocationRecordInlinedSpecialMethodWithNopGuard(reloRuntime, record);
         break;
      case TR_InlinedSpecialMethod:
         reloRecord = new (storage) TR_RelocationRecordInlinedSpecialMethod(reloRuntime, record);
         break;
      case TR_InlinedVirtualMethodWithNopGuard:
         reloRecord = new (storage) TR_RelocationRecordInlinedVirtualMethodWithNopGuard(reloRuntime, record);
         break;
      case TR_InlinedVirtualMethod:
         reloRecord = new (storage) TR_RelocationRecordInlinedVirtualMethod(reloRuntime, record);
         break;
      case TR_InlinedInterfaceMethodWithNopGuard:
         reloRecord = new (storage) TR_RelocationRecordInlinedInterfaceMethodWithNopGuard(reloRuntime, record);
         break;
      case TR_InlinedInterfaceMethod:
         reloRecord = new (storage) TR_RelocationRecordInlinedInterfaceMethod(reloRuntime, record);
         break;
      case TR_InlinedAbstractMethodWithNopGuard:
         reloRecord = new (storage) TR_RelocationRecordInlinedAbstractMethodWithNopGuard(reloRuntime, record);
         break;
      case TR_InlinedAbstractMethod:
         reloRecord = new (storage) TR_RelocationRecordInlinedAbstractMethod(reloRuntime, record);
         break;
      case TR_ProfiledInlinedMethodRelocation:
         reloRecord = new (storage) TR_RelocationRecordProfiledInlinedMethod(reloRuntime, record);
         break;
      case TR_ProfiledClassGuardRelocation:
         reloRecord = new (storage) TR_RelocationRecordProfiledClassGuard(reloRuntime, record);
         break;
      case TR_ProfiledMethodGuardRelocation:
         reloRecord = new (storage) TR_RelocationRecordProfiledMethodGuard(reloRuntime, record);
         break;
      case TR_ValidateClass:
         reloRecord = new (storage) TR_RelocationRecordValidateClass(reloRuntime, record);
         break;
      case TR_ValidateInstanceField:
         reloRecord = new (storage) TR_RelocationRecordValidateInstanceField(reloRuntime, record);
         break;
      case TR_ValidateStaticField:
         reloRecord = new (storage) TR_RelocationRecordValidateStaticField(reloRuntime, record);
         break;
      case TR_ValidateArbitraryClass:
         reloRecord = new (storage) TR_RelocationRecordValidateArbitraryClass(reloRuntime, record);
         break;
      case TR_RamMethod:
         reloRecord = new (storage) TR_RelocationRecordRamMethod(reloRuntime, record);
         break;
      case TR_CheckMethodEnter:
         reloRecord = new (storage) TR_RelocationRecordMethodEnterCheck(reloRuntime, record);
         break;
      case TR_CheckMethodExit:
         reloRecord = new (storage) TR_RelocationRecordMethodExitCheck(reloRuntime, record);
         break;
      case TR_AbsoluteHelperAddress:
         reloRecord = new (storage) TR_RelocationRecordAbsoluteHelperAddress(reloRuntime, record);
         break;
      case TR_RelativeMethodAddress:
      case TR_AbsoluteMethodAddress:
      case TR_AbsoluteMethodAddressOrderedPair:
         reloRecord = new (storage) TR_RelocationRecordMethodAddress(reloRuntime, record);
         break;
      case TR_DataAddress:
         reloRecord = new (storage) TR_RelocationRecordDataAddress(reloRuntime, record);
         break;
      case TR_StaticRamMethodConst:
         reloRecord = new (storage) TR_RelocationRecordStaticRamMethodConst(reloRuntime, record);
         break;
      case TR_VirtualRamMethodConst:
         reloRecord = new (storage) TR_RelocationRecordVirtualRamMethodConst(reloRuntime, record);
         break;
      case TR_SpecialRamMethodConst:
         reloRecord = new (storage) TR_RelocationRecordSpecialRamMethodConst(reloRuntime, record);
         break;
      case TR_JNIStaticTargetAddress:
         reloRecord = new (storage) TR_RelocationRecordDirectJNIStaticMethodCall(reloRuntime, record);
         break;
      case TR_JNIVirtualTargetAddress:
         reloRecord = new (storage) TR_RelocationRecordDirectJNIVirtualMethodCall(reloRuntime, record);
         break;
      case TR_JNISpecialTargetAddress:
         reloRecord = new (storage) TR_RelocationRecordDirectJNISpecialMethodCall(reloRuntime, record);
         break;
      case TR_ClassAddress:
         reloRecord = new (storage) TR_RelocationRecordClassAddress(reloRuntime, record);
         break;
      case TR_MethodObject:
         reloRecord = new (storage) TR_RelocationRecordMethodObject(reloRuntime, record);
         break;
      case TR_PicTrampolines:
         reloRecord = new (storage) TR_RelocationRecordPicTrampolines(reloRuntime, record);
         break;
      case TR_Trampolines:
         reloRecord = new (storage) TR_RelocationRecordTrampolines(reloRuntime, record);
         break;
      case TR_Thunks:
         reloRecord = new (storage) TR_RelocationRecordThunks(reloRuntime, record);
         break;
      case TR_J2IVirtualThunkPointer:
         reloRecord = new (storage) TR_RelocationRecordJ2IVirtualThunkPointer(reloRuntime, record);
         break;
      case TR_GlobalValue:
         reloRecord = new (storage) TR_RelocationRecordGlobalValue(reloRuntime, record);
         break;
      case TR_BodyInfoAddressLoad:
         reloRecord = new (storage) TR_RelocationRecordBodyInfoLoad(reloRuntime, record);
         break;
      case TR_ArrayCopyHelper:
         reloRecord = new (storage) TR_RelocationRecordArrayCopyHelper(reloRuntime, record);
         break;
      case TR_ArrayCopyToc:
         reloRecord = new (storage) TR_RelocationRecordArrayCopyToc(reloRuntime, record);
         break;
      case TR_RamMethodSequence:
         reloRecord = new (storage) TR_RelocationRecordRamSequence(reloRuntime, record);
         break;
      case TR_FixedSequenceAddress:
      case TR_FixedSequenceAddress2:
         reloRecord = new (storage) TR_RelocationRecordWithOffset(reloRuntime, record);
         break;
      case TR_HCR:
         reloRecord = new (storage) TR_RelocationRecordHCR(reloRuntime, record);
         break;
      case TR_ClassPointer:
         reloRecord = new (storage) TR_RelocationRecordClassPointer(reloRuntime, record);
         break;
      case TR_ArbitraryClassAddress:
         reloRecord = new (storage) TR_RelocationRecordArbitraryClassAddress(reloRuntime, record);
         break;
      case TR_MethodPointer:
         reloRecord = new (storage) TR_RelocationRecordMethodPointer(reloRuntime, record);
         break;
      case TR_EmitClass:
         reloRecord = new (storage) TR_RelocationRecordEmitClass(reloRuntime, record);
         break;
      case TR_DebugCounter:
         reloRecord = new (storage) TR_RelocationRecordDebugCounter(reloRuntime, record);
         break;
      case TR_ClassUnloadAssumption:
         reloRecord = new (storage) TR_RelocationRecordClassUnloadAssumption(reloRuntime, record);
         break;
      case TR_ValidateClassByName:
         reloRecord = new (storage) TR_RelocationRecordValidateClassByName(reloRuntime, record);
         break;
      case TR_ValidateProfiledClass:
         reloRecord = new (storage) TR_RelocationRecordValidateProfiledClass(reloRuntime, record);
         break;
      case TR_ValidateClassFromCP:
         reloRecord = new (storage) TR_RelocationRecordValidateClassFromCP(reloRuntime, record);
         break;
      case TR_ValidateDefiningClassFromCP:
         reloRecord = new (storage) TR_RelocationRecordValidateDefiningClassFromCP(reloRuntime, record);
         break;
      case TR_ValidateStaticClassFromCP:
         reloRecord = new (storage) TR_RelocationRecordValidateStaticClassFromCP(reloRuntime, record);
         break;
      case TR_ValidateArrayClassFromComponentClass:
         reloRecord = new (storage) TR_RelocationRecordValidateArrayClassFromComponentClass(reloRuntime, record);
         break;
      case TR_ValidateSuperClassFromClass:
         reloRecord = new (storage) TR_RelocationRecordValidateSuperClassFromClass(reloRuntime, record);
         break;
      case TR_ValidateClassInstanceOfClass:
         reloRecord = new (storage) TR_RelocationRecordValidateClassInstanceOfClass(reloRuntime, record);
         break;
      case TR_ValidateSystemClassByName:
         reloRecord = new (storage) TR_RelocationRecordValidateSystemClassByName(reloRuntime, record);
         break;
      case TR_ValidateClassFromITableIndexCP:
         reloRecord = new (storage) TR_RelocationRecordValidateClassFromITableIndexCP(reloRuntime, record);
         break;
      case TR_ValidateDeclaringClassFromFieldOrStatic:
         reloRecord = new (storage) TR_RelocationRecordValidateDeclaringClassFromFieldOrStatic(reloRuntime, record);
         break;
      case TR_ValidateConcreteSubClassFromClass:
         reloRecord = new (storage) TR_RelocationRecordValidateConcreteSubClassFromClass(reloRuntime, record);
         break;
      case TR_ValidateClassChain:
         reloRecord = new (storage) TR_RelocationRecordValidateClassChain(reloRuntime, record);
         break;
      case TR_ValidateMethodFromClass:
         reloRecord = new (storage) TR_RelocationRecordValidateMethodFromClass(reloRuntime, record);
         break;
      case TR_ValidateStaticMethodFromCP:
         reloRecord = new (storage) TR_RelocationRecordValidateStaticMethodFromCP(reloRuntime, record);
         break;
      case TR_ValidateSpecialMethodFromCP:
         reloRecord = new (storage) TR_RelocationRecordValidateSpecialMethodFromCP(reloRuntime, record);
         break;
      case TR_ValidateVirtualMethodFromCP:
         reloRecord = new (storage) TR_RelocationRecordValidateVirtualMethodFromCP(reloRuntime, record);
         break;
      case TR_ValidateVirtualMethodFromOffset:
         reloRecord = new (storage) TR_RelocationRecordValidateVirtualMethodFromOffset(reloRuntime, record);
         break;
      case TR_ValidateInterfaceMethodFromCP:
         reloRecord = new (storage) TR_RelocationRecordValidateInterfaceMethodFromCP(reloRuntime, record);
         break;
      case TR_ValidateImproperInterfaceMethodFromCP:
         reloRecord = new (storage) TR_RelocationRecordValidateImproperInterfaceMethodFromCP(reloRuntime, record);
         break;
      case TR_ValidateMethodFromClassAndSig:
         reloRecord = new (storage) TR_RelocationRecordValidateMethodFromClassAndSig(reloRuntime, record);
         break;
      case TR_ValidateStackWalkerMaySkipFramesRecord:
         reloRecord = new (storage) TR_RelocationRecordValidateStackWalkerMaySkipFrames(reloRuntime, record);
         break;
      case TR_ValidateClassInfoIsInitialized:
         reloRecord = new (storage) TR_RelocationRecordValidateClassInfoIsInitialized(reloRuntime, record);
         break;
      case TR_ValidateMethodFromSingleImplementer:
         reloRecord = new (storage) TR_RelocationRecordValidateMethodFromSingleImpl(reloRuntime, record);
         break;
      case TR_ValidateMethodFromSingleInterfaceImplementer:
         reloRecord = new (storage) TR_RelocationRecordValidateMethodFromSingleInterfaceImpl(reloRuntime, record);
         break;
      case TR_ValidateMethodFromSingleAbstractImplementer:
         reloRecord = new (storage) TR_RelocationRecordValidateMethodFromSingleAbstractImpl(reloRuntime, record);
         break;
      case TR_SymbolFromManager:
         reloRecord = new (storage) TR_RelocationRecordSymbolFromManager(reloRuntime, record);
         break;
      case TR_MethodCallAddress:
         reloRecord = new (storage) TR_RelocationRecordMethodCallAddress(reloRuntime, record);
         break;
      case TR_DiscontiguousSymbolFromManager:
         reloRecord = new (storage) TR_RelocationRecordDiscontiguousSymbolFromManager(reloRuntime, record);
         break;
      case TR_ResolvedTrampolines:
         reloRecord = new (storage) TR_RelocationRecordResolvedTrampolines(reloRuntime, record);
         break;
      default:
         // TODO: error condition
         printf("Unexpected relo record: %d\n", reloType);fflush(stdout);
         TR_ASSERT(0, "Unexpected relocation record type found");
         exit(0);
      }
      return reloRecord;
   }

void
TR_RelocationRecord::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();

   reloLogger->printf("%s %p\n", name(), _record);
   RELO_LOG(reloLogger, 7, "\tsize %x type %d flags %x reloFlags %x\n", size(reloTarget), type(reloTarget), flags(reloTarget), reloFlags(reloTarget));
   if (wideOffsets(reloTarget))
      RELO_LOG(reloLogger, 7, "\tFlag: Wide offsets\n");
   if (eipRelative(reloTarget))
      RELO_LOG(reloLogger, 7, "\tFlag: EIP relative\n");
   }

void
TR_RelocationRecord::clean(TR_RelocationTarget *reloTarget)
   {
   setSize(reloTarget, 0);
   reloTarget->storeUnsigned8b(0, (uint8_t *) &_record->_type);
   reloTarget->storeUnsigned8b(0, (uint8_t *) &_record->_flags);
   }

int32_t
TR_RelocationRecord::bytesInHeader(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_ExternalRelocationTargetKind kind = type(reloTarget);
   if (kind <= TR_NoRelocation || kind >= TR_NumExternalRelocationKinds)
      {
      RELO_LOG(reloRuntime->reloLogger(), 1, "bytesInHeader: Relocation at %p has unknown kind %d!\n", _record, kind);
      return -1;
      }
   return getSizeOfAOTRelocationHeader(kind);
   }

TR_RelocationRecordBinaryTemplate *
TR_RelocationRecord::nextBinaryRecord(TR_RelocationTarget *reloTarget)
   {
   return (TR_RelocationRecordBinaryTemplate*) (((uint8_t*)this->_record) + size(reloTarget));
   }

void
TR_RelocationRecord::setSize(TR_RelocationTarget *reloTarget, uint16_t size)
   {
   reloTarget->storeUnsigned16b(size,(uint8_t *) &_record->_size);
   }

uint16_t
TR_RelocationRecord::size(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &_record->_size);
   }


void
TR_RelocationRecord::setType(TR_RelocationTarget *reloTarget, TR_RelocationRecordType type)
   {
   reloTarget->storeUnsigned8b(type, (uint8_t *) &_record->_type);
   }

TR_RelocationRecordType
TR_RelocationRecord::type(TR_RelocationTarget *reloTarget)
   {
   return (TR_RelocationRecordType)_record->type(reloTarget);
   }


void
TR_RelocationRecord::setWideOffsets(TR_RelocationTarget *reloTarget)
   {
   setFlag(reloTarget, RELOCATION_TYPE_WIDE_OFFSET);
   }

bool
TR_RelocationRecord::wideOffsets(TR_RelocationTarget *reloTarget)
   {
   return (flags(reloTarget) & RELOCATION_TYPE_WIDE_OFFSET) != 0;
   }

void
TR_RelocationRecord::setEipRelative(TR_RelocationTarget *reloTarget)
   {
   setFlag(reloTarget, RELOCATION_TYPE_EIP_OFFSET);
   }

bool
TR_RelocationRecord::eipRelative(TR_RelocationTarget *reloTarget)
   {
   return (flags(reloTarget) & RELOCATION_TYPE_EIP_OFFSET) != 0;
   }

void
TR_RelocationRecord::setFlag(TR_RelocationTarget *reloTarget, uint8_t flag)
   {
   uint8_t flags = reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) | (flag & RELOCATION_CROSS_PLATFORM_FLAGS_MASK);
   reloTarget->storeUnsigned8b(flags, (uint8_t *) &_record->_flags);
   }

uint8_t
TR_RelocationRecord::flags(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) & RELOCATION_CROSS_PLATFORM_FLAGS_MASK;
   }

void
TR_RelocationRecord::setReloFlags(TR_RelocationTarget *reloTarget, uint8_t reloFlags)
   {
   TR_ASSERT_FATAL((reloFlags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
   uint8_t crossPlatFlags = flags(reloTarget);
   uint8_t flags = crossPlatFlags | (reloFlags & RELOCATION_RELOC_FLAGS_MASK);
   reloTarget->storeUnsigned8b(flags, (uint8_t *) &_record->_flags);
   }

uint8_t
TR_RelocationRecord::reloFlags(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) & RELOCATION_RELOC_FLAGS_MASK;
   }

void
TR_RelocationRecord::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   }

// Generic helper address computation for multiple relocation types
uint8_t *
TR_RelocationRecord::computeHelperAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *baseLocation)
   {
   TR_RelocationRecordHelperAddressPrivateData *reloPrivateData = &(privateData()->helperAddress);
   uint8_t *helperAddress = reloPrivateData->_helper;

   if (reloRuntime->options()->getOption(TR_StressTrampolines) || reloTarget->useTrampoline(helperAddress, baseLocation))
      {
      TR::VMAccessCriticalSection computeHelperAddress(reloRuntime->fej9());
      J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;
      helperAddress = (uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline(reloPrivateData->_helperID, (void *)baseLocation);
      }

   return helperAddress;
   }


TR_RelocationRecordAction
TR_RelocationRecord::action(TR_RelocationRuntime *reloRuntime)
   {
   return TR_RelocationRecordAction::apply;
   }

int32_t
TR_RelocationRecord::applyRelocationAtAllOffsets(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloOrigin)
   {
   int32_t sizeOfHeader = bytesInHeader(reloRuntime, reloTarget);
   if (sizeOfHeader <= 0)
      return compilationAotUnknownReloTypeFailure;

   if (reloTarget->isOrderedPairRelocation(this, reloTarget))
      {
      if (wideOffsets(reloTarget))
         {
         int32_t *offsetsBase = (int32_t *) (((uint8_t*)_record) + sizeOfHeader);
         int32_t *endOfOffsets = (int32_t *) nextBinaryRecord(reloTarget);
         for (int32_t *offsetPtr = offsetsBase;offsetPtr < endOfOffsets; offsetPtr+=2)
            {
            int32_t offsetHigh = *offsetPtr;
            int32_t offsetLow = *(offsetPtr+1);
            uint8_t *reloLocationHigh = reloOrigin + offsetHigh + 2; // Add 2 to skip the first 16 bits of instruction
            uint8_t *reloLocationLow = reloOrigin + offsetLow + 2; // Add 2 to skip the first 16 bits of instruction
            RELO_LOG(reloRuntime->reloLogger(), 6, "\treloLocation: from %p high %p low %p (offsetHigh %x offsetLow %x)\n", offsetPtr, reloLocationHigh, reloLocationLow, offsetHigh, offsetLow);
            int32_t rc = applyRelocation(reloRuntime, reloTarget, reloLocationHigh, reloLocationLow);
            if (rc != 0)
               {
               RELO_LOG(reloRuntime->reloLogger(), 6, "\tapplyRelocationAtAllOffsets: rc = %d\n", rc);
               return rc;
               }
            }
         }
      else
         {
         int16_t *offsetsBase = (int16_t *) (((uint8_t*)_record) + sizeOfHeader);
         int16_t *endOfOffsets = (int16_t *) nextBinaryRecord(reloTarget);
         for (int16_t *offsetPtr = offsetsBase;offsetPtr < endOfOffsets; offsetPtr+=2)
            {
            int16_t offsetHigh = *offsetPtr;
            int16_t offsetLow = *(offsetPtr+1);
            uint8_t *reloLocationHigh = reloOrigin + offsetHigh + 2; // Add 2 to skip the first 16 bits of instruction
            uint8_t *reloLocationLow = reloOrigin + offsetLow + 2; // Add 2 to skip the first 16 bits of instruction
            RELO_LOG(reloRuntime->reloLogger(), 6, "\treloLocation: from %p high %p low %p (offsetHigh %x offsetLow %x)\n", offsetPtr, reloLocationHigh, reloLocationLow, offsetHigh, offsetLow);
            int32_t rc = applyRelocation(reloRuntime, reloTarget, reloLocationHigh, reloLocationLow);
            if (rc != 0)
               {
               RELO_LOG(reloRuntime->reloLogger(), 6, "\tapplyRelocationAtAllOffsets: rc = %d\n", rc);
               return rc;
               }
            }
         }
      }
   else
      {
      if (wideOffsets(reloTarget))
         {
         int32_t *offsetsBase = (int32_t *) (((uint8_t*)_record) + sizeOfHeader);
         int32_t *endOfOffsets = (int32_t *) nextBinaryRecord(reloTarget);
         for (int32_t *offsetPtr = offsetsBase;offsetPtr < endOfOffsets; offsetPtr++)
            {
            int32_t offset = *offsetPtr;
            uint8_t *reloLocation = reloOrigin + offset;
            RELO_LOG(reloRuntime->reloLogger(), 6, "\treloLocation: from %p at %p (offset %x)\n", offsetPtr, reloLocation, offset);
            int32_t rc = applyRelocation(reloRuntime, reloTarget, reloLocation);
            if (rc != 0)
               {
               RELO_LOG(reloRuntime->reloLogger(), 6, "\tapplyRelocationAtAllOffsets: rc = %d\n", rc);
               return rc;
               }
            }
         }
      else
         {
         int16_t *offsetsBase = (int16_t *) (((uint8_t*)_record) + sizeOfHeader);
         int16_t *endOfOffsets = (int16_t *) nextBinaryRecord(reloTarget);
         for (int16_t *offsetPtr = offsetsBase;offsetPtr < endOfOffsets; offsetPtr++)
            {
            int16_t offset = *offsetPtr;
            uint8_t *reloLocation = reloOrigin + offset;
            RELO_LOG(reloRuntime->reloLogger(), 6, "\treloLocation: from %p at %p (offset %x)\n", offsetPtr, reloLocation, offset);
            int32_t rc = applyRelocation(reloRuntime, reloTarget, reloLocation);
            if (rc != 0)
               {
               RELO_LOG(reloRuntime->reloLogger(), 6, "\tapplyRelocationAtAllOffsets: rc = %d\n", rc);
               return rc;
               }
            }
         }
      }
      return 0;
   }

// Handlers for individual relocation record types

// Relocations with address sequences
//

void
TR_RelocationRecordWithOffset::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\toffset %x\n", offset(reloTarget));
   }

void
TR_RelocationRecordWithOffset::setOffset(TR_RelocationTarget *reloTarget, uintptr_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptr_t *) &((TR_RelocationRecordWithOffsetBinaryTemplate *)_record)->_offset);
   }

uintptr_t
TR_RelocationRecordWithOffset::offset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordWithOffsetBinaryTemplate *)_record)->_offset);
   }

void
TR_RelocationRecordWithOffset::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloPrivateData->_addressToPatch = offset(reloTarget) ? reloRuntime->newMethodCodeStart() + offset(reloTarget) : 0x0;
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: addressToPatch: %p \n", reloPrivateData->_addressToPatch);
   }

int32_t
TR_RelocationRecordWithOffset::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloTarget->storeAddressSequence(reloPrivateData->_addressToPatch, reloLocation, reloFlags(reloTarget));

   return 0;
   }

int32_t
TR_RelocationRecordWithOffset::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloTarget->storeAddress(reloPrivateData->_addressToPatch, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// TR_BlockFrequency
//
char *
TR_RelocationRecordBlockFrequency::name()
   {
   return "TR_BlockFrequency";
   }

void
TR_RelocationRecordBlockFrequency::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tfrequencyOffset %x\n", frequencyOffset(reloTarget));
   }

void
TR_RelocationRecordBlockFrequency::setFrequencyOffset(TR_RelocationTarget *reloTarget, uintptr_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptr_t *) &((TR_RelocationRecordBlockFrequencyBinaryTemplate *)_record)->_frequencyOffset);
   }

uintptr_t
TR_RelocationRecordBlockFrequency::frequencyOffset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordBlockFrequencyBinaryTemplate *)_record)->_frequencyOffset);
   }

void
TR_RelocationRecordBlockFrequency::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordBlockFrequencyPrivateData *reloPrivateData = &(privateData()->blockFrequency);
   reloPrivateData->_addressToPatch = NULL;

   TR_PersistentJittedBodyInfo *bodyInfo = reinterpret_cast<TR_PersistentJittedBodyInfo *>(reloRuntime->exceptionTable()->bodyInfo);
   if (bodyInfo)
      {
      TR_PersistentProfileInfo *profileInfo = bodyInfo->getProfileInfo();
      if (profileInfo && profileInfo->getBlockFrequencyInfo())
         {
         uintptr_t frequencyBase = reinterpret_cast<uintptr_t>(profileInfo->getBlockFrequencyInfo()->getFrequencyArrayBase());
         reloPrivateData->_addressToPatch = reinterpret_cast<uint8_t *>(frequencyBase + frequencyOffset(reloTarget));
         }
      }
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: addressToPatch: %p \n", reloPrivateData->_addressToPatch);
   }

int32_t
TR_RelocationRecordBlockFrequency::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordBlockFrequencyPrivateData *reloPrivateData = &(privateData()->blockFrequency);
   if (!reloPrivateData->_addressToPatch)
      {
      return compilationAotBlockFrequencyReloFailure;
      }
   reloTarget->storeAddressSequence(reloPrivateData->_addressToPatch, reloLocation, reloFlags(reloTarget));
   return 0;
   }

int32_t
TR_RelocationRecordBlockFrequency::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_RelocationRecordBlockFrequencyPrivateData *reloPrivateData = &(privateData()->blockFrequency);
   if (!reloPrivateData->_addressToPatch)
      {
      return compilationAotBlockFrequencyReloFailure;
      }
   reloTarget->storeAddress(reloPrivateData->_addressToPatch, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// TR_RecompQueuedFlag
//
char *
TR_RelocationRecordRecompQueuedFlag::name()
   {
   return "TR_RecompQueuedFlag";
   }

void
TR_RelocationRecordRecompQueuedFlag::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordRecompQueuedFlagPrivateData *reloPrivateData = &(privateData()->recompQueuedFlag);
   reloPrivateData->_addressToPatch = NULL;

   TR_PersistentJittedBodyInfo *bodyInfo = reinterpret_cast<TR_PersistentJittedBodyInfo *>(reloRuntime->exceptionTable()->bodyInfo);
   if (bodyInfo)
      {
      TR_PersistentProfileInfo *profileInfo = bodyInfo->getProfileInfo();
      if (profileInfo && profileInfo->getBlockFrequencyInfo())
         {
         reloPrivateData->_addressToPatch = (uint8_t *)profileInfo->getBlockFrequencyInfo()->getIsQueuedForRecompilation();
         }
      }
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: addressToPatch: %p \n", reloPrivateData->_addressToPatch);
   }


int32_t
TR_RelocationRecordRecompQueuedFlag::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordRecompQueuedFlagPrivateData *reloPrivateData = &(privateData()->recompQueuedFlag);
   if (!reloPrivateData->_addressToPatch)
      {
      return compilationAotRecompQueuedFlagReloFailure;
      }
   reloTarget->storeAddressSequence(reloPrivateData->_addressToPatch, reloLocation, reloFlags(reloTarget));
   return 0;
   }

int32_t
TR_RelocationRecordRecompQueuedFlag::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_RelocationRecordRecompQueuedFlagPrivateData *reloPrivateData = &(privateData()->recompQueuedFlag);
   if (!reloPrivateData->_addressToPatch)
      {
      return compilationAotRecompQueuedFlagReloFailure;
      }
   reloTarget->storeAddress(reloPrivateData->_addressToPatch, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// TR_GlobalValue
//
char *
TR_RelocationRecordGlobalValue::name()
   {
   return "TR_GlobalValue";
   }

void
TR_RelocationRecordGlobalValue::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloPrivateData->_addressToPatch = (uint8_t *)reloRuntime->getGlobalValue((TR_GlobalValueItem) offset(reloTarget));
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: global value %p \n", reloPrivateData->_addressToPatch);
   }

// TR_BodyInfoLoad
char *
TR_RelocationRecordBodyInfoLoad::name()
   {
   return "TR_BodyInfoLoad";
   }

void
TR_RelocationRecordBodyInfoLoad::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloPrivateData->_addressToPatch = (uint8_t *)reloRuntime->exceptionTable()->bodyInfo;
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: body info %p \n", reloPrivateData->_addressToPatch);
   }

int32_t
TR_RelocationRecordBodyInfoLoad::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloTarget->storeAddressSequence(reloPrivateData->_addressToPatch, reloLocation, reloFlags(reloTarget));
   return 0;
   }

int32_t
TR_RelocationRecordBodyInfoLoad::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloTarget->storeAddress(reloPrivateData->_addressToPatch, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// TR_ArrayCopyHelper
char *
TR_RelocationRecordArrayCopyHelper::name()
   {
   return "TR_ArrayCopyHelper";
   }

void
TR_RelocationRecordArrayCopyHelper::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordArrayCopyPrivateData *reloPrivateData = &(privateData()->arraycopy);
   J9JITConfig *jitConfig = reloRuntime->jitConfig();
   TR_ASSERT(jitConfig != NULL, "Relocation runtime doesn't have a jitConfig!");
   J9JavaVM *javaVM = jitConfig->javaVM;

   reloPrivateData->_addressToPatch = (uint8_t *)reloTarget->arrayCopyHelperAddress(javaVM);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: arraycopy helper %p\n", reloPrivateData->_addressToPatch);
   }

int32_t
TR_RelocationRecordArrayCopyHelper::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordArrayCopyPrivateData *reloPrivateData = &(privateData()->arraycopy);
   reloTarget->storeAddressSequence(reloPrivateData->_addressToPatch, reloLocation, reloFlags(reloTarget));

   return 0;
   }

int32_t
TR_RelocationRecordArrayCopyHelper::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_RelocationRecordArrayCopyPrivateData *reloPrivateData = &(privateData()->arraycopy);
   reloTarget->storeAddress(reloPrivateData->_addressToPatch, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// TR_ArrayCopyToc
char *
TR_RelocationRecordArrayCopyToc::name()
   {
   return "TR_ArrayCopyToc";
   }

void
TR_RelocationRecordArrayCopyToc::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordArrayCopyPrivateData *reloPrivateData = &(privateData()->arraycopy);
   J9JITConfig *jitConfig = reloRuntime->jitConfig();
   TR_ASSERT(jitConfig != NULL, "Relocation runtime doesn't have a jitConfig!");
   J9JavaVM *javaVM = jitConfig->javaVM;
   uintptr_t *funcdescrptr = (uintptr_t *)javaVM->memoryManagerFunctions->referenceArrayCopy;
   reloPrivateData->_addressToPatch = (uint8_t *)funcdescrptr[1];
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: arraycopy toc %p\n", reloPrivateData->_addressToPatch);
   }

// TR_RamMethodSequence
char *
TR_RelocationRecordRamSequence::name()
   {
   return "TR_RamMethodSequence";
   }

void
TR_RelocationRecordRamSequence::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   reloPrivateData->_addressToPatch = (uint8_t *)reloRuntime->exceptionTable()->ramMethod;
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: j9method %p\n", reloPrivateData->_addressToPatch);
   }

// WithInlinedSiteIndex
void
TR_RelocationRecordWithInlinedSiteIndex::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tinlined site index %p\n", inlinedSiteIndex(reloTarget));
   }

void
TR_RelocationRecordWithInlinedSiteIndex::setInlinedSiteIndex(TR_RelocationTarget *reloTarget, uintptr_t inlinedSiteIndex)
   {
   reloTarget->storeRelocationRecordValue(inlinedSiteIndex, (uintptr_t *) &((TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate *)_record)->_inlinedSiteIndex);
   }

uintptr_t
TR_RelocationRecordWithInlinedSiteIndex::inlinedSiteIndex(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate *)_record)->_inlinedSiteIndex);
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordWithInlinedSiteIndex::getInlinedSiteCallerMethod(TR_RelocationRuntime *reloRuntime)
   {
   uintptr_t siteIndex = inlinedSiteIndex(reloRuntime->reloTarget());
   TR_InlinedCallSite *inlinedCallSite = (TR_InlinedCallSite *)getInlinedCallSiteArrayElement(reloRuntime->exceptionTable(), siteIndex);
   uintptr_t callerIndex = inlinedCallSite->_byteCodeInfo.getCallerIndex();
   return getInlinedSiteMethod(reloRuntime, callerIndex);
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordWithInlinedSiteIndex::getInlinedSiteMethod(TR_RelocationRuntime *reloRuntime)
   {
   return getInlinedSiteMethod(reloRuntime, inlinedSiteIndex(reloRuntime->reloTarget()));
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordWithInlinedSiteIndex::getInlinedSiteMethod(TR_RelocationRuntime *reloRuntime, uintptr_t siteIndex)
   {
   TR_OpaqueMethodBlock *method = (TR_OpaqueMethodBlock *) reloRuntime->method();
   if (siteIndex != (uintptr_t)-1)
      {
      TR_InlinedCallSite *inlinedCallSite = (TR_InlinedCallSite *)getInlinedCallSiteArrayElement(reloRuntime->exceptionTable(), siteIndex);
      method = inlinedCallSite->_methodInfo;
      }
   return method;
   }

TR_RelocationRecordAction
TR_RelocationRecordWithInlinedSiteIndex::action(TR_RelocationRuntime *reloRuntime)
   {
   J9Method *method = (J9Method *)getInlinedSiteMethod(reloRuntime);

   /* Currently there is a coupling between the existence of guards and the
    * relocation of the inlined table in the metadata. This means that if the
    * JIT does not produce a guard for certain inlined sites, the associated
    * entry in the inlined table in the metadata will not be relocated. This
    * has the consequence of causing other relocations that need information
    * via this entry to not have the means to do so. The only way to proceed
    * is to fail the AOT load - if the compiler cannot materialize a value to
    * relocate a location that **MUST** be relocated, the code **CANNOT** be
    * loaded and executed.
    */
   if (method == reinterpret_cast<J9Method *>(-1))
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tAborting Load; method cannot be -1!\n");
      return TR_RelocationRecordAction::failCompilation;
      }

   /* It is safe to return true here because if classes were unloaded, then
    * the compilation will be aborted and the potentially unrelocated sections
    * of code will never be executed.
    */
   if (isUnloadedInlinedMethod(method))
      return TR_RelocationRecordAction::ignore;

   return TR_RelocationRecordAction::apply;
   }



// Constant Pool relocations
char *
TR_RelocationRecordConstantPool::name()
   {
   return "TR_ConstantPool";
   }

void
TR_RelocationRecordConstantPool::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecordWithInlinedSiteIndex::print(reloRuntime);
   reloLogger->printf("\tconstant pool %p\n", constantPool(reloTarget));
   }

void
TR_RelocationRecordConstantPool::setConstantPool(TR_RelocationTarget *reloTarget, uintptr_t constantPool)
   {
   reloTarget->storeRelocationRecordValue(constantPool, (uintptr_t *) &((TR_RelocationRecordConstantPoolBinaryTemplate *)_record)->_constantPool);
   }

uintptr_t
TR_RelocationRecordConstantPool::constantPool(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordConstantPoolBinaryTemplate *)_record)->_constantPool);
   }

uintptr_t
TR_RelocationRecordConstantPool::currentConstantPool(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uintptr_t oldValue)
   {
   uintptr_t oldCPBase = constantPool(reloTarget);
   uintptr_t newCP = oldValue - oldCPBase + (uintptr_t)reloRuntime->ramCP();

   return newCP;
   }

uintptr_t
TR_RelocationRecordConstantPool::findConstantPool(TR_RelocationTarget *reloTarget, uintptr_t oldValue, TR_OpaqueMethodBlock *ramMethod)
   {
   uintptr_t oldCPBase = constantPool(reloTarget);
   uintptr_t methodCP = oldValue - oldCPBase + (uintptr_t)J9_CP_FROM_METHOD((J9Method *)ramMethod);
   return methodCP;
   }


uintptr_t
TR_RelocationRecordConstantPool::computeNewConstantPool(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uintptr_t oldConstantPool)
   {
   uintptr_t newCP;
   UDATA thisInlinedSiteIndex = (UDATA) inlinedSiteIndex(reloTarget);
   if (thisInlinedSiteIndex != (UDATA) -1)
      {
      // Find CP from inlined method
      // Assume that the inlined call site has already been relocated
      // And assumes that the method is resolved already, otherwise, we would not have properly relocated the
      // ramMethod for the inlined callsite and trying to retrieve stuff from the bogus pointer will result in error
      TR_InlinedCallSite *inlinedCallSite = (TR_InlinedCallSite *)getInlinedCallSiteArrayElement(reloRuntime->exceptionTable(), thisInlinedSiteIndex);
      J9Method *ramMethod = (J9Method *) inlinedCallSite->_methodInfo;

      if (!isUnloadedInlinedMethod(ramMethod))
         {
         newCP = findConstantPool(reloTarget, oldConstantPool, (TR_OpaqueMethodBlock *) ramMethod);
         }
      else
         {
         RELO_LOG(reloRuntime->reloLogger(), 1, "\t\tcomputeNewConstantPool: method has been unloaded\n");
         return 0;
         }
      }
   else
      {
      newCP = currentConstantPool(reloRuntime, reloTarget, oldConstantPool);
      }

   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tcomputeNewConstantPool: newCP %p\n", newCP);
   return newCP;
   }

int32_t
TR_RelocationRecordConstantPool::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint8_t *baseLocation = 0;
   if (eipRelative(reloTarget))
      {
      //j9tty_printf(PORTLIB, "\nInternal Error AOT: relocateConstantPool: Relocation type was IP-relative.\n");
      // TODO: better error condition exit(-1);
      return 0;
      }

   uintptr_t oldValue =  (uintptr_t) reloTarget->loadAddress(reloLocation);
   uintptr_t newCP = computeNewConstantPool(reloRuntime, reloTarget, oldValue);
   reloTarget->storeAddress((uint8_t *)newCP, reloLocation);

   return 0;
   }

int32_t
TR_RelocationRecordConstantPool::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   if (eipRelative(reloTarget))
      {
      //j9tty_printf(PORTLIB, "\nInternal Error AOT: relocateConstantPool: Relocation type was IP-relative.\n");
      // TODO: better error condition exit(-1);
      return 0;
      }

   uintptr_t oldValue = (uintptr_t) reloTarget->loadAddress(reloLocationHigh, reloLocationLow);
   uintptr_t newCP = computeNewConstantPool(reloRuntime, reloTarget, oldValue);
   reloTarget->storeAddress((uint8_t *)newCP, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));

   return 0;
   }

// ConstantPoolWithIndex relocation base class
void
TR_RelocationRecordConstantPoolWithIndex::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecordConstantPool::print(reloRuntime);
   reloLogger->printf("\tcpIndex %p\n", cpIndex(reloTarget));
   }

void
TR_RelocationRecordConstantPoolWithIndex::setCpIndex(TR_RelocationTarget *reloTarget, uintptr_t cpIndex)
   {
   reloTarget->storeRelocationRecordValue(cpIndex, (uintptr_t *) &((TR_RelocationRecordConstantPoolWithIndexBinaryTemplate *)_record)->_index);
   }

uintptr_t
TR_RelocationRecordConstantPoolWithIndex::cpIndex(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordConstantPoolWithIndexBinaryTemplate *)_record)->_index);
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordConstantPoolWithIndex::getSpecialMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   TR::VMAccessCriticalSection getSpecialMethodFromCP(reloRuntime->fej9());
   J9ConstantPool *cp = (J9ConstantPool *) void_cp;
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();

   J9VMThread *vmThread = reloRuntime->currentThread();
   TR_OpaqueMethodBlock *method = (TR_OpaqueMethodBlock *) jitResolveSpecialMethodRef(vmThread, cp, cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
   RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: found special method %p\n", method);
   return method;
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordConstantPoolWithIndex::getVirtualMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   J9JavaVM *javaVM = reloRuntime->javaVM();
   J9ConstantPool *cp = (J9ConstantPool *) void_cp;
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();

   J9Method *method = NULL;

      {
      TR::VMAccessCriticalSection getVirtualMethodFromCP(reloRuntime->fej9());
      UDATA vTableOffset = javaVM->internalVMFunctions->resolveVirtualMethodRefInto(javaVM->internalVMFunctions->currentVMThread(javaVM),
                                                                                   cp,
                                                                                   cpIndex,
                                                                                   J9_RESOLVE_FLAG_JIT_COMPILE_TIME,
                                                                                   &method,
                                                                                   NULL);
      }

   if (method)
       {
       if ((UDATA)method->constantPool & J9_STARTPC_METHOD_IS_OVERRIDDEN)
          {
          RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: inlined method overridden, fail validation\n");
          method = NULL;
          }
       else
          {
          RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: found virtual method %p\n", method);
          }
       }

   return (TR_OpaqueMethodBlock *) method;
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordConstantPoolWithIndex::getStaticMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   TR::VMAccessCriticalSection getStaticMethodFromCP(reloRuntime->fej9());
   J9JavaVM *javaVM = reloRuntime->javaVM();
   J9ConstantPool *cp = (J9ConstantPool *) void_cp;
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();

   TR_OpaqueMethodBlock *method = (TR_OpaqueMethodBlock *) jitResolveStaticMethodRef(javaVM->internalVMFunctions->currentVMThread(javaVM),
                                                                                     cp,
                                                                                     cpIndex,
                                                                                     J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
   RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: found static method %p\n", method);
   return method;
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordConstantPoolWithIndex::getInterfaceMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);

   J9JavaVM *javaVM = reloRuntime->javaVM();
   TR_J9VMBase *fe = reloRuntime->fej9();
   TR_Memory *trMemory = reloRuntime->trMemory();

   J9ConstantPool *cp = (J9ConstantPool *) void_cp;
   J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)&cp->romConstantPool[cpIndex];

   TR_OpaqueClassBlock *interfaceClass;

      {
      TR::VMAccessCriticalSection getInterfaceMethodFromCP(reloRuntime->fej9());
      interfaceClass = (TR_OpaqueClassBlock *) javaVM->internalVMFunctions->resolveClassRef(reloRuntime->currentThread(),
                                                                                            cp,
                                                                                            romMethodRef->classRefCPIndex,
                                                                                            J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
      }

   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: interface class %p\n", interfaceClass);

   TR_OpaqueMethodBlock *calleeMethod = NULL;
   if (interfaceClass)
      {
      TR_PersistentCHTable * chTable = reloRuntime->getPersistentInfo()->getPersistentCHTable();
      TR_ResolvedMethod *callerResolvedMethod = fe->createResolvedMethod(trMemory, callerMethod, NULL);

      TR_ResolvedMethod *calleeResolvedMethod = chTable->findSingleInterfaceImplementer(interfaceClass, cpIndex, callerResolvedMethod, reloRuntime->comp(), false, false);

      if (calleeResolvedMethod)
         {
         if (!calleeResolvedMethod->virtualMethodIsOverridden())
            calleeMethod = calleeResolvedMethod->getPersistentIdentifier();
         else
            RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: callee method overridden\n");
         }
      }


   reloPrivateData->_receiverClass = interfaceClass;
   return calleeMethod;
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordConstantPoolWithIndex::getAbstractMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);

   J9JavaVM *javaVM = reloRuntime->javaVM();
   TR_J9VMBase *fe = reloRuntime->fej9();
   TR_Memory *trMemory = reloRuntime->trMemory();

   J9ConstantPool *cp = (J9ConstantPool *) void_cp;
   J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)&cp->romConstantPool[cpIndex];

   TR_OpaqueMethodBlock *calleeMethod = NULL;
   TR_OpaqueClassBlock *abstractClass = NULL;
   UDATA vTableOffset = (UDATA)-1;
   J9Method *method = NULL;

      {
      TR::VMAccessCriticalSection getAbstractMethodFromCP(reloRuntime->fej9());
      abstractClass = (TR_OpaqueClassBlock *) javaVM->internalVMFunctions->resolveClassRef(reloRuntime->currentThread(),
                                                                                            cp,
                                                                                            romMethodRef->classRefCPIndex,
                                                                                            J9_RESOLVE_FLAG_JIT_COMPILE_TIME);

      vTableOffset = javaVM->internalVMFunctions->resolveVirtualMethodRefInto(reloRuntime->currentThread(),
                                                                              cp,
                                                                              cpIndex,
                                                                              J9_RESOLVE_FLAG_JIT_COMPILE_TIME,
                                                                              &method,
                                                                              NULL);
      }

   if (abstractClass && method)
      {
      int32_t vftSlot = (int32_t)(-(vTableOffset - TR::Compiler->vm.getInterpreterVTableOffset()));
      TR_PersistentCHTable * chTable = reloRuntime->getPersistentInfo()->getPersistentCHTable();
      TR_ResolvedMethod *callerResolvedMethod = fe->createResolvedMethod(trMemory, callerMethod, NULL);

      TR_ResolvedMethod *calleeResolvedMethod = chTable->findSingleAbstractImplementer(abstractClass, vftSlot, callerResolvedMethod, reloRuntime->comp(), false, false);

      if (calleeResolvedMethod)
         {
         if (!calleeResolvedMethod->virtualMethodIsOverridden())
            calleeMethod = calleeResolvedMethod->getPersistentIdentifier();
         else
            RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: callee method overridden\n");
         }
      }

   reloPrivateData->_receiverClass = abstractClass;
   return calleeMethod;
   }

// TR_HelperAddress
char *
TR_RelocationRecordHelperAddress::name()
   {
   return "TR_HelperAddress";
   }

void
TR_RelocationRecordHelperAddress::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   uintptr_t helper = helperID(reloTarget);
   if (reloRuntime->comp())
      reloLogger->printf("\thelper %d %s\n", helper, reloRuntime->comp()->findOrCreateDebug()->getRuntimeHelperName(helper));
   else
      reloLogger->printf("\thelper %d\n", helper);
   }

void
TR_RelocationRecordHelperAddress::setHelperID(TR_RelocationTarget *reloTarget, uint32_t helperID)
   {
   reloTarget->storeUnsigned32b(helperID, (uint8_t *) &(((TR_RelocationRecordHelperAddressBinaryTemplate *)_record)->_helperID));
   }

uint32_t
TR_RelocationRecordHelperAddress::helperID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned32b((uint8_t *) &(((TR_RelocationRecordHelperAddressBinaryTemplate *)_record)->_helperID));
   }

void
TR_RelocationRecordHelperAddress::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordHelperAddressPrivateData *reloPrivateData = &(privateData()->helperAddress);

   J9JITConfig *jitConfig = reloRuntime->jitConfig();
   TR_ASSERT(jitConfig != NULL, "Relocation runtime doesn't have a jitConfig!");
   J9JavaVM *javaVM = jitConfig->javaVM;
   reloPrivateData->_helperID = helperID(reloTarget);
   reloPrivateData->_helper = (uint8_t *) (jitConfig->aotrt_getRuntimeHelper)(reloPrivateData->_helperID);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: helperAddress %p\n", reloPrivateData->_helper);
   }

int32_t
TR_RelocationRecordHelperAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint8_t *baseLocation = 0;
   if (eipRelative(reloTarget))
      baseLocation = reloTarget->eipBaseForCallOffset(reloLocation);

   uint8_t *helperAddress = (uint8_t *)computeHelperAddress(reloRuntime, reloTarget, baseLocation);
   uint8_t *helperOffset = helperAddress - (uintptr_t)baseLocation;
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: baseLocation %p helperAddress %p helperOffset %x\n", baseLocation, helperAddress, helperOffset);

   if (eipRelative(reloTarget))
      reloTarget->storeRelativeTarget((uintptr_t )helperOffset, reloLocation);
   else
      reloTarget->storeAddress(helperOffset, reloLocation);

   return 0;
   }

int32_t
TR_RelocationRecordHelperAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_ASSERT(0, "TR_RelocationRecordHelperAddress::applyRelocation for ordered pair, we should never call this");
   uint8_t *baseLocation = 0;

   uint8_t *helperOffset = (uint8_t *) ((uintptr_t)computeHelperAddress(reloRuntime, reloTarget, baseLocation) - (uintptr_t)baseLocation);

   reloTarget->storeAddress(helperOffset, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));

   return 0;
   }

char *
TR_RelocationRecordAbsoluteHelperAddress::name()
   {
   return "TR_AbsoluteHelperAddress";
   }

int32_t
TR_RelocationRecordAbsoluteHelperAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordHelperAddressPrivateData *reloPrivateData = &(privateData()->helperAddress);
   uint8_t *helperAddress = reloPrivateData->_helper;
   if (reloFlags(reloTarget) != 0)
      reloTarget->storeAddressSequence(helperAddress, reloLocation, reloFlags(reloTarget));
   else
      reloTarget->storeAddress(helperAddress, reloLocation);
   return 0;
   }

int32_t
TR_RelocationRecordAbsoluteHelperAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_RelocationRecordHelperAddressPrivateData *reloPrivateData = &(privateData()->helperAddress);
   uint8_t *helperAddress = reloPrivateData->_helper;
   reloTarget->storeAddress(helperAddress, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// Method Address Relocations
//
char *
TR_RelocationRecordMethodAddress::name()
   {
   return "TR_MethodAddress";
   }

uint8_t *
TR_RelocationRecordMethodAddress::currentMethodAddress(TR_RelocationRuntime *reloRuntime, uint8_t *oldMethodAddress)
   {
   TR_AOTMethodHeader *methodHdr = reloRuntime->aotMethodHeaderEntry();
   return oldMethodAddress - methodHdr->compileMethodCodeStartPC + (uintptr_t) reloRuntime->newMethodCodeStart();
   }

int32_t
TR_RelocationRecordMethodAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   bool eipRel = eipRelative(reloTarget);

   uint8_t *oldAddress;
   if (eipRel)
      oldAddress = reloTarget->loadCallTarget(reloLocation);
   else
      oldAddress = reloTarget->loadAddress(reloLocation);

   RELO_LOG(reloRuntime->reloLogger(), 5, "\t\tapplyRelocation: old method address %p\n", oldAddress);
   uint8_t *newAddress = currentMethodAddress(reloRuntime, oldAddress);
   RELO_LOG(reloRuntime->reloLogger(), 5, "\t\tapplyRelocation: new method address %p\n", newAddress);

   if (eipRel)
      reloTarget->storeCallTarget((uintptr_t)newAddress, reloLocation);
   else
      reloTarget->storeAddress(newAddress, reloLocation);

   return 0;
   }

int32_t
TR_RelocationRecordMethodAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_RelocationRecordWithOffsetPrivateData *reloPrivateData = &(privateData()->offset);
   uint8_t *oldAddress = reloTarget->loadAddress(reloLocationHigh, reloLocationLow);
   uint8_t *newAddress = currentMethodAddress(reloRuntime, oldAddress);

   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: oldAddress %p newAddress %p\n", oldAddress, newAddress);
   reloTarget->storeAddress(newAddress, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// Direct JNI Address Relocations

char *
TR_RelocationRecordDirectJNICall::name()
   {
   return "TR_JNITargetAddress";
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordDirectJNICall::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   TR_OpaqueMethodBlock *method = NULL;

   return method;
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordDirectJNISpecialMethodCall::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   return getSpecialMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordDirectJNIStaticMethodCall::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   return getStaticMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordDirectJNIVirtualMethodCall::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   return getVirtualMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

int32_t TR_RelocationRecordRamMethodConst::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   J9ConstantPool * newConstantPool =(J9ConstantPool *) computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   TR_OpaqueMethodBlock *ramMethod = getMethodFromCP(reloRuntime, newConstantPool, cpIndex(reloTarget));

   if (!ramMethod)
      {
      return compilationAotClassReloFailure;
      }

   reloTarget->storeAddressRAM((uint8_t *)ramMethod, reloLocation);
   return 0;
   }

int32_t
TR_RelocationRecordDirectJNICall::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uintptr_t oldAddress = (uintptr_t) reloTarget->loadAddress(reloLocation);

   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   J9ConstantPool * newConstantPool =(J9ConstantPool *) computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   TR_OpaqueMethodBlock *ramMethod = getMethodFromCP(reloRuntime, newConstantPool, cpIndex(reloTarget));

   if (!ramMethod) return compilationAotClassReloFailure;


   TR_ResolvedMethod *callerResolvedMethod = reloRuntime->fej9()->createResolvedMethod(reloRuntime->comp()->trMemory(), ramMethod, NULL);
   void * newAddress = NULL;
   if (callerResolvedMethod->isJNINative())
      newAddress = callerResolvedMethod->startAddressForJNIMethod(reloRuntime->comp());


   if (!newAddress) return compilationAotClassReloFailure;

   RELO_LOG(reloLogger, 6, "\tJNI call relocation: found JNI target address %p\n", newAddress);

   createJNICallSite((void *)ramMethod, (void *)reloLocation,getMetadataAssumptionList(reloRuntime->exceptionTable()));
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: registered JNI Call redefinition site\n");

   reloTarget->storeRelativeAddressSequence((uint8_t *)newAddress, reloLocation, fixedSequence1);
   return 0;

   }


// Data Address Relocations
char *
TR_RelocationRecordDataAddress::name()
   {
   return "TR_DataAddress";
   }

void
TR_RelocationRecordDataAddress::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecordConstantPoolWithIndex::print(reloRuntime);
   reloLogger->printf("\toffset %p\n", offset(reloTarget));
   }

void
TR_RelocationRecordDataAddress::setOffset(TR_RelocationTarget *reloTarget, uintptr_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptr_t *) &((TR_RelocationRecordDataAddressBinaryTemplate *)_record)->_offset);
   }

uintptr_t
TR_RelocationRecordDataAddress::offset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordDataAddressBinaryTemplate *)_record)->_offset);
   }

uint8_t *
TR_RelocationRecordDataAddress::findDataAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   J9JITConfig *jitConfig = reloRuntime->jitConfig();
   J9JavaVM *javaVM = jitConfig->javaVM;
   J9ROMFieldShape * fieldShape = 0;
   UDATA cpindex = cpIndex(reloTarget);
   J9ConstantPool *cp =  (J9ConstantPool *)computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));

   UDATA extraOffset = offset(reloTarget);
   uint8_t *address = NULL;

   if (cp)
      {
      TR::VMAccessCriticalSection findDataAddress(reloRuntime->fej9());
      J9VMThread *vmThread = reloRuntime->currentThread();
      J9Method *ramMethod;
      UDATA thisInlinedSiteIndex = (UDATA) inlinedSiteIndex(reloTarget);
      if (thisInlinedSiteIndex != (UDATA) -1) // Inlined method
         {
         TR_InlinedCallSite *inlinedCallSite = (TR_InlinedCallSite *)getInlinedCallSiteArrayElement(reloRuntime->exceptionTable(), thisInlinedSiteIndex);
         ramMethod = (J9Method *) inlinedCallSite->_methodInfo;
         }
      else
         {
         ramMethod = reloRuntime->method();
         }
      if (ramMethod && (ramMethod != reinterpret_cast<J9Method *>(-1)))
         address = (uint8_t *)jitCTResolveStaticFieldRefWithMethod(vmThread, ramMethod, cpindex, false, &fieldShape);
      }

   if (address == NULL)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tfindDataAddress: unresolved\n");
      return 0;
      }

   address = address + extraOffset;
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tfindDataAddress: field address %p\n", address);
   return address;
   }

int32_t
TR_RelocationRecordDataAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint8_t *newAddress = findDataAddress(reloRuntime, reloTarget);

#if defined(J9VM_OPT_JITSERVER)
   RELO_LOG(reloRuntime->reloLogger(), 6, "applyRelocation old ptr %p, new ptr %p\n", reloTarget->loadPointer(reloLocation), newAddress);
#endif

   if (!newAddress)
      return compilationAotStaticFieldReloFailure;

   TR_AOTStats *aotStats = reloRuntime->aotStats();
   if (aotStats)
      {
      aotStats->numRuntimeClassAddressReloUnresolvedCP++;
      }

   reloTarget->storeAddressSequence(newAddress, reloLocation, reloFlags(reloTarget));
   return 0;
   }

int32_t
TR_RelocationRecordDataAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   uint8_t *newAddress = findDataAddress(reloRuntime, reloTarget);

   if (!newAddress)
      return compilationAotStaticFieldReloFailure;
   reloTarget->storeAddress(newAddress, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// Class Object Relocations
char *
TR_RelocationRecordClassAddress::name()
   {
   return "TR_ClassAddress";
   }

TR_OpaqueClassBlock *
TR_RelocationRecordClassAddress::computeNewClassAddress(TR_RelocationRuntime *reloRuntime, uintptr_t newConstantPool, uintptr_t inlinedSiteIndex, uintptr_t cpIndex)
   {
   J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;

   TR_AOTStats *aotStats = reloRuntime->aotStats();

   if (!newConstantPool)
      {
      if (aotStats)
         {
         aotStats->numRuntimeClassAddressReloUnresolvedCP++;
         }
      return 0;
      }
   J9VMThread *vmThread = reloRuntime->currentThread();

   J9Class *resolvedClass;

      {
      TR::VMAccessCriticalSection computeNewClassObject(reloRuntime->fej9());
      resolvedClass = javaVM->internalVMFunctions->resolveClassRef(vmThread, (J9ConstantPool *)newConstantPool, cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
      }

   RELO_LOG(reloRuntime->reloLogger(), 6,"\tcomputeNewClassObject: resolvedClass %p\n", resolvedClass);

   if (resolvedClass)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tcomputeNewClassObject: resolvedClassName %.*s\n",
                                              J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(resolvedClass->romClass)),
                                              J9UTF8_DATA(J9ROMCLASS_CLASSNAME(resolvedClass->romClass)));
      }
   else if (aotStats)
      {
      aotStats->numRuntimeClassAddressReloUnresolvedClass++;
      }

   return (TR_OpaqueClassBlock *)resolvedClass;
   }

int32_t
TR_RelocationRecordClassAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uintptr_t oldAddress = (uintptr_t) reloTarget->loadAddress(reloLocation);

   uintptr_t newConstantPool = computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   TR_OpaqueClassBlock *newAddress = computeNewClassAddress(reloRuntime, newConstantPool, inlinedSiteIndex(reloTarget), cpIndex(reloTarget));

   if (!newAddress) return compilationAotClassReloFailure;

   if (TR::CodeGenerator::wantToPatchClassPointer(reloRuntime->comp(), newAddress, reloLocation))
      {
      createClassRedefinitionPicSite((void *)newAddress, (void *)reloLocation, sizeof(UDATA), 0,
                                     getMetadataAssumptionList(reloRuntime->exceptionTable()));
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: hcr enabled, registered class redefinition site\n");
      }

   reloTarget->storeAddressSequence((uint8_t *)newAddress, reloLocation, reloFlags(reloTarget));
   return 0;
   }

int32_t
TR_RelocationRecordClassAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   uintptr_t oldValue = (uintptr_t) reloTarget->loadAddress(reloLocationHigh, reloLocationLow);
   uintptr_t newConstantPool = computeNewConstantPool(reloRuntime, reloTarget, oldValue);
   TR_OpaqueClassBlock *newAddress = computeNewClassAddress(reloRuntime, newConstantPool, inlinedSiteIndex(reloTarget), cpIndex(reloTarget));

   if (!newAddress) return compilationAotClassReloFailure;

   if (TR::CodeGenerator::wantToPatchClassPointer(reloRuntime->comp(), newAddress, reloLocationHigh))
      {
      // This looks wrong
      createClassRedefinitionPicSite((void *)newAddress, (void *)reloLocationHigh, sizeof(UDATA), 0,
         getMetadataAssumptionList(reloRuntime->exceptionTable()));
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: hcr enabled, registered class redefinition site\n");
      }

   reloTarget->storeAddress((uint8_t *) newAddress, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// MethodObject Relocations
char *
TR_RelocationRecordMethodObject::name()
   {
   return "TR_MethodObject";
   }

int32_t
TR_RelocationRecordMethodObject::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uintptr_t oldAddress = (uintptr_t) reloTarget->loadAddress(reloLocation);
   uintptr_t newAddress = currentConstantPool(reloRuntime, reloTarget, oldAddress);
   reloTarget->storeAddress((uint8_t *) newAddress, reloLocation);
   return 0;
   }

int32_t
TR_RelocationRecordMethodObject::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   uintptr_t oldAddress = (uintptr_t) reloTarget->loadAddress(reloLocationHigh, reloLocationLow);
   uintptr_t newAddress = currentConstantPool(reloRuntime, reloTarget, oldAddress);
   reloTarget->storeAddress((uint8_t *) newAddress, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// TR_BodyInfoAddress Relocation
char *
TR_RelocationRecordBodyInfo::name()
   {
   return "TR_BodyInfo";
   }

int32_t
TR_RelocationRecordBodyInfo::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   J9JITExceptionTable *exceptionTable = reloRuntime->exceptionTable();
   reloTarget->storeAddress((uint8_t *) exceptionTable->bodyInfo, reloLocation);
#if defined(J9VM_OPT_JITSERVER)
   fixPersistentMethodInfo((void *)exceptionTable, !reloRuntime->fej9()->_compInfoPT->getMethodBeingCompiled()->isAotLoad());
#else
   fixPersistentMethodInfo((void *)exceptionTable, false);
#endif /* defined(J9VM_OPT_JITSERVER) */
   return 0;
   }

// TR_Thunks Relocation
char *
TR_RelocationRecordThunks::name()
   {
   return "TR_Thunks";
   }

int32_t
TR_RelocationRecordThunks::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint8_t *oldAddress = reloTarget->loadAddress(reloLocation);

   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: oldAddress %p\n", oldAddress);

   uintptr_t newConstantPool = computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   reloTarget->storeAddress((uint8_t *)newConstantPool, reloLocation);
   uintptr_t cpIndex = reloTarget->loadThunkCPIndex(reloLocation);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: loadThunkCPIndex is %d\n", cpIndex);
   return relocateAndRegisterThunk(reloRuntime, reloTarget, newConstantPool, cpIndex, reloLocation);
   }

int32_t
TR_RelocationRecordThunks::relocateAndRegisterThunk(
   TR_RelocationRuntime *reloRuntime,
   TR_RelocationTarget *reloTarget,
   uintptr_t cp,
   uintptr_t cpIndex,
   uint8_t *reloLocation)
   {
   J9JITConfig *jitConfig = reloRuntime->jitConfig();
   J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;
   J9ConstantPool *constantPool = (J9ConstantPool *)cp;

   J9ROMClass * romClass = J9_CLASS_FROM_CP(constantPool)->romClass;
   J9ROMMethodRef *romMethodRef = &J9ROM_CP_BASE(romClass, J9ROMMethodRef)[cpIndex];
   J9ROMNameAndSignature * nameAndSignature = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);

   bool matchFound = false;

   int32_t signatureLength = J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature));
   char *signatureString = (char *) J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature));

   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\trelocateAndRegisterThunk: %.*s%.*s\n", J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature)), J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature)),  signatureLength, signatureString);

   // Everything below is run with VM Access in hand
   TR::VMAccessCriticalSection relocateAndRegisterThunkCriticalSection(reloRuntime->fej9());

   void *existingThunk = j9ThunkLookupNameAndSig(jitConfig, nameAndSignature);
   if (existingThunk != NULL)
      {
      /* Matching thunk found */
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\t\trelocateAndRegisterThunk:found matching thunk %p\n", existingThunk);
      relocateJ2IVirtualThunkPointer(reloTarget, reloLocation, existingThunk);
      return 0; // return successful
      }

   // search shared cache for thunk, copy it over and create thunk entry
   J9SharedDataDescriptor firstDescriptor;
   firstDescriptor.address = NULL;

   javaVM->sharedClassConfig->findSharedData(reloRuntime->currentThread(),
                                             signatureString,
                                             signatureLength,
                                             J9SHR_DATA_TYPE_AOTTHUNK,
                                             false,
                                             &firstDescriptor,
                                             NULL);

   // if found thunk, then need to copy thunk into code cache, create thunk mapping, and register thunk mapping
   //
   if (firstDescriptor.address)
      {
      //Copy thunk from shared cache into local memory and relocate target address
      //
      uint8_t *coldCode;
      TR::CodeCache *codeCache = reloRuntime->codeCache();

      // Changed the code so that we fail this relocation/compilation if we cannot
      // allocate in the current code cache. The reason is that, when a new code cache is needed
      // the reservation of the old cache is cancelled and further allocation attempts from
      // the old cache (which is not switched) will fail
      U_8 *thunkStart = TR::CodeCacheManager::instance()->allocateCodeMemory(firstDescriptor.length, 0, &codeCache, &coldCode, true);
      U_8 *thunkAddress;
      if (thunkStart)
         {
         // Relocate the thunk
         //
         RELO_LOG(reloRuntime->reloLogger(), 7, "\t\t\trelocateAndRegisterThunk: thunkStart from cache %p\n", thunkStart);
         memcpy(thunkStart, firstDescriptor.address, firstDescriptor.length);

         thunkAddress = thunkStart + 2*sizeof(I_32);

         RELO_LOG(reloRuntime->reloLogger(), 7, "\t\t\trelocateAndRegisterThunk: thunkAddress %p\n", thunkAddress);
         void *vmHelper = j9ThunkVMHelperFromSignature(jitConfig, signatureLength, signatureString);
         RELO_LOG(reloRuntime->reloLogger(), 7, "\t\t\trelocateAndRegisterThunk: vmHelper %p\n", vmHelper);
         reloTarget->performThunkRelocation(thunkAddress, (UDATA)vmHelper);

         j9ThunkNewNameAndSig(jitConfig, nameAndSignature, thunkAddress);

         if (J9_EVENT_IS_HOOKED(javaVM->hookInterface, J9HOOK_VM_DYNAMIC_CODE_LOAD))
            ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(javaVM->hookInterface, javaVM->internalVMFunctions->currentVMThread(javaVM), NULL, (void *) thunkAddress, *((uint32_t *)thunkAddress - 2), "JIT virtual thunk", NULL);

         relocateJ2IVirtualThunkPointer(reloTarget, reloLocation, thunkAddress);
         }
      else
         {
         codeCache->unreserve(); // cancel the reservation
         // return error
         return compilationAotCacheFullReloFailure;
         }

      }
    else
      {
      // return error
      return compilationAotThunkReloFailure;
      }

   return 0;
   }

// TR_J2IVirtualThunkPointer Relocation
char *
TR_RelocationRecordJ2IVirtualThunkPointer::name()
   {
   return "TR_J2IVirtualThunkPointer";
   }

void
TR_RelocationRecordJ2IVirtualThunkPointer::setOffsetToJ2IVirtualThunkPointer(TR_RelocationTarget *reloTarget, uintptr_t j2iVirtualThunkPointer)
   {
   reloTarget->storeRelocationRecordValue(j2iVirtualThunkPointer, (uintptr_t *) &((TR_RelocationRecordJ2IVirtualThunkPointerBinaryTemplate *)_record)->_offsetToJ2IVirtualThunkPointer);
   }

uintptr_t
TR_RelocationRecordJ2IVirtualThunkPointer::getOffsetToJ2IVirtualThunkPointer(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordJ2IVirtualThunkPointerBinaryTemplate *)_record)->_offsetToJ2IVirtualThunkPointer);
   }

void
TR_RelocationRecordJ2IVirtualThunkPointer::relocateJ2IVirtualThunkPointer(
   TR_RelocationTarget *reloTarget,
   uint8_t *reloLocation,
   void *thunk)
   {
   TR_ASSERT_FATAL(thunk != NULL, "expected a j2i virtual thunk for relocation\n");

   // For uniformity with TR_Thunks, the reloLocation is not the location of the
   // J2I thunk pointer, but rather the location of the constant pool address.
   // Find the J2I thunk pointer relative to that.
   reloLocation += offsetToJ2IVirtualThunkPointer(reloTarget);
   reloTarget->storeAddress((uint8_t *)thunk, reloLocation);
   }

uintptr_t
TR_RelocationRecordJ2IVirtualThunkPointer::offsetToJ2IVirtualThunkPointer(
   TR_RelocationTarget *reloTarget)
   {
   auto recordData = (TR_RelocationRecordJ2IVirtualThunkPointerBinaryTemplate *)_record;
   auto offsetEA = (uintptr_t *) &recordData->_offsetToJ2IVirtualThunkPointer;
   return reloTarget->loadRelocationRecordValue(offsetEA);
   }

// TR_PicTrampolines Relocation
char *
TR_RelocationRecordPicTrampolines::name()
   {
   return "TR_PicTrampolines";
   }

void
TR_RelocationRecordPicTrampolines::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tnumTrampolines %d\n", numTrampolines(reloTarget));
   }

void
TR_RelocationRecordPicTrampolines::setNumTrampolines(TR_RelocationTarget *reloTarget, uint32_t numTrampolines)
   {
   reloTarget->storeUnsigned32b(numTrampolines, (uint8_t *) &(((TR_RelocationRecordPicTrampolineBinaryTemplate *)_record)->_numTrampolines));
   }

uint32_t
TR_RelocationRecordPicTrampolines::numTrampolines(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned32b((uint8_t *) &(((TR_RelocationRecordPicTrampolineBinaryTemplate *)_record)->_numTrampolines));
   }

int32_t
TR_RelocationRecordPicTrampolines::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   if (reloRuntime->codeCache()->reserveSpaceForTrampoline_bridge(numTrampolines(reloTarget)) != OMR::CodeCacheErrorCode::ERRORCODE_SUCCESS)
      {
      RELO_LOG(reloRuntime->reloLogger(), 1,"\t\tapplyRelocation: aborting AOT relocation because pic trampoline was not reserved. Will be retried.\n");
      return compilationAotPicTrampolineReloFailure;
      }

   return 0;
   }

// TR_Trampolines Relocation

char *
TR_RelocationRecordTrampolines::name()
   {
   return "TR_Trampolines";
   }

int32_t
TR_RelocationRecordTrampolines::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint8_t *oldAddress = reloTarget->loadAddress(reloLocation);

   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: oldAddress %p\n", oldAddress);

   uintptr_t newConstantPool = computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   reloTarget->storeAddress((uint8_t *)newConstantPool, reloLocation); // Store the new CP address (in snippet)
   uint32_t cpIndex = (uint32_t) reloTarget->loadCPIndex(reloLocation);
   if (reloRuntime->codeCache()->reserveUnresolvedTrampoline((void *)newConstantPool, cpIndex) != OMR::CodeCacheErrorCode::ERRORCODE_SUCCESS)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: aborting AOT relocation because trampoline was not reserved. Will be retried.\n");
      return compilationAotTrampolineReloFailure;
      }

   return 0;
   }

// TR_InlinedAllocation relocation
void
TR_RelocationRecordInlinedAllocation::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecordConstantPoolWithIndex::print(reloRuntime);
   reloLogger->printf("\tbranchOffset %p\n", branchOffset(reloTarget));
   }

void
TR_RelocationRecordInlinedAllocation::setBranchOffset(TR_RelocationTarget *reloTarget, uintptr_t branchOffset)
   {
   reloTarget->storeRelocationRecordValue(branchOffset, (uintptr_t *)&((TR_RelocationRecordInlinedAllocationBinaryTemplate *)_record)->_branchOffset);
   }

uintptr_t
TR_RelocationRecordInlinedAllocation::branchOffset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *)&((TR_RelocationRecordInlinedAllocationBinaryTemplate *)_record)->_branchOffset);
   }

void
TR_RelocationRecordInlinedAllocation::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordInlinedAllocationPrivateData *reloPrivateData = &(privateData()->inlinedAllocation);

   uintptr_t oldValue = constantPool(reloTarget);
   J9ConstantPool *newConstantPool = (J9ConstantPool *) computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));

   J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;
   TR_J9VMBase *fe = reloRuntime->fej9();
   J9Class *clazz;

   if (reloRuntime->comp()->getOption(TR_UseSymbolValidationManager))
      {
      uint16_t classID = (uint16_t)cpIndex(reloTarget);
      clazz = reloRuntime->comp()->getSymbolValidationManager()->getJ9ClassFromID(classID);
      }
   else
      {
      TR::VMAccessCriticalSection preparePrivateData(fe);
      clazz = javaVM->internalVMFunctions->resolveClassRef(javaVM->internalVMFunctions->currentVMThread(javaVM),
                                                                    newConstantPool,
                                                                    cpIndex(reloTarget),
                                                                    J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
      }

   bool inlinedCodeIsOkay = false;
   if (clazz)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: clazz %p %.*s\n",
                                                 clazz,
                                                 J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(clazz->romClass)),
                                                 J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->romClass)));

      if (verifyClass(reloRuntime, reloTarget, (TR_OpaqueClassBlock *)clazz))
         inlinedCodeIsOkay = true;
      }
   else
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: clazz NULL\n");

   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: inlinedCodeIsOkay %d\n", inlinedCodeIsOkay);

   reloPrivateData->_inlinedCodeIsOkay = inlinedCodeIsOkay;
   }

bool
TR_RelocationRecordInlinedAllocation::verifyClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *clazz)
   {
   TR_ASSERT(0, "TR_RelocationRecordInlinedAllocation::verifyClass should never be called");
   return false;
   }

int32_t
TR_RelocationRecordInlinedAllocation::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordInlinedAllocationPrivateData *reloPrivateData = &(privateData()->inlinedAllocation);
   reloRuntime->incNumInlinedAllocRelos();

   if (!reloPrivateData->_inlinedCodeIsOkay)
      {
      uint8_t *destination = (uint8_t *) (reloLocation + (UDATA) branchOffset(reloTarget));

      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: inlined alloc not OK, patch destination %p\n", destination);
      _patchVirtualGuard(reloLocation, destination, reloRuntime->comp()->target().isSMP());
      reloRuntime->incNumFailedAllocInlinedRelos();
      }
   else
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: inlined alloc looks OK\n");
      }
   return 0;
   }

// TR_VerifyRefArrayForAlloc Relocation
char *
TR_RelocationRecordVerifyRefArrayForAlloc::name()
   {
   return "TR_VerifyRefArrayForAlloc";
   }

bool
TR_RelocationRecordVerifyRefArrayForAlloc::verifyClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *clazz)
   {
   return (clazz && ((J9Class *)clazz)->arrayClass);
   }


// TR_VerifyClassObjectForAlloc Relocation
char *
TR_RelocationRecordVerifyClassObjectForAlloc::name()
   {
   return "TR_VerifyClassObjectForAlloc";
   }

void
TR_RelocationRecordVerifyClassObjectForAlloc::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecordConstantPoolWithIndex::print(reloRuntime);
   reloLogger->printf("\tallocationSize %p\n", allocationSize(reloTarget));
   }

void
TR_RelocationRecordVerifyClassObjectForAlloc::setAllocationSize(TR_RelocationTarget *reloTarget, uintptr_t allocationSize)
   {
   reloTarget->storeRelocationRecordValue(allocationSize, (uintptr_t *)&((TR_RelocationRecordVerifyClassObjectForAllocBinaryTemplate *)_record)->_allocationSize);
   }

uintptr_t
TR_RelocationRecordVerifyClassObjectForAlloc::allocationSize(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *)&((TR_RelocationRecordVerifyClassObjectForAllocBinaryTemplate *)_record)->_allocationSize);
   }

bool
TR_RelocationRecordVerifyClassObjectForAlloc::verifyClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *clazz)
   {
   bool inlineAllocation = false;
   TR::Compilation* comp = TR::comp();
   TR_J9VMBase *fe = (TR_J9VMBase *)(reloRuntime->fej9());
   if (comp->canAllocateInlineClass(clazz))
      {
      uintptr_t size = fe->getAllocationSize(NULL, clazz);
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tverifyClass: allocationSize %d\n", size);
      if (size == allocationSize(reloTarget))
         inlineAllocation = true;
      }
   else
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tverifyClass: cannot inline allocate class\n");

   return inlineAllocation;
   }

// TR_InlinedMethod Relocation
void
TR_RelocationRecordInlinedMethod::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecordConstantPoolWithIndex::print(reloRuntime);
   J9ROMClass *inlinedCodeRomClass = reloRuntime->fej9()->sharedCache()->romClassFromOffsetInSharedCache(romClassOffsetInSharedCache(reloTarget));
   J9UTF8 *inlinedCodeClassName = J9ROMCLASS_CLASSNAME(inlinedCodeRomClass);
   reloLogger->printf("\tromClassOffsetInSharedCache %x %.*s\n", romClassOffsetInSharedCache(reloTarget), J9UTF8_LENGTH(inlinedCodeClassName), J9UTF8_DATA(inlinedCodeClassName));
   //reloLogger->printf("\tromClassOffsetInSharedCache %x %.*s\n", romClassOffsetInSharedCache(reloTarget), J9UTF8_LENGTH(inlinedCodeClassname), J9UTF8_DATA(inlinedCodeClassName));
   }

void
TR_RelocationRecordInlinedMethod::setRomClassOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptr_t romClassOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(romClassOffsetInSharedCache, (uintptr_t *) &((TR_RelocationRecordInlinedMethodBinaryTemplate *)_record)->_romClassOffsetInSharedCache);
   }

uintptr_t
TR_RelocationRecordInlinedMethod::romClassOffsetInSharedCache(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordInlinedMethodBinaryTemplate *)_record)->_romClassOffsetInSharedCache);
   }

void
TR_RelocationRecordInlinedMethod::fixInlinedSiteInfo(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueMethodBlock *inlinedMethod)
   {
   TR_InlinedCallSite *inlinedCallSite = (TR_InlinedCallSite *)getInlinedCallSiteArrayElement(reloRuntime->exceptionTable(), inlinedSiteIndex(reloTarget));
   inlinedCallSite->_methodInfo = inlinedMethod;
   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   RELO_LOG(reloRuntime->reloLogger(), 5, "\tfixInlinedSiteInfo: [%p] set to %p, virtual guard address %p\n",inlinedCallSite, inlinedMethod, reloPrivateData->_destination);

   /*
    * An inlined site's _methodInfo field is used to resolve inlined methods' J9Classes and mark their respective heap objects as live during gc stackwalking.
    * If we unload a class from which we have inlined one or more methods, we need to invalidate the _methodInfo fields themselves in addition to any other uses,
    * to prevent us from attempting to mark unloaded classes as live.
    *
    * See also: frontend/j9/MetaData.cpp:populateInlineCalls(TR::Compilation* , TR_J9VMBase* , TR_MethodMetaData* , uint8_t* , uint32_t )
    */
   TR_OpaqueClassBlock *classOfInlinedMethod = reloRuntime->fej9()->getClassFromMethodBlock(inlinedMethod);
   if ( reloRuntime->fej9()->isUnloadAssumptionRequired( classOfInlinedMethod, reloRuntime->comp()->getCurrentMethod() ) )
      {
      reloTarget->addPICtoPatchPtrOnClassUnload(classOfInlinedMethod, &(inlinedCallSite->_methodInfo));
      }
   }

void
TR_RelocationRecordInlinedMethod::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_OpaqueMethodBlock *ramMethod = NULL;
   bool inlinedSiteIsValid = inlinedSiteValid(reloRuntime, reloTarget, &ramMethod);

   if (reloRuntime->comp()->getOption(TR_UseSymbolValidationManager))
      SVM_ASSERT(ramMethod != NULL, "inlinedSiteValid should not return a NULL method when using the SVM!");

   if (ramMethod)
      {
      // If validate passes, no patching needed since the fall-through path is the inlined code
      fixInlinedSiteInfo(reloRuntime, reloTarget,ramMethod);
      }

   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   reloPrivateData->_ramMethod = ramMethod;
   reloPrivateData->_failValidation = !inlinedSiteIsValid;
   RELO_LOG(reloRuntime->reloLogger(), 5, "\tpreparePrivateData: ramMethod %p inlinedSiteIsValid %d\n", ramMethod, inlinedSiteIsValid);
   }

bool
TR_RelocationRecordInlinedMethod::inlinedSiteCanBeActivated(TR_RelocationRuntime *reloRuntime,
                                                            TR_RelocationTarget *reloTarget,
                                                            J9Method *currentMethod)
   {
   TR::SimpleRegex * regex = reloRuntime->options()->getDisabledInlineSites();
   if (regex && TR::SimpleRegex::match(regex, inlinedSiteIndex(reloTarget)))
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteCanBeActivated: inlined site forcibly disabled by options\n");
      return false;
      }

   if (reloRuntime->fej9()->isAnyMethodTracingEnabled((TR_OpaqueMethodBlock *) currentMethod) ||
       reloRuntime->fej9()->canMethodEnterEventBeHooked() ||
       reloRuntime->fej9()->canMethodExitEventBeHooked())
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteCanBeActivated: target may need enter/exit tracing so disabling inline site\n");
      return false;
      }

   return true;
   }

bool
TR_RelocationRecordInlinedMethod::inlinedSiteValid(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueMethodBlock **theMethod)
   {
   J9Method *currentMethod = NULL;
   bool inlinedSiteIsValid = true;
   J9Method *callerMethod = (J9Method *) getInlinedSiteCallerMethod(reloRuntime);
   if (callerMethod == (J9Method *)-1)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: caller failed relocation so cannot validate inlined method\n");
      *theMethod = NULL;
      return false;
      }
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tvalidateSameClasses: caller method %p\n", callerMethod);
   J9UTF8 *callerClassName;
   J9UTF8 *callerMethodName;
   J9UTF8 *callerMethodSignature;
   getClassNameSignatureFromMethod(callerMethod, callerClassName, callerMethodName, callerMethodSignature);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: caller method %.*s.%.*s%.*s\n",
                                              J9UTF8_LENGTH(callerClassName), J9UTF8_DATA(callerClassName),
                                              J9UTF8_LENGTH(callerMethodName), J9UTF8_DATA(callerMethodName),
                                              J9UTF8_LENGTH(callerMethodSignature), J9UTF8_DATA(callerMethodSignature));

   J9ConstantPool *cp = NULL;
   if (!isUnloadedInlinedMethod(callerMethod))
      cp = J9_CP_FROM_METHOD(callerMethod);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: cp %p\n", cp);

   if (!cp)
      {
      inlinedSiteIsValid = false;
      }
   else
      {
      TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);

      if (reloRuntime->comp()->getOption(TR_UseSymbolValidationManager))
         {
         TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);

         uintptr_t data = (uintptr_t)cpIndex(reloTarget);
         uint16_t methodID = (uint16_t)(data & 0xFFFF);
         uint16_t receiverClassID = (uint16_t)((data >> 16) & 0xFFFF);

         // currentMethod is guaranteed to not be NULL because of the SVM
         currentMethod = reloRuntime->comp()->getSymbolValidationManager()->getJ9MethodFromID(methodID);
         if (needsReceiverClassFromID())
            reloPrivateData->_receiverClass = reloRuntime->comp()->getSymbolValidationManager()->getClassFromID(receiverClassID);
         else
            reloPrivateData->_receiverClass = NULL;

         if (reloFlags(reloTarget) != inlinedMethodIsStatic && reloFlags(reloTarget) != inlinedMethodIsSpecial)
            {
            TR_ResolvedMethod *calleeResolvedMethod = reloRuntime->fej9()->createResolvedMethod(reloRuntime->comp()->trMemory(),
                                                                                                (TR_OpaqueMethodBlock *)currentMethod,
                                                                                                NULL);
            if (calleeResolvedMethod->virtualMethodIsOverridden())
               inlinedSiteIsValid = false;
            }
         }
      else
         {
         currentMethod = (J9Method *) getMethodFromCP(reloRuntime, cp, cpIndex(reloTarget), (TR_OpaqueMethodBlock *) callerMethod);
         if (!currentMethod)
            inlinedSiteIsValid = false;
         }

      if (inlinedSiteIsValid)
         inlinedSiteIsValid = inlinedSiteCanBeActivated(reloRuntime, reloTarget, currentMethod);

      if (inlinedSiteIsValid)
         {
         /* Calculate the runtime rom class value from the code cache */
         J9ROMClass *compileRomClass = reloRuntime->fej9()->sharedCache()->romClassFromOffsetInSharedCache(romClassOffsetInSharedCache(reloTarget));
         J9ROMClass *currentRomClass = J9_CLASS_FROM_METHOD(currentMethod)->romClass;

         RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: compileRomClass %p currentRomClass %p\n", compileRomClass, currentRomClass);
         if (compileRomClass == currentRomClass)
            {
            J9UTF8 *className;
            J9UTF8 *methodName;
            J9UTF8 *methodSignature;
            getClassNameSignatureFromMethod(currentMethod, className, methodName, methodSignature);
            RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: inlined method %.*s.%.*s%.*s\n",
                                                       J9UTF8_LENGTH(className), J9UTF8_DATA(className),
                                                       J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName),
                                                       J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
            }
         else
            {
            if (reloRuntime->comp()->getOption(TR_UseSymbolValidationManager))
               SVM_ASSERT(false, "compileRomClass and currentRomClass should not be different!");

            inlinedSiteIsValid = false;
            }
         }
      }

   if (!inlinedSiteIsValid)
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tinlinedSiteValid: not valid\n");

   /* Even if the inlined site is disabled, the inlined site table in the metadata
    * should still be populated under AOT w/ SVM
    */
   *theMethod = reinterpret_cast<TR_OpaqueMethodBlock *>(currentMethod);

   return inlinedSiteIsValid;
   }

int32_t
TR_RelocationRecordInlinedMethod::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   reloRuntime->incNumInlinedMethodRelos();

   TR_AOTStats *aotStats = reloRuntime->aotStats();

   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   if (reloPrivateData->_failValidation)
      {
      // When not using the SVM, the safest thing to do is to fail the AOT Load
      if (!reloRuntime->comp()->getOption(TR_UseSymbolValidationManager))
         {
         RELO_LOG(reloRuntime->reloLogger(), 6,"\t\tapplyRelocation: Failing AOT Load\n");
         return compilationAotClassReloFailure;
         }

      RELO_LOG(reloRuntime->reloLogger(), 6,"\t\tapplyRelocation: invalidating guard\n");

      invalidateGuard(reloRuntime, reloTarget, reloLocation);

      reloRuntime->incNumFailedInlinedMethodRelos();
      if (aotStats)
         {
         aotStats->numInlinedMethodValidationFailed++;
         updateFailedStats(aotStats);
         }
      }
   else
      {
      RELO_LOG(reloRuntime->reloLogger(), 6,"\t\tapplyRelocation: activating inlined method\n");

      activateGuard(reloRuntime, reloTarget, reloLocation);

      if (aotStats)
         {
         aotStats->numInlinedMethodRelocated++;
         updateSucceededStats(aotStats);
         }
      }

   return 0;
   }

// TR_RelocationRecordNopGuard
void
TR_RelocationRecordNopGuard::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecordInlinedMethod::print(reloRuntime);
   reloLogger->printf("\tdestinationAddress %p\n", destinationAddress(reloTarget));
   }

void
TR_RelocationRecordNopGuard::setDestinationAddress(TR_RelocationTarget *reloTarget, uintptr_t destinationAddress)
   {
   reloTarget->storeRelocationRecordValue(destinationAddress, (uintptr_t *) &((TR_RelocationRecordNopGuardBinaryTemplate *)_record)->_destinationAddress);
   }

uintptr_t
TR_RelocationRecordNopGuard::destinationAddress(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordNopGuardBinaryTemplate *)_record)->_destinationAddress);
   }

void
TR_RelocationRecordNopGuard::invalidateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   _patchVirtualGuard(reloLocation, reloPrivateData->_destination, 1);
   }

void
TR_RelocationRecordNopGuard::activateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   createAssumptions(reloRuntime, reloLocation);
   if (reloRuntime->options()->getOption(TR_EnableHCR))
      {
      TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
      TR_PatchNOPedGuardSiteOnClassRedefinition::make(reloRuntime->fej9(), reloRuntime->trMemory()->trPersistentMemory(),
         (TR_OpaqueClassBlock*) J9_CLASS_FROM_METHOD((J9Method *)reloPrivateData->_ramMethod),
         reloLocation, reloPrivateData->_destination,
         getMetadataAssumptionList(reloRuntime->exceptionTable()));
      }
   }

void
TR_RelocationRecordNopGuard::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordInlinedMethod::preparePrivateData(reloRuntime, reloTarget);

   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   reloPrivateData->_destination = (uint8_t *) (destinationAddress(reloTarget) - reloRuntime->aotMethodHeaderEntry()->compileMethodCodeStartPC + (UDATA)reloRuntime->newMethodCodeStart());
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: guard backup destination %p\n", reloPrivateData->_destination);
   }

// TR_InlinedStaticMethodWithNopGuard
char *
TR_RelocationRecordInlinedStaticMethodWithNopGuard::name()
   {
   return "TR_InlinedStaticMethodWithNopGuard";
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordInlinedStaticMethodWithNopGuard::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   return getStaticMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

void
TR_RelocationRecordInlinedStaticMethodWithNopGuard::updateFailedStats(TR_AOTStats *aotStats)
   {
   aotStats->staticMethods.numFailedValidations++;
   }

void
TR_RelocationRecordInlinedStaticMethodWithNopGuard::updateSucceededStats(TR_AOTStats *aotStats)
   {
   aotStats->staticMethods.numSucceededValidations++;
   }


// TR_RelocationRecordInlinedStaticMethod
char *
TR_RelocationRecordInlinedStaticMethod::name()
   {
   return "TR_InlinedStaticMethod";
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordInlinedStaticMethod::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   return getStaticMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

// TR_InlinedSpecialMethodWithNopGuard
char *
TR_RelocationRecordInlinedSpecialMethodWithNopGuard::name()
   {
   return "TR_InlinedSpecialMethodWithNopGuard";
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordInlinedSpecialMethodWithNopGuard::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   return getSpecialMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

void
TR_RelocationRecordInlinedSpecialMethodWithNopGuard::updateFailedStats(TR_AOTStats *aotStats)
   {
   aotStats->specialMethods.numFailedValidations++;
   }

void
TR_RelocationRecordInlinedSpecialMethodWithNopGuard::updateSucceededStats(TR_AOTStats *aotStats)
   {
   aotStats->specialMethods.numSucceededValidations++;
   }

// TR_RelocationRecordInlinedSpecialMethod
char *
TR_RelocationRecordInlinedSpecialMethod::name()
   {
   return "TR_InlinedSpecialMethod";
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordInlinedSpecialMethod::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   return getSpecialMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

// TR_InlinedVirtualMethodWithNopGuard
char *
TR_RelocationRecordInlinedVirtualMethodWithNopGuard::name()
   {
   return "TR_InlinedVirtualMethodWithNopGuard";
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordInlinedVirtualMethodWithNopGuard::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   return getVirtualMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

void
TR_RelocationRecordInlinedVirtualMethodWithNopGuard::createAssumptions(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation)
   {
   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   TR_PatchNOPedGuardSiteOnMethodOverride::make(reloRuntime->fej9(), reloRuntime->trMemory()->trPersistentMemory(),
      (TR_OpaqueMethodBlock*) reloPrivateData->_ramMethod, reloLocation, reloPrivateData->_destination,
      getMetadataAssumptionList(reloRuntime->exceptionTable()));
   }

void
TR_RelocationRecordInlinedVirtualMethodWithNopGuard::updateFailedStats(TR_AOTStats *aotStats)
   {
   aotStats->virtualMethods.numFailedValidations++;
   }

void
TR_RelocationRecordInlinedVirtualMethodWithNopGuard::updateSucceededStats(TR_AOTStats *aotStats)
   {
   aotStats->virtualMethods.numSucceededValidations++;
   }

// TR_RelocationRecordInlinedVirtualMethod
char *
TR_RelocationRecordInlinedVirtualMethod::name()
   {
   return "TR_InlinedVirtualMethod";
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordInlinedVirtualMethod::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   return getVirtualMethodFromCP(reloRuntime, void_cp, cpIndex);
   }

// TR_InlinedInterfaceMethodWithNopGuard
char *
TR_RelocationRecordInlinedInterfaceMethodWithNopGuard::name()
   {
   return "TR_InlinedInterfaceMethodWithNopGuard";
   }

void
TR_RelocationRecordInlinedInterfaceMethodWithNopGuard::createAssumptions(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation)
   {
   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   TR_PersistentCHTable *table = reloRuntime->getPersistentInfo()->getPersistentCHTable();
   List<TR_VirtualGuardSite> sites(reloRuntime->comp()->trMemory());
   TR_VirtualGuardSite site;
   site.setLocation(reloLocation);
   site.setDestination(reloPrivateData->_destination);
   sites.add(&site);
   TR_ClassQueries::addAnAssumptionForEachSubClass(table, table->findClassInfo(reloPrivateData->_receiverClass), sites, reloRuntime->comp());
   }

void
TR_RelocationRecordInlinedInterfaceMethodWithNopGuard::updateFailedStats(TR_AOTStats *aotStats)
   {
   aotStats->interfaceMethods.numFailedValidations++;
   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   TR_OpaqueClassBlock *receiver = reloPrivateData->_receiverClass;
   }

void
TR_RelocationRecordInlinedInterfaceMethodWithNopGuard::updateSucceededStats(TR_AOTStats *aotStats)
   {
   aotStats->interfaceMethods.numSucceededValidations++;
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordInlinedInterfaceMethodWithNopGuard::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   return getInterfaceMethodFromCP(reloRuntime, void_cp, cpIndex, callerMethod);
   }

// TR_RelocationRecordInlinedInterfaceMethod
char *
TR_RelocationRecordInlinedInterfaceMethod::name()
   {
   return "TR_InlinedInterfaceMethod";
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordInlinedInterfaceMethod::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   return getInterfaceMethodFromCP(reloRuntime, void_cp, cpIndex, callerMethod);
   }

// TR_InlinedAbstractMethodWithNopGuard
char *
TR_RelocationRecordInlinedAbstractMethodWithNopGuard::name()
   {
   return "TR_InlinedAbstractMethodWithNopGuard";
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordInlinedAbstractMethodWithNopGuard::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   return getAbstractMethodFromCP(reloRuntime, void_cp, cpIndex, callerMethod);
   }

void
TR_RelocationRecordInlinedAbstractMethodWithNopGuard::createAssumptions(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation)
   {
   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   TR_PersistentCHTable *table = reloRuntime->getPersistentInfo()->getPersistentCHTable();
   List<TR_VirtualGuardSite> sites(reloRuntime->comp()->trMemory());
   TR_VirtualGuardSite site;
   site.setLocation(reloLocation);
   site.setDestination(reloPrivateData->_destination);
   sites.add(&site);
   TR_ClassQueries::addAnAssumptionForEachSubClass(table, table->findClassInfo(reloPrivateData->_receiverClass), sites, reloRuntime->comp());
   }

void
TR_RelocationRecordInlinedAbstractMethodWithNopGuard::updateFailedStats(TR_AOTStats *aotStats)
   {
   aotStats->abstractMethods.numFailedValidations++;
   }

void
TR_RelocationRecordInlinedAbstractMethodWithNopGuard::updateSucceededStats(TR_AOTStats *aotStats)
   {
   aotStats->abstractMethods.numSucceededValidations++;
   }

// TR_RelocationRecordInlinedAbstractMethod
char *
TR_RelocationRecordInlinedAbstractMethod::name()
   {
   return "TR_InlinedAbstractMethod";
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordInlinedAbstractMethod::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   return getAbstractMethodFromCP(reloRuntime, void_cp, cpIndex, callerMethod);
   }

// TR_ProfiledInlinedMethod
char *
TR_RelocationRecordProfiledInlinedMethod::name()
   {
   return "TR_ProfiledInlinedMethod";
   }

void
TR_RelocationRecordProfiledInlinedMethod::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationRecordInlinedMethod::print(reloRuntime);

   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   reloLogger->printf("\tclassChainIdentifyingLoaderOffsetInSharedCache %x\n", classChainIdentifyingLoaderOffsetInSharedCache(reloTarget));
   reloLogger->printf("\tclassChainForInlinedMethod %x\n", classChainForInlinedMethod(reloTarget));
   reloLogger->printf("\tmethodIndex %x\n", methodIndex(reloTarget));
   }

void
TR_RelocationRecordProfiledInlinedMethod::setClassChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptr_t classChainIdentifyingLoaderOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(classChainIdentifyingLoaderOffsetInSharedCache, (uintptr_t *) &((TR_RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_classChainIdentifyingLoaderOffsetInSharedCache);
   }

uintptr_t
TR_RelocationRecordProfiledInlinedMethod::classChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_classChainIdentifyingLoaderOffsetInSharedCache);
   }

void
TR_RelocationRecordProfiledInlinedMethod::setClassChainForInlinedMethod(TR_RelocationTarget *reloTarget, uintptr_t classChainForInlinedMethod)
   {
   reloTarget->storeRelocationRecordValue(classChainForInlinedMethod, (uintptr_t *) &((TR_RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_classChainForInlinedMethod);
   }

uintptr_t
TR_RelocationRecordProfiledInlinedMethod::classChainForInlinedMethod(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_classChainForInlinedMethod);
   }

void
TR_RelocationRecordProfiledInlinedMethod::setMethodIndex(TR_RelocationTarget *reloTarget, uintptr_t methodIndex)
   {
   reloTarget->storeRelocationRecordValue(methodIndex, (uintptr_t *) &((TR_RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_methodIndex);
   }

uintptr_t
TR_RelocationRecordProfiledInlinedMethod::methodIndex(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_methodIndex);
   }


bool
TR_RelocationRecordProfiledInlinedMethod::checkInlinedClassValidity(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *inlinedClass)
   {
   return true;
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordProfiledInlinedMethod::getInlinedMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *inlinedCodeClass)
   {
   J9Method *resolvedMethods = static_cast<J9Method *>(reloRuntime->fej9()->getMethods(inlinedCodeClass));
   J9Method *inlinedMethod = NULL;

   uint32_t numMethods = reloRuntime->fej9()->getNumMethods(inlinedCodeClass);
   uint32_t methodIndex = this->methodIndex(reloTarget);

   if (methodIndex < numMethods)
      {
      inlinedMethod = &(resolvedMethods[methodIndex]);
      }

   return reinterpret_cast<TR_OpaqueMethodBlock *>(inlinedMethod);
   }

void
TR_RelocationRecordProfiledInlinedMethod::setupInlinedMethodData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordProfiledInlinedMethodPrivateData *reloPrivateData = &(privateData()->profiledInlinedMethod);
   reloPrivateData->_guardValue = 0;
   }

void
TR_RelocationRecordProfiledInlinedMethod::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordProfiledInlinedMethodPrivateData *reloPrivateData = &(privateData()->profiledInlinedMethod);
   reloPrivateData->_needUnloadAssumption = false;
   reloPrivateData->_guardValue = 0;
   bool failValidation = true;
   TR_OpaqueClassBlock *inlinedCodeClass = NULL;

   if (reloRuntime->comp()->getOption(TR_UseSymbolValidationManager))
      {
      uint16_t inlinedCodeClassID = (uint16_t)cpIndex(reloTarget);
      inlinedCodeClass = (TR_OpaqueClassBlock *)reloRuntime->comp()->getSymbolValidationManager()->getJ9ClassFromID(inlinedCodeClassID);
      }
   else
      {
      J9ROMClass *inlinedCodeRomClass = reloRuntime->fej9()->sharedCache()->romClassFromOffsetInSharedCache(romClassOffsetInSharedCache(reloTarget));
      J9UTF8 *inlinedCodeClassName = J9ROMCLASS_CLASSNAME(inlinedCodeRomClass);
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: inlinedCodeRomClass %p %.*s\n", inlinedCodeRomClass, J9UTF8_LENGTH(inlinedCodeClassName), J9UTF8_DATA(inlinedCodeClassName));

      void *classChainIdentifyingLoader = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainIdentifyingLoaderOffsetInSharedCache(reloTarget));
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classChainIdentifyingLoader %p\n", classChainIdentifyingLoader);
      J9ClassLoader *classLoader = (J9ClassLoader *) reloRuntime->fej9()->sharedCache()->persistentClassLoaderTable()->lookupClassLoaderAssociatedWithClassChain(classChainIdentifyingLoader);

      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classLoader %p\n", classLoader);

      if (classLoader)
         {
         TR::VMAccessCriticalSection preparePrivateDataCriticalSection(reloRuntime->fej9());
         inlinedCodeClass = (TR_OpaqueClassBlock *) jitGetClassInClassloaderFromUTF8(reloRuntime->currentThread(),
                                                                                     classLoader,
                                                                                     J9UTF8_DATA(inlinedCodeClassName),
                                                                                     J9UTF8_LENGTH(inlinedCodeClassName));
         }
      }

   if (inlinedCodeClass && checkInlinedClassValidity(reloRuntime, inlinedCodeClass))
      {
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: inlined class valid\n");
      reloPrivateData->_inlinedCodeClass = inlinedCodeClass;

      bool inlinedSiteIsValid = true;
      TR_OpaqueMethodBlock *inlinedMethod = NULL;

      uintptr_t *chainData = (uintptr_t *) reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainForInlinedMethod(reloTarget));
      if (!reloRuntime->fej9()->sharedCache()->classMatchesCachedVersion(inlinedCodeClass, chainData))
         inlinedSiteIsValid = false;

      if (inlinedSiteIsValid)
         {
         inlinedMethod = getInlinedMethod(reloRuntime, reloTarget, inlinedCodeClass);
         if (!inlinedMethod)
            inlinedSiteIsValid = false;
         }

      if (inlinedSiteIsValid)
         inlinedSiteIsValid = inlinedSiteCanBeActivated(reloRuntime, reloTarget, reinterpret_cast<J9Method *>(inlinedMethod));

      if (inlinedSiteIsValid)
         {
         reloPrivateData->_needUnloadAssumption = !reloRuntime->fej9()->sameClassLoaders(inlinedCodeClass, reloRuntime->comp()->getCurrentMethod()->classOfMethod());
         setupInlinedMethodData(reloRuntime, reloTarget);
         }

      /* Even if the inlined site is disabled, the inlined site table in the metadata
       * should still be populated under AOT w/ SVM
       */
      if (inlinedMethod)
         fixInlinedSiteInfo(reloRuntime, reloTarget, inlinedMethod);
      else if (reloRuntime->comp()->getOption(TR_UseSymbolValidationManager))
         SVM_ASSERT(inlinedMethod != NULL, "inlinedMethod should not be NULL when using the SVM!");

      failValidation = !inlinedSiteIsValid;
      }

   reloPrivateData->_failValidation = failValidation;
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: needUnloadAssumption %d\n", reloPrivateData->_needUnloadAssumption);
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: guardValue %p\n", reloPrivateData->_guardValue);
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: failValidation %d\n", failValidation);
   }

void
TR_RelocationRecordProfiledInlinedMethod::updateSucceededStats(TR_AOTStats *aotStats)
   {
   aotStats->profiledInlinedMethods.numSucceededValidations++;
   }

void
TR_RelocationRecordProfiledInlinedMethod::updateFailedStats(TR_AOTStats *aotStats)
   {
   aotStats->profiledInlinedMethods.numFailedValidations++;
   }


// TR_ProfiledGuard
void
TR_RelocationRecordProfiledGuard::activateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   }

void
TR_RelocationRecordProfiledGuard::invalidateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   }

// TR_ProfileClassGuard
char *
TR_RelocationRecordProfiledClassGuard::name()
   {
   return "TR_ProfiledClassGuard";
   }

bool
TR_RelocationRecordProfiledClassGuard::checkInlinedClassValidity(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *inlinedCodeClass)
   {
   return true;
   return !reloRuntime->fej9()->classHasBeenExtended(inlinedCodeClass) && !reloRuntime->options()->getOption(TR_DisableProfiledInlining);
   }

void
TR_RelocationRecordProfiledClassGuard::setupInlinedMethodData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordProfiledInlinedMethodPrivateData *reloPrivateData = &(privateData()->profiledInlinedMethod);
   reloPrivateData->_guardValue = (uintptr_t) reloPrivateData->_inlinedCodeClass;
   }

void
TR_RelocationRecordProfiledClassGuard::updateSucceededStats(TR_AOTStats *aotStats)
   {
   aotStats->profiledClassGuards.numSucceededValidations++;
   }

void
TR_RelocationRecordProfiledClassGuard::updateFailedStats(TR_AOTStats *aotStats)
   {
   aotStats->profiledClassGuards.numFailedValidations++;
   }


// TR_ProfiledMethodGuard
char *
TR_RelocationRecordProfiledMethodGuard::name()
   {
   return "TR_ProfiledMethodGuard";
   }

bool
TR_RelocationRecordProfiledMethodGuard::checkInlinedClassValidity(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *inlinedCodeClass)
   {
   return true;
   return !reloRuntime->options()->getOption(TR_DisableProfiledMethodInlining);
   }

void
TR_RelocationRecordProfiledMethodGuard::setupInlinedMethodData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordProfiledInlinedMethodPrivateData *reloPrivateData = &(privateData()->profiledInlinedMethod);
   TR_OpaqueMethodBlock *inlinedMethod = getInlinedMethod(reloRuntime, reloTarget, reloPrivateData->_inlinedCodeClass);
   reloPrivateData->_guardValue = (uintptr_t) inlinedMethod;
   }

void
TR_RelocationRecordProfiledMethodGuard::updateSucceededStats(TR_AOTStats *aotStats)
   {
   aotStats->profiledMethodGuards.numSucceededValidations++;
   }

void
TR_RelocationRecordProfiledMethodGuard::updateFailedStats(TR_AOTStats *aotStats)
   {
   aotStats->profiledMethodGuards.numFailedValidations++;
   }


// TR_RamMethod Relocation
char *
TR_RelocationRecordRamMethod::name()
   {
   return "TR_RamMethod";
   }

int32_t
TR_RelocationRecordRamMethod::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: method pointer %p\n", reloRuntime->exceptionTable()->ramMethod);
   reloTarget->storeAddress((uint8_t *)reloRuntime->exceptionTable()->ramMethod, reloLocation);
   return 0;
   }

int32_t
TR_RelocationRecordRamMethod::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: method pointer %p\n", reloRuntime->exceptionTable()->ramMethod);
   reloTarget->storeAddress((uint8_t *)reloRuntime->exceptionTable()->ramMethod, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   return 0;
   }

// TR_MethodTracingCheck Relocation
void
TR_RelocationRecordMethodTracingCheck::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tdestinationAddress %x\n", destinationAddress(reloTarget));
   }

void
TR_RelocationRecordMethodTracingCheck::setDestinationAddress(TR_RelocationTarget *reloTarget, uintptr_t destinationAddress)
   {
   reloTarget->storeRelocationRecordValue(destinationAddress, (uintptr_t *) &((TR_RelocationRecordMethodTracingCheckBinaryTemplate *)_record)->_destinationAddress);
   }

uintptr_t
TR_RelocationRecordMethodTracingCheck::destinationAddress(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordMethodTracingCheckBinaryTemplate *)_record)->_destinationAddress);
   }

void
TR_RelocationRecordMethodTracingCheck::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordMethodTracingCheckPrivateData *reloPrivateData = &(privateData()->methodTracingCheck);

   uintptr_t destination = destinationAddress(reloTarget);
   reloPrivateData->_destinationAddress = (uint8_t *) (destinationAddress(reloTarget) - (UDATA) reloRuntime->aotMethodHeaderEntry()->compileMethodCodeStartPC + (UDATA)(reloRuntime->newMethodCodeStart()));
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: check destination %p\n", reloPrivateData->_destinationAddress);
   }

int32_t
TR_RelocationRecordMethodTracingCheck::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordMethodTracingCheckPrivateData *reloPrivateData = &(privateData()->methodTracingCheck);
   _patchVirtualGuard(reloLocation, reloPrivateData->_destinationAddress, 1 /* currently assume SMP only */);
   return 0;
   }


// TR_MethodEnterCheck
char *
TR_RelocationRecordMethodEnterCheck::name()
   {
   return "TR_MethodEnterCheck";
   }

TR_RelocationRecordAction
TR_RelocationRecordMethodEnterCheck::action(TR_RelocationRuntime *reloRuntime)
   {
   bool reportMethodEnter = reloRuntime->fej9()->isMethodTracingEnabled((TR_OpaqueMethodBlock *) reloRuntime->method()) || reloRuntime->fej9()->canMethodEnterEventBeHooked();
   RELO_LOG(reloRuntime->reloLogger(), 6,"\taction: reportMethodEnter %d\n", reportMethodEnter);
   return reportMethodEnter ? TR_RelocationRecordAction::apply : TR_RelocationRecordAction::ignore;
   }


// TR_MethodExitCheck
char *
TR_RelocationRecordMethodExitCheck::name()
   {
   return "TR_MethodExitCheck";
   }

TR_RelocationRecordAction
TR_RelocationRecordMethodExitCheck::action(TR_RelocationRuntime *reloRuntime)
   {
   bool reportMethodExit = reloRuntime->fej9()->isMethodTracingEnabled((TR_OpaqueMethodBlock *) reloRuntime->method()) || reloRuntime->fej9()->canMethodExitEventBeHooked();
   RELO_LOG(reloRuntime->reloLogger(), 6,"\taction: reportMethodExit %d\n", reportMethodExit);
   return reportMethodExit ? TR_RelocationRecordAction::apply : TR_RelocationRecordAction::ignore;
   }


// TR_RelocationRecordValidateClass
char *
TR_RelocationRecordValidateClass::name()
   {
   return "TR_ValidateClass";
   }

void
TR_RelocationRecordValidateClass::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecordConstantPoolWithIndex::print(reloRuntime);
   reloLogger->printf("\tclassChainOffsetInSharedClass %x\n", classChainOffsetInSharedCache(reloTarget));
   }

void
TR_RelocationRecordValidateClass::setClassChainOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptr_t classChainOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(classChainOffsetInSharedCache, (uintptr_t *) &((TR_RelocationRecordValidateClassBinaryTemplate *)_record)->_classChainOffsetInSharedCache);
   }

uintptr_t
TR_RelocationRecordValidateClass::classChainOffsetInSharedCache(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordValidateClassBinaryTemplate *)_record)->_classChainOffsetInSharedCache);
   }

void
TR_RelocationRecordValidateClass::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   }

bool
TR_RelocationRecordValidateClass::validateClass(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *clazz, void *classChainOrRomClass)
   {
   // for classes and instance fields, need to compare clazz to entire class chain to make sure they are identical
   // classChainOrRomClass, for classes and instance fields, is a class chain pointer from the relocation record

   void *classChain = classChainOrRomClass;
   return reloRuntime->fej9()->sharedCache()->classMatchesCachedVersion(clazz, (uintptr_t *) classChain);
   }

int32_t
TR_RelocationRecordValidateClass::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   reloRuntime->incNumValidations();

   J9ConstantPool *cp = (J9ConstantPool *)computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: cp %p\n", cp);
   TR_OpaqueClassBlock *definingClass = getClassFromCP(reloRuntime, reloTarget, (void *) cp);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: definingClass %p\n", definingClass);

   int32_t returnCode = 0;
   bool verified = false;
   if (definingClass)
      {
      void *classChainOrROMClass;
      if (isStaticFieldValidation())
         classChainOrROMClass = reloRuntime->fej9()->sharedCache()->romClassFromOffsetInSharedCache(classChainOffsetInSharedCache(reloTarget));
      else
         classChainOrROMClass = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainOffsetInSharedCache(reloTarget));

      verified = validateClass(reloRuntime, definingClass, classChainOrROMClass);
      }

   if (!verified)
      {
      RELO_LOG(reloRuntime->reloLogger(), 1, "\t\tapplyRelocation: could not verify class\n");
      returnCode = failureCode();
      }

   return returnCode;
   }

TR_OpaqueClassBlock *
TR_RelocationRecordValidateClass::getClassFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, void *void_cp)
   {
   TR_OpaqueClassBlock *definingClass = NULL;
   if (void_cp)
      {
      TR::VMAccessCriticalSection getClassFromCP(reloRuntime->fej9());
      J9JavaVM *javaVM = reloRuntime->javaVM();
      definingClass = (TR_OpaqueClassBlock *) javaVM->internalVMFunctions->resolveClassRef(javaVM->internalVMFunctions->currentVMThread(javaVM),
                                                                                           (J9ConstantPool *) void_cp,
                                                                                           cpIndex(reloTarget),
                                                                                           J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
      }

   return definingClass;
   }

int32_t
TR_RelocationRecordValidateClass::failureCode()
   {
   return compilationAotClassReloFailure;
   }


// TR_VerifyInstanceField
char *
TR_RelocationRecordValidateInstanceField::name()
   {
   return "TR_ValidateInstanceField";
   }

TR_OpaqueClassBlock *
TR_RelocationRecordValidateInstanceField::getClassFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, void *void_cp)
   {
   TR_OpaqueClassBlock *definingClass = NULL;
   if (void_cp)
      {
      definingClass = TR_ResolvedJ9Method::definingClassFromCPFieldRef(reloRuntime->comp(), (J9ConstantPool *) void_cp, cpIndex(reloTarget), false);
      }

   return definingClass;
   }

int32_t
TR_RelocationRecordValidateInstanceField::failureCode()
   {
   return compilationAotValidateFieldFailure;
   }

// TR_VerifyStaticField
char *
TR_RelocationRecordValidateStaticField::name()
   {
   return "TR_ValidateStaticField";
   }

void
TR_RelocationRecordValidateStaticField::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecordConstantPoolWithIndex::print(reloRuntime);
   reloLogger->printf("\tromClassOffsetInSharedClass %x\n", romClassOffsetInSharedCache(reloTarget));
   }

void
TR_RelocationRecordValidateStaticField::setRomClassOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptr_t romClassOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(romClassOffsetInSharedCache, (uintptr_t *) &((TR_RelocationRecordValidateStaticFieldBinaryTemplate *)_record)->_romClassOffsetInSharedCache);
   }

uintptr_t
TR_RelocationRecordValidateStaticField::romClassOffsetInSharedCache(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordValidateStaticFieldBinaryTemplate *)_record)->_romClassOffsetInSharedCache);
   }

TR_OpaqueClassBlock *
TR_RelocationRecordValidateStaticField::getClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, void *void_cp)
   {
   TR_OpaqueClassBlock *definingClass = NULL;
   if (void_cp)
      {
      definingClass = TR_ResolvedJ9Method::definingClassFromCPFieldRef(reloRuntime->comp(), (J9ConstantPool *) void_cp, cpIndex(reloTarget), true); 
      }

   return definingClass;
   }

bool
TR_RelocationRecordValidateStaticField::validateClass(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *clazz, void *classChainOrRomClass)
   {
   // for static fields, all that matters is the romclass of the class matches (statics cannot be inherited)
   // classChainOrRomClass, for static fields, is a romclass pointer from the relocation record

   J9ROMClass *romClass = (J9ROMClass *) classChainOrRomClass;
   J9Class *j9class = (J9Class *)clazz;
   return j9class->romClass == romClass;
   }


// TR_RelocationRecordValidateArbitraryClass
char *
TR_RelocationRecordValidateArbitraryClass::name()
   {
   return "TR_ValidateArbitraryClass";
   }

void
TR_RelocationRecordValidateArbitraryClass::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tclassChainIdentifyingLoaderOffset %p\n", classChainIdentifyingLoaderOffset(reloTarget));
   reloLogger->printf("\tclassChainOffsetForClassBeingValidated %p\n", classChainOffsetForClassBeingValidated(reloTarget));
   }


void
TR_RelocationRecordValidateArbitraryClass::setClassChainIdentifyingLoaderOffset(TR_RelocationTarget *reloTarget, uintptr_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptr_t *) &((TR_RelocationRecordValidateArbitraryClassBinaryTemplate *)_record)->_loaderClassChainOffset);
   }

uintptr_t
TR_RelocationRecordValidateArbitraryClass::classChainIdentifyingLoaderOffset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordValidateArbitraryClassBinaryTemplate *)_record)->_loaderClassChainOffset);
   }

void
TR_RelocationRecordValidateArbitraryClass::setClassChainOffsetForClassBeingValidated(TR_RelocationTarget *reloTarget, uintptr_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptr_t *) &((TR_RelocationRecordValidateArbitraryClassBinaryTemplate *)_record)->_classChainOffsetForClassBeingValidated);
   }

uintptr_t
TR_RelocationRecordValidateArbitraryClass::classChainOffsetForClassBeingValidated(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordValidateArbitraryClassBinaryTemplate *)_record)->_classChainOffsetForClassBeingValidated);
   }

void
TR_RelocationRecordValidateArbitraryClass::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   }

int32_t
TR_RelocationRecordValidateArbitraryClass::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_AOTStats *aotStats = reloRuntime->aotStats();
   if (aotStats)
      aotStats->numClassValidations++;

   void *classChainIdentifyingLoader = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainIdentifyingLoaderOffset(reloTarget));
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tpreparePrivateData: classChainIdentifyingLoader %p\n", classChainIdentifyingLoader);

   J9ClassLoader *classLoader = (J9ClassLoader *) reloRuntime->fej9()->sharedCache()->persistentClassLoaderTable()->lookupClassLoaderAssociatedWithClassChain(classChainIdentifyingLoader);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tpreparePrivateData: classLoader %p\n", classLoader);

   if (classLoader)
      {
      uintptr_t *classChainForClassBeingValidated = (uintptr_t *) reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainOffsetForClassBeingValidated(reloTarget));
      TR_OpaqueClassBlock *clazz = reloRuntime->fej9()->sharedCache()->lookupClassFromChainAndLoader(classChainForClassBeingValidated, classLoader);
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tpreparePrivateData: clazz %p\n", clazz);

      if (clazz)
         return 0;
      }

   if (aotStats)
      aotStats->numClassValidationsFailed++;

   return compilationAotClassReloFailure;
   }

int32_t
TR_RelocationRecordValidateClassByName::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = this->classID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   uintptr_t classChainOffset = this->classChainOffset(reloTarget);
   uintptr_t *classChain = (uintptr_t*)reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainOffset);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassByNameRecord(classID, beholderID, classChain))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateClassByName::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tclassID %d\n", classID(reloTarget));
   reloLogger->printf("\tbeholderID %d\n", beholderID(reloTarget));
   reloLogger->printf("\tclassChain %p\n", (void *)classChainOffset(reloTarget));
   }

void
TR_RelocationRecordValidateClassByName::setClassID(TR_RelocationTarget *reloTarget, uint16_t classID)
   {
   reloTarget->storeUnsigned16b(classID, (uint8_t *) &((TR_RelocationRecordValidateClassByNameBinaryTemplate *)_record)->_classID);
   }

uint16_t
TR_RelocationRecordValidateClassByName::classID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateClassByNameBinaryTemplate *)_record)->_classID);
   }

void
TR_RelocationRecordValidateClassByName::setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID)
   {
   reloTarget->storeUnsigned16b(beholderID, (uint8_t *) &((TR_RelocationRecordValidateClassByNameBinaryTemplate *)_record)->_beholderID);
   }

uint16_t
TR_RelocationRecordValidateClassByName::beholderID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateClassByNameBinaryTemplate *)_record)->_beholderID);
   }

void
TR_RelocationRecordValidateClassByName::setClassChainOffset(TR_RelocationTarget *reloTarget, uintptr_t classChainOffset)
   {
   return reloTarget->storeRelocationRecordValue(classChainOffset, (uintptr_t *) &((TR_RelocationRecordValidateClassByNameBinaryTemplate *)_record)->_classChainOffsetInSCC);
   }

uintptr_t
TR_RelocationRecordValidateClassByName::classChainOffset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordValidateClassByNameBinaryTemplate *)_record)->_classChainOffsetInSCC);
   }

int32_t
TR_RelocationRecordValidateProfiledClass::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = this->classID(reloTarget);

   uintptr_t classChainForCLOffset = this->classChainOffsetForClassLoader(reloTarget);
   void *classChainForCL = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainForCLOffset);

   uintptr_t classChainOffset = this->classChainOffset(reloTarget);
   void *classChain = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainOffset);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateProfiledClassRecord(classID, classChainForCL, classChain))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateProfiledClass::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tclassID %d\n", classID(reloTarget));
   reloLogger->printf("\tclassChainForCL %p\n", (void *)classChainOffsetForClassLoader(reloTarget));
   reloLogger->printf("\tclassChain %p\n", (void *)classChainOffset(reloTarget));
   }

void
TR_RelocationRecordValidateProfiledClass::setClassID(TR_RelocationTarget *reloTarget, uint16_t classID)
   {
   reloTarget->storeUnsigned16b(classID, (uint8_t *) &((TR_RelocationRecordValidateProfiledClassBinaryTemplate *)_record)->_classID);
   }

uint16_t
TR_RelocationRecordValidateProfiledClass::classID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateProfiledClassBinaryTemplate *)_record)->_classID);
   }

void
TR_RelocationRecordValidateProfiledClass::setClassChainOffset(TR_RelocationTarget *reloTarget, uintptr_t classChainOffset)
   {
   reloTarget->storeRelocationRecordValue(classChainOffset, (uintptr_t *) &((TR_RelocationRecordValidateProfiledClassBinaryTemplate *)_record)->_classChainOffsetInSCC);
   }

uintptr_t
TR_RelocationRecordValidateProfiledClass::classChainOffset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordValidateProfiledClassBinaryTemplate *)_record)->_classChainOffsetInSCC);
   }

void
TR_RelocationRecordValidateProfiledClass::setClassChainOffsetForClassLoader(TR_RelocationTarget *reloTarget, uintptr_t classChainOffsetForCL)
   {
   reloTarget->storeRelocationRecordValue(classChainOffsetForCL, (uintptr_t *) &((TR_RelocationRecordValidateProfiledClassBinaryTemplate *)_record)->_classChainOffsetForCLInScc);
   }

uintptr_t
TR_RelocationRecordValidateProfiledClass::classChainOffsetForClassLoader(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordValidateProfiledClassBinaryTemplate *)_record)->_classChainOffsetForCLInScc);
   }

int32_t
TR_RelocationRecordValidateClassFromCP::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = this->classID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   uint32_t cpIndex = this->cpIndex(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassFromCPRecord(classID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateClassFromCP::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tclassID %d\n", classID(reloTarget));
   reloLogger->printf("\tbeholderID %d\n", beholderID(reloTarget));
   reloLogger->printf("\tcpindex %d\n", cpIndex(reloTarget));
   }

void
TR_RelocationRecordValidateClassFromCP::setClassID(TR_RelocationTarget *reloTarget, uint16_t classID)
   {
   reloTarget->storeUnsigned16b(classID, (uint8_t *) &((TR_RelocationRecordValidateClassFromCPBinaryTemplate *)_record)->_classID);
   }

uint16_t
TR_RelocationRecordValidateClassFromCP::classID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateClassFromCPBinaryTemplate *)_record)->_classID);
   }

void
TR_RelocationRecordValidateClassFromCP::setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID)
   {
   reloTarget->storeUnsigned16b(beholderID, (uint8_t *) &((TR_RelocationRecordValidateClassFromCPBinaryTemplate *)_record)->_beholderID);
   }

uint16_t
TR_RelocationRecordValidateClassFromCP::beholderID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateClassFromCPBinaryTemplate *)_record)->_beholderID);
   }

void
TR_RelocationRecordValidateClassFromCP::setCpIndex(TR_RelocationTarget *reloTarget, uint32_t cpIndex)
   {
   reloTarget->storeUnsigned32b(cpIndex, (uint8_t *) &((TR_RelocationRecordValidateClassFromCPBinaryTemplate *)_record)->_cpIndex);
   }

uint32_t
TR_RelocationRecordValidateClassFromCP::cpIndex(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned32b((uint8_t *) &((TR_RelocationRecordValidateClassFromCPBinaryTemplate *)_record)->_cpIndex);
   }

int32_t
TR_RelocationRecordValidateDefiningClassFromCP::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = this->classID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   uint32_t cpIndex = this->cpIndex(reloTarget);
   bool isStatic = this->isStatic(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateDefiningClassFromCPRecord(classID, beholderID, cpIndex, isStatic))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateDefiningClassFromCP::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tisStatic %s\n", isStatic(reloTarget) ? "true" : "false");
   reloLogger->printf("\tclassID %d\n", classID(reloTarget));
   reloLogger->printf("\tbeholderID %d\n", beholderID(reloTarget));
   reloLogger->printf("\tcpindex %d\n", cpIndex(reloTarget));
   }

void
TR_RelocationRecordValidateDefiningClassFromCP::setIsStatic(TR_RelocationTarget *reloTarget, bool isStatic)
   {
   reloTarget->storeUnsigned8b((uint8_t)isStatic, (uint8_t *) &((TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate *)_record)->_isStatic);
   }

bool
TR_RelocationRecordValidateDefiningClassFromCP::isStatic(TR_RelocationTarget *reloTarget)
   {
   return (bool)reloTarget->loadUnsigned8b((uint8_t *) &((TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate *)_record)->_isStatic);
   }

void
TR_RelocationRecordValidateDefiningClassFromCP::setClassID(TR_RelocationTarget *reloTarget, uint16_t classID)
   {
   reloTarget->storeUnsigned16b(classID, (uint8_t *) &((TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate *)_record)->_classID);
   }

uint16_t
TR_RelocationRecordValidateDefiningClassFromCP::classID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate *)_record)->_classID);
   }

void
TR_RelocationRecordValidateDefiningClassFromCP::setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID)
   {
   reloTarget->storeUnsigned16b(beholderID, (uint8_t *) &((TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate *)_record)->_beholderID);
   }

uint16_t
TR_RelocationRecordValidateDefiningClassFromCP::beholderID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate *)_record)->_beholderID);
   }

void
TR_RelocationRecordValidateDefiningClassFromCP::setCpIndex(TR_RelocationTarget *reloTarget, uint32_t cpIndex)
   {
   reloTarget->storeUnsigned32b(cpIndex, (uint8_t *) &((TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate *)_record)->_cpIndex);
   }

uint32_t
TR_RelocationRecordValidateDefiningClassFromCP::cpIndex(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned32b((uint8_t *) &((TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate *)_record)->_cpIndex);
   }

int32_t
TR_RelocationRecordValidateStaticClassFromCP::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = this->classID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   uint32_t cpIndex = this->cpIndex(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateStaticClassFromCPRecord(classID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR_RelocationRecordValidateArrayClassFromComponentClass::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t arrayClassID = this->arrayClassID(reloTarget);
   uint16_t componentClassID = this->componentClassID(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateArrayClassFromComponentClassRecord(arrayClassID, componentClassID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateArrayClassFromComponentClass::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tarrayClassID %d\n", arrayClassID(reloTarget));
   reloLogger->printf("\tcomponentClassID %d\n", componentClassID(reloTarget));
   }

void
TR_RelocationRecordValidateArrayClassFromComponentClass::setArrayClassID(TR_RelocationTarget *reloTarget, uint16_t arrayClassID)
   {
   reloTarget->storeUnsigned16b(arrayClassID, (uint8_t *) &((TR_RelocationRecordValidateArrayFromCompBinaryTemplate *)_record)->_arrayClassID);
   }

uint16_t
TR_RelocationRecordValidateArrayClassFromComponentClass::arrayClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateArrayFromCompBinaryTemplate *)_record)->_arrayClassID);
   }

void
TR_RelocationRecordValidateArrayClassFromComponentClass::setComponentClassID(TR_RelocationTarget *reloTarget, uint16_t componentClassID)
   {
   reloTarget->storeUnsigned16b(componentClassID, (uint8_t *) &((TR_RelocationRecordValidateArrayFromCompBinaryTemplate *)_record)->_componentClassID);
   }

uint16_t
TR_RelocationRecordValidateArrayClassFromComponentClass::componentClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateArrayFromCompBinaryTemplate *)_record)->_componentClassID);
   }

int32_t
TR_RelocationRecordValidateSuperClassFromClass::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t superClassID = this->superClassID(reloTarget);
   uint16_t childClassID = this->childClassID(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateSuperClassFromClassRecord(superClassID, childClassID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateSuperClassFromClass::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tsuperClassID %d\n", superClassID(reloTarget));
   reloLogger->printf("\tchildClassID %d\n", childClassID(reloTarget));
   }

void
TR_RelocationRecordValidateSuperClassFromClass::setSuperClassID(TR_RelocationTarget *reloTarget, uint16_t superClassID)
   {
   reloTarget->storeUnsigned16b(superClassID, (uint8_t *) &((TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate *)_record)->_superClassID);
   }

uint16_t
TR_RelocationRecordValidateSuperClassFromClass::superClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate *)_record)->_superClassID);
   }

void
TR_RelocationRecordValidateSuperClassFromClass::setChildClassID(TR_RelocationTarget *reloTarget, uint16_t childClassID)
   {
   reloTarget->storeUnsigned16b(childClassID, (uint8_t *) &((TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate *)_record)->_childClassID);
   }

uint16_t
TR_RelocationRecordValidateSuperClassFromClass::childClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate *)_record)->_childClassID);
   }

int32_t
TR_RelocationRecordValidateClassInstanceOfClass::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classOneID = this->classOneID(reloTarget);
   uint16_t classTwoID = this->classTwoID(reloTarget);
   bool objectTypeIsFixed = this->objectTypeIsFixed(reloTarget);
   bool castTypeIsFixed = this->castTypeIsFixed(reloTarget);
   bool isInstanceOf = this->isInstanceOf(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassInstanceOfClassRecord(classOneID, classTwoID, objectTypeIsFixed, castTypeIsFixed, isInstanceOf))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateClassInstanceOfClass::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tobjectTypeIsFixed %s\n", objectTypeIsFixed(reloTarget) ? "true" : "false");
   reloLogger->printf("\tcastTypeIsFixed %s\n", castTypeIsFixed(reloTarget) ? "true" : "false");
   reloLogger->printf("\tisInstanceOf %s\n", isInstanceOf(reloTarget) ? "true" : "false");
   reloLogger->printf("\tclassOneID %d\n", classOneID(reloTarget));
   reloLogger->printf("\tclassTwoID %d\n", classTwoID(reloTarget));
   }

void
TR_RelocationRecordValidateClassInstanceOfClass::setObjectTypeIsFixed(TR_RelocationTarget *reloTarget, bool objectTypeIsFixed)
   {
   reloTarget->storeUnsigned8b((uint8_t)objectTypeIsFixed, (uint8_t *) &((TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_objectTypeIsFixed);
   }

bool
TR_RelocationRecordValidateClassInstanceOfClass::objectTypeIsFixed(TR_RelocationTarget *reloTarget)
   {
   return (bool)reloTarget->loadUnsigned8b((uint8_t *) &((TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_objectTypeIsFixed);
   }

void
TR_RelocationRecordValidateClassInstanceOfClass::setCastTypeIsFixed(TR_RelocationTarget *reloTarget, bool castTypeIsFixed)
   {
   reloTarget->storeUnsigned8b((uint8_t)castTypeIsFixed, (uint8_t *) &((TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_castTypeIsFixed);
   }

bool
TR_RelocationRecordValidateClassInstanceOfClass::castTypeIsFixed(TR_RelocationTarget *reloTarget)
   {
   return (bool)reloTarget->loadUnsigned8b((uint8_t *) &((TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_castTypeIsFixed);
   }

void
TR_RelocationRecordValidateClassInstanceOfClass::setIsInstanceOf(TR_RelocationTarget *reloTarget, bool isInstanceOf)
   {
   reloTarget->storeUnsigned8b((uint8_t)isInstanceOf, (uint8_t *) &((TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_isInstanceOf);
   }

bool
TR_RelocationRecordValidateClassInstanceOfClass::isInstanceOf(TR_RelocationTarget *reloTarget)
   {
   return (bool)reloTarget->loadUnsigned8b((uint8_t *) &((TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_isInstanceOf);
   }

void
TR_RelocationRecordValidateClassInstanceOfClass::setClassOneID(TR_RelocationTarget *reloTarget, uint16_t classOneID)
   {
   reloTarget->storeUnsigned16b(classOneID, (uint8_t *) &((TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_classOneID);
   }

uint16_t
TR_RelocationRecordValidateClassInstanceOfClass::classOneID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_classOneID);
   }

void
TR_RelocationRecordValidateClassInstanceOfClass::setClassTwoID(TR_RelocationTarget *reloTarget, uint16_t classTwoID)
   {
   reloTarget->storeUnsigned16b(classTwoID, (uint8_t *) &((TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_classTwoID);
   }

uint16_t
TR_RelocationRecordValidateClassInstanceOfClass::classTwoID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *)_record)->_classTwoID);
   }

int32_t
TR_RelocationRecordValidateSystemClassByName::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t systemClassID = this->systemClassID(reloTarget);
   uintptr_t classChainOffset = this->classChainOffset(reloTarget);
   uintptr_t *classChain = (uintptr_t*)reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainOffset);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateSystemClassByNameRecord(systemClassID, classChain))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateSystemClassByName::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tsystemClassID %d\n", systemClassID(reloTarget));
   reloLogger->printf("\tclassChain %p\n", (void *)classChainOffset(reloTarget));
   }

void
TR_RelocationRecordValidateSystemClassByName::setSystemClassID(TR_RelocationTarget *reloTarget, uint16_t systemClassID)
   {
   reloTarget->storeUnsigned16b(systemClassID, (uint8_t *) &((TR_RelocationRecordValidateSystemClassByNameBinaryTemplate *)_record)->_systemClassID);
   }

uint16_t
TR_RelocationRecordValidateSystemClassByName::systemClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateSystemClassByNameBinaryTemplate *)_record)->_systemClassID);
   }

void
TR_RelocationRecordValidateSystemClassByName::setClassChainOffset(TR_RelocationTarget *reloTarget, uintptr_t classChainOffset)
   {
   reloTarget->storeRelocationRecordValue(classChainOffset, (uintptr_t *) &((TR_RelocationRecordValidateSystemClassByNameBinaryTemplate *)_record)->_classChainOffsetInSCC);
   }

uintptr_t
TR_RelocationRecordValidateSystemClassByName::classChainOffset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordValidateSystemClassByNameBinaryTemplate *)_record)->_classChainOffsetInSCC);
   }

int32_t
TR_RelocationRecordValidateClassFromITableIndexCP::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = this->classID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   uint32_t cpIndex = this->cpIndex(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassFromITableIndexCPRecord(classID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR_RelocationRecordValidateDeclaringClassFromFieldOrStatic::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = this->classID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   uint32_t cpIndex = this->cpIndex(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateDeclaringClassFromFieldOrStaticRecord(classID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR_RelocationRecordValidateConcreteSubClassFromClass::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t superClassID = this->superClassID(reloTarget);
   uint16_t childClassID = this->childClassID(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateConcreteSubClassFromClassRecord(childClassID, superClassID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR_RelocationRecordValidateClassChain::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = this->classID(reloTarget);
   uintptr_t classChainOffset = this->classChainOffset(reloTarget);
   void *classChain = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainOffset);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassChainRecord(classID, classChain))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateClassChain::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tclassID %d\n", classID(reloTarget));
   reloLogger->printf("\tclassChain %p\n", (void *)classChainOffset(reloTarget));
   }

void
TR_RelocationRecordValidateClassChain::setClassID(TR_RelocationTarget *reloTarget, uint16_t classID)
   {
   reloTarget->storeUnsigned16b(classID, (uint8_t *) &((TR_RelocationRecordValidateClassChainBinaryTemplate *)_record)->_classID);
   }

uint16_t
TR_RelocationRecordValidateClassChain::classID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateClassChainBinaryTemplate *)_record)->_classID);
   }

void
TR_RelocationRecordValidateClassChain::setClassChainOffset(TR_RelocationTarget *reloTarget, uintptr_t classChainOffset)
   {
   reloTarget->storeRelocationRecordValue(classChainOffset, (uintptr_t *) &((TR_RelocationRecordValidateClassChainBinaryTemplate *)_record)->_classChainOffsetInSCC);
   }

uintptr_t
TR_RelocationRecordValidateClassChain::classChainOffset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordValidateClassChainBinaryTemplate *)_record)->_classChainOffsetInSCC);
   }

int32_t
TR_RelocationRecordValidateMethodFromClass::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = this->methodID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   uint32_t index = this->index(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateMethodFromClassRecord(methodID, beholderID, index))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateMethodFromClass::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tmethodID %d\n", methodID(reloTarget));
   reloLogger->printf("\tbeholderID %d\n", beholderID(reloTarget));
   reloLogger->printf("\tindex %d\n", index(reloTarget));
   }

void
TR_RelocationRecordValidateMethodFromClass::setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID)
   {
   reloTarget->storeUnsigned16b(methodID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromClassBinaryTemplate *)_record)->_methodID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromClass::methodID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromClassBinaryTemplate *)_record)->_methodID);
   }

void
TR_RelocationRecordValidateMethodFromClass::setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID)
   {
   reloTarget->storeUnsigned16b(beholderID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromClassBinaryTemplate *)_record)->_beholderID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromClass::beholderID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromClassBinaryTemplate *)_record)->_beholderID);
   }

void
TR_RelocationRecordValidateMethodFromClass::setIndex(TR_RelocationTarget *reloTarget, uint32_t index)
   {
   reloTarget->storeUnsigned32b(index, (uint8_t *) &((TR_RelocationRecordValidateMethodFromClassBinaryTemplate *)_record)->_index);
   }

uint32_t
TR_RelocationRecordValidateMethodFromClass::index(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned32b((uint8_t *) &((TR_RelocationRecordValidateMethodFromClassBinaryTemplate *)_record)->_index);
   }

void
TR_RelocationRecordValidateMethodFromCP::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tmethodID %d\n", methodID(reloTarget));
   reloLogger->printf("\tdefiningClassID %d\n", definingClassID(reloTarget));
   reloLogger->printf("\tbeholderID %d\n", beholderID(reloTarget));
   reloLogger->printf("\tcpIndex %d\n", cpIndex(reloTarget));
   }

void
TR_RelocationRecordValidateMethodFromCP::setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID)
   {
   reloTarget->storeUnsigned16b(methodID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromCPBinaryTemplate *)_record)->_methodID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromCP::methodID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromCPBinaryTemplate *)_record)->_methodID);
   }

void
TR_RelocationRecordValidateMethodFromCP::setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID)
   {
   reloTarget->storeUnsigned16b(definingClassID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromCPBinaryTemplate *)_record)->_definingClassID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromCP::definingClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromCPBinaryTemplate *)_record)->_definingClassID);
   }

void
TR_RelocationRecordValidateMethodFromCP::setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID)
   {
   reloTarget->storeUnsigned16b(beholderID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromCPBinaryTemplate *)_record)->_beholderID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromCP::beholderID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromCPBinaryTemplate *)_record)->_beholderID);
   }

void
TR_RelocationRecordValidateMethodFromCP::setCpIndex(TR_RelocationTarget *reloTarget, uint16_t cpIndex)
   {
   reloTarget->storeUnsigned16b(cpIndex, (uint8_t *) &((TR_RelocationRecordValidateMethodFromCPBinaryTemplate *)_record)->_cpIndex);
   }

uint16_t
TR_RelocationRecordValidateMethodFromCP::cpIndex(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromCPBinaryTemplate *)_record)->_cpIndex);
   }

int32_t
TR_RelocationRecordValidateStaticMethodFromCP::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = this->methodID(reloTarget);
   uint16_t definingClassID = this->definingClassID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   uint32_t cpIndex = this->cpIndex(reloTarget);

   if ((reloFlags(reloTarget) & TR_VALIDATE_STATIC_OR_SPECIAL_METHOD_FROM_CP_IS_SPLIT) != 0)
      cpIndex |= J9_STATIC_SPLIT_TABLE_INDEX_FLAG;

   if (reloRuntime->comp()->getSymbolValidationManager()->validateStaticMethodFromCPRecord(methodID, definingClassID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR_RelocationRecordValidateSpecialMethodFromCP::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = this->methodID(reloTarget);
   uint16_t definingClassID = this->definingClassID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   uint32_t cpIndex = this->cpIndex(reloTarget);

   if ((reloFlags(reloTarget) & TR_VALIDATE_STATIC_OR_SPECIAL_METHOD_FROM_CP_IS_SPLIT) != 0)
      cpIndex |= J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG;

   if (reloRuntime->comp()->getSymbolValidationManager()->validateSpecialMethodFromCPRecord(methodID, definingClassID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR_RelocationRecordValidateVirtualMethodFromCP::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = this->methodID(reloTarget);
   uint16_t definingClassID = this->definingClassID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   uint32_t cpIndex = this->cpIndex(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateVirtualMethodFromCPRecord(methodID, definingClassID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR_RelocationRecordValidateVirtualMethodFromOffset::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = this->methodID(reloTarget);
   uint16_t definingClassID = this->definingClassID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   uint16_t virtualCallOffsetAndIgnoreRtResolve = this->virtualCallOffsetAndIgnoreRtResolve(reloTarget);
   int32_t virtualCallOffset = (int32_t)(int16_t)(virtualCallOffsetAndIgnoreRtResolve & ~1);
   bool ignoreRtResolve = (virtualCallOffsetAndIgnoreRtResolve & 1) != 0;

   if (reloRuntime->comp()->getSymbolValidationManager()->validateVirtualMethodFromOffsetRecord(methodID, definingClassID, beholderID, virtualCallOffset, ignoreRtResolve))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateVirtualMethodFromOffset::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);

   uint16_t virtualCallOffsetAndIgnoreRtResolve = this->virtualCallOffsetAndIgnoreRtResolve(reloTarget);
   int32_t virtualCallOffset = (int32_t)(int16_t)(virtualCallOffsetAndIgnoreRtResolve & ~1);
   bool ignoreRtResolve = (virtualCallOffsetAndIgnoreRtResolve & 1) != 0;

   reloLogger->printf("\tmethodID %d\n", methodID(reloTarget));
   reloLogger->printf("\tdefiningClassID %d\n", definingClassID(reloTarget));
   reloLogger->printf("\tbeholderID %d\n", beholderID(reloTarget));
   reloLogger->printf("\tvirtualCallOffset %d\n", virtualCallOffset);
   reloLogger->printf("\tignoreRtResolve %s\n", ignoreRtResolve ? "true" : "false");
   }

void
TR_RelocationRecordValidateVirtualMethodFromOffset::setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID)
   {
   reloTarget->storeUnsigned16b(methodID, (uint8_t *) &((TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *)_record)->_methodID);
   }

uint16_t
TR_RelocationRecordValidateVirtualMethodFromOffset::methodID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *)_record)->_methodID);
   }

void
TR_RelocationRecordValidateVirtualMethodFromOffset::setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID)
   {
   reloTarget->storeUnsigned16b(definingClassID, (uint8_t *) &((TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *)_record)->_definingClassID);
   }

uint16_t
TR_RelocationRecordValidateVirtualMethodFromOffset::definingClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *)_record)->_definingClassID);
   }

void
TR_RelocationRecordValidateVirtualMethodFromOffset::setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID)
   {
   reloTarget->storeUnsigned16b(beholderID, (uint8_t *) &((TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *)_record)->_beholderID);
   }

uint16_t
TR_RelocationRecordValidateVirtualMethodFromOffset::beholderID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *)_record)->_beholderID);
   }

void
TR_RelocationRecordValidateVirtualMethodFromOffset::setVirtualCallOffsetAndIgnoreRtResolve(TR_RelocationTarget *reloTarget, uint16_t virtualCallOffsetAndIgnoreRtResolve)
   {
   reloTarget->storeUnsigned16b(virtualCallOffsetAndIgnoreRtResolve, (uint8_t *) &((TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *)_record)->_virtualCallOffsetAndIgnoreRtResolve);
   }

uint16_t
TR_RelocationRecordValidateVirtualMethodFromOffset::virtualCallOffsetAndIgnoreRtResolve(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *)_record)->_virtualCallOffsetAndIgnoreRtResolve);
   }

int32_t
TR_RelocationRecordValidateInterfaceMethodFromCP::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = this->methodID(reloTarget);
   uint16_t definingClassID = this->definingClassID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   uint16_t lookupID = this->lookupID(reloTarget);
   uint32_t cpIndex = this->cpIndex(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateInterfaceMethodFromCPRecord(methodID, definingClassID, beholderID, lookupID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }


void
TR_RelocationRecordValidateInterfaceMethodFromCP::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tmethodID %d\n", methodID(reloTarget));
   reloLogger->printf("\tdefiningClassID %d\n", definingClassID(reloTarget));
   reloLogger->printf("\tbeholderID %d\n", beholderID(reloTarget));
   reloLogger->printf("\tlookupID %d\n", lookupID(reloTarget));
   reloLogger->printf("\tcpIndex %d\n", cpIndex(reloTarget));
   }

void
TR_RelocationRecordValidateInterfaceMethodFromCP::setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID)
   {
   reloTarget->storeUnsigned16b(methodID, (uint8_t *) &((TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_methodID);
   }

uint16_t
TR_RelocationRecordValidateInterfaceMethodFromCP::methodID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_methodID);
   }

void
TR_RelocationRecordValidateInterfaceMethodFromCP::setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID)
   {
   reloTarget->storeUnsigned16b(definingClassID, (uint8_t *) &((TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_definingClassID);
   }

uint16_t
TR_RelocationRecordValidateInterfaceMethodFromCP::definingClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_definingClassID);
   }

void
TR_RelocationRecordValidateInterfaceMethodFromCP::setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID)
   {
   reloTarget->storeUnsigned16b(beholderID, (uint8_t *) &((TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_beholderID);
   }

uint16_t
TR_RelocationRecordValidateInterfaceMethodFromCP::beholderID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_beholderID);
   }

void
TR_RelocationRecordValidateInterfaceMethodFromCP::setLookupID(TR_RelocationTarget *reloTarget, uint16_t lookupID)
   {
   reloTarget->storeUnsigned16b(lookupID, (uint8_t *) &((TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_lookupID);
   }

uint16_t
TR_RelocationRecordValidateInterfaceMethodFromCP::lookupID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_lookupID);
   }

void
TR_RelocationRecordValidateInterfaceMethodFromCP::setCpIndex(TR_RelocationTarget *reloTarget, uint16_t cpIndex)
   {
   reloTarget->storeUnsigned16b(cpIndex, (uint8_t *) &((TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_cpIndex);
   }

uint16_t
TR_RelocationRecordValidateInterfaceMethodFromCP::cpIndex(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *)_record)->_cpIndex);
   }

int32_t
TR_RelocationRecordValidateImproperInterfaceMethodFromCP::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = this->methodID(reloTarget);
   uint16_t definingClassID = this->definingClassID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);
   int32_t cpIndex = this->cpIndex(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateImproperInterfaceMethodFromCPRecord(methodID, definingClassID, beholderID, cpIndex))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

int32_t
TR_RelocationRecordValidateMethodFromClassAndSig::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = this->methodID(reloTarget);
   uint16_t definingClassID = this->definingClassID(reloTarget);
   uint16_t lookupClassID = this->lookupClassID(reloTarget);
   uint16_t beholderID = this->beholderID(reloTarget);

   uintptr_t romMethodOffset = this->romMethodOffsetInSCC(reloTarget);
   J9ROMMethod *romMethod = reloRuntime->fej9()->sharedCache()->romMethodFromOffsetInSharedCache(romMethodOffset);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateMethodFromClassAndSignatureRecord(methodID, definingClassID, lookupClassID, beholderID, romMethod))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateMethodFromClassAndSig::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tmethodID %d\n", methodID(reloTarget));
   reloLogger->printf("\tdefiningClassID %d\n", definingClassID(reloTarget));
   reloLogger->printf("\tbeholderID %d\n", beholderID(reloTarget));
   reloLogger->printf("\tlookupClassID %d\n", lookupClassID(reloTarget));
   reloLogger->printf("\tromMethodOffsetInSCC %p\n", (void *)romMethodOffsetInSCC(reloTarget));
   }

void
TR_RelocationRecordValidateMethodFromClassAndSig::setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID)
   {
   reloTarget->storeUnsigned16b(methodID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_methodID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromClassAndSig::methodID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_methodID);
   }

void
TR_RelocationRecordValidateMethodFromClassAndSig::setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID)
   {
   reloTarget->storeUnsigned16b(definingClassID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_definingClassID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromClassAndSig::definingClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_definingClassID);
   }

void
TR_RelocationRecordValidateMethodFromClassAndSig::setLookupClassID(TR_RelocationTarget *reloTarget, uint16_t lookupClassID)
   {
   reloTarget->storeUnsigned16b(lookupClassID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_lookupClassID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromClassAndSig::lookupClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_lookupClassID);
   }

void
TR_RelocationRecordValidateMethodFromClassAndSig::setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID)
   {
   reloTarget->storeUnsigned16b(beholderID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_beholderID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromClassAndSig::beholderID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_beholderID);
   }

void
TR_RelocationRecordValidateMethodFromClassAndSig::setRomMethodOffsetInSCC(TR_RelocationTarget *reloTarget, uintptr_t romMethodOffsetInSCC)
   {
   reloTarget->storeRelocationRecordValue(romMethodOffsetInSCC, (uintptr_t *) &((TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_romMethodOffsetInSCC);
   }

uintptr_t
TR_RelocationRecordValidateMethodFromClassAndSig::romMethodOffsetInSCC(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *)_record)->_romMethodOffsetInSCC);
   }

int32_t
TR_RelocationRecordValidateStackWalkerMaySkipFrames::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = this->methodID(reloTarget);
   uint16_t methodClassID = this->methodClassID(reloTarget);
   bool skipFrames = this->skipFrames(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateStackWalkerMaySkipFramesRecord(methodID, methodClassID, skipFrames))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateStackWalkerMaySkipFrames::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tmethodID %d\n", methodID(reloTarget));
   reloLogger->printf("\tmethodClassID %d\n", methodClassID(reloTarget));
   reloLogger->printf("\tskipFrames %s\n", skipFrames(reloTarget) ? "true" : "false");
   }

void
TR_RelocationRecordValidateStackWalkerMaySkipFrames::setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID)
   {
   reloTarget->storeUnsigned16b(methodID, (uint8_t *) &((TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *)_record)->_methodID);
   }

uint16_t
TR_RelocationRecordValidateStackWalkerMaySkipFrames::methodID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *)_record)->_methodID);
   }

void
TR_RelocationRecordValidateStackWalkerMaySkipFrames::setMethodClassID(TR_RelocationTarget *reloTarget, uint16_t methodClassID)
   {
   reloTarget->storeUnsigned16b(methodClassID, (uint8_t *) &((TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *)_record)->_methodClassID);
   }

uint16_t
TR_RelocationRecordValidateStackWalkerMaySkipFrames::methodClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *)_record)->_methodClassID);
   }

void
TR_RelocationRecordValidateStackWalkerMaySkipFrames::setSkipFrames(TR_RelocationTarget *reloTarget, bool skipFrames)
   {
   reloTarget->storeUnsigned8b((uint8_t)skipFrames, (uint8_t *) &((TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *)_record)->_skipFrames);
   }

bool
TR_RelocationRecordValidateStackWalkerMaySkipFrames::skipFrames(TR_RelocationTarget *reloTarget)
   {
   return (bool)reloTarget->loadUnsigned8b((uint8_t *) &((TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *)_record)->_skipFrames);
   }

int32_t
TR_RelocationRecordValidateClassInfoIsInitialized::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t classID = this->classID(reloTarget);
   bool wasInitialized = this->isInitialized(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateClassInfoIsInitializedRecord(classID, wasInitialized))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateClassInfoIsInitialized::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tclassID %d\n", classID(reloTarget));
   reloLogger->printf("\tisInitialized %s\n", isInitialized(reloTarget) ? "true" : "false");
   }

void
TR_RelocationRecordValidateClassInfoIsInitialized::setClassID(TR_RelocationTarget *reloTarget, uint16_t classID)
   {
   reloTarget->storeUnsigned16b(classID, (uint8_t *) &((TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate *)_record)->_classID);
   }

uint16_t
TR_RelocationRecordValidateClassInfoIsInitialized::classID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate *)_record)->_classID);
   }

void
TR_RelocationRecordValidateClassInfoIsInitialized::setIsInitialized(TR_RelocationTarget *reloTarget, bool isInitialized)
   {
   reloTarget->storeUnsigned8b((uint8_t)isInitialized, (uint8_t *) &((TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate *)_record)->_isInitialized);
   }

bool
TR_RelocationRecordValidateClassInfoIsInitialized::isInitialized(TR_RelocationTarget *reloTarget)
   {
   return (bool)reloTarget->loadUnsigned8b((uint8_t *) &((TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate *)_record)->_isInitialized);
   }

int32_t
TR_RelocationRecordValidateMethodFromSingleImpl::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = this->methodID(reloTarget);
   uint16_t definingClassID = this->definingClassID(reloTarget);
   uint16_t thisClassID = this->thisClassID(reloTarget);
   int32_t cpIndexOrVftSlot = this->cpIndexOrVftSlot(reloTarget);
   uint16_t callerMethodID = this->callerMethodID(reloTarget);
   TR_YesNoMaybe useGetResolvedInterfaceMethod = this->useGetResolvedInterfaceMethod(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateMethodFromSingleImplementerRecord(methodID,
                                                                                                    definingClassID,
                                                                                                    thisClassID,
                                                                                                    cpIndexOrVftSlot,
                                                                                                    callerMethodID,
                                                                                                    useGetResolvedInterfaceMethod))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateMethodFromSingleImpl::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);

   const char *yesnomaybe;
   TR_YesNoMaybe useGetResolvedInterfaceMethod = this->useGetResolvedInterfaceMethod(reloTarget);
   if (useGetResolvedInterfaceMethod == TR_yes)
      yesnomaybe = "TR_yes";
   else if (useGetResolvedInterfaceMethod == TR_no)
      yesnomaybe = "TR_no";
   else if (useGetResolvedInterfaceMethod == TR_maybe)
      yesnomaybe = "TR_maybe";
   else
      TR_ASSERT_FATAL(false, "Unknown TR_YesNoMaybe %d\n", useGetResolvedInterfaceMethod);

   reloLogger->printf("\tmethodID %d\n", methodID(reloTarget));
   reloLogger->printf("\tdefiningClassID %d\n", definingClassID(reloTarget));
   reloLogger->printf("\tthisClassID %d\n", thisClassID(reloTarget));
   reloLogger->printf("\tcpIndexOrVftSlot %d\n", cpIndexOrVftSlot(reloTarget));
   reloLogger->printf("\tcallerMethodID %d\n", callerMethodID(reloTarget));
   reloLogger->printf("\tuseGetResolvedInterfaceMethod %s\n", yesnomaybe);
   }

void
TR_RelocationRecordValidateMethodFromSingleImpl::setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID)
   {
   reloTarget->storeUnsigned16b(methodID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_methodID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromSingleImpl::methodID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_methodID);
   }

void
TR_RelocationRecordValidateMethodFromSingleImpl::setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID)
   {
   reloTarget->storeUnsigned16b(definingClassID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_definingClassID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromSingleImpl::definingClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_definingClassID);
   }

void
TR_RelocationRecordValidateMethodFromSingleImpl::setThisClassID(TR_RelocationTarget *reloTarget, uint16_t thisClassID)
   {
   reloTarget->storeUnsigned16b(thisClassID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_thisClassID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromSingleImpl::thisClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_thisClassID);
   }

void
TR_RelocationRecordValidateMethodFromSingleImpl::setCpIndexOrVftSlot(TR_RelocationTarget *reloTarget, int32_t cpIndexOrVftSlot)
   {
   reloTarget->storeSigned32b(cpIndexOrVftSlot, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_cpIndexOrVftSlot);
   }

int32_t
TR_RelocationRecordValidateMethodFromSingleImpl::cpIndexOrVftSlot(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadSigned32b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_cpIndexOrVftSlot);
   }

void
TR_RelocationRecordValidateMethodFromSingleImpl::setCallerMethodID(TR_RelocationTarget *reloTarget, uint16_t callerMethodID)
   {
   reloTarget->storeUnsigned16b(callerMethodID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_callerMethodID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromSingleImpl::callerMethodID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_callerMethodID);
   }

void
TR_RelocationRecordValidateMethodFromSingleImpl::setUseGetResolvedInterfaceMethod(TR_RelocationTarget *reloTarget, TR_YesNoMaybe useGetResolvedInterfaceMethod)
   {
   reloTarget->storeUnsigned16b((uint16_t)useGetResolvedInterfaceMethod, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_useGetResolvedInterfaceMethod);
   }

TR_YesNoMaybe
TR_RelocationRecordValidateMethodFromSingleImpl::useGetResolvedInterfaceMethod(TR_RelocationTarget *reloTarget)
   {
   return (TR_YesNoMaybe)reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *)_record)->_useGetResolvedInterfaceMethod);
   }

int32_t
TR_RelocationRecordValidateMethodFromSingleInterfaceImpl::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = this->methodID(reloTarget);
   uint16_t definingClassID = this->definingClassID(reloTarget);
   uint16_t thisClassID = this->thisClassID(reloTarget);
   int32_t cpIndex = (int32_t)this->cpIndex(reloTarget);
   uint16_t callerMethodID = this->callerMethodID(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateMethodFromSingleInterfaceImplementerRecord(methodID,
                                                                                                             definingClassID,
                                                                                                             thisClassID,
                                                                                                             cpIndex,
                                                                                                             callerMethodID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateMethodFromSingleInterfaceImpl::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tmethodID %d\n", methodID(reloTarget));
   reloLogger->printf("\tdefiningClassID %d\n", definingClassID(reloTarget));
   reloLogger->printf("\tthisClassID %d\n", thisClassID(reloTarget));
   reloLogger->printf("\tcallerMethodID %d\n", callerMethodID(reloTarget));
   reloLogger->printf("\tcpIndex %d\n", cpIndex(reloTarget));
   }

void
TR_RelocationRecordValidateMethodFromSingleInterfaceImpl::setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID)
   {
   reloTarget->storeUnsigned16b(methodID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_methodID);
   }


uint16_t
TR_RelocationRecordValidateMethodFromSingleInterfaceImpl::methodID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_methodID);
   }

void
TR_RelocationRecordValidateMethodFromSingleInterfaceImpl::setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID)
   {
   reloTarget->storeUnsigned16b(definingClassID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_definingClassID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromSingleInterfaceImpl::definingClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_definingClassID);
   }

void
TR_RelocationRecordValidateMethodFromSingleInterfaceImpl::setThisClassID(TR_RelocationTarget *reloTarget, uint16_t thisClassID)
   {
   reloTarget->storeUnsigned16b(thisClassID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_thisClassID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromSingleInterfaceImpl::thisClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_thisClassID);
   }

void
TR_RelocationRecordValidateMethodFromSingleInterfaceImpl::setCallerMethodID(TR_RelocationTarget *reloTarget, uint16_t callerMethodID)
   {
   reloTarget->storeUnsigned16b(callerMethodID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_callerMethodID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromSingleInterfaceImpl::callerMethodID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_callerMethodID);
   }

void
TR_RelocationRecordValidateMethodFromSingleInterfaceImpl::setCpIndex(TR_RelocationTarget *reloTarget, uint16_t cpIndex)
   {
   reloTarget->storeUnsigned16b(cpIndex, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_cpIndex);
   }

uint16_t
TR_RelocationRecordValidateMethodFromSingleInterfaceImpl::cpIndex(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *)_record)->_cpIndex);
   }

int32_t
TR_RelocationRecordValidateMethodFromSingleAbstractImpl::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint16_t methodID = this->methodID(reloTarget);
   uint16_t definingClassID = this->definingClassID(reloTarget);
   uint16_t thisClassID = this->thisClassID(reloTarget);
   int32_t vftSlot = this->vftSlot(reloTarget);
   uint16_t callerMethodID = this->callerMethodID(reloTarget);

   if (reloRuntime->comp()->getSymbolValidationManager()->validateMethodFromSingleAbstractImplementerRecord(methodID,
                                                                                                            definingClassID,
                                                                                                            thisClassID,
                                                                                                            vftSlot,
                                                                                                            callerMethodID))
      return 0;
   else
      return compilationAotClassReloFailure;
   }

void
TR_RelocationRecordValidateMethodFromSingleAbstractImpl::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tmethodID %d\n", methodID(reloTarget));
   reloLogger->printf("\tdefiningClassID %d\n", definingClassID(reloTarget));
   reloLogger->printf("\tthisClassID %d\n", thisClassID(reloTarget));
   reloLogger->printf("\tcallerMethodID %d\n", callerMethodID(reloTarget));
   reloLogger->printf("\tvftSlot %d\n", vftSlot(reloTarget));
   }

void
TR_RelocationRecordValidateMethodFromSingleAbstractImpl::setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID)
   {
   reloTarget->storeUnsigned16b(methodID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_methodID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromSingleAbstractImpl::methodID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_methodID);
   }

void
TR_RelocationRecordValidateMethodFromSingleAbstractImpl::setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID)
   {
   reloTarget->storeUnsigned16b(definingClassID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_definingClassID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromSingleAbstractImpl::definingClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_definingClassID);
   }

void
TR_RelocationRecordValidateMethodFromSingleAbstractImpl::setThisClassID(TR_RelocationTarget *reloTarget, uint16_t thisClassID)
   {
   reloTarget->storeUnsigned16b(thisClassID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_thisClassID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromSingleAbstractImpl::thisClassID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_thisClassID);
   }

void
TR_RelocationRecordValidateMethodFromSingleAbstractImpl::setVftSlot(TR_RelocationTarget *reloTarget, int32_t vftSlot)
   {
   reloTarget->storeSigned32b(vftSlot, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_vftSlot);
   }

int32_t
TR_RelocationRecordValidateMethodFromSingleAbstractImpl::vftSlot(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadSigned32b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_vftSlot);
   }

void
TR_RelocationRecordValidateMethodFromSingleAbstractImpl::setCallerMethodID(TR_RelocationTarget *reloTarget, uint16_t callerMethodID)
   {
   reloTarget->storeUnsigned16b(callerMethodID, (uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_callerMethodID);
   }

uint16_t
TR_RelocationRecordValidateMethodFromSingleAbstractImpl::callerMethodID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *)_record)->_callerMethodID);
   }

void
TR_RelocationRecordSymbolFromManager::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);

   const char *symType;
   TR::SymbolType symbolType = this->symbolType(reloTarget);
   if (symbolType == TR::SymbolType::typeOpaque)
      symType = "typeOpaque";
   else if (symbolType == TR::SymbolType::typeClass)
      symType = "typeClass";
   else if (symbolType == TR::SymbolType::typeMethod)
      symType = "typeMethod";
   else
      TR_ASSERT_FATAL(false, "Unknown symbolType %d\n", symbolType);

   reloLogger->printf("\tsymbolID %d\n", symbolID(reloTarget));
   reloLogger->printf("\tsymbolType %s\n", symType);
   }

void
TR_RelocationRecordSymbolFromManager::setSymbolID(TR_RelocationTarget *reloTarget, uint16_t symbolID)
   {
   reloTarget->storeUnsigned16b(symbolID, (uint8_t *) &((TR_RelocationRecordSymbolFromManagerBinaryTemplate *)_record)->_symbolID);
   }

uint16_t
TR_RelocationRecordSymbolFromManager::symbolID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordSymbolFromManagerBinaryTemplate *)_record)->_symbolID);
   }

void
TR_RelocationRecordSymbolFromManager::setSymbolType(TR_RelocationTarget *reloTarget, TR::SymbolType symbolType)
   {
   reloTarget->storeUnsigned16b(static_cast<uint16_t>(symbolType), (uint8_t *) &((TR_RelocationRecordSymbolFromManagerBinaryTemplate *)_record)->_symbolType);
   }

TR::SymbolType
TR_RelocationRecordSymbolFromManager::symbolType(TR_RelocationTarget *reloTarget)
   {
   uint16_t type = reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordSymbolFromManagerBinaryTemplate *)_record)->_symbolType);
   return static_cast<TR::SymbolType>(type);
   }

void
TR_RelocationRecordSymbolFromManager::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationSymbolFromManagerPrivateData *reloPrivateData = &(privateData()->symbolFromManager);

   uint16_t symbolID = this->symbolID(reloTarget);
   TR::SymbolType symbolType = this->symbolType(reloTarget);

   reloPrivateData->_symbol = reloRuntime->comp()->getSymbolValidationManager()->getSymbolFromID(symbolID, symbolType);
   reloPrivateData->_symbolType = symbolType;
   }

int32_t
TR_RelocationRecordSymbolFromManager::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationSymbolFromManagerPrivateData *reloPrivateData = &(privateData()->symbolFromManager);

   void *symbol = reloPrivateData->_symbol;

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: symbol %p\n", symbol);
      }

   if (symbol)
      {
      storePointer(reloRuntime, reloTarget, reloLocation);
      activatePointer(reloRuntime, reloTarget, reloLocation);
      }
   else
      {
      return compilationAotClassReloFailure;
      }

   return 0;
   }

bool
TR_RelocationRecordSymbolFromManager::needsUnloadAssumptions(TR::SymbolType symbolType)
   {
   bool needsAssumptions = false;
   switch (symbolType)
      {
      case TR::SymbolType::typeClass:
      case TR::SymbolType::typeMethod:
         needsAssumptions = true;
         break;

      default:
         needsAssumptions = false;
      }
   return needsAssumptions;
   }

bool
TR_RelocationRecordSymbolFromManager::needsRedefinitionAssumption(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation, TR_OpaqueClassBlock *clazz, TR::SymbolType symbolType)
   {
   if (!reloRuntime->options()->getOption(TR_EnableHCR))
      return false;

   bool needsAssumptions = false;
   switch (symbolType)
      {
      case TR::SymbolType::typeClass:
         needsAssumptions =  TR::CodeGenerator::wantToPatchClassPointer(reloRuntime->comp(), clazz, reloLocation);
         break;

      case TR::SymbolType::typeMethod:
         needsAssumptions = true;
         break;

      default:
         needsAssumptions = false;
      }
   return needsAssumptions;
   }

void
TR_RelocationRecordSymbolFromManager::storePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationSymbolFromManagerPrivateData *reloPrivateData = &(privateData()->symbolFromManager);

   reloTarget->storePointer((uint8_t *)reloPrivateData->_symbol, reloLocation);
   }

void
TR_RelocationRecordDiscontiguousSymbolFromManager::storePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationSymbolFromManagerPrivateData *reloPrivateData = &(privateData()->symbolFromManager);

   reloTarget->storeAddressSequence((uint8_t *)reloPrivateData->_symbol, reloLocation, reloFlags(reloTarget));
   }

void
TR_RelocationRecordSymbolFromManager::activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationSymbolFromManagerPrivateData *reloPrivateData = &(privateData()->symbolFromManager);
   TR::SymbolType symbolType = (TR::SymbolType)reloPrivateData->_symbolType;

   TR_OpaqueClassBlock *clazz = NULL;
   if (symbolType == TR::SymbolType::typeClass)
      {
      clazz = (TR_OpaqueClassBlock *)reloPrivateData->_symbol;
      }
   else if (symbolType == TR::SymbolType::typeMethod)
      {
      clazz = (TR_OpaqueClassBlock *)J9_CLASS_FROM_METHOD((J9Method *)(reloPrivateData->_symbol));
      }

   if (needsUnloadAssumptions(symbolType))
      {
      SVM_ASSERT(clazz != NULL, "clazz must exist to add Unload Assumptions!");
      reloTarget->addPICtoPatchPtrOnClassUnload(clazz, reloLocation);
      }
   if (needsRedefinitionAssumption(reloRuntime, reloLocation, clazz, symbolType))
      {
      SVM_ASSERT(clazz != NULL, "clazz must exist to add Redefinition Assumptions!");
      createClassRedefinitionPicSite((void *)reloPrivateData->_symbol, (void *) reloLocation, sizeof(uintptr_t), false, reloRuntime->comp()->getMetadataAssumptionList());
      reloRuntime->comp()->setHasClassRedefinitionAssumptions();
      }
   }

void
TR_RelocationRecordResolvedTrampolines::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecord::print(reloRuntime);
   reloLogger->printf("\tsymbolID %d\n", symbolID(reloTarget));
   }

void
TR_RelocationRecordResolvedTrampolines::setSymbolID(TR_RelocationTarget *reloTarget, uint16_t symbolID)
   {
   reloTarget->storeUnsigned16b(symbolID, (uint8_t *) &((TR_RelocationRecordResolvedTrampolinesBinaryTemplate *)_record)->_symbolID);
   }

uint16_t
TR_RelocationRecordResolvedTrampolines::symbolID(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned16b((uint8_t *) &((TR_RelocationRecordResolvedTrampolinesBinaryTemplate *)_record)->_symbolID);
   }

void
TR_RelocationRecordResolvedTrampolines::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordResolvedTrampolinesPrivateData *reloPrivateData = &(privateData()->resolvedTrampolines);

   uint16_t symbolID = this->symbolID(reloTarget);

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tpreparePrivateData: symbolID %d\n", symbolID);
      }

   reloPrivateData->_method = reloRuntime->comp()->getSymbolValidationManager()->getMethodFromID(symbolID);
   }

int32_t
TR_RelocationRecordResolvedTrampolines::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordResolvedTrampolinesPrivateData *reloPrivateData = &(privateData()->resolvedTrampolines);
   TR_OpaqueMethodBlock *method = reloPrivateData->_method;

   if (reloRuntime->reloLogger()->logEnabled())
      {
      reloRuntime->reloLogger()->printf("%s\n", name());
      reloRuntime->reloLogger()->printf("\tapplyRelocation: method %p\n", method);
      }

   if (reloRuntime->codeCache()->reserveResolvedTrampoline(method, true) != OMR::CodeCacheErrorCode::ERRORCODE_SUCCESS)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: aborting AOT relocation because trampoline was not reserved. Will be retried.\n");
      return compilationAotTrampolineReloFailure;
      }

   return 0;
   }


// TR_HCR
char *
TR_RelocationRecordHCR::name()
   {
   return "TR_HCR";
   }

TR_RelocationRecordAction
TR_RelocationRecordHCR::action(TR_RelocationRuntime *reloRuntime)
   {
   bool hcrEnabled = reloRuntime->options()->getOption(TR_EnableHCR);
   RELO_LOG(reloRuntime->reloLogger(), 6,"\taction: hcrEnabled %d\n", hcrEnabled);
   return hcrEnabled ? TR_RelocationRecordAction::apply : TR_RelocationRecordAction::ignore;
   }

int32_t
TR_RelocationRecordHCR::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   void *methodAddress = (void *)reloRuntime->exceptionTable()->ramMethod;
   if (offset(reloTarget)) // non-NULL means resolved
      createClassRedefinitionPicSite(methodAddress, (void*)reloLocation, sizeof(UDATA), true, getMetadataAssumptionList(reloRuntime->exceptionTable()));
   else
   {
	   uint32_t locationSize = 1; // see OMR::RuntimeAssumption::isForAddressMaterializationSequence
	   if (reloFlags(reloTarget) & needsFullSizeRuntimeAssumption)
		   locationSize = sizeof(uintptr_t);
      createClassRedefinitionPicSite((void*)-1, (void*)reloLocation, locationSize, true, getMetadataAssumptionList(reloRuntime->exceptionTable()));
   }
   return 0;
   }

// TR_Pointer
void
TR_RelocationRecordPointer::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationRecordWithInlinedSiteIndex::print(reloRuntime);

   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   reloLogger->printf("\tclassChainIdentifyingLoaderOffsetInSharedCache %x\n", classChainIdentifyingLoaderOffsetInSharedCache(reloTarget));
   reloLogger->printf("\tclassChainForInlinedMethod %x\n", classChainForInlinedMethod(reloTarget));
   }

TR_RelocationRecordAction
TR_RelocationRecordPointer::action(TR_RelocationRuntime *reloRuntime)
   {
   // pointers must be updated because they tend to be guards controlling entry to inlined code
   // so even if the inlined site is disabled, the guard value needs to be changed to -1
   return TR_RelocationRecordAction::apply;
   }

void
TR_RelocationRecordPointer::setClassChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptr_t classChainIdentifyingLoaderOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(classChainIdentifyingLoaderOffsetInSharedCache, (uintptr_t *) &((TR_RelocationRecordPointerBinaryTemplate *)_record)->_classChainIdentifyingLoaderOffsetInSharedCache);
   }

uintptr_t
TR_RelocationRecordPointer::classChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordPointerBinaryTemplate *)_record)->_classChainIdentifyingLoaderOffsetInSharedCache);
   }

void
TR_RelocationRecordPointer::setClassChainForInlinedMethod(TR_RelocationTarget *reloTarget, uintptr_t classChainForInlinedMethod)
   {
   reloTarget->storeRelocationRecordValue(classChainForInlinedMethod, (uintptr_t *) &((TR_RelocationRecordPointerBinaryTemplate *)_record)->_classChainForInlinedMethod);
   }

uintptr_t
TR_RelocationRecordPointer::classChainForInlinedMethod(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordPointerBinaryTemplate *)_record)->_classChainForInlinedMethod);
   }

void
TR_RelocationRecordPointer::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);

   J9Class *classPointer = NULL;
   if (getInlinedSiteMethod(reloRuntime, inlinedSiteIndex(reloTarget)) != (TR_OpaqueMethodBlock *)-1)
      {
      J9ClassLoader *classLoader = NULL;
      void *classChainIdentifyingLoader = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainIdentifyingLoaderOffsetInSharedCache(reloTarget));
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classChainIdentifyingLoader %p\n", classChainIdentifyingLoader);
      classLoader = (J9ClassLoader *) reloRuntime->fej9()->sharedCache()->persistentClassLoaderTable()->lookupClassLoaderAssociatedWithClassChain(classChainIdentifyingLoader);
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classLoader %p\n", classLoader);

      if (classLoader != NULL)
         {
         uintptr_t *classChain = (uintptr_t *) reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache(classChainForInlinedMethod(reloTarget));
         RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classChain %p\n", classChain);
         classPointer = (J9Class *) reloRuntime->fej9()->sharedCache()->lookupClassFromChainAndLoader(classChain, (void *) classLoader);
         RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classPointer %p\n", classPointer);
         }
      }
   else
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: inlined site invalid\n");

   if (classPointer != NULL)
      {
      reloPrivateData->_activatePointer = true;
      reloPrivateData->_clazz = (TR_OpaqueClassBlock *) classPointer;
      reloPrivateData->_pointer = computePointer(reloTarget, reloPrivateData->_clazz);
      reloPrivateData->_needUnloadAssumption = !reloRuntime->fej9()->sameClassLoaders(reloPrivateData->_clazz, reloRuntime->comp()->getCurrentMethod()->classOfMethod());
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: pointer %p\n", reloPrivateData->_pointer);
      }
   else
      {
      reloPrivateData->_activatePointer = false;
      reloPrivateData->_clazz = (TR_OpaqueClassBlock *) -1;
      reloPrivateData->_pointer = (uintptr_t) -1;
      reloPrivateData->_needUnloadAssumption = false;
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: class or loader NULL, or invalid site\n");
      }
   }

void
TR_RelocationRecordPointer::activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);
   if (reloPrivateData->_needUnloadAssumption)
      {
      reloTarget->addPICtoPatchPtrOnClassUnload(reloPrivateData->_clazz, reloLocation);
      }
   }

void
TR_RelocationRecordPointer::registerHCRAssumption(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation)
   {
   TR_RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);
   createClassRedefinitionPicSite((void *) reloPrivateData->_pointer, (void *) reloLocation, sizeof(uintptr_t), false, reloRuntime->comp()->getMetadataAssumptionList());
   reloRuntime->comp()->setHasClassRedefinitionAssumptions();
   }

int32_t
TR_RelocationRecordPointer::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);
   reloTarget->storePointer((uint8_t *)reloPrivateData->_pointer, reloLocation);
   if (reloPrivateData->_activatePointer)
      activatePointer(reloRuntime, reloTarget, reloLocation);
   return 0;
   }

// TR_ClassPointer
char *
TR_RelocationRecordClassPointer::name()
   {
   return "TR_ClassPointer";
   }

uintptr_t
TR_RelocationRecordClassPointer::computePointer(TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *classPointer)
   {
   return (uintptr_t) classPointer;
   }

void
TR_RelocationRecordClassPointer::activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordPointer::activatePointer(reloRuntime, reloTarget, reloLocation);
   TR_RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);
   TR_ASSERT((void*)reloPrivateData->_pointer == (void*)reloPrivateData->_clazz, "Pointer differs from class pointer");
   if (TR::CodeGenerator::wantToPatchClassPointer(reloRuntime->comp(), reloPrivateData->_clazz, reloLocation))
      registerHCRAssumption(reloRuntime, reloLocation);
   }

// TR_ArbitraryClassAddress
char *
TR_RelocationRecordArbitraryClassAddress::name()
   {
   return "TR_ArbitraryClassAddress";
   }

int32_t
TR_RelocationRecordArbitraryClassAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);
   TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock*)reloPrivateData->_pointer;
   assertBootstrapLoader(reloRuntime, clazz);
   reloTarget->storeAddressSequence((uint8_t*)clazz, reloLocation, reloFlags(reloTarget));
   // No need to activatePointer(). See its definition below.
   return 0;
   }

int32_t
TR_RelocationRecordArbitraryClassAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR_RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);
   TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock*)reloPrivateData->_pointer;
   assertBootstrapLoader(reloRuntime, clazz);
   reloTarget->storeAddress((uint8_t*)clazz, reloLocationHigh, reloLocationLow, reloFlags(reloTarget));
   // No need to activatePointer(). See its definition below.
   return 0;
   }

void
TR_RelocationRecordArbitraryClassAddress::activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   // applyRelocation() for this class doesn't activatePointer(), because it's
   // unnecessary. Class pointers are stable through redefinitions, and this
   // relocation is only valid for classes loaded by the bootstrap loader,
   // which can't be unloaded.
   //
   // This non-implementation is here to ensure that we don't create runtime
   // assumptions that are inappropriate to the code layout, which may be
   // different from the layout expected for other "pointer" relocations.

   TR_ASSERT_FATAL(
      false,
      "TR_RelocationRecordArbitraryClassAddress::activatePointer() is unimplemented\n");
   }

void
TR_RelocationRecordArbitraryClassAddress::assertBootstrapLoader(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *clazz)
   {
   void *loader = reloRuntime->fej9()->getClassLoader(clazz);
   void *bootstrapLoader = reloRuntime->javaVM()->systemClassLoader;
   TR_ASSERT_FATAL(
      loader == bootstrapLoader,
      "TR_ArbitraryClassAddress relocation must use bootstrap loader\n");
   }

// TR_MethodPointer
char *
TR_RelocationRecordMethodPointer::name()
   {
   return "TR_MethodPointer";
   }

void
TR_RelocationRecordMethodPointer::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationRecordPointer::print(reloRuntime);

   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   reloLogger->printf("\tvTableSlot %x\n", vTableSlot(reloTarget));
   }

void
TR_RelocationRecordMethodPointer::setVTableSlot(TR_RelocationTarget *reloTarget, uintptr_t vTableSlot)
   {
   reloTarget->storeRelocationRecordValue(vTableSlot, (uintptr_t *) &((TR_RelocationRecordMethodPointerBinaryTemplate *)_record)->_vTableSlot);
   }

uintptr_t
TR_RelocationRecordMethodPointer::vTableSlot(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &((TR_RelocationRecordMethodPointerBinaryTemplate *)_record)->_vTableSlot);
   }

uintptr_t
TR_RelocationRecordMethodPointer::computePointer(TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *classPointer)
   {
   //return (uintptr_t) getInlinedSiteMethod(reloTarget->reloRuntime(), inlinedSiteIndex(reloTarget));

   J9Method *method = (J9Method *) *(uintptr_t *)(((uint8_t *)classPointer) + vTableSlot(reloTarget));
   TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock *) J9_CLASS_FROM_METHOD(method);
   if (0 && *((uint32_t *)clazz) != 0x99669966)
      {
      TR_RelocationRuntime *reloRuntime = reloTarget->reloRuntime();
      RELO_LOG(reloRuntime->reloLogger(), 7, "\tpreparePrivateData: SUSPICIOUS j9method %p\n", method);
      J9UTF8 *className = J9ROMCLASS_CLASSNAME(((J9Class*)classPointer)->romClass);
      RELO_LOG(reloRuntime->reloLogger(), 7, "\tpreparePrivateData: classPointer %p %.*s\n", classPointer, J9UTF8_LENGTH(className), J9UTF8_DATA(className));
      RELO_LOG(reloRuntime->reloLogger(), 7, "\tpreparePrivateData: method's clazz %p\n", clazz);
      abort();
      }

   return (uintptr_t) method;
   }

void
TR_RelocationRecordMethodPointer::activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordPointer::activatePointer(reloRuntime, reloTarget, reloLocation);
   if (reloRuntime->options()->getOption(TR_EnableHCR))
      registerHCRAssumption(reloRuntime, reloLocation);
   }

void
TR_RelocationRecordEmitClass::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordEmitClassPrivateData *reloPrivateData = &(privateData()->emitClass);

   reloPrivateData->_bcIndex = bcIndex(reloTarget);
   reloPrivateData->_method  = getInlinedSiteMethod(reloRuntime);
   }

int32_t
TR_RelocationRecordEmitClass::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordEmitClassPrivateData *reloPrivateData = &(privateData()->emitClass);

   reloRuntime->addClazzRecord(reloLocation, reloPrivateData->_bcIndex, reloPrivateData->_method);
   return 0;
   }

void
TR_RelocationRecordEmitClass::setBCIndex(TR_RelocationTarget *reloTarget, int32_t bcIndex)
   {
   reloTarget->storeSigned32b(bcIndex, (uint8_t *) &((TR_RelocationRecordEmitClassBinaryTemplate *)_record)->_bcIndex);
   }

int32_t
TR_RelocationRecordEmitClass::bcIndex(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadSigned32b((uint8_t *) &((TR_RelocationRecordEmitClassBinaryTemplate *)_record)->_bcIndex);
   }

char *
TR_RelocationRecordDebugCounter::name()
   {
   return "TR_RelocationRecordDebugCounter";
   }

void
TR_RelocationRecordDebugCounter::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   TR_RelocationRecordWithInlinedSiteIndex::print(reloRuntime);
   reloLogger->printf("\tbcIndex %d\n", bcIndex(reloTarget));
   reloLogger->printf("\tdelta %d\n", delta(reloTarget));
   reloLogger->printf("\tfidelity %d\n", static_cast<uint32_t>(fidelity(reloTarget)));
   reloLogger->printf("\tstaticDelta %d\n", staticDelta(reloTarget));
   reloLogger->printf("\toffsetOfNameString %d\n", reinterpret_cast<void *>(offsetOfNameString(reloTarget)));
   }

void
TR_RelocationRecordDebugCounter::setBCIndex(TR_RelocationTarget *reloTarget, int32_t bcIndex)
   {
   reloTarget->storeSigned32b(bcIndex, (uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_bcIndex));
   }

int32_t
TR_RelocationRecordDebugCounter::bcIndex(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadSigned32b((uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_bcIndex));
   }

void
TR_RelocationRecordDebugCounter::setDelta(TR_RelocationTarget *reloTarget, int32_t delta)
   {
   reloTarget->storeSigned32b(delta, (uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_delta));
   }

int32_t
TR_RelocationRecordDebugCounter::delta(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadSigned32b((uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_delta));
   }

void
TR_RelocationRecordDebugCounter::setFidelity(TR_RelocationTarget *reloTarget, int8_t fidelity)
   {
   reloTarget->storeSigned8b(fidelity, (uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_fidelity));
   }

int8_t
TR_RelocationRecordDebugCounter::fidelity(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadSigned8b((uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_fidelity));
   }

void
TR_RelocationRecordDebugCounter::setStaticDelta(TR_RelocationTarget *reloTarget, int32_t staticDelta)
   {
   reloTarget->storeSigned32b(staticDelta, (uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_staticDelta));
   }

int32_t
TR_RelocationRecordDebugCounter::staticDelta(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadSigned32b((uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_staticDelta));
   }

void
TR_RelocationRecordDebugCounter::setOffsetOfNameString(TR_RelocationTarget *reloTarget, uintptr_t offsetOfNameString)
   {
   reloTarget->storeRelocationRecordValue(offsetOfNameString, (uintptr_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_offsetOfNameString));
   }

uintptr_t
TR_RelocationRecordDebugCounter::offsetOfNameString(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptr_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_offsetOfNameString));
   }

void
TR_RelocationRecordDebugCounter::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordDebugCounterPrivateData *reloPrivateData = &(privateData()->debugCounter);

   IDATA callerIndex = (IDATA)inlinedSiteIndex(reloTarget);
   if (callerIndex != -1)
      {
      reloPrivateData->_method = getInlinedSiteMethod(reloRuntime, callerIndex);
      }
   else
      {
      reloPrivateData->_method = NULL;
      }

   reloPrivateData->_bcIndex     = bcIndex(reloTarget);
   reloPrivateData->_delta       = delta(reloTarget);
   reloPrivateData->_fidelity    = fidelity(reloTarget);
   reloPrivateData->_staticDelta = staticDelta(reloTarget);

   UDATA offset                  = offsetOfNameString(reloTarget);
   reloPrivateData->_name        =  reloRuntime->fej9()->sharedCache()->getDebugCounterName(offset);
   }

int32_t
TR_RelocationRecordDebugCounter::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR::DebugCounterBase *counter = findOrCreateCounter(reloRuntime);
   if (counter == NULL)
      {
      /*
       * We don't have to return -1 here and fail the relocation. We can always just allocate some memory
       * and patch the update location to that. However, given that it's likely that the developer wishes
       * to have debug counters run, it's probably better to fail the relocation.
       *
       */
      return -1;
      }

   // Update Counter Location
   reloTarget->storeAddressSequence((uint8_t *)counter->getBumpCountAddress(), reloLocation, reloFlags(reloTarget));

   return 0;
   }

int32_t
TR_RelocationRecordDebugCounter::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   TR::DebugCounterBase *counter = findOrCreateCounter(reloRuntime);
   if (counter == NULL)
      {
      /*
       * We don't have to return -1 here and fail the relocation. We can always just allocate some memory
       * and patch the update location to that. However, given that it's likely that the developer wishes
       * to have debug counters run, it's probably better to fail the relocation.
       *
       */
      return -1;
      }

   // Update Counter Location
   reloTarget->storeAddress((uint8_t *)counter->getBumpCountAddress(), reloLocationHigh, reloLocationLow, reloFlags(reloTarget));

   return 0;
   }

TR::DebugCounterBase *
TR_RelocationRecordDebugCounter::findOrCreateCounter(TR_RelocationRuntime *reloRuntime)
   {
   TR::DebugCounterBase *counter = NULL;
   TR_RelocationRecordDebugCounterPrivateData *reloPrivateData = &(privateData()->debugCounter);
   TR::Compilation *comp = reloRuntime->comp();
   bool isAggregateCounter = reloPrivateData->_delta == 0 ? false : true;

   if (reloPrivateData->_name == NULL ||
       (isAggregateCounter && reloPrivateData->_method == (TR_OpaqueMethodBlock *)-1))
      {
      return NULL;
      }

   // Find or Create Debug Counter
   if (isAggregateCounter)
      {
      counter = comp->getPersistentInfo()->getDynamicCounters()->findAggregation(reloPrivateData->_name, strlen(reloPrivateData->_name));
      if (!counter)
         {
         TR::DebugCounterAggregation *aggregatedCounters = comp->getPersistentInfo()->getDynamicCounters()->createAggregation(comp, reloPrivateData->_name);
         if (aggregatedCounters)
            {
            aggregatedCounters->aggregateStandardCounters(comp,
                                                          reloPrivateData->_method,
                                                          reloPrivateData->_bcIndex,
                                                          reloPrivateData->_name,
                                                          reloPrivateData->_delta,
                                                          reloPrivateData->_fidelity,
                                                          reloPrivateData->_staticDelta);
            if (!aggregatedCounters->hasAnyCounters())
               return NULL;
            }
         counter = aggregatedCounters;
         }
      }
   else
      {
      counter = TR::DebugCounter::getDebugCounter(comp,
                                                  reloPrivateData->_name,
                                                  reloPrivateData->_fidelity,
                                                  reloPrivateData->_staticDelta);
      }

   return counter;
   }

// ClassUnloadAssumption
char *
TR_RelocationRecordClassUnloadAssumption::name()
   {
   return "TR_ClassUnloadAssumption";
   }

int32_t
TR_RelocationRecordClassUnloadAssumption::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   reloTarget->addPICtoPatchPtrOnClassUnload((TR_OpaqueClassBlock *) -1, reloLocation);
   return 0;
   }

uint8_t *
TR_RelocationRecordMethodCallAddress::computeTargetMethodAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *baseLocation)
   {
   uint8_t *callTargetAddress = address(reloTarget);

   if (reloRuntime->options()->getOption(TR_StressTrampolines) || reloTarget->useTrampoline(callTargetAddress, baseLocation))
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tredirecting call to " POINTER_PRINTF_FORMAT " through trampoline\n", callTargetAddress);
      auto metadata = jitGetExceptionTableFromPC(reloRuntime->currentThread(), (UDATA)callTargetAddress);
      auto j9method = metadata->ramMethod;
      TR::VMAccessCriticalSection computeTargetMethodAddress(reloRuntime->fej9());
      // getResolvedTrampoline will create the trampoline if it doesn't exist, but we pass inBinaryEncoding=true because we want the compilation to fail
      // if the trampoline can't be allocated in the current code cache rather than switching to a new code cache, which can't be done during relocation.
      auto codeCache = reloRuntime->fej9()->getResolvedTrampoline(reloRuntime->comp(), reloRuntime->codeCache(), j9method, /* inBinaryEncoding */ true);
      callTargetAddress = reinterpret_cast<uint8_t *>(codeCache->findTrampoline((TR_OpaqueMethodBlock *)j9method));
      }

   return callTargetAddress;
   }

uint8_t*
TR_RelocationRecordMethodCallAddress::address(TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordMethodCallAddressBinaryTemplate *reloData = (TR_RelocationRecordMethodCallAddressBinaryTemplate *)_record;
   return reloTarget->loadAddress(reinterpret_cast<uint8_t *>(&reloData->_methodAddress));
   }

void
TR_RelocationRecordMethodCallAddress::setAddress(TR_RelocationTarget *reloTarget, uint8_t *callTargetAddress)
   {
   TR_RelocationRecordMethodCallAddressBinaryTemplate *reloData = (TR_RelocationRecordMethodCallAddressBinaryTemplate *)_record;
   reloTarget->storeAddress(callTargetAddress, reinterpret_cast<uint8_t *>(&reloData->_methodAddress));
   }

int32_t TR_RelocationRecordMethodCallAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint8_t *baseLocation = 0;
   if (eipRelative(reloTarget))
      {
      baseLocation = reloTarget->eipBaseForCallOffset(reloLocation);
      RELO_LOG(reloRuntime->reloLogger(), 6, "\teip-relative, adjusted location to " POINTER_PRINTF_FORMAT "\n", baseLocation);
      }

   uint8_t *callTargetAddress = computeTargetMethodAddress(reloRuntime, reloTarget, baseLocation);
   uint8_t *callTargetOffset = reinterpret_cast<uint8_t *>(callTargetAddress - baseLocation);
   RELO_LOG(reloRuntime->reloLogger(), 6,
            "\t\tapplyRelocation: reloLocation " POINTER_PRINTF_FORMAT " baseLocation " POINTER_PRINTF_FORMAT " callTargetAddress " POINTER_PRINTF_FORMAT " callTargetOffset %x\n",
            reloLocation, baseLocation, callTargetAddress, callTargetOffset);

   if (eipRelative(reloTarget))
      reloTarget->storeRelativeTarget(reinterpret_cast<uintptr_t>(callTargetOffset), reloLocation);
   else
      reloTarget->storeAddress(callTargetOffset, reloLocation);

   return 0;
   }

uint32_t TR_RelocationRecord::_relocationRecordHeaderSizeTable[TR_NumExternalRelocationKinds] =
   {
   sizeof(TR_RelocationRecordConstantPoolBinaryTemplate),                            // TR_ConstantPool                                 = 0
   sizeof(TR_RelocationRecordHelperAddressBinaryTemplate),                           // TR_HelperAddress                                = 1
   sizeof(TR_RelocationRecordBinaryTemplate),                                        // TR_RelativeMethodAddress                        = 2
   sizeof(TR_RelocationRecordBinaryTemplate),                                        // TR_AbsoluteMethodAddress                        = 3
   sizeof(TR_RelocationRecordDataAddressBinaryTemplate),                             // TR_DataAddress                                  = 4
   0,                                                                                // TR_ClassObject                                  = 5
   sizeof(TR_RelocationRecordConstantPoolBinaryTemplate),                            // TR_MethodObject                                 = 6
   0,                                                                                // TR_InterfaceObject                              = 7
   sizeof(TR_RelocationRecordHelperAddressBinaryTemplate),                           // TR_AbsoluteHelperAddress                        = 8
   sizeof(TR_RelocationRecordWithOffsetBinaryTemplate),                              // TR_FixedSeqAddress                              = 9
   sizeof(TR_RelocationRecordWithOffsetBinaryTemplate),                              // TR_FixedSeq2Address                             = 10
   sizeof(TR_RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR_JNIVirtualTargetAddress                      = 11
   sizeof(TR_RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR_JNIStaticTargetAddress                       = 12
   sizeof(TR_RelocationRecordBinaryTemplate),                                        // TR_ArrayCopyHelper                              = 13
   sizeof(TR_RelocationRecordBinaryTemplate),                                        // TR_ArrayCopyToc                                 = 14
   sizeof(TR_RelocationRecordBinaryTemplate),                                        // TR_BodyInfoAddress                              = 15
   sizeof(TR_RelocationRecordConstantPoolBinaryTemplate),                            // TR_Thunks                                       = 16
   sizeof(TR_RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR_StaticRamMethodConst                         = 17
   sizeof(TR_RelocationRecordConstantPoolBinaryTemplate),                            // TR_Trampolines                                  = 18
   sizeof(TR_RelocationRecordPicTrampolineBinaryTemplate),                           // TR_PicTrampolines                               = 19
   sizeof(TR_RelocationRecordMethodTracingCheckBinaryTemplate),                      // TR_CheckMethodEnter                             = 20
   sizeof(TR_RelocationRecordBinaryTemplate),                                        // TR_RamMethod                                    = 21
   sizeof(TR_RelocationRecordWithOffsetBinaryTemplate),                              // TR_RamMethodSequence                            = 22
   0,                                                                                // TR_RamMethodSequenceReg                         = 23
   sizeof(TR_RelocationRecordVerifyClassObjectForAllocBinaryTemplate),               // TR_VerifyClassObjectForAlloc                    = 24
   0,                                                                                // TR_ConstantPoolOrderedPair                      = 25
   sizeof(TR_RelocationRecordBinaryTemplate),                                        // TR_AbsoluteMethodAddressOrderedPair             = 26
   sizeof(TR_RelocationRecordInlinedAllocationBinaryTemplate),                       // TR_VerifyRefArrayForAlloc                       = 27
   0,                                                                                // TR_J2IThunks                                    = 28
   sizeof(TR_RelocationRecordWithOffsetBinaryTemplate),                              // TR_GlobalValue                                  = 29
   sizeof(TR_RelocationRecordBinaryTemplate),                                        // TR_BodyInfoAddressLoad                          = 30
   sizeof(TR_RelocationRecordValidateClassBinaryTemplate),                           // TR_ValidateInstanceField                        = 31
   sizeof(TR_RelocationRecordNopGuardBinaryTemplate),                                // TR_InlinedStaticMethodWithNopGuard              = 32
   sizeof(TR_RelocationRecordNopGuardBinaryTemplate),                                // TR_InlinedSpecialMethodWithNopGuard             = 33
   sizeof(TR_RelocationRecordNopGuardBinaryTemplate),                                // TR_InlinedVirtualMethodWithNopGuard             = 34
   sizeof(TR_RelocationRecordNopGuardBinaryTemplate),                                // TR_InlinedInterfaceMethodWithNopGuard           = 35
   sizeof(TR_RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR_SpecialRamMethodConst                        = 36
   0,                                                                                // TR_InlinedHCRMethod                             = 37
   sizeof(TR_RelocationRecordValidateStaticFieldBinaryTemplate),                     // TR_ValidateStaticField                          = 38
   sizeof(TR_RelocationRecordValidateClassBinaryTemplate),                           // TR_ValidateClass                                = 39
   sizeof(TR_RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR_ClassAddress                                 = 40
   sizeof(TR_RelocationRecordWithOffsetBinaryTemplate),                              // TR_HCR                                          = 41
   sizeof(TR_RelocationRecordProfiledInlinedMethodBinaryTemplate),                   // TR_ProfiledMethodGuardRelocation                = 42
   sizeof(TR_RelocationRecordProfiledInlinedMethodBinaryTemplate),                   // TR_ProfiledClassGuardRelocation                 = 43
   0,                                                                                // TR_HierarchyGuardRelocation                     = 44
   0,                                                                                // TR_AbstractGuardRelocation                      = 45
   sizeof(TR_RelocationRecordProfiledInlinedMethodBinaryTemplate),                   // TR_ProfiledInlinedMethodRelocation              = 46
   sizeof(TR_RelocationRecordMethodPointerBinaryTemplate),                           // TR_MethodPointer                                = 47
   sizeof(TR_RelocationRecordPointerBinaryTemplate),                                 // TR_ClassPointer                                 = 48
   sizeof(TR_RelocationRecordMethodTracingCheckBinaryTemplate),                      // TR_CheckMethodExit                              = 49
   sizeof(TR_RelocationRecordValidateArbitraryClassBinaryTemplate),                  // TR_ValidateArbitraryClass                       = 50
   sizeof(TR_RelocationRecordEmitClassBinaryTemplate),                               // TR_EmitClass                                    = 51
   sizeof(TR_RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR_JNISpecialTargetAddress                      = 52
   sizeof(TR_RelocationRecordConstantPoolWithIndexBinaryTemplate),                   // TR_VirtualRamMethodConst                        = 53
   sizeof(TR_RelocationRecordInlinedMethodBinaryTemplate),                           // TR_InlinedInterfaceMethod                       = 54
   sizeof(TR_RelocationRecordInlinedMethodBinaryTemplate),                           // TR_InlinedVirtualMethod                         = 55
   0,                                                                                // TR_NativeMethodAbsolute                         = 56
   0,                                                                                // TR_NativeMethodRelative                         = 57
   sizeof(TR_RelocationRecordPointerBinaryTemplate),                                 // TR_ArbitraryClassAddress                        = 58
   sizeof(TR_RelocationRecordDebugCounterBinaryTemplate),                            // TR_DebugCounter                                 = 59
   sizeof(TR_RelocationRecordClassUnloadAssumptionBinaryTemplate),                   // TR_ClassUnloadAssumption                        = 60
   sizeof(TR_RelocationRecordJ2IVirtualThunkPointerBinaryTemplate),                  // TR_J2IVirtualThunkPointer                       = 61
   sizeof(TR_RelocationRecordNopGuardBinaryTemplate),                                // TR_InlinedAbstractMethodWithNopGuard            = 62
   0,                                                                                // TR_ValidateRootClass                            = 63
   sizeof(TR_RelocationRecordValidateClassByNameBinaryTemplate),                     // TR_ValidateClassByName                          = 64
   sizeof(TR_RelocationRecordValidateProfiledClassBinaryTemplate),                   // TR_ValidateProfiledClass                        = 65
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),                     // TR_ValidateClassFromCP                          = 66
   sizeof(TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate),             // TR_ValidateDefiningClassFromCP                  = 67
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),                     // TR_ValidateStaticClassFromCP                    = 68
   0,                                                                                // TR_ValidateClassFromMethod                      = 69
   0,                                                                                // TR_ValidateComponentClassFromArrayClass         = 70
   sizeof(TR_RelocationRecordValidateArrayFromCompBinaryTemplate),                   // TR_ValidateArrayClassFromComponentClass         = 71
   sizeof(TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate),             // TR_ValidateSuperClassFromClass                  = 72
   sizeof(TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate),            // TR_ValidateClassInstanceOfClass                 = 73
   sizeof(TR_RelocationRecordValidateSystemClassByNameBinaryTemplate),               // TR_ValidateSystemClassByName                    = 74
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),                     // TR_ValidateClassFromITableIndexCP               = 75
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),                     // TR_ValidateDeclaringClassFromFieldOrStatic      = 76
   0,                                                                                // TR_ValidateClassClass                           = 77
   sizeof(TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate),             // TR_ValidateConcreteSubClassFromClass            = 78
   sizeof(TR_RelocationRecordValidateClassChainBinaryTemplate),                      // TR_ValidateClassChain                           = 79
   0,                                                                                // TR_ValidateRomClass                             = 80
   0,                                                                                // TR_ValidatePrimitiveClass                       = 81
   0,                                                                                // TR_ValidateMethodFromInlinedSite                = 82
   0,                                                                                // TR_ValidatedMethodByName                        = 83
   sizeof(TR_RelocationRecordValidateMethodFromClassBinaryTemplate),                 // TR_ValidatedMethodFromClass                     = 84
   sizeof(TR_RelocationRecordValidateMethodFromCPBinaryTemplate),                    // TR_ValidateStaticMethodFromCP                   = 85
   sizeof(TR_RelocationRecordValidateMethodFromCPBinaryTemplate),                    // TR_ValidateSpecialMethodFromCP                  = 86
   sizeof(TR_RelocationRecordValidateMethodFromCPBinaryTemplate),                    // TR_ValidateVirtualMethodFromCP                  = 87
   sizeof(TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate),         // TR_ValidateVirtualMethodFromOffset              = 88
   sizeof(TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate),           // TR_ValidateInterfaceMethodFromCP                = 89
   sizeof(TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate),           // TR_ValidateMethodFromClassAndSig                = 90
   sizeof(TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate),        // TR_ValidateStackWalkerMaySkipFramesRecord       = 91
   0,                                                                                // TR_ValidateArrayClassFromJavaVM                 = 92
   sizeof(TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate),          // TR_ValidateClassInfoIsInitialized               = 93
   sizeof(TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate),            // TR_ValidateMethodFromSingleImplementer          = 94
   sizeof(TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate),   // TR_ValidateMethodFromSingleInterfaceImplementer = 95
   sizeof(TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate),    // TR_ValidateMethodFromSingleAbstractImplementer  = 96
   sizeof(TR_RelocationRecordValidateMethodFromCPBinaryTemplate),                    // TR_ValidateImproperInterfaceMethodFromCP        = 97
   sizeof(TR_RelocationRecordSymbolFromManagerBinaryTemplate),                       // TR_SymbolFromManager                            = 98
   sizeof(TR_RelocationRecordMethodCallAddressBinaryTemplate),                       // TR_MethodCallAddress                            = 99
   sizeof(TR_RelocationRecordSymbolFromManagerBinaryTemplate),                       // TR_DiscontiguousSymbolFromManager               = 100
   sizeof(TR_RelocationRecordResolvedTrampolinesBinaryTemplate),                     // TR_ResolvedTrampolines                          = 101
   sizeof(TR_RelocationRecordBlockFrequencyBinaryTemplate),                          // TR_BlockFrequency                               = 102
   sizeof(TR_RelocationRecordBinaryTemplate),                                        // TR_RecompQueuedFlag                             = 103
   sizeof(TR_RelocationRecordInlinedMethodBinaryTemplate),                           // TR_InlinedStaticMethod                          = 104
   sizeof(TR_RelocationRecordInlinedMethodBinaryTemplate),                           // TR_InlinedSpecialMethod                         = 105
   sizeof(TR_RelocationRecordInlinedMethodBinaryTemplate),                           // TR_InlinedAbstractMethod                        = 106
   };

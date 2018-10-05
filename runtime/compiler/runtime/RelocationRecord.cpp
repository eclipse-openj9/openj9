/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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
#include "codegen/FrontEnd.hpp"
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
#include "il/symbol/StaticSymbol.hpp"
#include "infra/SimpleRegex.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/RelocationRecord.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRuntimeLogger.hpp"
#include "runtime/RelocationTarget.hpp"
#include "env/VMJ9.h"
#include "control/rossa.h"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "control/MethodToBeCompiled.hpp"

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


// These *BinaryTemplate structs describe the shape of the binary relocation records.
struct TR_RelocationRecordBinaryTemplate
   {
   uint8_t type(TR_RelocationTarget *reloTarget) { return reloTarget->loadUnsigned8b(&_type); }

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

struct TR_RelocationRecordHelperAddressBinaryTemplate
   {
   uint16_t _size;
   uint8_t _type;
   uint8_t _flags;
   uint32_t _helperID;
   };

struct TR_RelocationRecordPicTrampolineBinaryTemplate
   {
   uint16_t _size;
   uint8_t _type;
   uint8_t _flags;
   uint32_t _numTrampolines;
   };

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

typedef TR_RelocationRecordConstantPoolWithIndexBinaryTemplate TR_RelocationRecordClassObjectBinaryTemplate;

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

typedef TR_RelocationRecordNopGuardBinaryTemplate TR_RelocationRecordInlinedStaticMethodWithNopGuardBinaryTemplate;
typedef TR_RelocationRecordNopGuardBinaryTemplate TR_RelocationRecordInlinedSpecialMethodWithNopGuardBinaryTemplate;
typedef TR_RelocationRecordNopGuardBinaryTemplate TR_RelocationRecordInlinedVirtualMethodWithNopGuardBinaryTemplate;
typedef TR_RelocationRecordNopGuardBinaryTemplate TR_RelocationRecordInlinedInterfaceMethodWithNopGuardBinaryTemplate;


struct TR_RelocationRecordProfiledInlinedMethodBinaryTemplate : TR_RelocationRecordInlinedMethodBinaryTemplate
   {
   UDATA _classChainIdentifyingLoaderOffsetInSharedCache;
   UDATA _classChainForInlinedMethod;
   UDATA _vTableSlot;
   };

typedef TR_RelocationRecordProfiledInlinedMethodBinaryTemplate TR_RelocationRecordProfiledGuardBinaryTemplate;
typedef TR_RelocationRecordProfiledInlinedMethodBinaryTemplate TR_RelocationRecordProfiledClassGuardBinaryTemplate;
typedef TR_RelocationRecordProfiledInlinedMethodBinaryTemplate TR_RelocationRecordProfiledMethodGuardBinaryTemplate;


typedef TR_RelocationRecordBinaryTemplate TR_RelocationRecordRamMethodBinaryTemplate;

typedef TR_RelocationRecordBinaryTemplate TR_RelocationRecordArrayCopyHelperTemplate;

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

typedef TR_RelocationRecordPointerBinaryTemplate TR_RelocationRecordClassPointerBinaryTemplate;
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
   UDATA _bcIndex;
   UDATA _offsetOfNameString;
   UDATA _delta;
   UDATA _fidelity;
   UDATA _staticDelta;
   };


typedef TR_RelocationRecordBinaryTemplate TR_RelocationRecordClassUnloadAssumptionBinaryTemplate;
typedef TR_RelocationRecordBinaryTemplate TR_RelocationRecordClassUnloadBinaryTemplate;

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
TR_RelocationRecordGroup::firstRecord(TR_RelocationTarget *reloTarget)
   {
   // first word of the group is a pointer size field for the entire group
   return (TR_RelocationRecordBinaryTemplate *) (((uintptr_t *)_group)+1);
   }

TR_RelocationRecordBinaryTemplate *
TR_RelocationRecordGroup::pastLastRecord(TR_RelocationTarget *reloTarget)
   {
   return (TR_RelocationRecordBinaryTemplate *) ((uint8_t *)_group + size(reloTarget));
   }

int32_t
TR_RelocationRecordGroup::applyRelocations(TR_RelocationRuntime *reloRuntime,
                                           TR_RelocationTarget *reloTarget,
                                           uint8_t *reloOrigin)
   {
   TR_RelocationRecordBinaryTemplate *recordPointer = firstRecord(reloTarget);
   TR_RelocationRecordBinaryTemplate *endOfRecords = pastLastRecord(reloTarget);

   while (recordPointer < endOfRecords)
      {
      TR_RelocationRecord storage;
      // Create a specific type of relocation record based on the information
      // in the binary record pointed to by `recordPointer`
      TR_RelocationRecord *reloRecord = TR_RelocationRecord::create(&storage, reloRuntime, reloTarget, recordPointer);
      int32_t rc = handleRelocation(reloRuntime, reloTarget, reloRecord, reloOrigin);
      if (rc != 0)
         return rc;

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

   if (reloRecord->ignore(reloRuntime))
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tignore!\n");
      return 0;
      }

   reloRecord->preparePrivateData(reloRuntime, reloTarget);
   return reloRecord->applyRelocationAtAllOffsets(reloRuntime, reloTarget, reloOrigin);
   }

#define FLAGS_RELOCATION_WIDE_OFFSETS   0x80
#define FLAGS_RELOCATION_EIP_OFFSET     0x40
#define FLAGS_RELOCATION_TYPE_MASK      (TR_ExternalRelocationTargetKindMask)
#define FLAGS_RELOCATION_FLAG_MASK      ((uint8_t) (FLAGS_RELOCATION_WIDE_OFFSETS | FLAGS_RELOCATION_EIP_OFFSET))


TR_RelocationRecord *
TR_RelocationRecord::create(TR_RelocationRecord *storage, TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_RelocationRecordBinaryTemplate *record)
   {
   TR_RelocationRecord *reloRecord = NULL;
   // based on the type of the relocation record, create an object of a particular variety of TR_RelocationRecord object
   uint8_t reloType = record->type(reloTarget);
   switch (reloType)
      {
      case TR_HelperAddress:
         reloRecord = new (storage) TR_RelocationRecordHelperAddress(reloRuntime, record);
         break;
      case TR_ConstantPool:
      case TR_ConstantPoolOrderedPair:
         reloRecord = new (storage) TR_RelocationRecordConstantPool(reloRuntime, record);
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
      case TR_InlinedSpecialMethodWithNopGuard:
         reloRecord = new (storage) TR_RelocationRecordInlinedSpecialMethodWithNopGuard(reloRuntime, record);
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
      case TR_InlinedHCRMethod:
         reloRecord = new (storage) TR_RelocationRecordInlinedMethod(reloRuntime, record);
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
         reloRecord = new (storage) TR_RelocationRecordClassObject(reloRuntime, record);
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
      case TR_RamMethodSequenceReg:
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
TR_RelocationRecord::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordBinaryTemplate);
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
   setFlag(reloTarget, FLAGS_RELOCATION_WIDE_OFFSETS);
   }

bool
TR_RelocationRecord::wideOffsets(TR_RelocationTarget *reloTarget)
   {
   return (flags(reloTarget) & FLAGS_RELOCATION_WIDE_OFFSETS) != 0;
   }

void
TR_RelocationRecord::setEipRelative(TR_RelocationTarget *reloTarget)
   {
   setFlag(reloTarget, FLAGS_RELOCATION_EIP_OFFSET);
   }

bool
TR_RelocationRecord::eipRelative(TR_RelocationTarget *reloTarget)
   {
   return (flags(reloTarget) & FLAGS_RELOCATION_EIP_OFFSET) != 0;
   }

void
TR_RelocationRecord::setFlag(TR_RelocationTarget *reloTarget, uint8_t flag)
   {
   uint8_t flags = reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) | (flag & FLAGS_RELOCATION_FLAG_MASK);
   reloTarget->storeUnsigned8b(flags, (uint8_t *) &_record->_flags);
   }

uint8_t
TR_RelocationRecord::flags(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) & FLAGS_RELOCATION_FLAG_MASK;
   }

void
TR_RelocationRecord::setReloFlags(TR_RelocationTarget *reloTarget, uint8_t reloFlags)
   {
   TR_ASSERT((reloFlags & ~FLAGS_RELOCATION_FLAG_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
   uint8_t crossPlatFlags = flags(reloTarget);
   uint8_t flags = crossPlatFlags | (reloFlags & ~FLAGS_RELOCATION_FLAG_MASK);
   reloTarget->storeUnsigned8b(flags, (uint8_t *) &_record->_flags);
   }

uint8_t
TR_RelocationRecord::reloFlags(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b((uint8_t *) &_record->_flags) & ~FLAGS_RELOCATION_FLAG_MASK;
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
      helperAddress = (uint8_t *)TR::CodeCacheManager::instance()->findHelperTrampoline((void *)baseLocation, reloPrivateData->_helperID);
      }

   return helperAddress;
   }

#undef FLAGS_RELOCATION_WIDE_OFFSETS
#undef FLAGS_RELOCATION_EIP_OFFSET
#undef FLAGS_RELOCATION_ORDERED_PAIR
#undef FLAGS_RELOCATION_TYPE_MASK
#undef FLAGS_RELOCATION_FLAG_MASK


bool
TR_RelocationRecord::ignore(TR_RelocationRuntime *reloRuntime)
   {
   return false;
   }

int32_t
TR_RelocationRecord::applyRelocationAtAllOffsets(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloOrigin)
   {
   if (ignore(reloRuntime))
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tignore!\n");
      return 0;
      }

   if (reloTarget->isOrderedPairRelocation(this, reloTarget))
      {
      if (wideOffsets(reloTarget))
         {
         int32_t *offsetsBase = (int32_t *) (((uint8_t*)_record) + bytesInHeaderAndPayload());
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
         int16_t *offsetsBase = (int16_t *) (((uint8_t*)_record) + bytesInHeaderAndPayload());
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
         int32_t *offsetsBase = (int32_t *) (((uint8_t*)_record) + bytesInHeaderAndPayload());
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
         int16_t *offsetsBase = (int16_t *) (((uint8_t*)_record) + bytesInHeaderAndPayload());
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
TR_RelocationRecordWithOffset::setOffset(TR_RelocationTarget *reloTarget, uintptrj_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptrj_t *) &((TR_RelocationRecordWithOffsetBinaryTemplate *)_record)->_offset);
   }

uintptrj_t
TR_RelocationRecordWithOffset::offset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordWithOffsetBinaryTemplate *)_record)->_offset);
   }

int32_t
TR_RelocationRecordWithOffset::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordWithOffsetBinaryTemplate);
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
TR_RelocationRecordWithInlinedSiteIndex::setInlinedSiteIndex(TR_RelocationTarget *reloTarget, uintptrj_t inlinedSiteIndex)
   {
   reloTarget->storeRelocationRecordValue(inlinedSiteIndex, (uintptrj_t *) &((TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate *)_record)->_inlinedSiteIndex);
   }

uintptrj_t
TR_RelocationRecordWithInlinedSiteIndex::inlinedSiteIndex(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate *)_record)->_inlinedSiteIndex);
   }

int32_t
TR_RelocationRecordWithInlinedSiteIndex::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate);
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordWithInlinedSiteIndex::getInlinedSiteCallerMethod(TR_RelocationRuntime *reloRuntime)
   {
   uintptrj_t siteIndex = inlinedSiteIndex(reloRuntime->reloTarget());
   TR_InlinedCallSite *inlinedCallSite = (TR_InlinedCallSite *)getInlinedCallSiteArrayElement(reloRuntime->exceptionTable(), siteIndex);
   uintptrj_t callerIndex = inlinedCallSite->_byteCodeInfo.getCallerIndex();
   return getInlinedSiteMethod(reloRuntime, callerIndex);
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordWithInlinedSiteIndex::getInlinedSiteMethod(TR_RelocationRuntime *reloRuntime)
   {
   return getInlinedSiteMethod(reloRuntime, inlinedSiteIndex(reloRuntime->reloTarget()));
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordWithInlinedSiteIndex::getInlinedSiteMethod(TR_RelocationRuntime *reloRuntime, uintptrj_t siteIndex)
   {
   TR_OpaqueMethodBlock *method = (TR_OpaqueMethodBlock *) reloRuntime->method();
   if (siteIndex != (uintptrj_t)-1)
      {
      TR_InlinedCallSite *inlinedCallSite = (TR_InlinedCallSite *)getInlinedCallSiteArrayElement(reloRuntime->exceptionTable(), siteIndex);
      method = inlinedCallSite->_methodInfo;
      }
   return method;
   }

bool
TR_RelocationRecordWithInlinedSiteIndex::ignore(TR_RelocationRuntime *reloRuntime)
   {
   J9Method *method = (J9Method *)getInlinedSiteMethod(reloRuntime);

   // -1 means this inlined method isn't active, so can ignore relocations associated with it
   if (method == (J9Method *)(-1))
      return true;

   if (isUnloadedInlinedMethod(method))
      return true;

   return false;
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

int32_t
TR_RelocationRecordConstantPool::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordConstantPoolBinaryTemplate);
   }

void
TR_RelocationRecordConstantPool::setConstantPool(TR_RelocationTarget *reloTarget, uintptrj_t constantPool)
   {
   reloTarget->storeRelocationRecordValue(constantPool, (uintptrj_t *) &((TR_RelocationRecordConstantPoolBinaryTemplate *)_record)->_constantPool);
   }

uintptrj_t
TR_RelocationRecordConstantPool::constantPool(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordConstantPoolBinaryTemplate *)_record)->_constantPool);
   }

uintptrj_t
TR_RelocationRecordConstantPool::currentConstantPool(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uintptrj_t oldValue)
   {
   uintptrj_t oldCPBase = constantPool(reloTarget);
   uintptrj_t newCP = oldValue - oldCPBase + (uintptrj_t)reloRuntime->ramCP();

   return newCP;
   }

uintptrj_t
TR_RelocationRecordConstantPool::findConstantPool(TR_RelocationTarget *reloTarget, uintptrj_t oldValue, TR_OpaqueMethodBlock *ramMethod)
   {
   uintptrj_t oldCPBase = constantPool(reloTarget);
   uintptrj_t methodCP = oldValue - oldCPBase + (uintptrj_t)J9_CP_FROM_METHOD((J9Method *)ramMethod);
   return methodCP;
   }


uintptrj_t
TR_RelocationRecordConstantPool::computeNewConstantPool(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uintptrj_t oldConstantPool)
   {
   uintptrj_t newCP;
   UDATA thisInlinedSiteIndex = (UDATA) inlinedSiteIndex(reloTarget);
   if (thisInlinedSiteIndex != (UDATA) -1)
      {
      // Find CP from inlined method
      // Assume that the inlined call site has already been relocated
      // And assumes that the method is resolved already, otherwise, we would not have properly relocated the
      // ramMethod for the inlined callsite and trying to retreive stuff from the bogus pointer will result in error
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

   uintptrj_t oldValue =  (uintptrj_t) reloTarget->loadAddress(reloLocation);
   uintptrj_t newCP = computeNewConstantPool(reloRuntime, reloTarget, oldValue);
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

   uintptrj_t oldValue = (uintptrj_t) reloTarget->loadAddress(reloLocationHigh, reloLocationLow);
   uintptrj_t newCP = computeNewConstantPool(reloRuntime, reloTarget, oldValue);
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
TR_RelocationRecordConstantPoolWithIndex::setCpIndex(TR_RelocationTarget *reloTarget, uintptrj_t cpIndex)
   {
   reloTarget->storeRelocationRecordValue(cpIndex, (uintptrj_t *) &((TR_RelocationRecordConstantPoolWithIndexBinaryTemplate *)_record)->_index);
   }

uintptrj_t
TR_RelocationRecordConstantPoolWithIndex::cpIndex(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordConstantPoolWithIndexBinaryTemplate *)_record)->_index);
   }

int32_t
TR_RelocationRecordConstantPoolWithIndex::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordConstantPoolWithIndexBinaryTemplate);
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordConstantPoolWithIndex::getSpecialMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
   {
   TR::VMAccessCriticalSection getSpecialMethodFromCP(reloRuntime->fej9());
   J9ConstantPool *cp = (J9ConstantPool *) void_cp;
   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();

   J9VMThread *vmThread = reloRuntime->currentThread();
   TR_OpaqueMethodBlock *method = (TR_OpaqueMethodBlock *) jitResolveSpecialMethodRef(vmThread, cp, cpIndex, J9_RESOLVE_FLAG_AOT_LOAD_TIME);
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
                                                                                   J9_RESOLVE_FLAG_AOT_LOAD_TIME,
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
                                                                                     J9_RESOLVE_FLAG_AOT_LOAD_TIME);
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
                                                                                            J9_RESOLVE_FLAG_AOT_LOAD_TIME);
      }

   TR_RelocationRuntimeLogger *reloLogger = reloRuntime->reloLogger();
   RELO_LOG(reloLogger, 6, "\tgetMethodFromCP: interface class %p\n", interfaceClass);

   TR_OpaqueMethodBlock *calleeMethod = NULL;
   if (interfaceClass)
      {
      TR_PersistentCHTable * chTable = reloRuntime->getPersistentInfo()->getPersistentCHTable();
      TR_ResolvedMethod *callerResolvedMethod = fe->createResolvedMethod(trMemory, callerMethod, NULL);

      TR_ResolvedMethod *calleeResolvedMethod = chTable->findSingleInterfaceImplementer(interfaceClass, cpIndex, callerResolvedMethod, reloRuntime->comp());

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
   uintptrj_t helper = helperID(reloTarget);
   if (reloRuntime->comp())
      reloLogger->printf("\thelper %d %s\n", helper, reloRuntime->comp()->findOrCreateDebug()->getRuntimeHelperName(helper));
   else
      reloLogger->printf("\thelper %d\n", helper);
   }

int32_t
TR_RelocationRecordHelperAddress::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordHelperAddressBinaryTemplate);
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
   uint8_t *helperOffset = helperAddress - (uintptrj_t)baseLocation;
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
   uintptrj_t oldAddress = (uintptrj_t) reloTarget->loadAddress(reloLocation);

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
TR_RelocationRecordDataAddress::setOffset(TR_RelocationTarget *reloTarget, uintptrj_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptrj_t *) &((TR_RelocationRecordDataAddressBinaryTemplate *)_record)->_offset);
   }

uintptrj_t
TR_RelocationRecordDataAddress::offset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordDataAddressBinaryTemplate *)_record)->_offset);
   }

int32_t
TR_RelocationRecordDataAddress::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordDataAddressBinaryTemplate);
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
      address = (uint8_t *)jitCTResolveStaticFieldRefWithMethod(vmThread, ramMethod, cpindex, false, &fieldShape);
      }

   if (address == NULL)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tfindDataAddress: unresolved\n");
      }

   address = address + extraOffset;
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tfindDataAddress: field address %p\n", address);
   return address;
   }

int32_t
TR_RelocationRecordDataAddress::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uint8_t *newAddress = findDataAddress(reloRuntime, reloTarget);

   RELO_LOG(reloRuntime->reloLogger(), 6, "applyRelocation old ptr %p, new ptr %p\n", reloTarget->loadPointer(reloLocation), newAddress);

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
TR_RelocationRecordClassObject::name()
   {
   return "TR_ClassObject";
   }

TR_OpaqueClassBlock *
TR_RelocationRecordClassObject::computeNewClassObject(TR_RelocationRuntime *reloRuntime, uintptrj_t newConstantPool, uintptrj_t inlinedSiteIndex, uintptrj_t cpIndex)
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
      resolvedClass = javaVM->internalVMFunctions->resolveClassRef(vmThread, (J9ConstantPool *)newConstantPool, cpIndex, J9_RESOLVE_FLAG_AOT_LOAD_TIME);
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
TR_RelocationRecordClassObject::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   uintptrj_t oldAddress = (uintptrj_t) reloTarget->loadAddress(reloLocation);

   uintptrj_t newConstantPool = computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   TR_OpaqueClassBlock *newAddress = computeNewClassObject(reloRuntime, newConstantPool, inlinedSiteIndex(reloTarget), cpIndex(reloTarget));

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
TR_RelocationRecordClassObject::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   uintptrj_t oldValue = (uintptrj_t) reloTarget->loadAddress(reloLocationHigh, reloLocationLow);
   uintptrj_t newConstantPool = computeNewConstantPool(reloRuntime, reloTarget, oldValue);
   TR_OpaqueClassBlock *newAddress = computeNewClassObject(reloRuntime, newConstantPool, inlinedSiteIndex(reloTarget), cpIndex(reloTarget));

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
   uintptrj_t oldAddress = (uintptrj_t) reloTarget->loadAddress(reloLocation);
   uintptrj_t newAddress = currentConstantPool(reloRuntime, reloTarget, oldAddress);
   reloTarget->storeAddress((uint8_t *) newAddress, reloLocation);
   return 0;
   }

int32_t
TR_RelocationRecordMethodObject::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
   {
   uintptrj_t oldAddress = (uintptrj_t) reloTarget->loadAddress(reloLocationHigh, reloLocationLow);
   uintptrj_t newAddress = currentConstantPool(reloRuntime, reloTarget, oldAddress);
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
   fixPersistentMethodInfo((void *)exceptionTable, !reloRuntime->fej9()->_compInfoPT->getMethodBeingCompiled()->isAotLoad());
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

   uintptrj_t newConstantPool = computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
   reloTarget->storeAddress((uint8_t *)newConstantPool, reloLocation);
   uintptrj_t cpIndex = reloTarget->loadThunkCPIndex(reloLocation);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tapplyRelocation: loadThunkCPIndex is %d\n", cpIndex);
   return relocateAndRegisterThunk(reloRuntime, reloTarget, newConstantPool, cpIndex, reloLocation);
   }

int32_t
TR_RelocationRecordThunks::relocateAndRegisterThunk(
   TR_RelocationRuntime *reloRuntime,
   TR_RelocationTarget *reloTarget,
   uintptrj_t cp,
   uintptr_t cpIndex,
   uint8_t *reloLocation)
   {
   // XXX: Currently all thunks are batch-relocated elsewhere for JITaaS
   if (reloRuntime->fej9()->_compInfoPT->getMethodBeingCompiled()->isRemoteCompReq() && 
      !reloRuntime->fej9()->_compInfoPT->getMethodBeingCompiled()->isAotLoad())
      {
      return 0;
      }

   J9JITConfig *jitConfig = reloRuntime->jitConfig();
   J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;
   J9ConstantPool *constantPool = (J9ConstantPool *)cp;

   J9ROMClass * romClass = J9_CLASS_FROM_CP(constantPool)->romClass;
   J9ROMMethodRef *romMethodRef = &J9ROM_CP_BASE(romClass, J9ROMMethodRef)[cpIndex];
   J9ROMNameAndSignature * nameAndSignature = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);

   bool matchFound = false;

   int32_t signatureLength = J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature));
   char *signatureString = (char *) &(J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature)));

   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\trelocateAndRegisterThunk: %.*s%.*s\n", J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature)), &(J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature))),  signatureLength, signatureString);

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
      // allocate in the current code cache. The reason is that, when a a new code cache is needed
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

int32_t
TR_RelocationRecordJ2IVirtualThunkPointer::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordJ2IVirtualThunkPointerBinaryTemplate);
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

uintptrj_t
TR_RelocationRecordJ2IVirtualThunkPointer::offsetToJ2IVirtualThunkPointer(
   TR_RelocationTarget *reloTarget)
   {
   auto recordData = (TR_RelocationRecordJ2IVirtualThunkPointerBinaryTemplate *)_record;
   auto offsetEA = (uintptrj_t *) &recordData->_offsetToJ2IVirtualThunkPointer;
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

int32_t
TR_RelocationRecordPicTrampolines::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordPicTrampolineBinaryTemplate);
   }

void
TR_RelocationRecordPicTrampolines::setNumTrampolines(TR_RelocationTarget *reloTarget, int numTrampolines)
   {
   reloTarget->storeUnsigned8b(numTrampolines, (uint8_t *) &(((TR_RelocationRecordPicTrampolineBinaryTemplate *)_record)->_numTrampolines));
   }

uint8_t
TR_RelocationRecordPicTrampolines::numTrampolines(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadUnsigned8b((uint8_t *) &(((TR_RelocationRecordPicTrampolineBinaryTemplate *)_record)->_numTrampolines));
   }

int32_t
TR_RelocationRecordPicTrampolines::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   if (reloRuntime->codeCache()->reserveNTrampolines(numTrampolines(reloTarget)) != OMR::CodeCacheErrorCode::ERRORCODE_SUCCESS)
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

   uintptrj_t newConstantPool = computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));
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

int32_t
TR_RelocationRecordInlinedAllocation::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordInlinedAllocationBinaryTemplate);
   }

void
TR_RelocationRecordInlinedAllocation::setBranchOffset(TR_RelocationTarget *reloTarget, uintptrj_t branchOffset)
   {
   reloTarget->storeRelocationRecordValue(branchOffset, (uintptrj_t *)&((TR_RelocationRecordInlinedAllocationBinaryTemplate *)_record)->_branchOffset);
   }

uintptrj_t
TR_RelocationRecordInlinedAllocation::branchOffset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *)&((TR_RelocationRecordInlinedAllocationBinaryTemplate *)_record)->_branchOffset);
   }

void
TR_RelocationRecordInlinedAllocation::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordInlinedAllocationPrivateData *reloPrivateData = &(privateData()->inlinedAllocation);

   uintptrj_t oldValue = constantPool(reloTarget);
   J9ConstantPool *newConstantPool = (J9ConstantPool *) computeNewConstantPool(reloRuntime, reloTarget, constantPool(reloTarget));

   J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;
   TR_J9VMBase *fe = reloRuntime->fej9();
   J9Class *clazz;

      {
      TR::VMAccessCriticalSection preparePrivateData(fe);
      clazz = javaVM->internalVMFunctions->resolveClassRef(javaVM->internalVMFunctions->currentVMThread(javaVM),
                                                                    newConstantPool,
                                                                    cpIndex(reloTarget),
                                                                    J9_RESOLVE_FLAG_AOT_LOAD_TIME);
      }

   bool inlinedCodeIsOkay = false;
   if (clazz)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tpreparePrivateData: clazz %p %.*s\n",
                                                 clazz,
                                                 J9ROMCLASS_CLASSNAME(clazz->romClass)->length,
                                                 J9ROMCLASS_CLASSNAME(clazz->romClass)->data);

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
      _patchVirtualGuard(reloLocation, destination, TR::Compiler->target.isSMP());
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

int32_t
TR_RelocationRecordVerifyClassObjectForAlloc::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordVerifyClassObjectForAllocBinaryTemplate);
   }

void
TR_RelocationRecordVerifyClassObjectForAlloc::setAllocationSize(TR_RelocationTarget *reloTarget, uintptrj_t allocationSize)
   {
   reloTarget->storeRelocationRecordValue(allocationSize, (uintptrj_t *)&((TR_RelocationRecordVerifyClassObjectForAllocBinaryTemplate *)_record)->_allocationSize);
   }

uintptrj_t
TR_RelocationRecordVerifyClassObjectForAlloc::allocationSize(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *)&((TR_RelocationRecordVerifyClassObjectForAllocBinaryTemplate *)_record)->_allocationSize);
   }

bool
TR_RelocationRecordVerifyClassObjectForAlloc::verifyClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *clazz)
   {
   bool inlineAllocation = false;
   TR::Compilation* comp = TR::comp();
   TR_J9VMBase *fe = (TR_J9VMBase *)(reloRuntime->fej9());
   if (comp->canAllocateInlineClass(clazz))
      {
      uintptrj_t size = fe->getAllocationSize(NULL, clazz);
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
   J9ROMClass *inlinedCodeRomClass = (J9ROMClass *)reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) romClassOffsetInSharedCache(reloTarget));
   J9UTF8 *inlinedCodeClassName = J9ROMCLASS_CLASSNAME(inlinedCodeRomClass);
   reloLogger->printf("\tromClassOffsetInSharedCache %x %.*s\n", romClassOffsetInSharedCache(reloTarget), inlinedCodeClassName->length, inlinedCodeClassName->data );
   //reloLogger->printf("\tromClassOffsetInSharedCache %x %.*s\n", romClassOffsetInSharedCache(reloTarget), J9UTF8_LENGTH(inlinedCodeClassname), J9UTF8_DATA(inlinedCodeClassName));
   }

void
TR_RelocationRecordInlinedMethod::setRomClassOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptrj_t romClassOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(romClassOffsetInSharedCache, (uintptrj_t *) &((TR_RelocationRecordInlinedMethodBinaryTemplate *)_record)->_romClassOffsetInSharedCache);
   }

uintptrj_t
TR_RelocationRecordInlinedMethod::romClassOffsetInSharedCache(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordInlinedMethodBinaryTemplate *)_record)->_romClassOffsetInSharedCache);
   }

int32_t
TR_RelocationRecordInlinedMethod::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordInlinedMethodBinaryTemplate);
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
   J9Method *ramMethod = NULL;
   bool failValidation = true;

   if (validateClassesSame(reloRuntime, reloTarget, ((TR_OpaqueMethodBlock **)&ramMethod)) &&
       !reloRuntime->options()->getOption(TR_DisableCHOpts))
      {
      // If validate passes, no patching needed since the fall-through path is the inlined code
      fixInlinedSiteInfo(reloRuntime, reloTarget, (TR_OpaqueMethodBlock *) ramMethod);
      failValidation = false;
      }

   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   reloPrivateData->_ramMethod = (TR_OpaqueMethodBlock *) ramMethod;
   reloPrivateData->_failValidation = failValidation;
   RELO_LOG(reloRuntime->reloLogger(), 5, "\tpreparePrivateData: ramMethod %p failValidation %d\n", ramMethod, failValidation);
   }


bool
TR_RelocationRecordInlinedMethod::validateClassesSame(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueMethodBlock **theMethod)
   {
   J9Method *callerMethod = (J9Method *) getInlinedSiteCallerMethod(reloRuntime);
   if (callerMethod == (J9Method *)-1)
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tvalidateClassesSame: caller failed relocation so cannot validate inlined method\n");
      *theMethod = NULL;
      return false;
      }
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tvalidateSameClasses: caller method %p\n", callerMethod);
   J9UTF8 *callerClassName;
   J9UTF8 *callerMethodName;
   J9UTF8 *callerMethodSignature;
   getClassNameSignatureFromMethod(callerMethod, callerClassName, callerMethodName, callerMethodSignature);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tvalidateClassesSame: caller method %.*s.%.*s%.*s\n",
                                              callerClassName->length, callerClassName->data,
                                              callerMethodName->length, callerMethodName->data,
                                              callerMethodSignature->length, callerMethodSignature->data);

   TR::SimpleRegex * regex = reloRuntime->options()->getDisabledInlineSites();
   if (regex && TR::SimpleRegex::match(regex, inlinedSiteIndex(reloTarget)))
      {
      RELO_LOG(reloRuntime->reloLogger(), 6, "\tvalidateClassesSame: inlined site forcibly disabled by options\n");
      *theMethod = NULL;
      return false;
      }

   J9ConstantPool *cp = NULL;
   if (!isUnloadedInlinedMethod(callerMethod))
      cp = J9_CP_FROM_METHOD(callerMethod);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\tvalidateClassesSame: cp %p\n", cp);

   if (cp)
      {
      J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;
      void *romClass = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) romClassOffsetInSharedCache(reloTarget));
      J9Method *currentMethod = (J9Method *) getMethodFromCP(reloRuntime, cp, cpIndex(reloTarget), (TR_OpaqueMethodBlock *) callerMethod);

      if (currentMethod != NULL &&
          (reloRuntime->fej9()->isAnyMethodTracingEnabled((TR_OpaqueMethodBlock *) currentMethod) ||
           reloRuntime->fej9()->canMethodEnterEventBeHooked() ||
           reloRuntime->fej9()->canMethodExitEventBeHooked()))
         {
         RELO_LOG(reloRuntime->reloLogger(), 6, "\tvalidateClassesSame: target may need enter/exit tracing so disabling inline site\n");
         currentMethod = NULL;
         }

      if (currentMethod)
         {
         /* Calculate the runtime rom class value from the code cache */
         void *compileRomClass = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) romClassOffsetInSharedCache(reloTarget));
         void *currentRomClass = (void *)J9_CLASS_FROM_METHOD(currentMethod)->romClass;

         RELO_LOG(reloRuntime->reloLogger(), 6, "\tvalidateClassesSame: compileRomClass %p currentRomClass %p\n", compileRomClass, currentRomClass);
         if (compileRomClass == currentRomClass)
            {
            //unresolvedButStillHaveRightMethodCounter++;
            *theMethod = (TR_OpaqueMethodBlock *)currentMethod;
            J9UTF8 *className;
            J9UTF8 *methodName;
            J9UTF8 *methodSignature;
            getClassNameSignatureFromMethod(currentMethod, className, methodName, methodSignature);
            RELO_LOG(reloRuntime->reloLogger(), 6, "\tvalidateClassesSame: inlined method %.*s.%.*s%.*s\n",
                                                       className->length, className->data,
                                                       methodName->length, methodName->data,
                                                       methodSignature->length, methodSignature->data);
            //RELO_LOG(reloRuntime->reloLogger(), 6, "\tvalidateClassesSame: inlined method %.*s.%.*s%.*s\n",
            //                                           J9UTF8_LENGTH(className), J9UTF8_DATA(className),
            //                                           J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName),
            //                                           J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
            return true;
            }
         else
            {
            //stillNoMatchingClassCounter++;
            }
         }
      else
         {
         //reallyUnresolvedCounter++;
         }

      }
   else
      {
      //reallyUnresolvedCounter++;
      }

   RELO_LOG(reloRuntime->reloLogger(), 6, "\tvalidateClassesSame: not same\n");
   *theMethod = NULL;
   return false;
   }

int32_t
TR_RelocationRecordInlinedMethod::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   reloRuntime->incNumInlinedMethodRelos();

   TR_AOTStats *aotStats = reloRuntime->aotStats();

   TR_RelocationRecordInlinedMethodPrivateData *reloPrivateData = &(privateData()->inlinedMethod);
   if (reloPrivateData->_failValidation)
      {
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
TR_RelocationRecordNopGuard::setDestinationAddress(TR_RelocationTarget *reloTarget, uintptrj_t destinationAddress)
   {
   reloTarget->storeRelocationRecordValue(destinationAddress, (uintptrj_t *) &((TR_RelocationRecordNopGuardBinaryTemplate *)_record)->_destinationAddress);
   }

uintptrj_t
TR_RelocationRecordNopGuard::destinationAddress(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordNopGuardBinaryTemplate *)_record)->_destinationAddress);
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

int32_t
TR_RelocationRecordNopGuard::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordNopGuardBinaryTemplate);
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

void
TR_RelocationRecordInlinedVirtualMethod::print(TR_RelocationRuntime *reloRuntime)
   {
   Base::print(reloRuntime);
   }

void
TR_RelocationRecordInlinedVirtualMethod::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   Base::preparePrivateData(reloRuntime, reloTarget);
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordInlinedVirtualMethod::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex)
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

void
TR_RelocationRecordInlinedInterfaceMethod::print(TR_RelocationRuntime *reloRuntime)
   {
   TR_RelocationRecordInlinedMethod::print(reloRuntime);
   }

void
TR_RelocationRecordInlinedInterfaceMethod::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordInlinedMethod::preparePrivateData(reloRuntime, reloTarget);
   }

TR_OpaqueMethodBlock *
TR_RelocationRecordInlinedInterfaceMethod::getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod)
   {
   return getInterfaceMethodFromCP(reloRuntime, void_cp, cpIndex, callerMethod);
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
   reloLogger->printf("\tvTableSlot %x\n", vTableSlot(reloTarget));
   }

int32_t
TR_RelocationRecordProfiledInlinedMethod::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordProfiledInlinedMethodBinaryTemplate);
   }

void
TR_RelocationRecordProfiledInlinedMethod::setClassChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptrj_t classChainIdentifyingLoaderOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(classChainIdentifyingLoaderOffsetInSharedCache, (uintptrj_t *) &((TR_RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_classChainIdentifyingLoaderOffsetInSharedCache);
   }

uintptrj_t
TR_RelocationRecordProfiledInlinedMethod::classChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_classChainIdentifyingLoaderOffsetInSharedCache);
   }

void
TR_RelocationRecordProfiledInlinedMethod::setClassChainForInlinedMethod(TR_RelocationTarget *reloTarget, uintptrj_t classChainForInlinedMethod)
   {
   reloTarget->storeRelocationRecordValue(classChainForInlinedMethod, (uintptrj_t *) &((TR_RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_classChainForInlinedMethod);
   }

uintptrj_t
TR_RelocationRecordProfiledInlinedMethod::classChainForInlinedMethod(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_classChainForInlinedMethod);
   }

void
TR_RelocationRecordProfiledInlinedMethod::setVTableSlot(TR_RelocationTarget *reloTarget, uintptrj_t vTableSlot)
   {
   reloTarget->storeRelocationRecordValue(vTableSlot, (uintptrj_t *) &((TR_RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_vTableSlot);
   }

uintptrj_t
TR_RelocationRecordProfiledInlinedMethod::vTableSlot(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordProfiledInlinedMethodBinaryTemplate *)_record)->_vTableSlot);
   }


bool
TR_RelocationRecordProfiledInlinedMethod::checkInlinedClassValidity(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *inlinedClass)
   {
   return true;
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

   J9ROMClass *inlinedCodeRomClass = (J9ROMClass *) reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) romClassOffsetInSharedCache(reloTarget));
   J9UTF8 *inlinedCodeClassName = J9ROMCLASS_CLASSNAME(inlinedCodeRomClass);
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: inlinedCodeRomClass %p %.*s\n", inlinedCodeRomClass, inlinedCodeClassName->length, inlinedCodeClassName->data);

#if defined(PUBLIC_BUILD)
   J9ClassLoader *classLoader = NULL;
#else
   void *classChainIdentifyingLoader = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) classChainIdentifyingLoaderOffsetInSharedCache(reloTarget));
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classChainIdentifyingLoader %p\n", classChainIdentifyingLoader);
   J9ClassLoader *classLoader = (J9ClassLoader *) reloRuntime->fej9()->sharedCache()->persistentClassLoaderTable()->lookupClassLoaderAssociatedWithClassChain(classChainIdentifyingLoader);
#endif
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classLoader %p\n", classLoader);

   if (classLoader)
      {
      if (0 && *((uint32_t *)classLoader->classLoaderObject) != 0x99669966)
         {
         RELO_LOG(reloRuntime->reloLogger(), 1,"\tpreparePrivateData: SUSPICIOUS class loader found\n");
         RELO_LOG(reloRuntime->reloLogger(), 1,"\tpreparePrivateData: inlinedCodeRomClass %p %.*s\n", inlinedCodeRomClass, inlinedCodeClassName->length, inlinedCodeClassName->data);
         RELO_LOG(reloRuntime->reloLogger(), 1,"\tpreparePrivateData: classChainIdentifyingLoader %p\n", classChainIdentifyingLoader);
         RELO_LOG(reloRuntime->reloLogger(), 1,"\tpreparePrivateData: classLoader %p\n", classLoader);
         }
      J9JavaVM *javaVM = reloRuntime->jitConfig()->javaVM;
      J9VMThread *vmThread = reloRuntime->currentThread();

      TR_OpaqueClassBlock *inlinedCodeClass;

         {
         TR::VMAccessCriticalSection preparePrivateDataCriticalSection(reloRuntime->fej9());
         inlinedCodeClass = (TR_OpaqueClassBlock *) jitGetClassInClassloaderFromUTF8(vmThread,
                                                                                     classLoader,
                                                                                     J9UTF8_DATA(inlinedCodeClassName),
                                                                                     J9UTF8_LENGTH(inlinedCodeClassName));
         }

      if (inlinedCodeClass && checkInlinedClassValidity(reloRuntime, inlinedCodeClass))
         {
         RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: inlined class valid\n");
         reloPrivateData->_inlinedCodeClass = inlinedCodeClass;
         uintptrj_t *chainData = (uintptrj_t *) reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) classChainForInlinedMethod(reloTarget));
         if (reloRuntime->fej9()->sharedCache()->classMatchesCachedVersion(inlinedCodeClass, chainData))
            {
            RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classes match\n");
            TR_OpaqueMethodBlock *inlinedMethod = * (TR_OpaqueMethodBlock **) (((uint8_t *)reloPrivateData->_inlinedCodeClass) + vTableSlot(reloTarget));

            if (reloRuntime->fej9()->isAnyMethodTracingEnabled(inlinedMethod))
               {
               RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: target may need enter/exit tracing so disable inline site\n");
               }
            else
               {
               fixInlinedSiteInfo(reloRuntime, reloTarget, inlinedMethod);

               reloPrivateData->_needUnloadAssumption = !reloRuntime->fej9()->sameClassLoaders(inlinedCodeClass, reloRuntime->comp()->getCurrentMethod()->classOfMethod());
               setupInlinedMethodData(reloRuntime, reloTarget);
               failValidation = false;
               }
            }
         }
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
   reloPrivateData->_guardValue = (uintptrj_t) reloPrivateData->_inlinedCodeClass;
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
   TR_OpaqueMethodBlock *inlinedMethod = * (TR_OpaqueMethodBlock **) (((uint8_t *)reloPrivateData->_inlinedCodeClass) + vTableSlot(reloTarget));
   reloPrivateData->_guardValue = (uintptrj_t) inlinedMethod;
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

int32_t
TR_RelocationRecordMethodTracingCheck::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordMethodTracingCheckBinaryTemplate);
   }

void
TR_RelocationRecordMethodTracingCheck::setDestinationAddress(TR_RelocationTarget *reloTarget, uintptrj_t destinationAddress)
   {
   reloTarget->storeRelocationRecordValue(destinationAddress, (uintptrj_t *) &((TR_RelocationRecordMethodTracingCheckBinaryTemplate *)_record)->_destinationAddress);
   }

uintptrj_t
TR_RelocationRecordMethodTracingCheck::destinationAddress(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordMethodTracingCheckBinaryTemplate *)_record)->_destinationAddress);
   }

void
TR_RelocationRecordMethodTracingCheck::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordMethodTracingCheckPrivateData *reloPrivateData = &(privateData()->methodTracingCheck);

   uintptrj_t destination = destinationAddress(reloTarget);
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

bool
TR_RelocationRecordMethodEnterCheck::ignore(TR_RelocationRuntime *reloRuntime)
   {
   bool reportMethodEnter = reloRuntime->fej9()->isMethodEnterTracingEnabled((TR_OpaqueMethodBlock *) reloRuntime->method()) || reloRuntime->fej9()->canMethodEnterEventBeHooked();
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tignore: reportMethodEnter %d\n", reportMethodEnter);
   return !reportMethodEnter;
   }


// TR_MethodExitCheck
char *
TR_RelocationRecordMethodExitCheck::name()
   {
   return "TR_MethodExitCheck";
   }

bool
TR_RelocationRecordMethodExitCheck::ignore(TR_RelocationRuntime *reloRuntime)
   {
   bool reportMethodExit = reloRuntime->fej9()->isMethodExitTracingEnabled((TR_OpaqueMethodBlock *) reloRuntime->method()) || reloRuntime->fej9()->canMethodExitEventBeHooked();
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tignore: reportMethodExit %d\n", reportMethodExit);
   return !reportMethodExit;
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

int32_t
TR_RelocationRecordValidateClass::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordValidateClassBinaryTemplate);
   }

void
TR_RelocationRecordValidateClass::setClassChainOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptrj_t classChainOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(classChainOffsetInSharedCache, (uintptrj_t *) &((TR_RelocationRecordValidateClassBinaryTemplate *)_record)->_classChainOffsetInSharedCache);
   }

uintptrj_t
TR_RelocationRecordValidateClass::classChainOffsetInSharedCache(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordValidateClassBinaryTemplate *)_record)->_classChainOffsetInSharedCache);
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
   return reloRuntime->fej9()->sharedCache()->classMatchesCachedVersion(clazz, (uintptrj_t *) classChain);
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
      void *classChain = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) classChainOffsetInSharedCache(reloTarget));
      verified = validateClass(reloRuntime, definingClass, classChain);
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
                                                                                           J9_RESOLVE_FLAG_AOT_LOAD_TIME);
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
      J9JavaVM *javaVM = reloRuntime->javaVM();
      definingClass = reloRuntime->getClassFromCP(javaVM->internalVMFunctions->currentVMThread(javaVM), javaVM, (J9ConstantPool *) void_cp, cpIndex(reloTarget), false);
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

int32_t
TR_RelocationRecordValidateStaticField::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordValidateStaticFieldBinaryTemplate);
   }

void
TR_RelocationRecordValidateStaticField::setRomClassOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptrj_t romClassOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(romClassOffsetInSharedCache, (uintptrj_t *) &((TR_RelocationRecordValidateStaticFieldBinaryTemplate *)_record)->_romClassOffsetInSharedCache);
   }

uintptrj_t
TR_RelocationRecordValidateStaticField::romClassOffsetInSharedCache(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordValidateStaticFieldBinaryTemplate *)_record)->_romClassOffsetInSharedCache);
   }

TR_OpaqueClassBlock *
TR_RelocationRecordValidateStaticField::getClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, void *void_cp)
   {
   TR_OpaqueClassBlock *definingClass = NULL;
   if (void_cp)
      {
      J9JavaVM *javaVM = reloRuntime->javaVM();
      definingClass = reloRuntime->getClassFromCP(javaVM->internalVMFunctions->currentVMThread(javaVM), javaVM, (J9ConstantPool *) void_cp, cpIndex(reloTarget), true);
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
TR_RelocationRecordValidateArbitraryClass::setClassChainIdentifyingLoaderOffset(TR_RelocationTarget *reloTarget, uintptrj_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptrj_t *) &((TR_RelocationRecordValidateArbitraryClassBinaryTemplate *)_record)->_loaderClassChainOffset);
   }

uintptrj_t
TR_RelocationRecordValidateArbitraryClass::classChainIdentifyingLoaderOffset(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordValidateArbitraryClassBinaryTemplate *)_record)->_loaderClassChainOffset);
   }

void
TR_RelocationRecordValidateArbitraryClass::setClassChainOffsetForClassBeingValidated(TR_RelocationTarget *reloTarget, uintptrj_t offset)
   {
   reloTarget->storeRelocationRecordValue(offset, (uintptrj_t *) &((TR_RelocationRecordValidateArbitraryClassBinaryTemplate *)_record)->_classChainOffsetForClassBeingValidated);
   }

uintptrj_t
TR_RelocationRecordValidateArbitraryClass::classChainOffsetForClassBeingValidated(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordValidateArbitraryClassBinaryTemplate *)_record)->_classChainOffsetForClassBeingValidated);
   }

int32_t
TR_RelocationRecordValidateArbitraryClass::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordValidateArbitraryClassBinaryTemplate);
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

   void *classChainIdentifyingLoader = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void*)classChainIdentifyingLoaderOffset(reloTarget));
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tpreparePrivateData: classChainIdentifyingLoader %p\n", classChainIdentifyingLoader);

   J9ClassLoader *classLoader = (J9ClassLoader *) reloRuntime->fej9()->sharedCache()->persistentClassLoaderTable()->lookupClassLoaderAssociatedWithClassChain(classChainIdentifyingLoader);
   RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tpreparePrivateData: classLoader %p\n", classLoader);

   if (classLoader)
      {
      uintptrj_t *classChainForClassBeingValidated = (uintptrj_t *) reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void*)classChainOffsetForClassBeingValidated(reloTarget));
      TR_OpaqueClassBlock *clazz = reloRuntime->fej9()->sharedCache()->lookupClassFromChainAndLoader(classChainForClassBeingValidated, classLoader);
      RELO_LOG(reloRuntime->reloLogger(), 6, "\t\tpreparePrivateData: clazz %p\n", clazz);

      if (clazz)
         return 0;
      }

   if (aotStats)
      aotStats->numClassValidationsFailed++;

   return compilationAotClassReloFailure;
   }


// TR_HCR
char *
TR_RelocationRecordHCR::name()
   {
   return "TR_HCR";
   }

bool
TR_RelocationRecordHCR::ignore(TR_RelocationRuntime *reloRuntime)
   {
   bool hcrEnabled = reloRuntime->options()->getOption(TR_EnableHCR);
   RELO_LOG(reloRuntime->reloLogger(), 6,"\tignore: hcrEnabled %d\n", hcrEnabled);
   return !hcrEnabled;
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
		   locationSize = sizeof(uintptrj_t);
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

int32_t
TR_RelocationRecordPointer::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordPointerBinaryTemplate);
   }

bool
TR_RelocationRecordPointer::ignore(TR_RelocationRuntime *reloRuntime)
   {
   // pointers must be updated because they tend to be guards controlling entry to inlined code
   // so even if the inlined site is disabled, the guard value needs to be changed to -1
   return false;
   }

void
TR_RelocationRecordPointer::setClassChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptrj_t classChainIdentifyingLoaderOffsetInSharedCache)
   {
   reloTarget->storeRelocationRecordValue(classChainIdentifyingLoaderOffsetInSharedCache, (uintptrj_t *) &((TR_RelocationRecordPointerBinaryTemplate *)_record)->_classChainIdentifyingLoaderOffsetInSharedCache);
   }

uintptrj_t
TR_RelocationRecordPointer::classChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordPointerBinaryTemplate *)_record)->_classChainIdentifyingLoaderOffsetInSharedCache);
   }

void
TR_RelocationRecordPointer::setClassChainForInlinedMethod(TR_RelocationTarget *reloTarget, uintptrj_t classChainForInlinedMethod)
   {
   reloTarget->storeRelocationRecordValue(classChainForInlinedMethod, (uintptrj_t *) &((TR_RelocationRecordPointerBinaryTemplate *)_record)->_classChainForInlinedMethod);
   }

uintptrj_t
TR_RelocationRecordPointer::classChainForInlinedMethod(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordPointerBinaryTemplate *)_record)->_classChainForInlinedMethod);
   }

void
TR_RelocationRecordPointer::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordPointerPrivateData *reloPrivateData = &(privateData()->pointer);

   J9Class *classPointer = NULL;
   if (getInlinedSiteMethod(reloRuntime, inlinedSiteIndex(reloTarget)) != (TR_OpaqueMethodBlock *)-1)
      {
      J9ClassLoader *classLoader = NULL;
#if !defined(PUBLIC_BUILD)
      void *classChainIdentifyingLoader = reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) classChainIdentifyingLoaderOffsetInSharedCache(reloTarget));
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classChainIdentifyingLoader %p\n", classChainIdentifyingLoader);
      classLoader = (J9ClassLoader *) reloRuntime->fej9()->sharedCache()->persistentClassLoaderTable()->lookupClassLoaderAssociatedWithClassChain(classChainIdentifyingLoader);
#endif
      RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classLoader %p\n", classLoader);

      if (classLoader != NULL)
         {
         uintptrj_t *classChain = (uintptrj_t *) reloRuntime->fej9()->sharedCache()->pointerFromOffsetInSharedCache((void *) classChainForInlinedMethod(reloTarget));
         RELO_LOG(reloRuntime->reloLogger(), 6,"\tpreparePrivateData: classChain %p\n", classChain);

#if !defined(PUBLIC_BUILD)
         classPointer = (J9Class *) reloRuntime->fej9()->sharedCache()->lookupClassFromChainAndLoader(classChain, (void *) classLoader);
#endif
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
      reloPrivateData->_pointer = (uintptrj_t) -1;
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
   createClassRedefinitionPicSite((void *) reloPrivateData->_pointer, (void *) reloLocation, sizeof(uintptrj_t), false, reloRuntime->comp()->getMetadataAssumptionList());
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

uintptrj_t
TR_RelocationRecordClassPointer::computePointer(TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *classPointer)
   {
   return (uintptrj_t) classPointer;
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
TR_RelocationRecordMethodPointer::setVTableSlot(TR_RelocationTarget *reloTarget, uintptrj_t vTableSlot)
   {
   reloTarget->storeRelocationRecordValue(vTableSlot, (uintptrj_t *) &((TR_RelocationRecordMethodPointerBinaryTemplate *)_record)->_vTableSlot);
   }

uintptrj_t
TR_RelocationRecordMethodPointer::vTableSlot(TR_RelocationTarget *reloTarget)
   {
   return reloTarget->loadRelocationRecordValue((uintptrj_t *) &((TR_RelocationRecordMethodPointerBinaryTemplate *)_record)->_vTableSlot);
   }

int32_t
TR_RelocationRecordMethodPointer::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordMethodPointerBinaryTemplate);
   }

uintptrj_t
TR_RelocationRecordMethodPointer::computePointer(TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *classPointer)
   {
   //return (uintptrj_t) getInlinedSiteMethod(reloTarget->reloRuntime(), inlinedSiteIndex(reloTarget));

   J9Method *method = (J9Method *) *(uintptrj_t *)(((uint8_t *)classPointer) + vTableSlot(reloTarget));
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

   return (uintptrj_t) method;
   }

void
TR_RelocationRecordMethodPointer::activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordPointer::activatePointer(reloRuntime, reloTarget, reloLocation);
   if (reloRuntime->options()->getOption(TR_EnableHCR))
      registerHCRAssumption(reloRuntime, reloLocation);
   }

int32_t
TR_RelocationRecordEmitClass::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordEmitClassBinaryTemplate);
   }

void
TR_RelocationRecordEmitClass::preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget)
   {
   TR_RelocationRecordEmitClassPrivateData *reloPrivateData = &(privateData()->emitClass);

   reloPrivateData->_bcIndex              = reloTarget->loadSigned32b((uint8_t *) &(((TR_RelocationRecordEmitClassBinaryTemplate *)_record)->_bcIndex));
   reloPrivateData->_method               = getInlinedSiteMethod(reloRuntime);
   }

int32_t
TR_RelocationRecordEmitClass::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   TR_RelocationRecordEmitClassPrivateData *reloPrivateData = &(privateData()->emitClass);

   reloRuntime->addClazzRecord(reloLocation, reloPrivateData->_bcIndex, reloPrivateData->_method);
   return 0;
   }


char *
TR_RelocationRecordDebugCounter::name()
   {
   return "TR_RelocationRecordDebugCounter";
   }

int32_t
TR_RelocationRecordDebugCounter::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordDebugCounterBinaryTemplate);
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

   reloPrivateData->_bcIndex     = reloTarget->loadSigned32b((uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_bcIndex));
   reloPrivateData->_delta       = reloTarget->loadSigned32b((uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_delta));
   reloPrivateData->_fidelity    = reloTarget->loadUnsigned8b((uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_fidelity));
   reloPrivateData->_staticDelta = reloTarget->loadSigned32b((uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_staticDelta));

   UDATA offset                  = (UDATA)reloTarget->loadPointer((uint8_t *) &(((TR_RelocationRecordDebugCounterBinaryTemplate *)_record)->_offsetOfNameString));
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
TR_RelocationRecordClassUnloadAssumption::bytesInHeaderAndPayload()
   {
   return sizeof(TR_RelocationRecordClassUnloadAssumptionBinaryTemplate);
   }

int32_t
TR_RelocationRecordClassUnloadAssumption::applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
   {
   reloTarget->addPICtoPatchPtrOnClassUnload((TR_OpaqueClassBlock *) -1, reloLocation);
   return 0;
   }


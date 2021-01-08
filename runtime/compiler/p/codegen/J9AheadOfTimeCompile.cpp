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

#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "compile/AOTClassInfo.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CHTable.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/SharedCache.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "ras/DebugCounter.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRecord.hpp"
#include "runtime/SymbolValidationManager.hpp"

J9::Power::AheadOfTimeCompile::AheadOfTimeCompile(TR::CodeGenerator *cg) :
         J9::AheadOfTimeCompile(_relocationTargetTypeToHeaderSizeMap, cg->comp()),
         _cg(cg)
   {
   }

void J9::Power::AheadOfTimeCompile::processRelocations()
   {
   TR::Compilation *comp = _cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_cg->fe());
   TR::IteratedExternalRelocation *r;

   for (auto iterator = getRelocationList().begin(); iterator != getRelocationList().end(); ++iterator)
      (*iterator)->mapRelocation(_cg);

   auto aotIterator = _cg->getExternalRelocationList().begin();
   while (aotIterator != _cg->getExternalRelocationList().end())
      {
	  (*aotIterator)->addExternalRelocation(_cg);
      ++aotIterator;
      }

   for (r = getAOTRelocationTargets().getFirst(); r != NULL; r = r->getNext())
      {
      addToSizeOfAOTRelocations(r->getSizeOfRelocationData());
      }

   // now allocate the memory  size of all iterated relocations + the header (total length field)

   // Note that when using the SymbolValidationManager, the well-known classes
   // must be checked even if no explicit records were generated, since they
   // might be responsible for the lack of records.
   bool useSVM = comp->getOption(TR_UseSymbolValidationManager);
   if (self()->getSizeOfAOTRelocations() != 0 || useSVM)
      {
      // It would be more straightforward to put the well-known classes offset
      // in the AOT method header, but that would use space for AOT bodies that
      // don't use the SVM. TODO: Move it once SVM takes over?
      int wellKnownClassesOffsetSize = useSVM ? SIZEPOINTER : 0;
      uintptr_t reloBufferSize =
         self()->getSizeOfAOTRelocations() + SIZEPOINTER + wellKnownClassesOffsetSize;
      uint8_t *relocationDataCursor = self()->setRelocationData(
         fej9->allocateRelocationData(comp, reloBufferSize));
      // set up the size for the region
      *(uintptr_t*)relocationDataCursor = reloBufferSize;
      relocationDataCursor += SIZEPOINTER;

      if (useSVM)
         {
         TR::SymbolValidationManager *svm =
            comp->getSymbolValidationManager();
         void *offsets = const_cast<void*>(svm->wellKnownClassChainOffsets());
         *(uintptr_t *)relocationDataCursor =
            self()->offsetInSharedCacheFromPointer(fej9->sharedCache(), offsets);
         relocationDataCursor += SIZEPOINTER;
         }

      // set up pointers for each iterated relocation and initialize header
      TR::IteratedExternalRelocation *s;
      for (s = getAOTRelocationTargets().getFirst(); s != NULL; s = s->getNext())
         {
         s->setRelocationData(relocationDataCursor);
         s->initializeRelocation(_cg);
         relocationDataCursor += s->getSizeOfRelocationData();
         }
      }
   }

uint8_t *J9::Power::AheadOfTimeCompile::initializeAOTRelocationHeader(TR::IteratedExternalRelocation *relocation)
   {
   TR::Compilation *comp = _cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_cg->fe());
   TR_SharedCache *sharedCache = fej9->sharedCache();
   TR_RelocationRuntime *reloRuntime = comp->reloRuntime();
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();

   uint8_t * aotMethodCodeStart = (uint8_t *) comp->getRelocatableMethodCodeStart();
   uint8_t flags = 0;

   uint8_t *cursor         = relocation->getRelocationData();
   uint8_t targetKind      = relocation->getTargetKind();
   uint16_t sizeOfReloData = relocation->getSizeOfRelocationData();

   // Zero-initialize header
   memset(cursor, 0, sizeOfReloData);

   TR_RelocationRecord storage;
   TR_RelocationRecord *reloRecord = TR_RelocationRecord::create(&storage, reloRuntime, targetKind, reinterpret_cast<TR_RelocationRecordBinaryTemplate *>(cursor));

   uint8_t wideOffsets = relocation->needsWideOffsets() ? RELOCATION_TYPE_WIDE_OFFSET : 0;
   reloRecord->setSize(reloTarget, sizeOfReloData);
   reloRecord->setType(reloTarget, static_cast<TR_RelocationRecordType>(targetKind));
   reloRecord->setFlag(reloTarget, wideOffsets);

   switch (targetKind)
      {
      case TR_ClassAddress:
         {
         TR_RelocationRecordClassAddress *caRecord = reinterpret_cast<TR_RelocationRecordClassAddress *>(reloRecord);
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress();

         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(recordInfo->data1);
         uintptr_t inlinedSiteIndex = reinterpret_cast<uintptr_t>(recordInfo->data2);
         uint8_t flags = static_cast<uint8_t>(recordInfo->data3);

         void *constantPool = symRef->getOwningMethod(comp)->constantPool();
         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(constantPool, inlinedSiteIndex);

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         caRecord->setReloFlags(reloTarget, flags);
         caRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         caRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(constantPool));
         caRecord->setCpIndex(reloTarget, symRef->getCPIndex());

         cursor = relocation->getRelocationData() + TR_RelocationRecord::getSizeOfAOTRelocationHeader(static_cast<TR_RelocationRecordType>(targetKind));
         }
         break;

      case TR_DataAddress:
         {
         TR_RelocationRecordDataAddress *daRecord = reinterpret_cast<TR_RelocationRecordDataAddress *>(reloRecord);
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress();

         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(recordInfo->data1);
         uintptr_t inlinedSiteIndex = reinterpret_cast<uintptr_t>(recordInfo->data2);
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(recordInfo->data3));

         void *constantPool = symRef->getOwningMethod(comp)->constantPool();
         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(constantPool, inlinedSiteIndex);

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         daRecord->setReloFlags(reloTarget, flags);
         daRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         daRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(constantPool));
         daRecord->setCpIndex(reloTarget, symRef->getCPIndex());
         daRecord->setOffset(reloTarget, symRef->getOffset());

         cursor = relocation->getRelocationData() + TR_RelocationRecord::getSizeOfAOTRelocationHeader(static_cast<TR_RelocationRecordType>(targetKind));
         }
         break;

      case TR_AbsoluteMethodAddressOrderedPair:
         {
         TR_RelocationRecordMethodAddress *maRecord = reinterpret_cast<TR_RelocationRecordMethodAddress *>(reloRecord);

         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *) relocation->getTargetAddress();
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(recordInfo->data3));

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         maRecord->setReloFlags(reloTarget, flags);

         cursor = relocation->getRelocationData() + TR_RelocationRecord::getSizeOfAOTRelocationHeader(static_cast<TR_RelocationRecordType>(targetKind));
         }
         break;

      case TR_FixedSequenceAddress:
         {
         TR_RelocationRecordWithOffset *rwoRecord = reinterpret_cast<TR_RelocationRecordWithOffset *>(reloRecord);

         TR::LabelSymbol *table = reinterpret_cast<TR::LabelSymbol *>(relocation->getTargetAddress());
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));
         uint8_t *codeLocation = table->getCodeLocation();

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         rwoRecord->setReloFlags(reloTarget, flags);
         if (comp->target().is64Bit())
            {
            rwoRecord->setOffset(reloTarget, static_cast<uintptr_t>(codeLocation - aotMethodCodeStart));
            }
         else
            {
            TR_ASSERT_FATAL(false, "Creating TR_FixedSeqAddress/TR_FixedSeq2Address relo for 32-bit target");
            }

         cursor = relocation->getRelocationData() + TR_RelocationRecord::getSizeOfAOTRelocationHeader(static_cast<TR_RelocationRecordType>(targetKind));
         }
         break;

      case TR_FixedSequenceAddress2:
         {
         TR_RelocationRecordWithOffset *rwoRecord = reinterpret_cast<TR_RelocationRecordWithOffset *>(reloRecord);
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         rwoRecord->setReloFlags(reloTarget, flags);
         if (comp->target().is64Bit())
            {
            TR_ASSERT_FATAL(relocation->getTargetAddress(), "target address is NULL");

            uintptr_t offset = relocation->getTargetAddress()
                     ? static_cast<uintptr_t>(relocation->getTargetAddress() - aotMethodCodeStart)
                     : 0x0;

            rwoRecord->setOffset(reloTarget, offset);
            }
         else
            {
            TR_ASSERT_FATAL(0, "Creating TR_LoadAddress/TR_LoadAddressTempReg relo for 32-bit target");
            }

         cursor = relocation->getRelocationData() + TR_RelocationRecord::getSizeOfAOTRelocationHeader(static_cast<TR_RelocationRecordType>(targetKind));
         }
         break;

      case TR_ArrayCopyHelper:
      case TR_ArrayCopyToc:
      case TR_BodyInfoAddressLoad:
      case TR_RecompQueuedFlag:
         {
         TR_RelocationRecord *rRecord = reinterpret_cast<TR_RelocationRecord *>(reloRecord);
         uint8_t flags;

         if (comp->target().is64Bit())
            {
            flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));
            }
         else
            {
            TR_RelocationRecordInformation *recordInfo = reinterpret_cast<TR_RelocationRecordInformation *>(relocation->getTargetAddress());
            flags = static_cast<uint8_t>(recordInfo->data3);
            }

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         rRecord->setReloFlags(reloTarget, flags);

         cursor = relocation->getRelocationData() + TR_RelocationRecord::getSizeOfAOTRelocationHeader(static_cast<TR_RelocationRecordType>(targetKind));
         }
         break;

      case TR_RamMethodSequence:
         {
         TR_RelocationRecordRamSequence *rsRecord = reinterpret_cast<TR_RelocationRecordRamSequence *>(reloRecord);
         uint8_t flags;

         if (comp->target().is64Bit())
            {
            flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));

            TR_ASSERT_FATAL(relocation->getTargetAddress(), "target address is NULL");

            uintptr_t offset = relocation->getTargetAddress()
                     ? static_cast<uintptr_t>(relocation->getTargetAddress() - aotMethodCodeStart)
                     : 0x0;

            rsRecord->setOffset(reloTarget, offset);
            }
         else
            {
            TR_RelocationRecordInformation *recordInfo = reinterpret_cast<TR_RelocationRecordInformation *>(relocation->getTargetAddress());
            flags = static_cast<uint8_t>(recordInfo->data3);

            // Skip Offset
            }

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         rsRecord->setReloFlags(reloTarget, flags);

         cursor = relocation->getRelocationData() + TR_RelocationRecord::getSizeOfAOTRelocationHeader(static_cast<TR_RelocationRecordType>(targetKind));
         }
         break;

      case TR_ArbitraryClassAddress:
         {
         TR_RelocationRecordArbitraryClassAddress *acaRecord = reinterpret_cast<TR_RelocationRecordArbitraryClassAddress *>(reloRecord);

         // ExternalRelocation data is as expected for TR_ClassAddress
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress();

         auto symRef = (TR::SymbolReference *)recordInfo->data1;
         auto sym = symRef->getSymbol()->castToStaticSymbol();
         auto j9class = (TR_OpaqueClassBlock *)sym->getStaticAddress();
         uint8_t flags = (uint8_t)recordInfo->data3;
         uintptr_t inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(symRef->getOwningMethod(comp)->constantPool(), recordInfo->data2);

         uintptr_t classChainIdentifyingLoaderOffsetInSharedCache = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(j9class);
         uintptr_t classChainOffsetInSharedCache = self()->getClassChainOffset(j9class);

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         acaRecord->setReloFlags(reloTarget, flags);
         acaRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         acaRecord->setClassChainIdentifyingLoaderOffsetInSharedCache(reloTarget, classChainIdentifyingLoaderOffsetInSharedCache);
         acaRecord->setClassChainForInlinedMethod(reloTarget, classChainOffsetInSharedCache);

         cursor = relocation->getRelocationData() + TR_RelocationRecord::getSizeOfAOTRelocationHeader(static_cast<TR_RelocationRecordType>(targetKind));
         }
         break;

      case TR_GlobalValue:
         {
         TR_RelocationRecordGlobalValue *gvRecord = reinterpret_cast<TR_RelocationRecordGlobalValue *>(reloRecord);

         uintptr_t gv;
         uint8_t flags;

         if (comp->target().is64Bit())
            {
            gv = reinterpret_cast<uintptr_t>(relocation->getTargetAddress());
            flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));
            }
         else
            {
            TR_RelocationRecordInformation *recordInfo = reinterpret_cast<TR_RelocationRecordInformation*>(relocation->getTargetAddress());
            gv = recordInfo->data1;
            flags = static_cast<uint8_t>(recordInfo->data3);
            }

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         gvRecord->setReloFlags(reloTarget, flags);
         gvRecord->setOffset(reloTarget, gv);

         cursor = relocation->getRelocationData() + TR_RelocationRecord::getSizeOfAOTRelocationHeader(static_cast<TR_RelocationRecordType>(targetKind));
         }
         break;

      case TR_DiscontiguousSymbolFromManager:
         {
         TR_RelocationRecordDiscontiguousSymbolFromManager *dsfmRecord = reinterpret_cast<TR_RelocationRecordDiscontiguousSymbolFromManager *>(reloRecord);

         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress();

         uint8_t *symbol = (uint8_t *)recordInfo->data1;
         uint16_t symbolID = comp->getSymbolValidationManager()->getIDFromSymbol(static_cast<void *>(symbol));

         uint16_t symbolType = (uint16_t)recordInfo->data2;

         uint8_t flags = (uint8_t) recordInfo->data3;
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");

         dsfmRecord->setSymbolID(reloTarget, symbolID);
         dsfmRecord->setSymbolType(reloTarget, static_cast<TR::SymbolType>(symbolType));
         dsfmRecord->setReloFlags(reloTarget, flags);

         cursor = relocation->getRelocationData() + TR_RelocationRecord::getSizeOfAOTRelocationHeader(static_cast<TR_RelocationRecordType>(targetKind));
         }
         break;

      case TR_HCR:
         {
         TR_RelocationRecordHCR *hcrRecord = reinterpret_cast<TR_RelocationRecordHCR *>(reloRecord);

         uintptr_t gv = reinterpret_cast<uintptr_t>(relocation->getTargetAddress());
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         hcrRecord->setReloFlags(reloTarget, flags);
         hcrRecord->setOffset(reloTarget, gv);

         cursor = relocation->getRelocationData() + TR_RelocationRecord::getSizeOfAOTRelocationHeader(static_cast<TR_RelocationRecordType>(targetKind));
         }
         break;

      default:
         cursor = self()->initializeCommonAOTRelocationHeader(relocation, reloRecord);

      }
   return cursor;
   }

#if defined (TR_HOST_64BIT)
uint32_t J9::Power::AheadOfTimeCompile::_relocationTargetTypeToHeaderSizeMap[TR_NumExternalRelocationKinds] =
   {
   24,                                       // TR_ConstantPool                        = 0
   8,                                        // TR_HelperAddress                       = 1
   24,                                       // TR_RelativeMethodAddress               = 2
   8,                                        // TR_AbsoluteMethodAddress               = 3
   40,                                       // TR_DataAddress                         = 4
   24,                                       // TR_ClassObject                         = 5
   24,                                       // TR_MethodObject                        = 6
   24,                                       // TR_InterfaceObject                     = 7
   8,                                        // TR_AbsoluteHelperAddress               = 8
   16,                                       // TR_FixedSequenceAddress                = 9
   16,                                       // TR_FixedSequenceAddress2               = 10
   32,                                       // TR_JNIVirtualTargetAddress             = 11
   32,                                       // TR_JNIStaticTargetAddress              = 12
   8,                                        // TR_ArrayCopyHelper                     = 13
   8,                                        // TR_ArrayCopyToc                        = 14
   8,                                        // TR_BodyInfoAddress                     = 15
   24,                                       // TR_Thunks                              = 16
   32,                                       // TR_StaticRamMethodConst                = 17
   24,                                       // TR_Trampolines                         = 18
   8,                                        // Dummy for TR_PicTrampolines            = 19
   16,                                       // TR_CheckMethodEnter                    = 20
   8,                                        // TR_RamMethod                           = 21
   16,                                       // TR_RamMethodSequence                   = 22
   16,                                       // TR_RamMethodSequenceReg                = 23
   48,                                       // TR_VerifyClassObjectForAlloc           = 24
   24,                                       // TR_ConstantPoolOrderedPair             = 25
   8,                                        // TR_AbsoluteMethodAddressOrderedPair    = 26
   40,                                       // TR_VerifyRefArrayForAlloc              = 27
   24,                                       // TR_J2IThunks                           = 28
   16,                                       // TR_GlobalValue                         = 29
   8,                                        // TR_BodyInfoAddressLoad                 = 30
   40,                                       // TR_ValidateInstanceField               = 31
   48,                                       // TR_InlinedStaticMethodWithNopGuard     = 32
   48,                                       // TR_InlinedSpecialMethodWithNopGuard    = 33
   48,                                       // TR_InlinedVirtualMethodWithNopGuard    = 34
   48,                                       // TR_InlinedInterfaceMethodWithNopGuard  = 35
   32,                                       // TR_SpecialRamMethodConst               = 36
   48,                                       // TR_InlinedHCRMethod                    = 37
   40,                                       // TR_ValidateStaticField                 = 38
   40,                                       // TR_ValidateClass                       = 39
   32,                                       // TR_ClassAddress                        = 40
   16,                                       // TR_HCR                                 = 41
   64,                                       // TR_ProfiledMethodGuardRelocation       = 42
   64,                                       // TR_ProfiledClassGuardRelocation        = 43
   0,                                        // TR_HierarchyGuardRelocation            = 44
   0,                                        // TR_AbstractGuardRelocation             = 45
   64,                                       // TR_ProfiledInlinedMethod               = 46
   40,                                       // TR_MethodPointer                       = 47
   32,                                       // TR_ClassPointer                        = 48
   16,                                       // TR_CheckMethodExit                     = 49
   24,                                       // TR_ValidateArbitraryClass              = 50
   0,                                        // TR_EmitClass(not used)                 = 51
   32,                                       // TR_JNISpecialTargetAddress             = 52
   32,                                       // TR_VirtualRamMethodConst               = 53
   40,                                       // TR_InlinedInterfaceMethod              = 54
   40,                                       // TR_InlinedVirtualMethod                = 55
   0,                                        // TR_NativeMethodAbsolute                = 56,
   0,                                        // TR_NativeMethodRelative                = 57,
   32,                                       // TR_ArbitraryClassAddress               = 58,
   56,                                       // TR_DebugCounter                        = 59
   8,                                        // TR_ClassUnloadAssumption               = 60
   32,                                       // TR_J2IVirtualThunkPointer              = 61
   48,                                              // TR_InlinedAbstractMethodWithNopGuard   = 62,
   0,                                                                  // TR_ValidateRootClass                   = 63,
   sizeof(TR_RelocationRecordValidateClassByNameBinaryTemplate),       // TR_ValidateClassByName                 = 64,
   sizeof(TR_RelocationRecordValidateProfiledClassBinaryTemplate),     // TR_ValidateProfiledClass               = 65,
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),       // TR_ValidateClassFromCP                 = 66,
   sizeof(TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate),//TR_ValidateDefiningClassFromCP         = 67,
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),       // TR_ValidateStaticClassFromCP           = 68,
   0,                                                                  // TR_ValidateClassFromMethod             = 69,
   0,                                                                  // TR_ValidateComponentClassFromArrayClass= 70,
   sizeof(TR_RelocationRecordValidateArrayFromCompBinaryTemplate),     // TR_ValidateArrayClassFromComponentClass= 71,
   sizeof(TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate),//TR_ValidateSuperClassFromClass         = 72,
   sizeof(TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate),//TR_ValidateClassInstanceOfClass       = 73,
   sizeof(TR_RelocationRecordValidateSystemClassByNameBinaryTemplate), //TR_ValidateSystemClassByName            = 74,
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),       //TR_ValidateClassFromITableIndexCP       = 75,
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),       //TR_ValidateDeclaringClassFromFieldOrStatic=76,
   0,                                                                  // TR_ValidateClassClass                  = 77,
   sizeof(TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate),//TR_ValidateConcreteSubClassFromClass   = 78,
   sizeof(TR_RelocationRecordValidateClassChainBinaryTemplate),        // TR_ValidateClassChain                  = 79,
   0,                                                                  // TR_ValidateRomClass                    = 80,
   0,                                                                  // TR_ValidatePrimitiveClass              = 81,
   0,                                                                  // TR_ValidateMethodFromInlinedSite       = 82,
   0,                                                                  // TR_ValidatedMethodByName               = 83,
   sizeof(TR_RelocationRecordValidateMethodFromClassBinaryTemplate),   // TR_ValidatedMethodFromClass            = 84,
   sizeof(TR_RelocationRecordValidateMethodFromCPBinaryTemplate),      // TR_ValidateStaticMethodFromCP          = 85,
   sizeof(TR_RelocationRecordValidateMethodFromCPBinaryTemplate),      //TR_ValidateSpecialMethodFromCP         = 86,
   sizeof(TR_RelocationRecordValidateMethodFromCPBinaryTemplate),      //TR_ValidateVirtualMethodFromCP         = 87,
   sizeof(TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate),//TR_ValidateVirtualMethodFromOffset = 88,
   sizeof(TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate),//TR_ValidateInterfaceMethodFromCP     = 89,
   sizeof(TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate),//TR_ValidateMethodFromClassAndSig     = 90,
   sizeof(TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate),//TR_ValidateStackWalkerMaySkipFramesRecord= 91,
   0,                                                                     //TR_ValidateArrayClassFromJavaVM      = 92,
   sizeof(TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate),//TR_ValidateClassInfoIsInitialized   = 93,
   sizeof(TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate),//TR_ValidateMethodFromSingleImplementer= 94,
   sizeof(TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate),//TR_ValidateMethodFromSingleInterfaceImplementer= 95,
   sizeof(TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate),//TR_ValidateMethodFromSingleAbstractImplementer= 96,
   sizeof(TR_RelocationRecordValidateMethodFromCPBinaryTemplate),      //TR_ValidateImproperInterfaceMethodFromCP= 97,
   sizeof(TR_RelocationRecordSymbolFromManagerBinaryTemplate),         // TR_SymbolFromManager = 98,
   sizeof(TR_RelocationRecordMethodCallAddressBinaryTemplate),         // TR_MethodCallAddress = 99,
   sizeof(TR_RelocationRecordSymbolFromManagerBinaryTemplate),         // TR_DiscontiguousSymbolFromManager = 100,
   sizeof(TR_RelocationRecordResolvedTrampolinesBinaryTemplate),       // TR_ResolvedTrampolines = 101,
   sizeof(TR_RelocationRecordBlockFrequencyBinaryTemplate),            // TR_BlockFrequency = 102,
   sizeof(TR_RelocationRecordBinaryTemplate),                          // TR_RecompQueuedFlag = 103,
   };
#else
uint32_t J9::Power::AheadOfTimeCompile::_relocationTargetTypeToHeaderSizeMap[TR_NumExternalRelocationKinds] =
   {
   12,                                       // TR_ConstantPool                        = 0
   8,                                        // TR_HelperAddress                       = 1
   12,                                       // TR_RelativeMethodAddress               = 2
   4,                                        // TR_AbsoluteMethodAddress               = 3
   20,                                       // TR_DataAddress                         = 4
   12,                                       // TR_ClassObject                         = 5
   12,                                       // TR_MethodObject                        = 6
   12,                                       // TR_InterfaceObject                     = 7
   8,                                        // TR_AbsoluteHelperAddress               = 8
   8,                                        // TR_FixedSequenceAddress                = 9
   8,                                        // TR_FixedSequenceAddress2               = 10
   16,                                       // TR_JNIVirtualTargetAddress             = 11
   16,                                       // TR_JNIStaticTargetAddress              = 12
   4,                                        // TR_ArrayCopyHelper                     = 13
   4,                                        // TR_ArrayCopyToc                        = 14
   4,                                        // TR_BodyInfoAddress                     = 15
   12,                                       // TR_Thunks                              = 16
   16,                                       // TR_StaticRamMethodConst                = 17
   12,                                       // TR_Trampolines                         = 18
   8,                                        // TR_PicTrampolines                      = 19
   8,                                        // TR_CheckMethodEnter                    = 20
   4,                                        // TR_RamMethod                           = 21
   8,                                        // TR_RamMethodSequence                   = 22
   8,                                        // TR_RamMethodSequenceReg                = 23
   24,                                       // TR_VerifyClassObjectForAlloc           = 24
   12,                                       // TR_ConstantPoolOrderedPair             = 25
   4,                                        // TR_AbsoluteMethodAddressOrderedPair    = 26
   20,                                       // TR_VerifyRefArrayForAlloc              = 27
   12,                                       // TR_J2IThunks                           = 28
   8,                                        // TR_GlobalValue                         = 29
   4,                                        // TR_BodyInfoAddressLoad                 = 30
   20,                                       // TR_ValidateInstanceField               = 31
   24,                                       // TR_InlinedStaticMethodWithNopGuard     = 32
   24,                                       // TR_InlinedSpecialMethodWithNopGuard    = 33
   24,                                       // TR_InlinedVirtualMethodWithNopGuard    = 34
   24,                                       // TR_InlinedInterfaceMethodWithNopGuard  = 35
   16,                                       // TR_SpecialRamMethodConst               = 36
   24,                                       // TR_InlinedHCRMethod                    = 37
   20,                                       // TR_ValidateStaticField                 = 38
   20,                                       // TR_ValidateClass                       = 39
   16,                                       // TR_ClassAddress                        = 40
   8,                                        // TR_HCR                                 = 41
   32,                                       // TR_ProfiledMethodGuardRelocation       = 42
   32,                                       // TR_ProfiledClassGuardRelocation        = 43
   0,                                        // TR_HierarchyGuardRelocation            = 44
   0,                                        // TR_AbstractGuardRelocation             = 45
   32,                                       // TR_ProfiledInlinedMethod               = 46
   20,                                       // TR_MethodPointer                       = 47
   16,                                       // TR_ClassPointer                        = 48
   8,                                        // TR_CheckMethodExit                     = 49
   12,                                       // TR_ValidateArbitraryClass              = 50
   0,                                        // TR_EmitClass(not used)                 = 51
   16,                                       // TR_JNISpecialTargetAddress             = 52
   16,                                       // TR_VirtualRamMethodConst               = 53
   20,                                       // TR_InlinedInterfaceMethod              = 54
   20,                                       // TR_InlinedVirtualMethod                = 55
   0,                                        // TR_NativeMethodAbsolute                = 56,
   0,                                        // TR_NativeMethodRelative                = 57,
   16,                                       // TR_ArbitraryClassAddress               = 58,
   28,                                       // TR_DebugCounter                        = 59
   4,                                        // TR_ClassUnloadAssumption               = 60
   16,                                       // TR_J2IVirtualThunkPointer              = 61
   24,                                       // TR_InlinedAbstractMethodWithNopGuard   = 62,
   0,                                                                  // TR_ValidateRootClass                   = 63,
   sizeof(TR_RelocationRecordValidateClassByNameBinaryTemplate),       // TR_ValidateClassByName                 = 64,
   sizeof(TR_RelocationRecordValidateProfiledClassBinaryTemplate),     // TR_ValidateProfiledClass               = 65,
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),       // TR_ValidateClassFromCP                 = 66,
   sizeof(TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate),//TR_ValidateDefiningClassFromCP         = 67,
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),       // TR_ValidateStaticClassFromCP           = 68,
   0,                                                                  // TR_ValidateClassFromMethod             = 69,
   0,                                                                  // TR_ValidateComponentClassFromArrayClass= 70,
   sizeof(TR_RelocationRecordValidateArrayFromCompBinaryTemplate),     // TR_ValidateArrayClassFromComponentClass= 71,
   sizeof(TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate),//TR_ValidateSuperClassFromClass         = 72,
   sizeof(TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate),//TR_ValidateClassInstanceOfClass       = 73,
   sizeof(TR_RelocationRecordValidateSystemClassByNameBinaryTemplate), //TR_ValidateSystemClassByName            = 74,
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),       //TR_ValidateClassFromITableIndexCP       = 75,
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),       //TR_ValidateDeclaringClassFromFieldOrStatic=76,
   0,                                                                  // TR_ValidateClassClass                  = 77,
   sizeof(TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate),//TR_ValidateConcreteSubClassFromClass   = 78,
   sizeof(TR_RelocationRecordValidateClassChainBinaryTemplate),        // TR_ValidateClassChain                  = 79,
   0,                                                                  // TR_ValidateRomClass                    = 80,
   0,                                                                  // TR_ValidatePrimitiveClass              = 81,
   0,                                                                  // TR_ValidateMethodFromInlinedSite       = 82,
   0,                                                                  // TR_ValidatedMethodByName               = 83,
   sizeof(TR_RelocationRecordValidateMethodFromClassBinaryTemplate),   // TR_ValidatedMethodFromClass            = 84,
   sizeof(TR_RelocationRecordValidateMethodFromCPBinaryTemplate),      // TR_ValidateStaticMethodFromCP          = 85,
   sizeof(TR_RelocationRecordValidateMethodFromCPBinaryTemplate),      //TR_ValidateSpecialMethodFromCP         = 86,
   sizeof(TR_RelocationRecordValidateMethodFromCPBinaryTemplate),      //TR_ValidateVirtualMethodFromCP         = 87,
   sizeof(TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate),//TR_ValidateVirtualMethodFromOffset = 88,
   sizeof(TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate),//TR_ValidateInterfaceMethodFromCP     = 89,
   sizeof(TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate),//TR_ValidateMethodFromClassAndSig     = 90,
   sizeof(TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate),//TR_ValidateStackWalkerMaySkipFramesRecord= 91,
   0,                                                                     //TR_ValidateArrayClassFromJavaVM      = 92,
   sizeof(TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate),//TR_ValidateClassInfoIsInitialized   = 93,
   sizeof(TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate),//TR_ValidateMethodFromSingleImplementer= 94,
   sizeof(TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate),//TR_ValidateMethodFromSingleInterfaceImplementer= 95,
   sizeof(TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate),//TR_ValidateMethodFromSingleAbstractImplementer= 96,
   sizeof(TR_RelocationRecordValidateMethodFromCPBinaryTemplate),      //TR_ValidateImproperInterfaceMethodFromCP= 97,
   sizeof(TR_RelocationRecordSymbolFromManagerBinaryTemplate),         // TR_SymbolFromManager = 98,
   sizeof(TR_RelocationRecordMethodCallAddressBinaryTemplate),         // TR_MethodCallAddress = 99,
   sizeof(TR_RelocationRecordSymbolFromManagerBinaryTemplate),         // TR_DiscontiguousSymbolFromManager = 100,
   sizeof(TR_RelocationRecordResolvedTrampolinesBinaryTemplate),       // TR_ResolvedTrampolines = 101,
   sizeof(TR_RelocationRecordBlockFrequencyBinaryTemplate),            // TR_BlockFrequency = 102,
   sizeof(TR_RelocationRecordBinaryTemplate),                          // TR_RecompQueuedFlag = 103,
   };

#endif

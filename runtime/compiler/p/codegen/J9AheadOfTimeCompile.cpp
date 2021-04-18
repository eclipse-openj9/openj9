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
         J9::AheadOfTimeCompile(NULL, cg->comp()),
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

void
J9::Power::AheadOfTimeCompile::initializePlatformSpecificAOTRelocationHeader(TR::IteratedExternalRelocation *relocation,
                                                                             TR_RelocationTarget *reloTarget,
                                                                             TR_RelocationRecord *reloRecord,
                                                                             uint8_t targetKind)
   {
   TR::Compilation *comp = self()->comp();
   TR_J9VMBase *fej9 = comp->fej9();
   TR_SharedCache *sharedCache = fej9->sharedCache();
   uint8_t * aotMethodCodeStart = (uint8_t *) comp->getRelocatableMethodCodeStart();

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
         }
         break;

      case TR_AbsoluteHelperAddress:
         {
         TR_RelocationRecordAbsoluteHelperAddress *ahaRecord = reinterpret_cast<TR_RelocationRecordAbsoluteHelperAddress *>(reloRecord);
         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(relocation->getTargetAddress());
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));

         ahaRecord->setHelperID(reloTarget, static_cast<uint32_t>(symRef->getReferenceNumber()));
         ahaRecord->setReloFlags(reloTarget, flags);
         }
         break;

      case TR_AbsoluteMethodAddressOrderedPair:
         {
         TR_RelocationRecordMethodAddress *maRecord = reinterpret_cast<TR_RelocationRecordMethodAddress *>(reloRecord);

         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *) relocation->getTargetAddress();
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(recordInfo->data3));

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         maRecord->setReloFlags(reloTarget, flags);
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
         }
         break;

      default:
         self()->initializeCommonAOTRelocationHeader(relocation, reloTarget, reloRecord, targetKind);
      }
   }


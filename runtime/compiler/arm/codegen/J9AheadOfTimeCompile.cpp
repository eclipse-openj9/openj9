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
#include "codegen/ARMAOTRelocation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "compile/AOTClassInfo.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CHTable.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/SharedCache.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRecord.hpp"

#define  NON_HELPER         0

J9::ARM::AheadOfTimeCompile::AheadOfTimeCompile(TR::CodeGenerator *cg)
   : J9::AheadOfTimeCompile(NULL, cg->comp()),
     _cg(cg),
     _relocationList(self()->trMemory())
   {
   }

void J9::ARM::AheadOfTimeCompile::processRelocations()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->cg()->fe());
   ListIterator<TR::ARMRelocation>  iterator(&self()->getRelocationList());
   TR::ARMRelocation               *relocation;
   TR::IteratedExternalRelocation  *r;

   for (relocation=iterator.getFirst();
        relocation!=NULL;
        relocation=iterator.getNext())
      {
      relocation->mapRelocation(self()->cg());
      }

   for (auto aotIterator = self()->cg()->getExternalRelocationList().begin(); aotIterator != self()->cg()->getExternalRelocationList().end(); ++aotIterator)
      {
	  (*aotIterator)->addExternalRelocation(self()->cg());
      }

   for (r = self()->getAOTRelocationTargets().getFirst();
        r != NULL;
        r = r->getNext())
      {
      self()->addToSizeOfAOTRelocations(r->getSizeOfRelocationData());
      }

   // now allocate the memory  size of all iterated relocations + the header (total length field)

   if (self()->getSizeOfAOTRelocations() != 0)
      {
      uint8_t *relocationDataCursor = self()->setRelocationData(fej9->allocateRelocationData(self()->comp(), self()->getSizeOfAOTRelocations() + 4));

      // set up the size for the region
      *(uint32_t *)relocationDataCursor = self()->getSizeOfAOTRelocations() + 4;
      relocationDataCursor += 4;

      // set up pointers for each iterated relocation and initialize header
      TR::IteratedExternalRelocation *s;
      for (s = self()->getAOTRelocationTargets().getFirst();
           s != NULL;
           s = s->getNext())
         {
         s->setRelocationData(relocationDataCursor);
         s->initializeRelocation(_cg);
         relocationDataCursor += s->getSizeOfRelocationData();
         }
      }
   }

void
J9::ARM::AheadOfTimeCompile::initializePlatformSpecificAOTRelocationHeader(TR::IteratedExternalRelocation *relocation,
                                                                           TR_RelocationTarget *reloTarget,
                                                                           TR_RelocationRecord *reloRecord,
                                                                           uint8_t targetKind)
   {
   TR::Compilation* comp = self()->comp();
   TR_J9VMBase *fej9 = comp->fej9();
   TR_SharedCache *sharedCache = fej9->sharedCache();
   uint8_t * aotMethodCodeStart = (uint8_t *) comp->getRelocatableMethodCodeStart();

   switch (targetKind)
      {
      case TR_MethodObject:
         {
         TR_RelocationRecordMethodObject *moRecord = reinterpret_cast<TR_RelocationRecordMethodObject *>(reloRecord);
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress();

         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(recordInfo->data1);
         uintptr_t inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(symRef->getOwningMethod(comp)->constantPool(), recordInfo->data2);
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(recordInfo->data3));

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");

         moRecord->setInlinedSiteIndex(reloTarget, reinterpret_cast<uintptr_t>(inlinedSiteIndex));
         moRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(symRef->getOwningMethod(comp)->constantPool()));
         moRecord->setReloFlags(reloTarget, flags);
         }
         break;

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

      case TR_FixedSequenceAddress2:
         {
         TR_RelocationRecordWithOffset *rwoRecord = reinterpret_cast<TR_RelocationRecordWithOffset *>(reloRecord);
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         rwoRecord->setReloFlags(reloTarget, flags);

         uintptr_t offset = relocation->getTargetAddress()
                  ? static_cast<uintptr_t>(relocation->getTargetAddress() - aotMethodCodeStart)
                  : 0x0;

         rwoRecord->setOffset(reloTarget, offset);
         }
         break;

      case TR_BodyInfoAddressLoad:
         {
         TR_RelocationRecord *rRecord = reinterpret_cast<TR_RelocationRecord *>(reloRecord);

         uint8_t flags = flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         rRecord->setReloFlags(reloTarget, flags);
         }
         break;

      case TR_RamMethodSequence:
         {
         TR_RelocationRecordRamSequence *rsRecord = reinterpret_cast<TR_RelocationRecordRamSequence *>(reloRecord);
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         rsRecord->setReloFlags(reloTarget, flags);

         // Skip Offset
         }
         break;

      case TR_GlobalValue:
      case TR_HCR:
         {
         TR_RelocationRecordWithOffset *rwoRecord = reinterpret_cast<TR_RelocationRecordWithOffset *>(reloRecord);

         uintptr_t gv = reinterpret_cast<uintptr_t>(relocation->getTargetAddress());
         uint8_t flags = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         rwoRecord->setReloFlags(reloTarget, flags);
         rwoRecord->setOffset(reloTarget, gv);
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
         // flags stored in data3 are currently unused
         uintptr_t inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(symRef->getOwningMethod(comp)->constantPool(), recordInfo->data2);

         uintptr_t classChainIdentifyingLoaderOffsetInSharedCache = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(j9class);
         uintptr_t classChainOffsetInSharedCache = self()->getClassChainOffset(j9class);

         acaRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         acaRecord->setClassChainIdentifyingLoaderOffsetInSharedCache(reloTarget, classChainIdentifyingLoaderOffsetInSharedCache);
         acaRecord->setClassChainForInlinedMethod(reloTarget, classChainOffsetInSharedCache);
         }
         break;

      default:
         self()->initializeCommonAOTRelocationHeader(relocation, reloTarget, reloRecord, targetKind);
      }
   }

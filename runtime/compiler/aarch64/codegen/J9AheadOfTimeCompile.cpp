/*******************************************************************************
 * Copyright (c) 2019, 2021 IBM Corp. and others
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
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRecord.hpp"

J9::ARM64::AheadOfTimeCompile::AheadOfTimeCompile(TR::CodeGenerator *cg) :
      J9::AheadOfTimeCompile(NULL, cg->comp()),
      _cg(cg)
   {
   }

void J9::ARM64::AheadOfTimeCompile::processRelocations()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_cg->fe());
   TR::IteratedExternalRelocation *r;

   for (auto aotIterator = _cg->getExternalRelocationList().begin(); aotIterator != _cg->getExternalRelocationList().end(); ++aotIterator)
      {
      (*aotIterator)->addExternalRelocation(_cg);
      }

   for (r = getAOTRelocationTargets().getFirst(); r != NULL; r = r->getNext())
      {
      addToSizeOfAOTRelocations(r->getSizeOfRelocationData());
      }

   // now allocate the memory  size of all iterated relocations + the header (total length field)

   // Note that when using the SymbolValidationManager, the well-known classes
   // must be checked even if no explicit records were generated, since they
   // might be responsible for the lack of records.
   bool useSVM = self()->comp()->getOption(TR_UseSymbolValidationManager);

   if (self()->getSizeOfAOTRelocations() != 0 || useSVM)
      {
      // It would be more straightforward to put the well-known classes offset
      // in the AOT method header, but that would use space for AOT bodies that
      // don't use the SVM.
      int wellKnownClassesOffsetSize = useSVM ? SIZEPOINTER : 0;
      uintptr_t reloBufferSize =
         self()->getSizeOfAOTRelocations() + SIZEPOINTER + wellKnownClassesOffsetSize;
      uint8_t *relocationDataCursor =
         self()->setRelocationData(fej9->allocateRelocationData(self()->comp(), reloBufferSize));

      // set up the size for the region
      *(uintptr_t *)relocationDataCursor = reloBufferSize;
      relocationDataCursor += SIZEPOINTER;

      if (useSVM)
         {
         TR::SymbolValidationManager *svm =
            self()->comp()->getSymbolValidationManager();
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
J9::ARM64::AheadOfTimeCompile::initializePlatformSpecificAOTRelocationHeader(TR::IteratedExternalRelocation *relocation,
                                                                             TR_RelocationTarget *reloTarget,
                                                                             TR_RelocationRecord *reloRecord,
                                                                             uint8_t targetKind)
   {
   switch (targetKind)
      {
      case TR_DiscontiguousSymbolFromManager:
         {
         TR_RelocationRecordDiscontiguousSymbolFromManager *dsfmRecord = reinterpret_cast<TR_RelocationRecordDiscontiguousSymbolFromManager *>(reloRecord);

         uint8_t *symbol = (uint8_t *)relocation->getTargetAddress();
         uint16_t symbolID = self()->comp()->getSymbolValidationManager()->getIDFromSymbol(static_cast<void *>(symbol));

         uint16_t symbolType = (uint16_t)(uintptr_t)relocation->getTargetAddress2();

         dsfmRecord->setSymbolID(reloTarget, symbolID);
         dsfmRecord->setSymbolType(reloTarget, static_cast<TR::SymbolType>(symbolType));
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


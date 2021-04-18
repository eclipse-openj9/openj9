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

#pragma csect(CODE,"TRJ9ZAOTComp#C")
#pragma csect(STATIC,"TRJ9ZAOTComp#S")
#pragma csect(TEST,"TRJ9ZAOTComp#T")

#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/AOTClassInfo.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/CHTable.hpp"
#include "env/CompilerEnv.hpp"
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

#define  WIDE_OFFSETS       0x80
#define  EIP_RELATIVE       0x40
#define  ORDERED_PAIR       0x20
#define  NON_HELPER         0

J9::Z::AheadOfTimeCompile::AheadOfTimeCompile(TR::CodeGenerator *cg)
   : J9::AheadOfTimeCompile(NULL, cg->comp()),
     _relocationList(getTypedAllocator<TR::S390Relocation*>(cg->comp()->allocator())),
     _cg(cg)
   {
   }

void J9::Z::AheadOfTimeCompile::processRelocations()
   {
   TR::Compilation *comp = self()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_cg->fe());
   TR::IteratedExternalRelocation  *r;

   for (auto iterator = self()->getRelocationList().begin();
        iterator != self()->getRelocationList().end();
        ++iterator)
      {
	   (*iterator)->mapRelocation(_cg);
      }

   for (auto aotIterator = _cg->getExternalRelocationList().begin(); aotIterator != _cg->getExternalRelocationList().end(); ++aotIterator)
	  (*aotIterator)->addExternalRelocation(_cg);

   for (r = self()->getAOTRelocationTargets().getFirst();
        r != NULL;
        r = r->getNext())
      {
      self()->addToSizeOfAOTRelocations(r->getSizeOfRelocationData());
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
      *(uintptr_t *)relocationDataCursor = reloBufferSize;
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
J9::Z::AheadOfTimeCompile::initializePlatformSpecificAOTRelocationHeader(TR::IteratedExternalRelocation *relocation,
                                                                         TR_RelocationTarget *reloTarget,
                                                                         TR_RelocationRecord *reloRecord,
                                                                         uint8_t targetKind)
   {
   switch (targetKind)
      {
      case TR_EmitClass:
         {
         TR_RelocationRecordEmitClass *ecRecord = reinterpret_cast<TR_RelocationRecordEmitClass *>(reloRecord);

         TR_ByteCodeInfo *bcInfo = reinterpret_cast<TR_ByteCodeInfo *>(relocation->getTargetAddress());
         int32_t bcIndex = bcInfo->getByteCodeIndex();
         uintptr_t inlinedSiteIndex = reinterpret_cast<uintptr_t>(relocation->getTargetAddress2());

         ecRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         ecRecord->setBCIndex(reloTarget, bcIndex);
         }
         break;

      default:
         self()->initializeCommonAOTRelocationHeader(relocation, reloTarget, reloRecord, targetKind);
      }
   }


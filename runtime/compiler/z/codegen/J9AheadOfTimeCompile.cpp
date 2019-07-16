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

#pragma csect(CODE,"TRJ9ZAOTComp#C")
#pragma csect(STATIC,"TRJ9ZAOTComp#S")
#pragma csect(TEST,"TRJ9ZAOTComp#T")

#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
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
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRecord.hpp"

#define  WIDE_OFFSETS       0x80
#define  EIP_RELATIVE       0x40
#define  ORDERED_PAIR       0x20
#define  NON_HELPER         0

J9::Z::AheadOfTimeCompile::AheadOfTimeCompile(TR::CodeGenerator *cg)
   : J9::AheadOfTimeCompile(_relocationTargetTypeToHeaderSizeMap, cg->comp()),
     _relocationList(getTypedAllocator<TR::S390Relocation*>(cg->comp()->allocator())),
     _cg(cg)
   {
   }

void J9::Z::AheadOfTimeCompile::processRelocations()
   {
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
   bool useSVM = self()->comp()->getOption(TR_UseSymbolValidationManager);
   if (self()->getSizeOfAOTRelocations() != 0 || useSVM)
      {
      // It would be more straightforward to put the well-known classes offset
      // in the AOT method header, but that would use space for AOT bodies that
      // don't use the SVM. TODO: Move it once SVM takes over?
      int wellKnownClassesOffsetSize = useSVM ? SIZEPOINTER : 0;
      uintptrj_t reloBufferSize =
         self()->getSizeOfAOTRelocations() + SIZEPOINTER + wellKnownClassesOffsetSize;
      uint8_t *relocationDataCursor = self()->setRelocationData(
         fej9->allocateRelocationData(self()->comp(), reloBufferSize));
      // set up the size for the region
      *(uintptrj_t *)relocationDataCursor = reloBufferSize;
      relocationDataCursor += SIZEPOINTER;

      if (useSVM)
         {
         TR::SymbolValidationManager *svm =
            self()->comp()->getSymbolValidationManager();
         void *offsets = const_cast<void*>(svm->wellKnownClassChainOffsets());
         *(uintptrj_t *)relocationDataCursor = reinterpret_cast<uintptrj_t>(
            fej9->sharedCache()->offsetInSharedCacheFromPointer(offsets));
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

uint8_t *J9::Z::AheadOfTimeCompile::initializeAOTRelocationHeader(TR::IteratedExternalRelocation *relocation)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_cg->fe());
   TR_SharedCache *sharedCache = fej9->sharedCache();

   TR_VirtualGuard *guard;
   uint8_t flags = 0;
   TR_ResolvedMethod *resolvedMethod;

   uint8_t *cursor = relocation->getRelocationData();

   TR_RelocationRuntime *reloRuntime = comp()->reloRuntime();
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();

   // size of relocation goes first in all types
   *(uint16_t *)cursor = relocation->getSizeOfRelocationData();
   AOTcgDiag5(comp(), "initializeAOTRelocationHeader cursor=%x size=%x wide=%x pair=%x kind=%x\n",
      cursor, *(uint16_t *)cursor, relocation->needsWideOffsets(), relocation->isOrderedPair(), relocation->getTargetKind());

   cursor += 2;

   uint8_t  modifier = 0;
   uint8_t *relativeBitCursor = cursor;

   if (relocation->needsWideOffsets())
      modifier |= WIDE_OFFSETS;
   *cursor   = (uint8_t)relocation->getTargetKind();
   AOTcgDiag1(comp(), "final type =%x\n", *cursor);
   cursor++;
   uint8_t *flagsCursor = cursor++;
   *flagsCursor = modifier;
   uint32_t *wordAfterHeader = (uint32_t*)cursor;
#if defined(TR_HOST_64BIT)
   cursor += 4; // padding
#endif

   // This has to be created after the kind has been written into the header
   TR_RelocationRecord storage;
   TR_RelocationRecord *reloRecord = TR_RelocationRecord::create(&storage, reloRuntime, reloTarget, reinterpret_cast<TR_RelocationRecordBinaryTemplate *>(relocation->getRelocationData()));
   TR::Compilation *comp = _cg->comp();
   TR::SymbolValidationManager *symValManager = comp->getSymbolValidationManager();

   switch (relocation->getTargetKind())
      {
      case TR_MethodObject:
         {
         TR::SymbolReference *tempSR = (TR::SymbolReference *)relocation->getTargetAddress();

         if (TR::Compiler->target.is64Bit())
            {

            // first word is the inlined site index for the constant pool
            // that indicates the particular relocation target
            *(uint64_t *)cursor = (uint64_t)relocation->getTargetAddress2();
            cursor += SIZEPOINTER;

            // next word is the address of the constant pool
            *(uint64_t *)cursor = (uint64_t)(uintptrj_t)tempSR->getOwningMethod(self()->comp())->constantPool();
            cursor += SIZEPOINTER;

            //*(uint64_t *)cursor = (uint64_t)tempSR->getCPIndex();
            }
         else
            {
            // first word is the inlined site index for the constant pool
            *(uint32_t *)cursor = (uint32_t)(uintptrj_t)relocation->getTargetAddress2();
            cursor += SIZEPOINTER;

            // next word is the address of the constant pool
            *(uint32_t *)cursor = (uintptrj_t) tempSR->getOwningMethod(self()->comp())->constantPool();
            cursor += SIZEPOINTER;

            //*(uint32_t *)cursor = (uint32_t)tempSR->getCPIndex();
            }
         }
         break;

      case TR_JNISpecialTargetAddress:
      case TR_VirtualRamMethodConst:
         {
         TR::SymbolReference *tempSR = (TR::SymbolReference *)relocation->getTargetAddress();
         uintptr_t inlinedSiteIndex = (uintptr_t)relocation->getTargetAddress2();

         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(tempSR->getOwningMethod(self()->comp())->constantPool(), inlinedSiteIndex);

         *(uintptrj_t *)cursor = inlinedSiteIndex;  // inlinedSiteIndex
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)tempSR->getOwningMethod(self()->comp())->constantPool(); // constantPool
         cursor += SIZEPOINTER;

         uintptrj_t cpIndex=(uintptrj_t)tempSR->getCPIndex();
         *(uintptrj_t *)cursor =cpIndex;// cpIndex
         cursor += SIZEPOINTER;
         }
         break;
      case TR_ClassAddress:
         {
         TR::SymbolReference *tempSR = (TR::SymbolReference *)relocation->getTargetAddress();
         uintptr_t inlinedSiteIndex = (uintptr_t)relocation->getTargetAddress2();
         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(tempSR->getOwningMethod(self()->comp())->constantPool(), inlinedSiteIndex);

         *(uintptrj_t *)cursor = inlinedSiteIndex; // inlinedSiteIndex
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)tempSR->getOwningMethod(self()->comp())->constantPool(); // constantPool
         cursor += SIZEPOINTER;

         uintptrj_t cpIndex=(uintptrj_t)tempSR->getCPIndex();
         *(uintptrj_t *)cursor =cpIndex;// cpIndex
         cursor += SIZEPOINTER;
         }
         break;
      case TR_DataAddress:
         {
         TR::SymbolReference *tempSR = (TR::SymbolReference *)relocation->getTargetAddress();
         uintptr_t inlinedSiteIndex = (uintptr_t)relocation->getTargetAddress2();

         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(tempSR->getOwningMethod(self()->comp())->constantPool(), inlinedSiteIndex);


         *(uintptrj_t *)cursor = inlinedSiteIndex;  // inlinedSiteIndex
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)tempSR->getOwningMethod(self()->comp())->constantPool(); // constantPool
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = tempSR->getCPIndex(); // cpIndex
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = tempSR->getOffset(); // offset
         cursor += SIZEPOINTER;

         break;
         }

      case TR_AbsoluteMethodAddressOrderedPair:
         break;
      case TR_ArrayCopyHelper:
         {
#if defined(TR_HOST_64BIT)
         *wordAfterHeader = (uint32_t)0;
#endif
         }
         break;

      case TR_ConstantPoolOrderedPair:
         {
         // constant pool address is placed as the last word of the header
         if (TR::Compiler->target.is64Bit())
            {
            *(uint64_t *)cursor = (uint64_t)(uintptrj_t)relocation->getTargetAddress2();
            cursor += 8;

            *(uint64_t *)cursor = (uint64_t)(uintptrj_t)relocation->getTargetAddress();
            cursor += 8;
            }
         else
            {
            *(uint32_t *)cursor = (uint32_t)(uintptrj_t)relocation->getTargetAddress2();
            cursor += 4;

            *(uint32_t *)cursor = (uint32_t)(uintptrj_t)relocation->getTargetAddress();
            cursor += 4;
            }
         }
         break;

      case TR_J2IVirtualThunkPointer:
         {
         auto info = (TR_RelocationRecordInformation*)relocation->getTargetAddress();

         *(uintptrj_t *)cursor = (uintptrj_t)info->data2; // inlined site index
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)info->data1; // constantPool
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)info->data3; // offset to J2I virtual thunk pointer
         cursor += SIZEPOINTER;
         }
         break;

      case TR_CheckMethodExit:
         {
         if (TR::Compiler->target.is64Bit())
            {
            *(uint64_t *)cursor = (uint64_t)(uintptrj_t)relocation->getTargetAddress();
            cursor += 8;
            }
         else
            {
            *(uint32_t *)cursor = (uint32_t)(uintptrj_t)relocation->getTargetAddress();
            cursor += 4;
            }
         }
         break;

      case TR_ValidateClassByName:
         {
         TR::ClassByNameRecord *record = reinterpret_cast<TR::ClassByNameRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateClassByNameBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateClassByNameBinaryTemplate *>(cursor);
         // Store class chain to get name of class. Checking the class chain for
         // this record eliminates the need for a separate class chain validation.
         void *classChainOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(record->_classChain);
         binaryTemplate->_classID = symValManager->getIDFromSymbol(record->_class);
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(record->_beholder);
         binaryTemplate->_classChainOffsetInSCC = reinterpret_cast<uintptrj_t>(classChainOffsetInSharedCache);
         cursor += sizeof(TR_RelocationRecordValidateClassByNameBinaryTemplate);
         }
         break;
      case TR_ValidateProfiledClass:
         {
         TR::ProfiledClassRecord *record = reinterpret_cast<TR::ProfiledClassRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateProfiledClassBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateProfiledClassBinaryTemplate *>(cursor);
         TR_OpaqueClassBlock *classToValidate = record->_class;
         void *classChainForClassToValidate = record->_classChain;
         //store the classchain's offset for the classloader for the class
         uintptrj_t classChainOffsetInSharedCacheForCL = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(classToValidate);
         //store the classchain's offset for the class that needs to be validated in the second run
         void * classChainOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(classChainForClassToValidate);
         binaryTemplate->_classID = symValManager->getIDFromSymbol(static_cast<void *>(classToValidate));
         binaryTemplate->_classChainOffsetInSCC = reinterpret_cast<uintptrj_t>(classChainOffsetInSharedCache);
         binaryTemplate->_classChainOffsetForCLInScc = classChainOffsetInSharedCacheForCL;
         cursor += sizeof(TR_RelocationRecordValidateProfiledClassBinaryTemplate);
         }
         break;
      case TR_ValidateClassFromCP:
         {
         TR::ClassFromCPRecord *record = reinterpret_cast<TR::ClassFromCPRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateClassFromCPBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateClassFromCPBinaryTemplate *>(cursor);
         binaryTemplate->_classID = symValManager->getIDFromSymbol(static_cast<void *>(record->_class));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_cpIndex = record->_cpIndex;
         cursor += sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate);
         }
         break;
      case TR_ValidateDefiningClassFromCP:
         {
         TR::DefiningClassFromCPRecord *record = reinterpret_cast<TR::DefiningClassFromCPRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate *>(cursor);
         binaryTemplate->_isStatic = record->_isStatic;
         binaryTemplate->_classID = symValManager->getIDFromSymbol(static_cast<void *>(record->_class));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_cpIndex = record->_cpIndex;
         cursor += sizeof(TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate);
         }
         break;
      case TR_ValidateStaticClassFromCP:
         {
         TR::StaticClassFromCPRecord *record = reinterpret_cast<TR::StaticClassFromCPRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateStaticClassFromCPBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateStaticClassFromCPBinaryTemplate *>(cursor);
         binaryTemplate->_classID = symValManager->getIDFromSymbol(static_cast<void *>(record->_class));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_cpIndex = record->_cpIndex;
         cursor += sizeof(TR_RelocationRecordValidateStaticClassFromCPBinaryTemplate);
         }
         break;
      case TR_ValidateArrayClassFromComponentClass:
         {
         TR::ArrayClassFromComponentClassRecord *record = reinterpret_cast<TR::ArrayClassFromComponentClassRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateArrayFromCompBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateArrayFromCompBinaryTemplate *>(cursor);
         binaryTemplate->_arrayClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->_arrayClass));
         binaryTemplate->_componentClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->_componentClass));
         cursor += sizeof(TR_RelocationRecordValidateArrayFromCompBinaryTemplate);
         }
         break;
      case TR_ValidateSuperClassFromClass:
         {
         TR::SuperClassFromClassRecord *record = reinterpret_cast<TR::SuperClassFromClassRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate *>(cursor);
         binaryTemplate->_superClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->_superClass));
         binaryTemplate->_childClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->_childClass));
         cursor += sizeof(TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate);
         }
         break;
      case TR_ValidateClassInstanceOfClass:
         {
         TR::ClassInstanceOfClassRecord *record = reinterpret_cast<TR::ClassInstanceOfClassRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *>(cursor);
         binaryTemplate->_objectTypeIsFixed = record->_objectTypeIsFixed;
         binaryTemplate->_castTypeIsFixed = record->_castTypeIsFixed;
         binaryTemplate->_isInstanceOf = record->_isInstanceOf;
         binaryTemplate->_classOneID = symValManager->getIDFromSymbol(static_cast<void *>(record->_classOne));
         binaryTemplate->_classTwoID = symValManager->getIDFromSymbol(static_cast<void *>(record->_classTwo));
         cursor += sizeof(TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate);
         }
         break;
      case TR_ValidateSystemClassByName:
         {
         TR::SystemClassByNameRecord *record = reinterpret_cast<TR::SystemClassByNameRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateSystemClassByNameBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateSystemClassByNameBinaryTemplate *>(cursor);
         // Store class chain to get name of class. Checking the class chain for
         // this record eliminates the need for a separate class chain validation.
         void *classChainOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(record->_classChain);
         binaryTemplate->_systemClassID = symValManager->getIDFromSymbol(record->_class);
         binaryTemplate->_classChainOffsetInSCC = reinterpret_cast<uintptrj_t>(classChainOffsetInSharedCache);
         cursor += sizeof(TR_RelocationRecordValidateSystemClassByNameBinaryTemplate);
         }
         break;
      case TR_ValidateClassFromITableIndexCP:
         {
         TR::ClassFromITableIndexCPRecord *record = reinterpret_cast<TR::ClassFromITableIndexCPRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateClassFromITableIndexCPBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateClassFromITableIndexCPBinaryTemplate *>(cursor);
         binaryTemplate->_classID = symValManager->getIDFromSymbol(static_cast<void *>(record->_class));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_cpIndex = record->_cpIndex;
         cursor += sizeof(TR_RelocationRecordValidateClassFromITableIndexCPBinaryTemplate);
         }
         break;
      case TR_ValidateDeclaringClassFromFieldOrStatic:
         {
         TR::DeclaringClassFromFieldOrStaticRecord *record = reinterpret_cast<TR::DeclaringClassFromFieldOrStaticRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate *>(cursor);
         binaryTemplate->_classID = symValManager->getIDFromSymbol(static_cast<void *>(record->_class));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_cpIndex = record->_cpIndex;
         cursor += sizeof(TR_RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate);
         }
         break;
      case TR_ValidateConcreteSubClassFromClass:
         {
         TR::ConcreteSubClassFromClassRecord *record = reinterpret_cast<TR::ConcreteSubClassFromClassRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateConcreteSubFromClassBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateConcreteSubFromClassBinaryTemplate *>(cursor);
         binaryTemplate->_childClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->_childClass));
         binaryTemplate->_superClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->_superClass));
         cursor += sizeof(TR_RelocationRecordValidateConcreteSubFromClassBinaryTemplate);
         }
         break;
      case TR_ValidateClassChain:
         {
         TR::ClassChainRecord *record = reinterpret_cast<TR::ClassChainRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateClassChainBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateClassChainBinaryTemplate *>(cursor);
         void *classToValidate = static_cast<void *>(record->_class);
         void *classChainForClassToValidate = record->_classChain;
         void *classChainOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(classChainForClassToValidate);
         binaryTemplate->_classID = symValManager->getIDFromSymbol(classToValidate);
         binaryTemplate->_classChainOffsetInSCC = reinterpret_cast<uintptrj_t>(classChainOffsetInSharedCache);
         cursor += sizeof(TR_RelocationRecordValidateClassChainBinaryTemplate);
         }
         break;
      case TR_ValidateMethodFromClass:
         {
         TR::MethodFromClassRecord *record = reinterpret_cast<TR::MethodFromClassRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateMethodFromClassBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateMethodFromClassBinaryTemplate *>(cursor);
         binaryTemplate->_methodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_method));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_index = static_cast<uintptrj_t>(record->_index);
         cursor += sizeof(TR_RelocationRecordValidateMethodFromClassBinaryTemplate);
         }
         break;
      case TR_ValidateStaticMethodFromCP:
         {
         TR::StaticMethodFromCPRecord *record = reinterpret_cast<TR::StaticMethodFromCPRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateStaticMethodFromCPBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateStaticMethodFromCPBinaryTemplate *>(cursor);
         TR_ASSERT_FATAL(
            (record->_cpIndex & J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG) == 0,
            "static method cpIndex has special split table flag set");
         if ((record->_cpIndex & J9_STATIC_SPLIT_TABLE_INDEX_FLAG) != 0)
            *flagsCursor |= TR_VALIDATE_STATIC_OR_SPECIAL_METHOD_FROM_CP_IS_SPLIT;
         binaryTemplate->_methodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_method));
         binaryTemplate->_definingClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->definingClass()));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_cpIndex = static_cast<uint16_t>(record->_cpIndex & J9_SPLIT_TABLE_INDEX_MASK);
         cursor += sizeof(TR_RelocationRecordValidateStaticMethodFromCPBinaryTemplate);
         }
         break;
      case TR_ValidateSpecialMethodFromCP:
         {
         TR::SpecialMethodFromCPRecord *record = reinterpret_cast<TR::SpecialMethodFromCPRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateSpecialMethodFromCPBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateSpecialMethodFromCPBinaryTemplate *>(cursor);
         TR_ASSERT_FATAL(
            (record->_cpIndex & J9_STATIC_SPLIT_TABLE_INDEX_FLAG) == 0,
            "special method cpIndex has static split table flag set");
         if ((record->_cpIndex & J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG) != 0)
            *flagsCursor |= TR_VALIDATE_STATIC_OR_SPECIAL_METHOD_FROM_CP_IS_SPLIT;
         binaryTemplate->_methodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_method));
         binaryTemplate->_definingClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->definingClass()));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_cpIndex = static_cast<uint16_t>(record->_cpIndex & J9_SPLIT_TABLE_INDEX_MASK);
         cursor += sizeof(TR_RelocationRecordValidateSpecialMethodFromCPBinaryTemplate);
         }
         break;
      case TR_ValidateVirtualMethodFromCP:
         {
         TR::VirtualMethodFromCPRecord *record = reinterpret_cast<TR::VirtualMethodFromCPRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateVirtualMethodFromCPBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateVirtualMethodFromCPBinaryTemplate *>(cursor);
         binaryTemplate->_methodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_method));
         binaryTemplate->_definingClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->definingClass()));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_cpIndex = static_cast<uint16_t>(record->_cpIndex);
         cursor += sizeof(TR_RelocationRecordValidateVirtualMethodFromCPBinaryTemplate);
         }
         break;
      case TR_ValidateVirtualMethodFromOffset:
         {
         TR::VirtualMethodFromOffsetRecord *record = reinterpret_cast<TR::VirtualMethodFromOffsetRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *>(cursor);
         TR_ASSERT_FATAL((record->_virtualCallOffset & 1) == 0, "virtualCallOffset must be even");
         TR_ASSERT_FATAL(
            record->_virtualCallOffset == (int32_t)(int16_t)record->_virtualCallOffset,
            "virtualCallOffset must fit in a 16-bit signed integer");
         binaryTemplate->_methodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_method));
         binaryTemplate->_definingClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->definingClass()));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_virtualCallOffsetAndIgnoreRtResolve = (uint16_t)(record->_virtualCallOffset | (int)record->_ignoreRtResolve);
         cursor += sizeof(TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate);
         }
         break;
      case TR_ValidateInterfaceMethodFromCP:
         {
         TR::InterfaceMethodFromCPRecord *record = reinterpret_cast<TR::InterfaceMethodFromCPRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *>(cursor);
         binaryTemplate->_methodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_method));
         binaryTemplate->_definingClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->definingClass()));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_lookupID = symValManager->getIDFromSymbol(static_cast<void *>(record->_lookup));
         binaryTemplate->_cpIndex = static_cast<uintptrj_t>(record->_cpIndex);
         cursor += sizeof(TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate);
         }
         break;
      case TR_ValidateImproperInterfaceMethodFromCP:
         {
         TR::ImproperInterfaceMethodFromCPRecord *record = reinterpret_cast<TR::ImproperInterfaceMethodFromCPRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate *>(cursor);
         binaryTemplate->_methodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_method));
         binaryTemplate->_definingClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->definingClass()));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_cpIndex = static_cast<uint16_t>(record->_cpIndex);
         cursor += sizeof(TR_RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate);
         }
         break;
      case TR_ValidateMethodFromClassAndSig:
         {
         TR::MethodFromClassAndSigRecord *record = reinterpret_cast<TR::MethodFromClassAndSigRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *>(cursor);
         // Store rom method to get name of method
         J9Method *methodToValidate = reinterpret_cast<J9Method *>(record->_method);
         J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(methodToValidate);
         void *romMethodOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(romMethod);
         binaryTemplate->_methodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_method));
         binaryTemplate->_definingClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->definingClass()));
         binaryTemplate->_lookupClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->_lookupClass));
         binaryTemplate->_beholderID = symValManager->getIDFromSymbol(static_cast<void *>(record->_beholder));
         binaryTemplate->_romMethodOffsetInSCC = reinterpret_cast<uintptrj_t>(romMethodOffsetInSharedCache);
         cursor += sizeof(TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate);
         }
         break;
      case TR_ValidateMethodFromSingleImplementer:
         {
         TR::MethodFromSingleImplementer *record = reinterpret_cast<TR::MethodFromSingleImplementer *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *>(cursor);
         binaryTemplate->_methodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_method));
         binaryTemplate->_definingClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->definingClass()));
         binaryTemplate->_thisClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->_thisClass));
         binaryTemplate->_cpIndexOrVftSlot = record->_cpIndexOrVftSlot;
         binaryTemplate->_callerMethodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_callerMethod));
         binaryTemplate->_useGetResolvedInterfaceMethod = (uint16_t)record->_useGetResolvedInterfaceMethod;
         cursor += sizeof(TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate);
         }
         break;
      case TR_ValidateMethodFromSingleInterfaceImplementer:
         {
         TR::MethodFromSingleInterfaceImplementer *record = reinterpret_cast<TR::MethodFromSingleInterfaceImplementer *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *>(cursor);
         binaryTemplate->_methodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_method));
         binaryTemplate->_definingClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->definingClass()));
         binaryTemplate->_thisClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->_thisClass));
         binaryTemplate->_callerMethodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_callerMethod));
         binaryTemplate->_cpIndex = (uint16_t)record->_cpIndex;
         cursor += sizeof(TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate);
         }
         break;
      case TR_ValidateMethodFromSingleAbstractImplementer:
         {
         TR::MethodFromSingleAbstractImplementer *record = reinterpret_cast<TR::MethodFromSingleAbstractImplementer *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *>(cursor);
         binaryTemplate->_methodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_method));
         binaryTemplate->_definingClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->definingClass()));
         binaryTemplate->_thisClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->_thisClass));
         binaryTemplate->_vftSlot = record->_vftSlot;
         binaryTemplate->_callerMethodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_callerMethod));
         cursor += sizeof(TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate);
         }
         break;
      case TR_ValidateStackWalkerMaySkipFramesRecord:
         {
         TR::StackWalkerMaySkipFramesRecord *record = reinterpret_cast<TR::StackWalkerMaySkipFramesRecord *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *>(cursor);
         binaryTemplate->_methodID = symValManager->getIDFromSymbol(static_cast<void *>(record->_method));
         binaryTemplate->_methodClassID = symValManager->getIDFromSymbol(static_cast<void *>(record->_methodClass));
         binaryTemplate->_skipFrames = record->_skipFrames;
         cursor += sizeof(TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate);
         }
         break;
      case TR_ValidateClassInfoIsInitialized:
         {
         TR::ClassInfoIsInitialized *record = reinterpret_cast<TR::ClassInfoIsInitialized *>(relocation->getTargetAddress());
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate *>(cursor);
         binaryTemplate->_classID = symValManager->getIDFromSymbol(static_cast<void *>(record->_class));
         binaryTemplate->_isInitialized = record->_isInitialized;
         cursor += sizeof(TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate);
         }
         break;
      case TR_SymbolFromManager:
         {
         uint8_t *symbol = relocation->getTargetAddress();
         uint16_t symbolID = comp->getSymbolValidationManager()->getIDFromSymbol(static_cast<void *>(symbol));
         uint16_t symbolType = (uint16_t)(uintptrj_t)relocation->getTargetAddress2();
         TR_ASSERT_FATAL(symbolID, "symbolID should exist!\n");
         cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
         TR_RelocationRecordSymbolFromManagerBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordSymbolFromManagerBinaryTemplate *>(cursor);
         binaryTemplate->_symbolID = symbolID;
         binaryTemplate->_symbolType = symbolType;
         cursor += sizeof(TR_RelocationRecordSymbolFromManagerBinaryTemplate);
         }
         break;

      case TR_MethodPointer:
         {
         TR::Node *aconstNode = (TR::Node *) relocation->getTargetAddress();

         uintptrj_t inlinedSiteIndex = (uintptrj_t)aconstNode->getInlinedSiteIndex();
         *(uintptrj_t *)cursor = inlinedSiteIndex;
         cursor += SIZEPOINTER;

         TR_OpaqueMethodBlock *j9method = (TR_OpaqueMethodBlock *) aconstNode->getAddress();
         if (aconstNode->getOpCodeValue() == TR::loadaddr)
            j9method = (TR_OpaqueMethodBlock *) aconstNode->getSymbolReference()->getSymbol()->castToStaticSymbol()->getStaticAddress();
         TR_OpaqueClassBlock *j9class = fej9->getClassFromMethodBlock(j9method);

         uintptrj_t classChainOffsetInSharedCache = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(j9class);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;

         cursor = self()->emitClassChainOffset(cursor, j9class);

         uintptrj_t vTableOffset = (uintptrj_t) fej9->getInterpreterVTableSlot(j9method, j9class);
         *(uintptrj_t *)cursor = vTableOffset;
         cursor += SIZEPOINTER;
         }
         break;

      case TR_ClassPointer:
         {
         TR::Node *aconstNode = (TR::Node *) relocation->getTargetAddress();

         uintptrj_t inlinedSiteIndex = (uintptrj_t)aconstNode->getInlinedSiteIndex();
         *(uintptrj_t *)cursor = inlinedSiteIndex;
         cursor += SIZEPOINTER;

         //for optimizations where we are trying to relocate either profiled j9class or getfrom signature we can't use node to get the target address
         //so we need to pass it to relocation in targetaddress2 for now
         //two instances where use this relotype in such way are: profile checkcast and arraystore check object check optimizations

         TR_OpaqueClassBlock *j9class = NULL;
         if (relocation->getTargetAddress2())
            j9class = (TR_OpaqueClassBlock *) relocation->getTargetAddress2();
         else
            {
            j9class = (TR_OpaqueClassBlock *) aconstNode->getAddress();
            if (aconstNode->getOpCodeValue() == TR::loadaddr)
               j9class = (TR_OpaqueClassBlock *) aconstNode->getSymbolReference()->getSymbol()->castToStaticSymbol()->getStaticAddress();
            }

         uintptrj_t classChainOffsetInSharedCache = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(j9class);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;

         cursor = self()->emitClassChainOffset(cursor, j9class);
         }
         break;

      case TR_ArbitraryClassAddress:
         {
         // ExternalRelocation data is as expected for TR_ClassAddress
         auto symRef = (TR::SymbolReference *)relocation->getTargetAddress();
         auto sym = symRef->getSymbol()->castToStaticSymbol();
         auto j9class = (TR_OpaqueClassBlock *)sym->getStaticAddress();
         uintptr_t inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(symRef->getOwningMethod(self()->comp())->constantPool(), (uintptr_t)relocation->getTargetAddress2());

         // Data identifying the class is as though for TR_ClassPointer
         // (TR_RelocationRecordPointerBinaryTemplate)
         *(uintptrj_t *)cursor = inlinedSiteIndex;
         cursor += SIZEPOINTER;

         uintptrj_t classChainOffsetInSharedCache = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(j9class);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;

         cursor = self()->emitClassChainOffset(cursor, j9class);
         }
         break;

      case TR_InlinedAbstractMethodWithNopGuard:
      case TR_InlinedVirtualMethod:
      case TR_InlinedInterfaceMethod:
         {
         guard = (TR_VirtualGuard *)relocation->getTargetAddress2();

         // Setup flags field with type of method that needs to be validated at relocation time
         if (guard->getSymbolReference()->getSymbol()->getMethodSymbol()->isStatic())
            flags = inlinedMethodIsStatic;
         if (guard->getSymbolReference()->getSymbol()->getMethodSymbol()->isSpecial())
            flags = inlinedMethodIsSpecial;
         if (guard->getSymbolReference()->getSymbol()->getMethodSymbol()->isVirtual())
            flags = inlinedMethodIsVirtual;

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);

         int32_t inlinedSiteIndex = guard->getCurrentInlinedSiteIndex();
         *(uintptrj_t *)cursor = (uintptrj_t)inlinedSiteIndex;
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)guard->getSymbolReference()->getOwningMethod(self()->comp())->constantPool(); // record constant pool
         cursor += SIZEPOINTER;

         if (relocation->getTargetKind() == TR_InlinedInterfaceMethodWithNopGuard ||
             relocation->getTargetKind() == TR_InlinedInterfaceMethod ||
             relocation->getTargetKind() == TR_InlinedAbstractMethodWithNopGuard)
            {
            TR_InlinedCallSite *inlinedCallSite = &self()->comp()->getInlinedCallSite(inlinedSiteIndex);
            TR_AOTMethodInfo *aotMethodInfo = (TR_AOTMethodInfo *)inlinedCallSite->_methodInfo;
            resolvedMethod = aotMethodInfo->resolvedMethod;
            }
         else
            {
            resolvedMethod = guard->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod();
            }

         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            TR_OpaqueMethodBlock *method = resolvedMethod->getPersistentIdentifier();
            TR_OpaqueClassBlock *thisClass = guard->getThisClass();
            uint16_t methodID = symValManager->getIDFromSymbol(static_cast<void *>(method));
            uint16_t receiverClassID = 0;
            if (relocation->getTargetKind() == TR_InlinedInterfaceMethodWithNopGuard ||
               relocation->getTargetKind() == TR_InlinedAbstractMethodWithNopGuard)
               {
               receiverClassID = symValManager->getIDFromSymbol(static_cast<void *>(thisClass));
               }

            *(uintptrj_t*)cursor = (((uintptrj_t)receiverClassID << 16) | (uintptrj_t)methodID);
            }
         else
            {
            *(uintptrj_t*)cursor = (uintptrj_t)guard->getSymbolReference()->getCPIndex(); // record cpIndex
            }
         cursor += SIZEPOINTER;

         TR_OpaqueClassBlock *inlinedMethodClass = resolvedMethod->containingClass();
         void *romClass = (void *)fej9->getPersistentClassPointerFromClassPointer(inlinedMethodClass);
         void *romClassOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(romClass);

         *(uintptrj_t *)cursor = (uintptrj_t)romClassOffsetInSharedCache;
         cursor += SIZEPOINTER;

         if (relocation->getTargetKind() != TR_InlinedInterfaceMethod &&
             relocation->getTargetKind() != TR_InlinedVirtualMethod)
            {
            *(uintptrj_t *)cursor = (uintptrj_t)relocation->getTargetAddress(); // record Patch Destination Address
            cursor += SIZEPOINTER;
            }
         }
         break;

      case TR_ProfiledInlinedMethodRelocation:
      case TR_ProfiledClassGuardRelocation:
      case TR_ProfiledMethodGuardRelocation :
         {
         guard = (TR_VirtualGuard *)relocation->getTargetAddress2();

         int32_t inlinedSiteIndex = guard->getCurrentInlinedSiteIndex();
         *(uintptrj_t *)cursor = (uintptrj_t)inlinedSiteIndex;
         cursor += SIZEPOINTER;

         TR::SymbolReference *callSymRef = guard->getSymbolReference();
         TR_ResolvedMethod *owningMethod = callSymRef->getOwningMethod(self()->comp());

         TR_InlinedCallSite & ics = comp->getInlinedCallSite(inlinedSiteIndex);
         TR_ResolvedMethod *inlinedMethod = ((TR_AOTMethodInfo *)ics._methodInfo)->resolvedMethod;
         TR_OpaqueClassBlock *inlinedCodeClass = reinterpret_cast<TR_OpaqueClassBlock *>(inlinedMethod->classOfMethod());

         *(uintptrj_t *)cursor = (uintptrj_t)owningMethod->constantPool(); // record constant pool
         cursor += SIZEPOINTER;

         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            uint16_t inlinedCodeClassID = symValManager->getIDFromSymbol(static_cast<void *>(inlinedCodeClass));
            uintptrj_t data = (uintptrj_t)inlinedCodeClassID;
            *(uintptrj_t*)cursor = data;
            }
         else
            {
            *(uintptrj_t*)cursor = (uintptrj_t)callSymRef->getCPIndex(); // record cpIndex
            }
         cursor += SIZEPOINTER;

         void *romClass = (void *)fej9->getPersistentClassPointerFromClassPointer(inlinedCodeClass);
         void *romClassOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(romClass);
         traceMsg(self()->comp(), "class is %p, romclass is %p, offset is %p\n", inlinedCodeClass, romClass, romClassOffsetInSharedCache);
         *(uintptrj_t *)cursor = (uintptrj_t) romClassOffsetInSharedCache;
         cursor += SIZEPOINTER;

         uintptrj_t classChainOffsetInSharedCache = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(inlinedCodeClass);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;

         cursor = self()->emitClassChainOffset(cursor, inlinedCodeClass);

         uintptrj_t methodIndex = fej9->getMethodIndexInClass(inlinedCodeClass, inlinedMethod->getNonPersistentIdentifier());
         *(uintptrj_t *)cursor = methodIndex;
         cursor += SIZEPOINTER;
         }
         break;

      case TR_GlobalValue:
         *(uintptrj_t*)cursor = (uintptr_t) relocation->getTargetAddress();
         cursor += SIZEPOINTER;
         break;

      case TR_ValidateClass:
         {
         *(uintptrj_t*)cursor = (uintptrj_t)relocation->getTargetAddress(); // Inlined site index
         cursor += SIZEPOINTER;

         TR::AOTClassInfo *aotCI = (TR::AOTClassInfo*)relocation->getTargetAddress2();
         *(uintptrj_t*)cursor = (uintptrj_t) aotCI->_constantPool;
         cursor += SIZEPOINTER;

         *(uintptrj_t*)cursor = (uintptrj_t) aotCI->_cpIndex;
         cursor += SIZEPOINTER;

         void *classChainOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(aotCI->_classChain);
         *(uintptrj_t *)cursor = (uintptrj_t) classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;
         }
         break;

      case TR_ValidateStaticField:
         {

         *(uintptrj_t*)cursor = (uintptrj_t)relocation->getTargetAddress(); // Inlined site index
         cursor += SIZEPOINTER;

         TR::AOTClassInfo *aotCI = (TR::AOTClassInfo*)relocation->getTargetAddress2();
         *(uintptrj_t*)cursor = (uintptrj_t) aotCI->_constantPool;
         cursor += SIZEPOINTER;

         *(uintptrj_t*)cursor = (uintptrj_t) aotCI->_cpIndex;
         cursor += SIZEPOINTER;

         void *romClass = (void *)fej9->getPersistentClassPointerFromClassPointer(aotCI->_clazz);
         void *romClassOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(romClass);
         *(uintptrj_t *)cursor = (uintptrj_t) romClassOffsetInSharedCache;
         cursor += SIZEPOINTER;
         }
         break;

      case TR_ValidateArbitraryClass:
         {
         TR::AOTClassInfo *aotCI = (TR::AOTClassInfo*) relocation->getTargetAddress2();
         TR_OpaqueClassBlock *classToValidate = aotCI->_clazz;

         //store the classchain's offset for the classloader for the class
         uintptrj_t classChainOffsetInSharedCacheForCL = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(classToValidate);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCacheForCL;
         cursor += SIZEPOINTER;

         //store the classchain's offset for the class that needs to be validated in the second run
         void *romClass = (void *)fej9->getPersistentClassPointerFromClassPointer(classToValidate);
         uintptrj_t *classChainForClassToValidate = (uintptrj_t *) aotCI->_classChain;
         void* classChainOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(classChainForClassToValidate);
         *(uintptrj_t *)cursor = (uintptrj_t) classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;
         }
         break;

      case TR_HCR:
         {
         *(uintptrj_t*)cursor = (uintptrj_t)relocation->getTargetAddress();
         cursor += SIZEPOINTER;
         }
         break;

      case TR_EmitClass:
         {
         *(uintptrj_t *)cursor = (uintptrj_t) relocation->getTargetAddress2(); // Inlined call site index
         cursor += SIZEPOINTER;

         TR_ByteCodeInfo *bcInfo = (TR_ByteCodeInfo *) relocation->getTargetAddress();
         *(int32_t *)cursor = (int32_t) bcInfo->getByteCodeIndex(); // Bytecode index
         cursor += 4;
#if defined(TR_HOST_64BIT)
         cursor += 4; // padding
#endif
         }
         break;

      case TR_DebugCounter:
         {
         TR::DebugCounterBase *counter = (TR::DebugCounterBase *) relocation->getTargetAddress();
         if (!counter || !counter->getReloData() || !counter->getName())
            comp->failCompilation<TR::CompilationException>("Failed to generate debug counter relo data");

         TR::DebugCounterReloData *counterReloData = counter->getReloData();

         uintptrj_t offset = (uintptrj_t)fej9->sharedCache()->rememberDebugCounterName(counter->getName());

         *(uintptrj_t *)cursor = (uintptrj_t)counterReloData->_callerIndex;
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t)counterReloData->_bytecodeIndex;
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = offset;
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t)counterReloData->_delta;
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t)counterReloData->_fidelity;
         cursor += SIZEPOINTER;
         *(uintptrj_t *)cursor = (uintptrj_t)counterReloData->_staticDelta;
         cursor += SIZEPOINTER;
         }
         break;

      default:
         // initializeCommonAOTRelocationHeader is currently in the process
         // of becoming the canonical place to initialize the platform agnostic
         // relocation headers; new relocation records' header should be
         // initialized here.
         cursor = self()->initializeCommonAOTRelocationHeader(relocation, reloRecord);

      }
   return cursor;
   }

#if 0

void J9::Z::AheadOfTimeCompile::dumpRelocationData()
   {
   uint8_t *cursor;
   AOTcgDiag0(comp(), "J9::Z::AheadOfTimeCompile::dumpRelocationData\n");
   cursor = getRelocationData();
   AOTcgDiag1(comp(), "cursor=%x\n", cursor);
   if (cursor)
      {
      uint8_t *endOfData;
      if (TR::Compiler->target.is64Bit())
         {
         endOfData = cursor + *(uint64_t *)cursor;
         diagnostic("Size of relocation data in AOT object is %d bytes\n"
                    "Size field in relocation data is         %d bytes\n", getSizeOfAOTRelocations(),
                    *(uint64_t *)cursor);
         cursor += 8;
         }
      else
         {
         endOfData = cursor + *(uint32_t *)cursor;
         diagnostic("Size of relocation data in AOT object is %d bytes\n"
                    "Size field in relocation data is         %d bytes\n", getSizeOfAOTRelocations(),
                    *(uint32_t *)cursor);
         cursor += 4;
         }

      while (cursor < endOfData)
         {
         diagnostic("\nSize of relocation is %d bytes.\n", *(uint16_t *)cursor);
         uint8_t *endOfCurrentRecord = cursor + *(uint16_t *)cursor;
         cursor += 2;
         TR_ExternalRelocationTargetKind kind = (TR_ExternalRelocationTargetKind)(*cursor & TR_ExternalRelocationTargetKindMask);
         diagnostic("Relocation type is %s.\n", TR::ExternalRelocation::getName(kind));
         int32_t offsetSize = (*cursor & WIDE_OFFSETS) ? 4 : 2;
         bool    isOrderedPair = (*cursor & ORDERED_PAIR)? true : false;
         diagnostic("Relocation uses %d-byte wide offsets.\n", offsetSize);
         diagnostic("Relocation uses %s offsets.\n", isOrderedPair ? "ordered-pair" : "non-paired");
         diagnostic("Relocation is %s.\n", (*cursor & EIP_RELATIVE) ? "EIP Relative" : "Absolute");
         cursor++;
         diagnostic("Target Info:\n");
         switch (kind)
            {
            case TR_ConstantPool:
               // constant pool address is placed as the last word of the header
               if (TR::Compiler->target.is64Bit())
                  {
                  cursor++;    // unused field
                  cursor += 4; // padding
                  diagnostic("Constant pool %p", *(uint64_t *)cursor);
                  cursor += 8;
                  }
               else
                  {
                  diagnostic("Constant pool %p", *(uint32_t *)++cursor);
                  cursor += 4;
                  }
               break;
            case TR_HelperAddress:
               {
               // final byte of header is the index which indicates the particular helper being relocated to
               TR::SymbolReference *tempSR = comp()->getSymRefTab()->getSymRef(*cursor);
               diagnostic("Helper method address of %s(%d)", comp()->getDebug()->getName(tempSR), (int32_t)*cursor++);
               }
               break;
            case TR_RelativeMethodAddress:
               // next word is the address of the constant pool to which the index refers
               // second word is the index in the above stored constant pool that indicates the particular
               // relocation target
               if (TR::Compiler->target.is64Bit())
                  {
                  cursor++; // unused field
                  cursor += 4; // padding
                  diagnostic("Relative Method Address: Constant pool = %p, index = %d",
                              *(uint64_t *)(cursor),
                              *(uint64_t *)(cursor+8));
                  cursor += 16;
                  }
               else
                  {
                  diagnostic("Relative Method Address: Constant pool = %p, index = %d",
                              *(uint32_t *)(cursor+1),
                              *(uint32_t *)(cursor+5));
                  cursor += 9;
                  }
               break;
            case TR_AbsoluteMethodAddress:
               // Reference to the current method, no other information
               diagnostic("Absolute Method Address:");
               cursor++;
               break;
            case TR_DataAddress:
               // next word is the address of the constant pool to which the index refers
               // second word is the index in the above stored constant pool that indicates the particular
               // relocation target
               if (TR::Compiler->target.is64Bit())
                  {
                  cursor++; // unused field
                  cursor += 4; // padding
                  diagnostic("Data Address: Constant pool = %p, index = %d",
                              *(uint64_t *)(cursor),
                              *(uint64_t *)(cursor+8));
                  cursor += 16;
                  }
               else
                  {
                  diagnostic("Data Address: Constant pool = %p, index = %d",
                              *(uint32_t *)(cursor+1),
                              *(uint32_t *)(cursor+5));
                  cursor += 9;
                  }
               break;
            case TR_ClassObject:
               // next word is the address of the constant pool to which the index refers
               // second word is the index in the above stored constant pool that indicates the particular
               // relocation target
               if (TR::Compiler->target.is64Bit())
                  {
                  cursor++; // unused field
                  cursor += 4; // padding
                  diagnostic("Class Object: Constant pool = %p, index = %d",
                              *(uint64_t *)(cursor),
                              *(uint64_t *)(cursor+8));
                  cursor += 16;
                  }
               else
                  {
                  diagnostic("Class Object: Constant pool = %p, index = %d",
                              *(uint32_t *)(cursor+1),
                              *(uint32_t *)(cursor+5));
                  cursor += 9;
                  }
               break;
            case TR_MethodObject:
               // next word is the address of the constant pool to which the index refers
               // second word is the index in the above stored constant pool that indicates the particular
               // relocation target
               if (TR::Compiler->target.is64Bit())
                  {
                  cursor++; // unused field
                  cursor += 4; // padding
                  diagnostic("Method Object: Constant pool = %p, index = %d",
                              *(uint64_t *)(cursor),
                              *(uint64_t *)(cursor+8));
                  cursor += 16;
                  }
               else
                  {
                  diagnostic("Method Object: Constant pool = %p, index = %d",
                              *(uint32_t *)(cursor+1),
                              *(uint32_t *)(cursor+5));
                  cursor += 9;
                  }
               break;
            case TR_InterfaceObject:
               // next word is the address of the constant pool to which the index refers
               // second word is the index in the above stored constant pool that indicates the particular
               // relocation target
               if (TR::Compiler->target.is64Bit())
                  {
                  cursor++; // unused field
                  cursor += 4; // padding
                  diagnostic("Interface Object: Constant pool = %p, index = %d",
                              *(uint64_t *)(cursor),
                              *(uint64_t *)(cursor+8));
                  cursor += 16;
                  }
               else
                  {
                  diagnostic("Interface Object: Constant pool = %p, index = %d",
                              *(uint32_t *)(cursor+1),
                              *(uint32_t *)(cursor+5));
                  cursor += 9;
                  }
               break;
            case TR_BodyInfoAddress:
               // Reference to the method, no other information
               diagnostic("Body Info Address:");
               cursor++;
               break;
            }
         diagnostic("\nUpdate location offsets:");
         uint8_t count = 0;
         if (offsetSize == 2)
            {
            while (cursor < endOfCurrentRecord)
               {
               if ((isOrderedPair && (count % 4)==0) ||
                   (!isOrderedPair && (count % 16)==0))
                  {
                  diagnostic("\n");
                  }
               count++;
               if (isOrderedPair)
                  {
                  diagnostic("(%04x ", *(uint16_t *)cursor);
                  cursor += 2;
                  diagnostic("%04x) ", *(uint16_t *)cursor);
                  cursor += 2;
                  }
               else
                  {
                  diagnostic("%04x ", *(uint16_t *)cursor);
                  cursor += 2;
                  }
               }
            }
         else
            {
            while (cursor < endOfCurrentRecord)
               {
               if ((isOrderedPair && (count % 2)==0) ||
                   (!isOrderedPair && (count % 8)==0))
                  {
                  diagnostic("\n");
                  }
               count++;
               if (isOrderedPair)
                  {
                  diagnostic("(%08x ", *(uint32_t *)cursor);
                  cursor += 4;
                  diagnostic("%08x) ", *(uint32_t *)cursor);
                  cursor += 4;
                  }
               else
                  {
                  diagnostic("%08x ", *(uint32_t *)cursor);
                  cursor += 4;
                  }
               }
            }
         diagnostic("\n");
         }

      diagnostic("\n\n");
      }
   else
      {
      diagnostic("No relocation data allocated\n");
      }
   }
#endif

#if defined(TR_HOST_64BIT)
uint32_t J9::Z::AheadOfTimeCompile::_relocationTargetTypeToHeaderSizeMap[TR_NumExternalRelocationKinds] =
   {
   24,                                        // TR_ConstantPool                        = 0
   8,                                         // TR_HelperAddress                       = 1
   24,                                        // TR_RelativeMethodAddress               = 2
    8,                                        // TR_AbsoluteMethodAddress               = 3
   40,                                        // TR_DataAddress                         = 4
   24,                                        // TR_ClassObject                         = 5
   24,                                        // TR_MethodObject                        = 6
   24,                                        // TR_InterfaceObject                     = 7
   8,                                         //                                        = 8
   16,                                        // Dummy for TR_FixedSeqAddress           = 9
   16,                                        // Dummy for TR_FixedSeq2Address          = 10
   32,                                        // TR_JNIVirtualTargetAddress             = 11
   32,                                        // TR_JNIStaticTargetAddress              = 12
    8,                                        // TR_ArrayCopyHelper                     = 13
   16,                                        // Dummy for TR_ArrayCopyToc              = 14
    8,                                        // TR_BodyInfoAddress                     = 15
   24,                                        // TR_Thunks                              = 16
   32,                                        // TR_StaticRamMethodConst                = 17
   24,                                        // TR_Trampolines                         = 18
    8,                                        // Dummy for TR_PicTrampolines            = 19
   16,                                        // TR_CheckMethodEnter                    = 20
    8,                                        // TR_RamMethod                           = 21
    4,                                        // Dummy                                  = 22
    4,                                        // Dummy                                  = 23
   48,                                        // TR_VerifyClassObjectForAlloc           = 24
   16,                                        // TR_ConstantPoolOrderedPair             = 25
    8,                                        // TR_AbsoluteMethodAddressOrderedPair    = 26
   40,                                        // TR_VerifyRefArrayForAlloc              = 27
   24,                                        // TR_J2IThunks                           = 28
   16,                                        // TR_GlobalValue                         = 29
    4,                                        // dummy for TR_BodyInfoAddress           = 30
   40,                                        // TR_ValidateInstanceField               = 31
   48,                                        // TR_InlinedStaticMethodWithNopGuard     = 32
   48,                                        // TR_InlinedSpecialMethodWithNopGuard    = 33
   48,                                        // TR_InlinedVirtualMethodWithNopGuard    = 34
   48,                                        // TR_InlinedInterfaceMethodWithNopGuard  = 35
   32,                                        // TR_SpecialRamMethodConst               = 36
   48,                                        // TR_InlinedHCRMethod                    = 37
   40,                                        // TR_ValidateStaticField                 = 38
   40,                                        // TR_ValidateClass                       = 39
   32,                                        // TR_ClassAddress                        = 40
   16,                                        // TR_HCR                                 = 41
   64,                                        // TR_ProfiledMethodGuardRelocation       = 42
   64,                                        // TR_ProfiledClassGuardRelocation        = 43
   0,                                         // TR_HierarchyGuardRelocation            = 44
   0,                                         // TR_AbstractGuardRelocation             = 45
   64,                                        // TR_ProfiledInlinedMethod               = 46
   40,                                        // TR_MethodPointer                       = 47
   32,                                        // TR_ClassPointer                        = 48
   16,                                        // TR_CheckMethodExit                     = 49
   24,                                        // TR_ClassValidation                     = 50
   24,                                        // TR_EmitClass                           = 51
   32,                                        // TR_JNISpecialTargetAddress             = 52
   32,                                        // TR_VirtualRamMethodConst               = 53
   40,                                        // TR_InlinedInterfaceMethod              = 54
   40,                                        // TR_InlinedVirtualMethod                = 55
   0,                                         // TR_NativeMethodAbsolute                = 56,
   0,                                         // TR_NativeMethodRelative                = 57,
   32,                                        // TR_ArbitraryClassAddress               = 58,
   56,                                        // TR_DebugCounter                        = 59
   8,                                         // TR_ClassUnloadAssumption               = 60
   32,                                        // TR_J2IVirtualThunkPointer              = 61
   48,                                        // TR_InlinedAbstractMethodWithNopGuard   = 62
   0,                                         // TR_ValidateRootClass                   = 63,
   sizeof(TR_RelocationRecordValidateClassByNameBinaryTemplate),       // TR_ValidateClassByName                 = 64,
   sizeof(TR_RelocationRecordValidateProfiledClassBinaryTemplate),     // TR_ValidateProfiledClass               = 65,
   sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate),       // TR_ValidateClassFromCP                 = 66,
   sizeof(TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate),//TR_ValidateDefiningClassFromCP         = 67,
   sizeof(TR_RelocationRecordValidateStaticClassFromCPBinaryTemplate), // TR_ValidateStaticClassFromCP           = 68,
   0,                                                                  // TR_ValidateClassFromMethod             = 69,
   0,                                                                  // TR_ValidateComponentClassFromArrayClass= 70,
   sizeof(TR_RelocationRecordValidateArrayFromCompBinaryTemplate),     // TR_ValidateArrayClassFromComponentClass= 71,
   sizeof(TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate),//TR_ValidateSuperClassFromClass         = 72,
   sizeof(TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate),//TR_ValidateClassInstanceOfClass       = 73,
   sizeof(TR_RelocationRecordValidateSystemClassByNameBinaryTemplate), //TR_ValidateSystemClassByName            = 74,
   sizeof(TR_RelocationRecordValidateClassFromITableIndexCPBinaryTemplate),//TR_ValidateClassFromITableIndexCP   = 75,
   sizeof(TR_RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate),//TR_ValidateDeclaringClassFromFieldOrStatic=76,
   0,                                                                  // TR_ValidateClassClass                  = 77,
   sizeof(TR_RelocationRecordValidateConcreteSubFromClassBinaryTemplate),//TR_ValidateConcreteSubClassFromClass  = 78,
   sizeof(TR_RelocationRecordValidateClassChainBinaryTemplate),        // TR_ValidateClassChain                  = 79,
   0,                                                                  // TR_ValidateRomClass                    = 80,
   0,                                                                  // TR_ValidatePrimitiveClass              = 81,
   0,                                                                  // TR_ValidateMethodFromInlinedSite       = 82,
   0,                                                                  // TR_ValidatedMethodByName               = 83,
   sizeof(TR_RelocationRecordValidateMethodFromClassBinaryTemplate),   // TR_ValidatedMethodFromClass            = 84,
   sizeof(TR_RelocationRecordValidateStaticMethodFromCPBinaryTemplate),// TR_ValidateStaticMethodFromCP          = 85,
   sizeof(TR_RelocationRecordValidateSpecialMethodFromCPBinaryTemplate),//TR_ValidateSpecialMethodFromCP         = 86,
   sizeof(TR_RelocationRecordValidateVirtualMethodFromCPBinaryTemplate),//TR_ValidateVirtualMethodFromCP         = 87,
   sizeof(TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate),//TR_ValidateVirtualMethodFromOffset = 88,
   sizeof(TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate),//TR_ValidateInterfaceMethodFromCP     = 89,
   sizeof(TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate),//TR_ValidateMethodFromClassAndSig     = 90,
   sizeof(TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate),//TR_ValidateStackWalkerMaySkipFramesRecord= 91,
   0,                                                                    //TR_ValidateArrayClassFromJavaVM      = 92,
   sizeof(TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate),//TR_ValidateClassInfoIsInitialized   = 93,
   sizeof(TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate),//TR_ValidateMethodFromSingleImplementer= 94,
   sizeof(TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate),//TR_ValidateMethodFromSingleInterfaceImplementer= 95,
   sizeof(TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate),//TR_ValidateMethodFromSingleAbstractImplementer= 96,
   sizeof(TR_RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate),//TR_ValidateImproperInterfaceMethodFromCP= 97,
   sizeof(TR_RelocationRecordSymbolFromManagerBinaryTemplate),         // TR_SymbolFromManager = 98,
   };

#else

uint32_t J9::Z::AheadOfTimeCompile::_relocationTargetTypeToHeaderSizeMap[TR_NumExternalRelocationKinds] =
   {
   12,                                        // TR_ConstantPool                        = 0
    8,                                        // TR_HelperAddress                       = 1
   12,                                        // TR_RelativeMethodAddress               = 2
    4,                                        // TR_AbsoluteMethodAddress               = 3
   20,                                        // TR_DataAddress                         = 4
   12,                                        // TR_ClassObject                         = 5
   12,                                        // TR_MethodObject                        = 6
   12,                                        // TR_InterfaceObject                     = 7
    8,                                        //                                        = 8
    8,                                        // Dummy for TR_FixedSeqAddress           = 9
    8,                                        // Dummy for TR_FixedSeq2Address          = 10
   16,                                        // TR_JNIVirtualTargetAddress             = 11
   16,                                        // TR_JNIStaticTargetAddress              = 12
    4,                                        // TR_ArrayCopyHelper                     = 13
   16,                                        // Dummy for TR_ArrayCopyToc              = 14
    4,                                        // TR_BodyInfoAddress                     = 15
   12,                                        // TR_Thunks                              = 16
   16,                                        // TR_StaticRamMethodConst                = 17
   12,                                        // TR_Trampolines                         = 18
    8,                                        // Dummy for TR_PicTrampolines            = 19
    8,                                        // TR_CheckMethodEnter                    = 20
    4,                                        // TR_RamMethod                           = 21
    4,                                        // Dummy                                  = 22
    4,                                        // Dummy                                  = 23
   24,                                        // TR_VerifyClassObjectForAlloc           = 24
    8,                                        // TR_ConstantPoolOrderedPair             = 25
    4,                                        // TR_AbsoluteMethodAddressOrderedPair    = 26
   20,                                        // TR_VerifyRefArrayForAlloc              = 27
   12,                                        // TR_J2IThunks                           = 28
    8,                                        // TR_GlobalValue                         = 29
    4,                                        // dummy for TR_BodyInfoAddressLoad       = 30
   20,                                        // TR_ValidateInstanceField               = 31
   24,                                        // TR_InlinedStaticMethodWithNopGuard     = 32
   24,                                        // TR_InlinedSpecialMethodWithNopGuard    = 33
   24,                                        // TR_InlinedVirtualMethodWithNopGuard    = 34
   24,                                        // TR_InlinedInterfaceMethodWithNopGuard  = 35
   16,                                        // TR_SpecialRamMethodConst               = 36
   24,                                        // TR_InlinedHCRMethod                    = 37
   20,                                        // TR_ValidateStaticField                 = 38
   20,                                        // TR_ValidateClass                       = 39
   16,                                        // TR_ClassAddress                        = 40
   8,                                         // TR_HCR                                 = 41
   32,                                        // TR_ProfiledMethodGuardRelocation       = 42
   32,                                        // TR_ProfiledClassGuardRelocation        = 43
   0,                                         // TR_HierarchyGuardRelocation            = 44
   0,                                         // TR_AbstractGuardRelocation             = 45
   32,                                        // TR_ProfiledInlinedMethod               = 46
   20,                                        // TR_MethodPointer                       = 47
   16,                                        // TR_ClassPointer                        = 48
    8,                                        // TR_CheckMethodExit                     = 49
   12,                                        // TR_ValidateArbitraryClass              = 50
   12,                                        // TR_EmitClass                           = 51
   16,                                        // TR_JNISpecialTargetAddress             = 52
   16,                                        // TR_VirtualRamMethodConst               = 53
   20,                                        // TR_InlinedInterfaceMethod              = 54
   20,                                        // TR_InlinedVirtualMethod                = 55
   0,                                         // TR_NativeMethodAbsolute                = 56,
   0,                                         // TR_NativeMethodRelative                = 57,
   16,                                        // TR_ArbitraryClassAddress               = 58,
   28,                                         // TR_DebugCounter                        = 59
   4,                                         // TR_ClassUnloadAssumption               = 60
   16,                                        // TR_J2IVirtualThunkPointer              = 61
   24,                                        // TR_InlinedAbstractMethodWithNopGuard   = 62
   };

#endif


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

#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
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
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "ras/DebugCounter.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRecord.hpp"

J9::Power::AheadOfTimeCompile::AheadOfTimeCompile(TR::CodeGenerator *cg) :
         J9::AheadOfTimeCompile(_relocationTargetTypeToHeaderSizeMap, cg->comp()),
         _cg(cg)
   {
   }

void J9::Power::AheadOfTimeCompile::processRelocations()
   {
   TR::Compilation* comp = _cg->comp();
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
   if (getSizeOfAOTRelocations() != 0)
      {
      uint8_t *relocationDataCursor;

      if (TR::Compiler->target.is64Bit())
         {
         relocationDataCursor = setRelocationData(fej9->allocateRelocationData(comp, getSizeOfAOTRelocations() + 8));

         // set up the size for the region
         *(uint64_t *) relocationDataCursor = getSizeOfAOTRelocations() + 8;
         relocationDataCursor += 8;

         }
      else
         {
         relocationDataCursor = setRelocationData(fej9->allocateRelocationData(comp, getSizeOfAOTRelocations() + 4));

         // set up the size for the region
         *(uint32_t *) relocationDataCursor = getSizeOfAOTRelocations() + 4;
         relocationDataCursor += 4;
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
   TR::Compilation* comp = TR::comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(_cg->fe());
   TR_SharedCache *sharedCache = fej9->sharedCache();

   TR_VirtualGuard *guard;
   uint8_t flags = 0;
   TR_ResolvedMethod *resolvedMethod;

   uint8_t *cursor = relocation->getRelocationData();

   TR_RelocationRuntime *reloRuntime = comp->reloRuntime();
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();

   uint8_t * aotMethodCodeStart = (uint8_t *) comp->getRelocatableMethodCodeStart();
   // size of relocation goes first in all types
   *(uint16_t *) cursor = relocation->getSizeOfRelocationData();

   cursor += 2;

   uint8_t modifier = 0;
   uint8_t *relativeBitCursor = cursor;
   TR::LabelSymbol *table;
   uint8_t *codeLocation;

   if (relocation->needsWideOffsets())
      modifier |= RELOCATION_TYPE_WIDE_OFFSET;

   uint8_t targetKind = relocation->getTargetKind();
   *cursor++ = targetKind;
   uint8_t *flagsCursor = cursor++;
   *flagsCursor = modifier;
   uint32_t *wordAfterHeader = (uint32_t*)cursor;
#if defined(TR_HOST_64BIT)
   cursor += 4; // padding
#endif

   // This has to be created after the kind has been written into the header
   TR_RelocationRecord storage;
   TR_RelocationRecord *reloRecord = TR_RelocationRecord::create(&storage, reloRuntime, reloTarget, reinterpret_cast<TR_RelocationRecordBinaryTemplate *>(relocation->getRelocationData()));

   switch (targetKind)
      {
      case TR_AbsoluteHelperAddress:
         {
         TR::SymbolReference *tempSR = (TR::SymbolReference *) relocation->getTargetAddress();

         // final byte of header is the index which indicates the
         // particular helper being relocated to
         *wordAfterHeader = (uint32_t) tempSR->getReferenceNumber();
         cursor = (uint8_t*)wordAfterHeader;
         cursor +=4;
         }
         break;
      case TR_RelativeMethodAddress:
         {
         // set the relative relocation bit for references to code addresses
         *flagsCursor |= RELOCATION_TYPE_EIP_OFFSET;
         }
         // deliberate fall-through
         //      case TR_DataAddress:
      case TR_ClassObject:
      case TR_MethodObject:
      case TR_InterfaceObject:
         {
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress();
         TR::SymbolReference *tempSR = (TR::SymbolReference *) recordInfo->data1;

         // next word is the address of the constant pool to
         // which the index refers

         uint8_t flags = (uint8_t) recordInfo->data3;

         if (TR::Compiler->target.is64Bit())
            {
            *(uint64_t *) cursor = (uint64_t) (uintptrj_t) tempSR->getOwningMethod(comp)->constantPool();
            cursor += 8;

            // final word is the index in the above stored constant pool
            // that indicates the particular relocation target
            *(uint64_t *) cursor = (uint64_t) recordInfo->data2;
            cursor += 8;
            }
         else
            {
            TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
            *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);
            *(uint32_t *) cursor = (uint32_t) (uintptrj_t) tempSR->getOwningMethod(comp)->constantPool();
            cursor += 4;

            // final word is the index in the above stored constant pool
            // that indicates the particular relocation target
            *(uint32_t *) cursor = (uint32_t) recordInfo->data2;
            cursor += 4;
            }

         }
         break;
      case TR_JNIStaticTargetAddress:
      case TR_JNISpecialTargetAddress:
      case TR_JNIVirtualTargetAddress:
      case TR_StaticRamMethodConst:
      case TR_VirtualRamMethodConst:
      case TR_SpecialRamMethodConst:
         {
         TR::SymbolReference *tempSR = (TR::SymbolReference *)relocation->getTargetAddress();
         uintptr_t inlinedSiteIndex = (uintptr_t)relocation->getTargetAddress2();

         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(tempSR->getOwningMethod(comp)->constantPool(), inlinedSiteIndex);

         *(uintptrj_t *)cursor = inlinedSiteIndex;  // inlinedSiteIndex
         cursor += SIZEPOINTER;

         *(uintptrj_t *)cursor = (uintptrj_t)tempSR->getOwningMethod(comp)->constantPool(); // constantPool
         cursor += SIZEPOINTER;

         uintptrj_t cpIndex=(uintptrj_t)tempSR->getCPIndex();
         *(uintptrj_t *)cursor =cpIndex;// cpIndex
         cursor += SIZEPOINTER;
         }
         break;
      case TR_ClassAddress:
         {
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress();
         TR::SymbolReference *tempSR = (TR::SymbolReference *) recordInfo->data1;
         uint8_t flags = (uint8_t) recordInfo->data3;

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);

         if (TR::Compiler->target.is64Bit())
            {
            *(uint64_t *) cursor = (uint64_t) self()->findCorrectInlinedSiteIndex(tempSR->getOwningMethod(comp)->constantPool(), recordInfo->data2); //inlineSiteIndex
            cursor += 8;

            *(uint64_t *) cursor = (uint64_t) (uintptrj_t) tempSR->getOwningMethod(comp)->constantPool();
            cursor += 8;

            *(uint64_t *) cursor = tempSR->getCPIndex(); // cpIndex
            cursor += 8;

            }
         else
            {
            *(uint32_t *) cursor = (uint32_t) self()->findCorrectInlinedSiteIndex(tempSR->getOwningMethod(comp)->constantPool(), recordInfo->data2); //inlineSiteIndex

            cursor += 4;

            *(uint32_t *) cursor = (uint32_t) (uintptrj_t) tempSR->getOwningMethod(comp)->constantPool(); //cp
            cursor += 4;

            *(uint32_t *) cursor = tempSR->getCPIndex(); // cpIndex
            cursor += 4;

            }
         }
         break;
      case TR_DataAddress:
         {
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *) relocation->getTargetAddress();
         TR::SymbolReference *tempSR = (TR::SymbolReference *) recordInfo->data1;
         uintptr_t inlinedSiteIndex = (uintptr_t) recordInfo->data2;
         uint8_t flags = (uint8_t) recordInfo->data3;
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);

         // next word is the address of the constant pool to which the index refers
         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(tempSR->getOwningMethod(comp)->constantPool(), inlinedSiteIndex);

         // relocation target
         *(uintptrj_t *) cursor = inlinedSiteIndex; // inlinedSiteIndex
         cursor += SIZEPOINTER;

         *(uintptrj_t *) cursor = (uintptrj_t) tempSR->getOwningMethod(comp)->constantPool(); // constantPool
         cursor += SIZEPOINTER;

         *(uintptrj_t *) cursor = tempSR->getCPIndex(); // cpIndex
         cursor += SIZEPOINTER;

         *(uintptrj_t *) cursor = tempSR->getOffset(); // offset
         cursor += SIZEPOINTER;

         break;
         }

      case TR_AbsoluteMethodAddressOrderedPair:
         {
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *) relocation->getTargetAddress();
         uint8_t flags = (uint8_t) recordInfo->data3;
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);
         }
         break;

      case TR_FixedSequenceAddress:
         {
         uint8_t flags = (uint8_t) ((uintptr_t) relocation->getTargetAddress2());
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);

         table = (TR::LabelSymbol *) relocation->getTargetAddress();
         codeLocation = table->getCodeLocation();

         if (TR::Compiler->target.is64Bit())
            {
            *(uint64_t *) cursor = (uint64_t)(codeLocation - aotMethodCodeStart);
            cursor += 8;
            }
         else
            {
            TR_ASSERT(0, "Creating TR_FixedSeqAddress/TR_FixedSeq2Address relo for 32-bit target");
            cursor += 4;
            }
         }
         break;
      case TR_FixedSequenceAddress2:
         {
         uint8_t flags = (uint8_t) ((uintptr_t) relocation->getTargetAddress2());
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);

         if (TR::Compiler->target.is64Bit())
            {
            if (relocation->getTargetAddress() == NULL)
               printf("target address NULL!!\n");
            *(uint64_t *) cursor = relocation->getTargetAddress() ?
               (uint64_t)((uint8_t *) relocation->getTargetAddress() - aotMethodCodeStart) : 0x0;
            cursor += 8;
            }
         else
            {
            TR_ASSERT(0, "Creating TR_LoadAddress/TR_LoadAddressTempReg relo for 32-bit target");
            cursor += 4;
            }
         }
         break;

      case TR_ArrayCopyHelper:
      case TR_ArrayCopyToc:
         {
         if (TR::Compiler->target.is64Bit())
            {
            uint8_t flags = (uint8_t) ((uintptr_t) relocation->getTargetAddress2());
            TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
            *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);
            }
         else
            {
            TR_RelocationRecordInformation *recordInfo =
                     (TR_RelocationRecordInformation *) relocation->getTargetAddress();
            uint8_t flags = (uint8_t) recordInfo->data3;
            TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
            *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);
            }
         }
         break;


      case TR_AbsoluteMethodAddress:
      case TR_BodyInfoAddress:
         break;
      case TR_BodyInfoAddressLoad:
         {
         if (TR::Compiler->target.is64Bit())
            {
            uint8_t flags = (uint8_t) ((uintptr_t) relocation->getTargetAddress2());
            TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
            *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);
            }
         else
            {
            TR_RelocationRecordInformation *recordInfo =
                     (TR_RelocationRecordInformation*) relocation->getTargetAddress();
            uint8_t flags = (uint8_t) recordInfo->data3;
            TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
            *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);
            }
         }
         break;
      case TR_ConstantPoolOrderedPair:
         {
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress();
         uint8_t flags = (uint8_t) recordInfo->data3;
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);
         if (TR::Compiler->target.is64Bit())
            {
            *(uint64_t *) cursor = (uint64_t) (uintptrj_t) recordInfo->data2;
            cursor += 8;
            *(uint64_t *) cursor = (uint64_t) (uintptrj_t) recordInfo->data1;
            cursor += 8;
            }
         else
            {
            *(uint32_t *) cursor = (uint32_t) (uintptrj_t) recordInfo->data2;
            cursor += 4;
            *(uint32_t *) cursor = (uint32_t) (uintptrj_t) recordInfo->data1;
            cursor += 4;
            }
         break;
         }

      case TR_Trampolines:
      case TR_Thunks:
         {
         // constant pool address is placed as the last word of the header
         if (TR::Compiler->target.is64Bit())
            {
            *(uint64_t *) cursor = (uint64_t) (uintptrj_t) relocation->getTargetAddress2();
            cursor += 8;
            *(uint64_t *) cursor = (uint64_t) (uintptrj_t) relocation->getTargetAddress();
            cursor += 8;
            }
         else
            {
            *(uint32_t *) cursor = (uint32_t) (uintptrj_t) relocation->getTargetAddress2();
            cursor += 4;
            *(uint32_t *) cursor = (uint32_t) (uintptrj_t) relocation->getTargetAddress();
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

      case TR_CheckMethodEnter:
      case TR_CheckMethodExit:
         {
         if (TR::Compiler->target.is64Bit())
            {
            *(uint64_t *) cursor = (uint64_t) (uintptrj_t) relocation->getTargetAddress();
            cursor += 8;
            }
         else
            {
            *(uint32_t *) cursor = (uint32_t) (uintptrj_t) relocation->getTargetAddress();
            cursor += 4;
            }
         }
         break;

      case TR_RamMethod:
         break;

      case TR_RamMethodSequence:
      case TR_RamMethodSequenceReg:
         {
         if (TR::Compiler->target.is64Bit())
            {
            uint8_t flags = (uint8_t) ((uintptr_t) relocation->getTargetAddress2());
            TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
            *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);
            *(uint64_t *) cursor = relocation->getTargetAddress() ?
               (uint64_t)((uint8_t *) relocation->getTargetAddress() - aotMethodCodeStart) : 0x0;
            cursor += 8;
            }
         else
            {
            TR_RelocationRecordInformation *recordInfo =
                     (TR_RelocationRecordInformation *) relocation->getTargetAddress();
            uint8_t flags = (uint8_t) recordInfo->data3;
            TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
            *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);
            cursor += 4;
            }
         }
         break;

      case TR_MethodPointer:
         {
         TR::Node *aconstNode = (TR::Node *) relocation->getTargetAddress();
         uintptrj_t inlinedSiteIndex = (uintptrj_t)aconstNode->getInlinedSiteIndex();
         *(uintptrj_t *)cursor = inlinedSiteIndex;
         cursor += SIZEPOINTER;

         TR_OpaqueMethodBlock *j9method = (TR_OpaqueMethodBlock *) aconstNode->getAddress();
         TR_OpaqueClassBlock *j9class = fej9->getClassFromMethodBlock(j9method);

         void *loaderForClazz = fej9->getClassLoader(j9class);
         void *classChainIdentifyingLoaderForClazz = sharedCache->persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazz);
         uintptrj_t classChainOffsetInSharedCache = (uintptrj_t) sharedCache->offsetInSharedCacheFromPointer(classChainIdentifyingLoaderForClazz);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;

         cursor = emitClassChainOffset(cursor, j9class);

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

         TR_OpaqueClassBlock *j9class = (TR_OpaqueClassBlock *) aconstNode->getAddress();

         void *loaderForClazz = fej9->getClassLoader(j9class);
         void *classChainIdentifyingLoaderForClazz = sharedCache->persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazz);
         uintptrj_t classChainOffsetInSharedCache = (uintptrj_t) sharedCache->offsetInSharedCacheFromPointer(classChainIdentifyingLoaderForClazz);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;

         cursor = emitClassChainOffset(cursor, j9class);
         }
         break;

      case TR_ArbitraryClassAddress:
         {
         // ExternalRelocation data is as expected for TR_ClassAddress
         auto recordInfo = (TR_RelocationRecordInformation *)relocation->getTargetAddress();
         auto symRef = (TR::SymbolReference *)recordInfo->data1;
         auto sym = symRef->getSymbol()->castToStaticSymbol();
         auto j9class = (TR_OpaqueClassBlock *)sym->getStaticAddress();
         uint8_t flags = (uint8_t)recordInfo->data3;
         uintptr_t inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(symRef->getOwningMethod(comp)->constantPool(), recordInfo->data2);

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);

         // Data identifying the class is as though for TR_ClassPointer
         // (TR_RelocationRecordPointerBinaryTemplate)
         *(uintptrj_t *)cursor = inlinedSiteIndex;
         cursor += SIZEPOINTER;

         void *loaderForClazz = fej9->getClassLoader(j9class);
         void *classChainIdentifyingLoaderForClazz = sharedCache->persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazz);
         uintptrj_t classChainOffsetInSharedCache = (uintptrj_t) sharedCache->offsetInSharedCacheFromPointer(classChainIdentifyingLoaderForClazz);
         *(uintptrj_t *)cursor = classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;

         cursor = emitClassChainOffset(cursor, j9class);
         }
         break;

      case TR_InlinedStaticMethodWithNopGuard:
      case TR_InlinedSpecialMethodWithNopGuard:
      case TR_InlinedVirtualMethodWithNopGuard:
      case TR_InlinedVirtualMethod:
      case TR_InlinedInterfaceMethodWithNopGuard:
      case TR_InlinedInterfaceMethod:
      case TR_InlinedHCRMethod:
         {
         guard = (TR_VirtualGuard *) relocation->getTargetAddress2();

         if (relocation->getTargetKind() == TR_InlinedHCRMethod)
            {
            // Setup flags field with type of method that needs to be validated at relocation time
            if (guard->getSymbolReference()->getSymbol()->getMethodSymbol()->isStatic())
               flags = inlinedMethodIsStatic;
            if (guard->getSymbolReference()->getSymbol()->getMethodSymbol()->isSpecial())
               flags = inlinedMethodIsSpecial;
            if (guard->getSymbolReference()->getSymbol()->getMethodSymbol()->isVirtual())
               flags = inlinedMethodIsVirtual;
            }

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);

         int32_t inlinedSiteIndex = guard->getCurrentInlinedSiteIndex();
         *(uintptrj_t *) cursor = (uintptrj_t) inlinedSiteIndex;
         cursor += SIZEPOINTER;

         *(uintptrj_t *) cursor = (uintptrj_t) guard->getSymbolReference()->getOwningMethod(comp)->constantPool(); // record constant pool
         cursor += SIZEPOINTER;

         *(uintptrj_t*) cursor = (uintptrj_t) guard->getSymbolReference()->getCPIndex(); // record cpIndex
         cursor += SIZEPOINTER;

         if (relocation->getTargetKind() == TR_InlinedInterfaceMethodWithNopGuard ||
             relocation->getTargetKind() == TR_InlinedInterfaceMethod)
            {
            TR_InlinedCallSite *inlinedCallSite = &comp->getInlinedCallSite(inlinedSiteIndex);
            TR_AOTMethodInfo *aotMethodInfo = (TR_AOTMethodInfo *) inlinedCallSite->_methodInfo;
            resolvedMethod = aotMethodInfo->resolvedMethod;
            }
         else
            {
            resolvedMethod = guard->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod();
            }

         TR_OpaqueClassBlock *inlinedMethodClass = resolvedMethod->containingClass();
         void *romClass = (void *) fej9->getPersistentClassPointerFromClassPointer(inlinedMethodClass);
         void *romClassOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(romClass);

         *(uintptrj_t *) cursor = (uintptrj_t) romClassOffsetInSharedCache;
         cursor += SIZEPOINTER;

         if (relocation->getTargetKind() != TR_InlinedInterfaceMethod &&
             relocation->getTargetKind() != TR_InlinedVirtualMethod)
            {
            *(uintptrj_t *) cursor = (uintptrj_t) relocation->getTargetAddress(); // record Patch Destination Address
            cursor += SIZEPOINTER;
            }
         }
         break;

      case TR_ProfiledInlinedMethodRelocation:
      case TR_ProfiledClassGuardRelocation:
      case TR_ProfiledMethodGuardRelocation:
         {
         guard = (TR_VirtualGuard *) relocation->getTargetAddress2();

         int32_t inlinedSiteIndex = guard->getCurrentInlinedSiteIndex();
         *(uintptrj_t *) cursor = (uintptrj_t) inlinedSiteIndex;
         cursor += SIZEPOINTER;

         TR::SymbolReference *callSymRef = guard->getSymbolReference();
         TR_ResolvedMethod *owningMethod = callSymRef->getOwningMethod(comp);
         *(uintptrj_t *) cursor = (uintptrj_t) owningMethod->constantPool(); // record constant pool
         cursor += SIZEPOINTER;

         *(uintptrj_t*) cursor = (uintptrj_t) callSymRef->getCPIndex(); // record cpIndex
         cursor += SIZEPOINTER;

         TR_OpaqueClassBlock *inlinedCodeClass = guard->getThisClass();
         int32_t len;
         char *className = fej9->getClassNameChars(inlinedCodeClass, len);

         void *romClass = (void *) fej9->getPersistentClassPointerFromClassPointer(inlinedCodeClass);
         void *romClassOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(romClass);
         traceMsg(comp, "class is %p, romclass is %p, offset is %p\n", inlinedCodeClass, romClass, romClassOffsetInSharedCache);
         *(uintptrj_t *) cursor = (uintptrj_t) romClassOffsetInSharedCache;
         cursor += SIZEPOINTER;

         void *loaderForClazz = fej9->getClassLoader(inlinedCodeClass);
         void *classChainIdentifyingLoaderForClazz =
                  sharedCache->persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazz);
         uintptrj_t classChainOffsetInSharedCache = (uintptrj_t) sharedCache->offsetInSharedCacheFromPointer(classChainIdentifyingLoaderForClazz);
         *(uintptrj_t *) cursor = classChainOffsetInSharedCache;
         cursor += SIZEPOINTER;

         cursor = emitClassChainOffset(cursor, inlinedCodeClass);

         uintptrj_t offset;
         TR::MethodSymbol *calleeSymbol = callSymRef->getSymbol()->castToMethodSymbol();
         if (!TR::Compiler->cls.isInterfaceClass(comp, inlinedCodeClass) && calleeSymbol->isInterface())
            {
            int32_t index = owningMethod->getResolvedInterfaceMethodOffset(inlinedCodeClass, callSymRef->getCPIndex());
            offset = (uintptrj_t) index;
            if (index < 0)
               offset = (uintptrj_t) fej9->virtualCallOffsetToVTableSlot(index);
            }
         else
            {
            offset = (int32_t) callSymRef->getOffset();
            offset = (uintptrj_t) fej9->virtualCallOffsetToVTableSlot((int32_t) callSymRef->getOffset());
            }

         *(uintptrj_t *) cursor = offset;
         cursor += SIZEPOINTER;
         }
         break;

      case TR_VerifyClassObjectForAlloc:
         {
         TR::SymbolReference * classSymRef = (TR::SymbolReference *) relocation->getTargetAddress();
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress2();
         TR::LabelSymbol *label = (TR::LabelSymbol *) recordInfo->data3;
         TR::Instruction *instr = (TR::Instruction *) recordInfo->data4;

         *(uintptrj_t *) cursor = (uintptrj_t) recordInfo->data2; // inlined call site index
         cursor += SIZEPOINTER;

         *(uintptrj_t *) cursor = (uintptrj_t) classSymRef->getOwningMethod(comp)->constantPool();
         cursor += SIZEPOINTER;

         *(uintptrj_t *) cursor = (uintptrj_t) classSymRef->getCPIndex(); // cp Index
         cursor += SIZEPOINTER;

         uint32_t branchOffset = (uint32_t) (label->getCodeLocation() - instr->getBinaryEncoding());
         *(uintptrj_t *) cursor = (uintptrj_t) branchOffset;
         cursor += SIZEPOINTER;

         *(uintptrj_t *) cursor = (uintptrj_t) recordInfo->data1; // allocation size.
         cursor += SIZEPOINTER;
         }
         break;

      case TR_VerifyRefArrayForAlloc:
         {
         TR::SymbolReference * classSymRef = (TR::SymbolReference *) relocation->getTargetAddress();
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress2();
         TR::LabelSymbol *label = (TR::LabelSymbol *) recordInfo->data3;
         TR::Instruction *instr = (TR::Instruction *) recordInfo->data4;

         *(uintptrj_t *) cursor = (uintptrj_t) recordInfo->data2; // inlined call site index
         cursor += SIZEPOINTER;

         *(uintptrj_t *) cursor = (uintptrj_t) classSymRef->getOwningMethod(comp)->constantPool();
         cursor += SIZEPOINTER;

         *(uintptrj_t *) cursor = (uintptrj_t) classSymRef->getCPIndex(); // cp Index
         cursor += SIZEPOINTER;

         uint32_t branchOffset = (uint32_t) (label->getCodeLocation() - instr->getBinaryEncoding());
         *(uintptrj_t *) cursor = (uintptrj_t) branchOffset;
         cursor += SIZEPOINTER;
         }
         break;
      case TR_GlobalValue:
         {
#if defined(TR_HOST_64BIT)
         uint8_t flags = (uint8_t)((uintptr_t)relocation->getTargetAddress2());
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);
         *(uintptrj_t*)cursor = (uintptr_t)relocation->getTargetAddress();
#else

         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*) relocation->getTargetAddress();
         uint8_t flags = (uint8_t) recordInfo->data3;
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);
         *(uintptrj_t*) cursor = (uintptr_t) recordInfo->data1;
#endif

         cursor += SIZEPOINTER;
         break;
         }

      case TR_ValidateClass:
      case TR_ValidateInstanceField:
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
         void *loaderForClazzToValidate = fej9->getClassLoader(classToValidate);
         void *classChainIdentifyingLoaderForClazzToValidate = sharedCache->persistentClassLoaderTable()->lookupClassChainAssociatedWithClassLoader(loaderForClazzToValidate);
         uintptrj_t classChainOffsetInSharedCacheForCL = (uintptrj_t) sharedCache->offsetInSharedCacheFromPointer(classChainIdentifyingLoaderForClazzToValidate);
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
         flags = 0;
         if (((TR_HCRAssumptionFlags)((uintptrj_t)(relocation->getTargetAddress2()))) == needsFullSizeRuntimeAssumption)
        	 flags = needsFullSizeRuntimeAssumption;
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);

         *(uintptrj_t*) cursor = (uintptrj_t) relocation->getTargetAddress();
         cursor += SIZEPOINTER;
         }
         break;

      case TR_DebugCounter:
         {
         TR::DebugCounterBase *counter = (TR::DebugCounterBase *) relocation->getTargetAddress();
         if (!counter || !counter->getReloData() || !counter->getName())
            comp->failCompilation<TR::CompilationException>("Failed to generate debug counter relo data");

         TR::DebugCounterReloData *counterReloData = counter->getReloData();

         uintptrj_t offset = (uintptrj_t)fej9->sharedCache()->rememberDebugCounterName(counter->getName());

         uint8_t flags = (uint8_t)counterReloData->_seqKind;
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         *flagsCursor |= (flags & RELOCATION_RELOC_FLAGS_MASK);

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
   28,                                        // TR_DebugCounter                        = 59
   4,                                        // TR_ClassUnloadAssumption               = 60
   16,                                       // TR_J2IVirtualThunkPointer              = 61
   };

#endif


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

#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "env/SharedCache.hpp"
#include "env/jittypes.h"
#include "env/ClassLoaderTable.hpp"
#include "exceptions/PersistenceFailure.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "il/StaticSymbol.hpp"
#include "env/VMJ9.h"
#include "codegen/AheadOfTimeCompile.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRecord.hpp"
#include "runtime/SymbolValidationManager.hpp"

extern bool isOrderedPair(uint8_t reloType);

uintptr_t
J9::AheadOfTimeCompile::getClassChainOffset(TR_OpaqueClassBlock *classToRemember)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->comp()->fe());
   TR_SharedCache * sharedCache = fej9->sharedCache();
   void *classChainForInlinedMethod = sharedCache->rememberClass(classToRemember);
   if (!classChainForInlinedMethod)
      self()->comp()->failCompilation<J9::ClassChainPersistenceFailure>("classChainForInlinedMethod == NULL");
   return self()->offsetInSharedCacheFromPointer(sharedCache, classChainForInlinedMethod);
   }

uintptr_t
J9::AheadOfTimeCompile::offsetInSharedCacheFromPointer(TR_SharedCache *sharedCache, void *ptr)
   {
   uintptr_t offset = 0;
   if (sharedCache->isPointerInSharedCache(ptr, &offset))
      return offset;
   else
      self()->comp()->failCompilation<J9::ClassChainPersistenceFailure>("Failed to find pointer %p in SCC", ptr);

   return offset;
   }

uintptr_t
J9::AheadOfTimeCompile::offsetInSharedCacheFromROMClass(TR_SharedCache *sharedCache, J9ROMClass *romClass)
   {
   uintptr_t offset = 0;
   if (sharedCache->isROMClassInSharedCache(romClass, &offset))
      return offset;
   else
      self()->comp()->failCompilation<J9::ClassChainPersistenceFailure>("Failed to find romClass %p in SCC", romClass);

   return offset;
   }

uintptr_t
J9::AheadOfTimeCompile::offsetInSharedCacheFromROMMethod(TR_SharedCache *sharedCache, J9ROMMethod *romMethod)
   {
   uintptr_t offset = 0;
   if (sharedCache->isROMMethodInSharedCache(romMethod, &offset))
      return offset;
   else
      self()->comp()->failCompilation<J9::ClassChainPersistenceFailure>("Failed to find romMethod %p in SCC", romMethod);

   return offset;
   }

uintptr_t
J9::AheadOfTimeCompile::findCorrectInlinedSiteIndex(void *constantPool, uintptr_t currentInlinedSiteIndex)
   {
   TR::Compilation *comp = self()->comp();
   uintptr_t constantPoolForSiteIndex = 0;
   uintptr_t inlinedSiteIndex = currentInlinedSiteIndex;

   if (inlinedSiteIndex == (uintptr_t)-1)
      {
      constantPoolForSiteIndex = (uintptr_t)comp->getCurrentMethod()->constantPool();
      }
   else
      {
      TR_InlinedCallSite &inlinedCallSite = comp->getInlinedCallSite(inlinedSiteIndex);
      TR_AOTMethodInfo *callSiteMethodInfo = (TR_AOTMethodInfo *)inlinedCallSite._methodInfo;
      constantPoolForSiteIndex = (uintptr_t)callSiteMethodInfo->resolvedMethod->constantPool();
      }

   bool matchFound = false;

   if ((uintptr_t)constantPool == constantPoolForSiteIndex)
      {
      // The constant pool and site index match, no need to find anything.
      matchFound = true;
      }
   else
      {
      if ((uintptr_t)constantPool == (uintptr_t)comp->getCurrentMethod()->constantPool())
         {
         // The constant pool belongs to the current method being compiled, the correct site index is -1.
         matchFound = true;
         inlinedSiteIndex = (uintptr_t)-1;
         }
      else
         {
         // Look for the first call site whose inlined method's constant pool matches ours and return that site index as the correct one.
         for (uintptr_t i = 0; i < comp->getNumInlinedCallSites(); i++)
            {
            TR_InlinedCallSite &inlinedCallSite = comp->getInlinedCallSite(i);
            TR_AOTMethodInfo *callSiteMethodInfo = (TR_AOTMethodInfo *)inlinedCallSite._methodInfo;

            if ((uintptr_t)constantPool == (uintptr_t)callSiteMethodInfo->resolvedMethod->constantPool())
               {
               matchFound = true;
               inlinedSiteIndex = i;
               break;
               }
            }
         }
      }

   if (!matchFound)
      self()->comp()->failCompilation<J9::AOTRelocationRecordGenerationFailure>("AOT header initialization can't find CP in inlined site list");
   return inlinedSiteIndex;
   }

static const char* getNameForMethodRelocation (int type)
   {
   switch ( type )
      {
         case TR_JNISpecialTargetAddress:
            return "TR_JNISpecialTargetAddress";
         case TR_JNIVirtualTargetAddress:
            return "TR_JNIVirtualTargetAddress";
         case TR_JNIStaticTargetAddress:
            return "TR_JNIStaticTargetAddress";
         case TR_StaticRamMethodConst:
            return "TR_StaticRamMethodConst";
         case TR_SpecialRamMethodConst:
            return "TR_SpecialRamMethodConst";
         case TR_VirtualRamMethodConst:
            return "TR_VirtualRamMethodConst";
         default:
            TR_ASSERT(0, "We already cleared one switch, hard to imagine why we would have a different type here");
            break;
      }

   return NULL;
   }

uint8_t *
J9::AheadOfTimeCompile::initializeAOTRelocationHeader(TR::IteratedExternalRelocation *relocation)
   {
   TR::Compilation *comp = self()->comp();
   TR_RelocationRuntime *reloRuntime = comp->reloRuntime();
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();

   uint8_t *cursor         = relocation->getRelocationData();
   uint8_t targetKind      = relocation->getTargetKind();
   uint16_t sizeOfReloData = relocation->getSizeOfRelocationData();
   uint8_t wideOffsets     = relocation->needsWideOffsets() ? RELOCATION_TYPE_WIDE_OFFSET : 0;

   // Zero-initialize header
   memset(cursor, 0, sizeOfReloData);

   TR_RelocationRecord storage;
   TR_RelocationRecord *reloRecord = TR_RelocationRecord::create(&storage, reloRuntime, targetKind, reinterpret_cast<TR_RelocationRecordBinaryTemplate *>(cursor));

   reloRecord->setSize(reloTarget, sizeOfReloData);
   reloRecord->setType(reloTarget, static_cast<TR_RelocationRecordType>(targetKind));
   reloRecord->setFlag(reloTarget, wideOffsets);

   self()->initializePlatformSpecificAOTRelocationHeader(relocation, reloTarget, reloRecord, targetKind);

   cursor += self()->getSizeOfAOTRelocationHeader(static_cast<TR_RelocationRecordType>(targetKind));
   return cursor;
   }

void
J9::AheadOfTimeCompile::initializeCommonAOTRelocationHeader(TR::IteratedExternalRelocation *relocation,
                                                            TR_RelocationTarget *reloTarget,
                                                            TR_RelocationRecord *reloRecord,
                                                            uint8_t kind)
   {
   TR::Compilation *comp = self()->comp();
   TR::SymbolValidationManager *symValManager = comp->getSymbolValidationManager();
   TR_J9VMBase *fej9 = comp->fej9();
   TR_SharedCache *sharedCache = fej9->sharedCache();
   uint8_t * aotMethodCodeStart = reinterpret_cast<uint8_t *>(comp->getRelocatableMethodCodeStart());

   switch (kind)
      {
      case TR_ConstantPool:
      case TR_Thunks:
      case TR_Trampolines:
         {
         TR_RelocationRecordConstantPool * cpRecord = reinterpret_cast<TR_RelocationRecordConstantPool *>(reloRecord);

         cpRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(relocation->getTargetAddress()));
         cpRecord->setInlinedSiteIndex(reloTarget, reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));
         }
         break;

      case TR_HelperAddress:
         {
         TR_RelocationRecordHelperAddress *haRecord = reinterpret_cast<TR_RelocationRecordHelperAddress *>(reloRecord);
         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(relocation->getTargetAddress());

         haRecord->setEipRelative(reloTarget);
         haRecord->setHelperID(reloTarget, static_cast<uint32_t>(symRef->getReferenceNumber()));
         }
         break;

      case TR_RelativeMethodAddress:
         {
         TR_RelocationRecordMethodAddress *rmaRecord = reinterpret_cast<TR_RelocationRecordMethodAddress *>(reloRecord);

         rmaRecord->setEipRelative(reloTarget);
         }
         break;

      case TR_AbsoluteMethodAddress:
      case TR_BodyInfoAddress:
      case TR_RamMethod:
      case TR_ClassUnloadAssumption:
      case TR_AbsoluteMethodAddressOrderedPair:
      case TR_ArrayCopyHelper:
      case TR_ArrayCopyToc:
      case TR_BodyInfoAddressLoad:
      case TR_RecompQueuedFlag:
         {
         // Nothing to do
         }
         break;

      case TR_MethodCallAddress:
         {
         TR_RelocationRecordMethodCallAddress *mcaRecord = reinterpret_cast<TR_RelocationRecordMethodCallAddress *>(reloRecord);

         mcaRecord->setEipRelative(reloTarget);
         mcaRecord->setAddress(reloTarget, relocation->getTargetAddress());
         }
         break;

      case TR_AbsoluteHelperAddress:
         {
         TR_RelocationRecordAbsoluteHelperAddress *ahaRecord = reinterpret_cast<TR_RelocationRecordAbsoluteHelperAddress *>(reloRecord);
         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(relocation->getTargetAddress());

         ahaRecord->setHelperID(reloTarget, static_cast<uint32_t>(symRef->getReferenceNumber()));
         }
         break;

      case TR_JNIVirtualTargetAddress:
      case TR_JNIStaticTargetAddress:
      case TR_JNISpecialTargetAddress:
      case TR_StaticRamMethodConst:
      case TR_SpecialRamMethodConst:
      case TR_VirtualRamMethodConst:
      case TR_ClassAddress:
         {
         TR_RelocationRecordConstantPoolWithIndex *cpiRecord = reinterpret_cast<TR_RelocationRecordConstantPoolWithIndex *>(reloRecord);
         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(relocation->getTargetAddress());
         uintptr_t inlinedSiteIndex = reinterpret_cast<uintptr_t>(relocation->getTargetAddress2());

         void *constantPool = symRef->getOwningMethod(comp)->constantPool();
         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(constantPool, inlinedSiteIndex);

         cpiRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         cpiRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(constantPool));
         cpiRecord->setCpIndex(reloTarget, symRef->getCPIndex());
         }
         break;

      case TR_CheckMethodEnter:
      case TR_CheckMethodExit:
         {
         TR_RelocationRecordMethodTracingCheck *mtRecord = reinterpret_cast<TR_RelocationRecordMethodTracingCheck *>(reloRecord);

         mtRecord->setDestinationAddress(reloTarget, reinterpret_cast<uintptr_t>(relocation->getTargetAddress()));
         }
         break;

      case TR_VerifyClassObjectForAlloc:
         {
         TR_RelocationRecordVerifyClassObjectForAlloc *allocRecord = reinterpret_cast<TR_RelocationRecordVerifyClassObjectForAlloc *>(reloRecord);

         TR::SymbolReference * classSymRef = reinterpret_cast<TR::SymbolReference *>(relocation->getTargetAddress());
         TR_RelocationRecordInformation *recordInfo = reinterpret_cast<TR_RelocationRecordInformation*>(relocation->getTargetAddress2());
         TR::LabelSymbol *label = reinterpret_cast<TR::LabelSymbol *>(recordInfo->data3);
         TR::Instruction *instr = reinterpret_cast<TR::Instruction *>(recordInfo->data4);

         uint32_t branchOffset = static_cast<uint32_t>(label->getCodeLocation() - instr->getBinaryEncoding());

         allocRecord->setInlinedSiteIndex(reloTarget, static_cast<uintptr_t>(recordInfo->data2));
         allocRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(classSymRef->getOwningMethod(comp)->constantPool()));
         allocRecord->setBranchOffset(reloTarget, static_cast<uintptr_t>(branchOffset));
         allocRecord->setAllocationSize(reloTarget, static_cast<uintptr_t>(recordInfo->data1));

         /* Temporary, will be cleaned up in a future PR */
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            TR_OpaqueClassBlock *classOfMethod = reinterpret_cast<TR_OpaqueClassBlock *>(recordInfo->data5);
            uint16_t classID = symValManager->getIDFromSymbol(static_cast<void *>(classOfMethod));
            allocRecord->setCpIndex(reloTarget, static_cast<uintptr_t>(classID));
            }
         else
            {
            allocRecord->setCpIndex(reloTarget, static_cast<uintptr_t>(classSymRef->getCPIndex()));
            }
         }
         break;

      case TR_VerifyRefArrayForAlloc:
         {
         TR_RelocationRecordVerifyRefArrayForAlloc *allocRecord = reinterpret_cast<TR_RelocationRecordVerifyRefArrayForAlloc *>(reloRecord);

         TR::SymbolReference * classSymRef = reinterpret_cast<TR::SymbolReference *>(relocation->getTargetAddress());
         TR_RelocationRecordInformation *recordInfo = reinterpret_cast<TR_RelocationRecordInformation*>(relocation->getTargetAddress2());
         TR::LabelSymbol *label = reinterpret_cast<TR::LabelSymbol *>(recordInfo->data3);
         TR::Instruction *instr = reinterpret_cast<TR::Instruction *>(recordInfo->data4);

         uint32_t branchOffset = static_cast<uint32_t>(label->getCodeLocation() - instr->getBinaryEncoding());

         allocRecord->setInlinedSiteIndex(reloTarget, static_cast<uintptr_t>(recordInfo->data2));
         allocRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(classSymRef->getOwningMethod(comp)->constantPool()));
         allocRecord->setBranchOffset(reloTarget, static_cast<uintptr_t>(branchOffset));

         /* Temporary, will be cleaned up in a future PR */
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            TR_OpaqueClassBlock *classOfMethod = reinterpret_cast<TR_OpaqueClassBlock *>(recordInfo->data5);
            uint16_t classID = symValManager->getIDFromSymbol(static_cast<void *>(classOfMethod));
            allocRecord->setCpIndex(reloTarget, static_cast<uintptr_t>(classID));
            }
         else
            {
            allocRecord->setCpIndex(reloTarget, static_cast<uintptr_t>(classSymRef->getCPIndex()));
            }
         }
         break;

      case TR_ValidateInstanceField:
         {
         TR_RelocationRecordValidateInstanceField *fieldRecord = reinterpret_cast<TR_RelocationRecordValidateInstanceField *>(reloRecord);
         uintptr_t inlinedSiteIndex = reinterpret_cast<uintptr_t>(relocation->getTargetAddress());
         TR::AOTClassInfo *aotCI = reinterpret_cast<TR::AOTClassInfo*>(relocation->getTargetAddress2());
         uintptr_t classChainOffsetInSharedCache = self()->offsetInSharedCacheFromPointer(sharedCache, aotCI->_classChain);

         fieldRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         fieldRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(aotCI->_constantPool));
         fieldRecord->setCpIndex(reloTarget, static_cast<uintptr_t>(aotCI->_cpIndex));
         fieldRecord->setClassChainOffsetInSharedCache(reloTarget, classChainOffsetInSharedCache);
         }
         break;

      case TR_InlinedStaticMethodWithNopGuard:
      case TR_InlinedSpecialMethodWithNopGuard:
      case TR_InlinedVirtualMethodWithNopGuard:
      case TR_InlinedInterfaceMethodWithNopGuard:
      case TR_InlinedAbstractMethodWithNopGuard:
      case TR_InlinedInterfaceMethod:
      case TR_InlinedVirtualMethod:
      case TR_InlinedStaticMethod:
      case TR_InlinedSpecialMethod:
      case TR_InlinedAbstractMethod:
         {
         TR_RelocationRecordInlinedMethod *imRecord = reinterpret_cast<TR_RelocationRecordInlinedMethod *>(reloRecord);

         TR_RelocationRecordInformation *info = reinterpret_cast<TR_RelocationRecordInformation *>(relocation->getTargetAddress());

         int32_t inlinedSiteIndex        = static_cast<int32_t>(info->data1);
         TR::SymbolReference *callSymRef = reinterpret_cast<TR::SymbolReference *>(info->data2);
         TR_OpaqueClassBlock *thisClass  = reinterpret_cast<TR_OpaqueClassBlock *>(info->data3);
         uintptr_t destinationAddress    = info->data4;

         uint8_t flags = 0;
         // Setup flags field with type of method that needs to be validated at relocation time
         if (callSymRef->getSymbol()->getMethodSymbol()->isStatic())
            flags = inlinedMethodIsStatic;
         else if (callSymRef->getSymbol()->getMethodSymbol()->isSpecial())
            flags = inlinedMethodIsSpecial;
         else if (callSymRef->getSymbol()->getMethodSymbol()->isVirtual())
            flags = inlinedMethodIsVirtual;
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");

         TR_ResolvedMethod *resolvedMethod;
         if (kind == TR_InlinedInterfaceMethodWithNopGuard ||
             kind == TR_InlinedInterfaceMethod ||
             kind == TR_InlinedAbstractMethodWithNopGuard ||
             kind == TR_InlinedAbstractMethod)
            {
            TR_InlinedCallSite *inlinedCallSite = &comp->getInlinedCallSite(inlinedSiteIndex);
            TR_AOTMethodInfo *aotMethodInfo = (TR_AOTMethodInfo *)inlinedCallSite->_methodInfo;
            resolvedMethod = aotMethodInfo->resolvedMethod;
            }
         else
            {
            resolvedMethod = callSymRef->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod();
            }

         // Ugly; this will be cleaned up in a future PR
         uintptr_t cpIndexOrData = 0;
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            TR_OpaqueMethodBlock *method = resolvedMethod->getPersistentIdentifier();
            uint16_t methodID = symValManager->getIDFromSymbol(static_cast<void *>(method));
            uint16_t receiverClassID = symValManager->getIDFromSymbol(static_cast<void *>(thisClass));

            cpIndexOrData = (((uintptr_t)receiverClassID << 16) | (uintptr_t)methodID);
            }
         else
            {
            cpIndexOrData = static_cast<uintptr_t>(callSymRef->getCPIndex());
            }

         TR_OpaqueClassBlock *inlinedMethodClass = resolvedMethod->containingClass();
         J9ROMClass *romClass = reinterpret_cast<J9ROMClass *>(fej9->getPersistentClassPointerFromClassPointer(inlinedMethodClass));
         uintptr_t romClassOffsetInSharedCache = self()->offsetInSharedCacheFromROMClass(sharedCache, romClass);

         imRecord->setReloFlags(reloTarget, flags);
         imRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         imRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(callSymRef->getOwningMethod(comp)->constantPool()));
         imRecord->setCpIndex(reloTarget, cpIndexOrData);
         imRecord->setRomClassOffsetInSharedCache(reloTarget, romClassOffsetInSharedCache);

         if (kind != TR_InlinedInterfaceMethod
             && kind != TR_InlinedVirtualMethod
             && kind != TR_InlinedSpecialMethod
             && kind != TR_InlinedStaticMethod
             && kind != TR_InlinedAbstractMethod)
            {
            reinterpret_cast<TR_RelocationRecordNopGuard *>(imRecord)->setDestinationAddress(reloTarget, destinationAddress);
            }
         }
         break;

      case TR_ValidateStaticField:
         {
         TR_RelocationRecordValidateStaticField *vsfRecord = reinterpret_cast<TR_RelocationRecordValidateStaticField *>(reloRecord);

         uintptr_t inlinedSiteIndex = reinterpret_cast<uintptr_t>(relocation->getTargetAddress());
         TR::AOTClassInfo *aotCI = reinterpret_cast<TR::AOTClassInfo*>(relocation->getTargetAddress2());

         J9ROMClass *romClass = reinterpret_cast<J9ROMClass *>(fej9->getPersistentClassPointerFromClassPointer(aotCI->_clazz));
         uintptr_t romClassOffsetInSharedCache = self()->offsetInSharedCacheFromROMClass(sharedCache, romClass);

         vsfRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         vsfRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(aotCI->_constantPool));
         vsfRecord->setCpIndex(reloTarget, aotCI->_cpIndex);
         vsfRecord->setRomClassOffsetInSharedCache(reloTarget, romClassOffsetInSharedCache);
         }
         break;

      case TR_ValidateClass:
         {
         TR_RelocationRecordValidateClass *vcRecord = reinterpret_cast<TR_RelocationRecordValidateClass *>(reloRecord);

         uintptr_t inlinedSiteIndex = reinterpret_cast<uintptr_t>(relocation->getTargetAddress());
         TR::AOTClassInfo *aotCI = reinterpret_cast<TR::AOTClassInfo*>(relocation->getTargetAddress2());

         uintptr_t classChainOffsetInSharedCache = self()->offsetInSharedCacheFromPointer(sharedCache, aotCI->_classChain);

         vcRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         vcRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(aotCI->_constantPool));
         vcRecord->setCpIndex(reloTarget, aotCI->_cpIndex);
         vcRecord->setClassChainOffsetInSharedCache(reloTarget, classChainOffsetInSharedCache);
         }
         break;

      case TR_ProfiledMethodGuardRelocation:
      case TR_ProfiledClassGuardRelocation:
      case TR_ProfiledInlinedMethodRelocation:
         {
         TR_RelocationRecordProfiledInlinedMethod *pRecord = reinterpret_cast<TR_RelocationRecordProfiledInlinedMethod *>(reloRecord);

         TR_RelocationRecordInformation *info = reinterpret_cast<TR_RelocationRecordInformation *>(relocation->getTargetAddress());

         int32_t inlinedSiteIndex        = static_cast<int32_t>(info->data1);
         TR::SymbolReference *callSymRef = reinterpret_cast<TR::SymbolReference *>(info->data2);

         TR_ResolvedMethod *owningMethod = callSymRef->getOwningMethod(comp);

         TR_InlinedCallSite & ics = comp->getInlinedCallSite(inlinedSiteIndex);
         TR_ResolvedMethod *inlinedMethod = ((TR_AOTMethodInfo *)ics._methodInfo)->resolvedMethod;
         TR_OpaqueClassBlock *inlinedCodeClass = reinterpret_cast<TR_OpaqueClassBlock *>(inlinedMethod->classOfMethod());

         J9ROMClass *romClass = reinterpret_cast<J9ROMClass *>(fej9->getPersistentClassPointerFromClassPointer(inlinedCodeClass));
         uintptr_t romClassOffsetInSharedCache = self()->offsetInSharedCacheFromROMClass(sharedCache, romClass);
         traceMsg(comp, "class is %p, romclass is %p, offset is %llu\n", inlinedCodeClass, romClass, romClassOffsetInSharedCache);

         uintptr_t classChainIdentifyingLoaderOffsetInSharedCache = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(inlinedCodeClass);

         uintptr_t classChainOffsetInSharedCache = self()->getClassChainOffset(inlinedCodeClass);

         uintptr_t methodIndex = fej9->getMethodIndexInClass(inlinedCodeClass, inlinedMethod->getNonPersistentIdentifier());

         // Ugly; this will be cleaned up in a future PR
         uintptr_t cpIndexOrData = 0;
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            uint16_t inlinedCodeClassID = symValManager->getIDFromSymbol(static_cast<void *>(inlinedCodeClass));
            cpIndexOrData = static_cast<uintptr_t>(inlinedCodeClassID);
            }
         else
            {
            cpIndexOrData = static_cast<uintptr_t>(callSymRef->getCPIndex());
            }

         pRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         pRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(owningMethod->constantPool()));
         pRecord->setCpIndex(reloTarget, cpIndexOrData);
         pRecord->setRomClassOffsetInSharedCache(reloTarget, romClassOffsetInSharedCache);
         pRecord->setClassChainIdentifyingLoaderOffsetInSharedCache(reloTarget, classChainIdentifyingLoaderOffsetInSharedCache);
         pRecord->setClassChainForInlinedMethod(reloTarget, classChainOffsetInSharedCache);
         pRecord->setMethodIndex(reloTarget, methodIndex);
         }
         break;

      case TR_MethodPointer:
         {
         TR_RelocationRecordMethodPointer *mpRecord = reinterpret_cast<TR_RelocationRecordMethodPointer *>(reloRecord);

         TR::Node *aconstNode = reinterpret_cast<TR::Node *>(relocation->getTargetAddress());
         uintptr_t inlinedSiteIndex = static_cast<uintptr_t>(aconstNode->getInlinedSiteIndex());

         TR_OpaqueMethodBlock *j9method = reinterpret_cast<TR_OpaqueMethodBlock *>(aconstNode->getAddress());
         if (aconstNode->getOpCodeValue() == TR::loadaddr)
            j9method = reinterpret_cast<TR_OpaqueMethodBlock *>(aconstNode->getSymbolReference()->getSymbol()->castToStaticSymbol()->getStaticAddress());

         TR_OpaqueClassBlock *j9class = fej9->getClassFromMethodBlock(j9method);

         uintptr_t classChainOffsetOfCLInSharedCache = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(j9class);
         uintptr_t classChainForInlinedMethodOffsetInSharedCache = self()->getClassChainOffset(j9class);

         uintptr_t vTableOffset = static_cast<uintptr_t>(fej9->getInterpreterVTableSlot(j9method, j9class));

         mpRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         mpRecord->setClassChainForInlinedMethod(reloTarget, classChainForInlinedMethodOffsetInSharedCache);
         mpRecord->setClassChainIdentifyingLoaderOffsetInSharedCache(reloTarget, classChainOffsetOfCLInSharedCache);
         mpRecord->setVTableSlot(reloTarget, vTableOffset);
         }
         break;

      case TR_InlinedMethodPointer:
         {
         TR_RelocationRecordInlinedMethodPointer *impRecord = reinterpret_cast<TR_RelocationRecordInlinedMethodPointer *>(reloRecord);

         impRecord->setInlinedSiteIndex(reloTarget, reinterpret_cast<uintptr_t>(relocation->getTargetAddress()));
         }
         break;

      case TR_ClassPointer:
         {
         TR_RelocationRecordClassPointer *cpRecord = reinterpret_cast<TR_RelocationRecordClassPointer *>(reloRecord);

         TR::Node *aconstNode = reinterpret_cast<TR::Node *>(relocation->getTargetAddress());
         uintptr_t inlinedSiteIndex = static_cast<uintptr_t>(aconstNode->getInlinedSiteIndex());

         TR_OpaqueClassBlock *j9class = NULL;
         if (relocation->getTargetAddress2())
            {
            j9class = reinterpret_cast<TR_OpaqueClassBlock *>(relocation->getTargetAddress2());
            }
         else
            {
            if (aconstNode->getOpCodeValue() == TR::loadaddr)
               j9class = reinterpret_cast<TR_OpaqueClassBlock *>(aconstNode->getSymbolReference()->getSymbol()->castToStaticSymbol()->getStaticAddress());
            else
               j9class = reinterpret_cast<TR_OpaqueClassBlock *>(aconstNode->getAddress());
            }

         uintptr_t classChainOffsetOfCLInSharedCache = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(j9class);
         uintptr_t classChainForInlinedMethodOffsetInSharedCache = self()->getClassChainOffset(j9class);

         cpRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         cpRecord->setClassChainForInlinedMethod(reloTarget, classChainForInlinedMethodOffsetInSharedCache);
         cpRecord->setClassChainIdentifyingLoaderOffsetInSharedCache(reloTarget, classChainOffsetOfCLInSharedCache);
         }
         break;

      case TR_ValidateArbitraryClass:
         {
         TR_RelocationRecordValidateArbitraryClass *vacRecord = reinterpret_cast<TR_RelocationRecordValidateArbitraryClass *>(reloRecord);

         TR::AOTClassInfo *aotCI = reinterpret_cast<TR::AOTClassInfo*>(relocation->getTargetAddress2());
         TR_OpaqueClassBlock *classToValidate = aotCI->_clazz;

         uintptr_t classChainOffsetInSharedCacheForCL = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(classToValidate);

         void *classChainForClassToValidate = aotCI->_classChain;
         uintptr_t classChainOffsetInSharedCache = self()->offsetInSharedCacheFromPointer(sharedCache, classChainForClassToValidate);

         vacRecord->setClassChainIdentifyingLoaderOffset(reloTarget, classChainOffsetInSharedCacheForCL);
         vacRecord->setClassChainOffsetForClassBeingValidated(reloTarget, classChainOffsetInSharedCache);
         }
         break;

      case TR_J2IVirtualThunkPointer:
         {
         TR_RelocationRecordJ2IVirtualThunkPointer *vtpRecord = reinterpret_cast<TR_RelocationRecordJ2IVirtualThunkPointer *>(reloRecord);

         TR_RelocationRecordInformation *info = reinterpret_cast<TR_RelocationRecordInformation*>(relocation->getTargetAddress());

         vtpRecord->setConstantPool(reloTarget, info->data1);
         vtpRecord->setInlinedSiteIndex(reloTarget, info->data2);
         vtpRecord->setOffsetToJ2IVirtualThunkPointer(reloTarget, info->data3);
         }
         break;

      case TR_ValidateClassByName:
         {
         TR_RelocationRecordValidateClassByName *cbnRecord = reinterpret_cast<TR_RelocationRecordValidateClassByName *>(reloRecord);

         TR::ClassByNameRecord *svmRecord = reinterpret_cast<TR::ClassByNameRecord *>(relocation->getTargetAddress());

         uintptr_t classChainOffsetInSharedCache = self()->offsetInSharedCacheFromPointer(sharedCache, svmRecord->_classChain);

         cbnRecord->setClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_class));
         cbnRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         cbnRecord->setClassChainOffset(reloTarget, classChainOffsetInSharedCache);
         }
         break;

      case TR_ValidateProfiledClass:
         {
         TR_RelocationRecordValidateProfiledClass *pcRecord = reinterpret_cast<TR_RelocationRecordValidateProfiledClass *>(reloRecord);

         TR::ProfiledClassRecord *svmRecord = reinterpret_cast<TR::ProfiledClassRecord *>(relocation->getTargetAddress());

         TR_OpaqueClassBlock *classToValidate = svmRecord->_class;
         void *classChainForClassToValidate = svmRecord->_classChain;

         //store the classchain's offset for the classloader for the class
         uintptr_t classChainOffsetInSharedCacheForCL = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(classToValidate);

         //store the classchain's offset for the class that needs to be validated in the second run
         uintptr_t classChainOffsetInSharedCache = self()->offsetInSharedCacheFromPointer(sharedCache, classChainForClassToValidate);

         pcRecord->setClassID(reloTarget, symValManager->getIDFromSymbol(classToValidate));
         pcRecord->setClassChainOffset(reloTarget, classChainOffsetInSharedCache);
         pcRecord->setClassChainOffsetForClassLoader(reloTarget, classChainOffsetInSharedCacheForCL);
         }
         break;

      case TR_ValidateClassFromCP:
         {
         TR_RelocationRecordValidateClassFromCP *cpRecord = reinterpret_cast<TR_RelocationRecordValidateClassFromCP *>(reloRecord);

         TR::ClassFromCPRecord *svmRecord = reinterpret_cast<TR::ClassFromCPRecord *>(relocation->getTargetAddress());

         cpRecord->setClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_class));
         cpRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         cpRecord->setCpIndex(reloTarget, svmRecord->_cpIndex);
         }
         break;

      case TR_ValidateDefiningClassFromCP:
         {
         TR_RelocationRecordValidateDefiningClassFromCP *dcpRecord = reinterpret_cast<TR_RelocationRecordValidateDefiningClassFromCP *>(reloRecord);

         TR::DefiningClassFromCPRecord *svmRecord = reinterpret_cast<TR::DefiningClassFromCPRecord *>(relocation->getTargetAddress());

         dcpRecord->setIsStatic(reloTarget, svmRecord->_isStatic);
         dcpRecord->setClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_class));
         dcpRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         dcpRecord->setCpIndex(reloTarget, svmRecord->_cpIndex);
         }
         break;

      case TR_ValidateStaticClassFromCP:
         {
         TR_RelocationRecordValidateStaticClassFromCP *scpRecord = reinterpret_cast<TR_RelocationRecordValidateStaticClassFromCP *>(reloRecord);

         TR::StaticClassFromCPRecord *svmRecord = reinterpret_cast<TR::StaticClassFromCPRecord *>(relocation->getTargetAddress());

         scpRecord->setClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_class));
         scpRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         scpRecord->setCpIndex(reloTarget, svmRecord->_cpIndex);
         }
         break;

      case TR_ValidateArrayClassFromComponentClass:
         {
         TR_RelocationRecordValidateArrayClassFromComponentClass *acRecord = reinterpret_cast<TR_RelocationRecordValidateArrayClassFromComponentClass *>(reloRecord);

         TR::ArrayClassFromComponentClassRecord *svmRecord = reinterpret_cast<TR::ArrayClassFromComponentClassRecord *>(relocation->getTargetAddress());

         acRecord->setArrayClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_arrayClass));
         acRecord->setComponentClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_componentClass));
         }
         break;

      case TR_ValidateSuperClassFromClass:
         {
         TR_RelocationRecordValidateSuperClassFromClass *scRecord = reinterpret_cast<TR_RelocationRecordValidateSuperClassFromClass *>(reloRecord);

         TR::SuperClassFromClassRecord *svmRecord = reinterpret_cast<TR::SuperClassFromClassRecord *>(relocation->getTargetAddress());

         scRecord->setSuperClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_superClass));
         scRecord->setChildClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_childClass));
         }
         break;

      case TR_ValidateClassInstanceOfClass:
         {
         TR_RelocationRecordValidateClassInstanceOfClass *cicRecord = reinterpret_cast<TR_RelocationRecordValidateClassInstanceOfClass *>(reloRecord);

         TR::ClassInstanceOfClassRecord *svmRecord = reinterpret_cast<TR::ClassInstanceOfClassRecord *>(relocation->getTargetAddress());

         cicRecord->setObjectTypeIsFixed(reloTarget, svmRecord->_objectTypeIsFixed);
         cicRecord->setCastTypeIsFixed(reloTarget, svmRecord->_castTypeIsFixed);
         cicRecord->setIsInstanceOf(reloTarget, svmRecord->_isInstanceOf);
         cicRecord->setClassOneID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_classOne));
         cicRecord->setClassTwoID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_classTwo));
         }
         break;

      case TR_ValidateSystemClassByName:
         {
         TR_RelocationRecordValidateSystemClassByName *scmRecord = reinterpret_cast<TR_RelocationRecordValidateSystemClassByName *>(reloRecord);

         TR::SystemClassByNameRecord *svmRecord = reinterpret_cast<TR::SystemClassByNameRecord *>(relocation->getTargetAddress());

         TR_OpaqueClassBlock *classToValidate = svmRecord->_class;
         void *classChainForClassToValidate = svmRecord->_classChain;

         // Store class chain to get name of class. Checking the class chain for
         // this record eliminates the need for a separate class chain validation.
         uintptr_t classChainOffsetInSharedCache = self()->offsetInSharedCacheFromPointer(sharedCache, classChainForClassToValidate);

         scmRecord->setSystemClassID(reloTarget, symValManager->getIDFromSymbol(classToValidate));
         scmRecord->setClassChainOffset(reloTarget, classChainOffsetInSharedCache);
         }
         break;

      case TR_ValidateClassFromITableIndexCP:
         {
         TR_RelocationRecordValidateClassFromITableIndexCP *cfitRecord = reinterpret_cast<TR_RelocationRecordValidateClassFromITableIndexCP *>(reloRecord);

         TR::ClassFromITableIndexCPRecord *svmRecord = reinterpret_cast<TR::ClassFromITableIndexCPRecord *>(relocation->getTargetAddress());

         cfitRecord->setClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_class));
         cfitRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         cfitRecord->setCpIndex(reloTarget, svmRecord->_cpIndex);
         }
         break;

      case TR_ValidateDeclaringClassFromFieldOrStatic:
         {
         TR_RelocationRecordValidateDeclaringClassFromFieldOrStatic *dcfsRecord = reinterpret_cast<TR_RelocationRecordValidateDeclaringClassFromFieldOrStatic *>(reloRecord);

         TR::DeclaringClassFromFieldOrStaticRecord *svmRecord = reinterpret_cast<TR::DeclaringClassFromFieldOrStaticRecord *>(relocation->getTargetAddress());

         dcfsRecord->setClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_class));
         dcfsRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         dcfsRecord->setCpIndex(reloTarget, svmRecord->_cpIndex);
         }
         break;

      case TR_ValidateConcreteSubClassFromClass:
         {
         TR_RelocationRecordValidateConcreteSubClassFromClass *csccRecord = reinterpret_cast<TR_RelocationRecordValidateConcreteSubClassFromClass *>(reloRecord);

         TR::ConcreteSubClassFromClassRecord *svmRecord = reinterpret_cast<TR::ConcreteSubClassFromClassRecord *>(relocation->getTargetAddress());

         csccRecord->setSuperClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_superClass));
         csccRecord->setChildClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_childClass));
         }
         break;

      case TR_ValidateClassChain:
         {
         TR_RelocationRecordValidateClassChain *ccRecord = reinterpret_cast<TR_RelocationRecordValidateClassChain *>(reloRecord);

         TR::ClassChainRecord *svmRecord = reinterpret_cast<TR::ClassChainRecord *>(relocation->getTargetAddress());

         TR_OpaqueClassBlock *classToValidate = svmRecord->_class;
         void *classChainForClassToValidate = svmRecord->_classChain;

         // Store class chain to get name of class. Checking the class chain for
         // this record eliminates the need for a separate class chain validation.
         uintptr_t classChainOffsetInSharedCache = self()->offsetInSharedCacheFromPointer(sharedCache, classChainForClassToValidate);

         ccRecord->setClassID(reloTarget, symValManager->getIDFromSymbol(classToValidate));
         ccRecord->setClassChainOffset(reloTarget, classChainOffsetInSharedCache);
         }
         break;

      case TR_ValidateMethodFromClass:
         {
         TR_RelocationRecordValidateMethodFromClass *mfcRecord = reinterpret_cast<TR_RelocationRecordValidateMethodFromClass *>(reloRecord);

         TR::MethodFromClassRecord *svmRecord = reinterpret_cast<TR::MethodFromClassRecord *>(relocation->getTargetAddress());

         mfcRecord->setMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_method));
         mfcRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         mfcRecord->setIndex(reloTarget, svmRecord->_index);
         }
         break;

      case TR_ValidateStaticMethodFromCP:
         {
         TR_RelocationRecordValidateStaticMethodFromCP *smfcpRecord = reinterpret_cast<TR_RelocationRecordValidateStaticMethodFromCP *>(reloRecord);

         TR::StaticMethodFromCPRecord *svmRecord = reinterpret_cast<TR::StaticMethodFromCPRecord *>(relocation->getTargetAddress());

         TR_ASSERT_FATAL(
            (svmRecord->_cpIndex & J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG) == 0,
            "static method cpIndex has special split table flag set");

         if ((svmRecord->_cpIndex & J9_STATIC_SPLIT_TABLE_INDEX_FLAG) != 0)
            smfcpRecord->setReloFlags(reloTarget, TR_VALIDATE_STATIC_OR_SPECIAL_METHOD_FROM_CP_IS_SPLIT);

         smfcpRecord->setMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_method));
         smfcpRecord->setDefiningClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_definingClass));
         smfcpRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         smfcpRecord->setCpIndex(reloTarget, static_cast<uint16_t>(svmRecord->_cpIndex & J9_SPLIT_TABLE_INDEX_MASK));
         }
         break;

      case TR_ValidateSpecialMethodFromCP:
         {
         TR_RelocationRecordValidateSpecialMethodFromCP *smfcpRecord = reinterpret_cast<TR_RelocationRecordValidateSpecialMethodFromCP *>(reloRecord);

         TR::SpecialMethodFromCPRecord *svmRecord = reinterpret_cast<TR::SpecialMethodFromCPRecord *>(relocation->getTargetAddress());

         TR_ASSERT_FATAL(
            (svmRecord->_cpIndex & J9_STATIC_SPLIT_TABLE_INDEX_FLAG) == 0,
            "special method cpIndex has static split table flag set");

         if ((svmRecord->_cpIndex & J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG) != 0)
            smfcpRecord->setReloFlags(reloTarget, TR_VALIDATE_STATIC_OR_SPECIAL_METHOD_FROM_CP_IS_SPLIT);

         smfcpRecord->setMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_method));
         smfcpRecord->setDefiningClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_definingClass));
         smfcpRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         smfcpRecord->setCpIndex(reloTarget, static_cast<uint16_t>(svmRecord->_cpIndex & J9_SPLIT_TABLE_INDEX_MASK));
         }
         break;

      case TR_ValidateVirtualMethodFromCP:
         {
         TR_RelocationRecordValidateVirtualMethodFromCP *vmfcpRecord = reinterpret_cast<TR_RelocationRecordValidateVirtualMethodFromCP *>(reloRecord);

         TR::VirtualMethodFromCPRecord *svmRecord = reinterpret_cast<TR::VirtualMethodFromCPRecord *>(relocation->getTargetAddress());

         vmfcpRecord->setMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_method));
         vmfcpRecord->setDefiningClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_definingClass));
         vmfcpRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         vmfcpRecord->setCpIndex(reloTarget, static_cast<uint16_t>(svmRecord->_cpIndex));
         }
         break;

      case TR_ValidateVirtualMethodFromOffset:
         {
         TR_RelocationRecordValidateVirtualMethodFromOffset *vmfoRecord = reinterpret_cast<TR_RelocationRecordValidateVirtualMethodFromOffset *>(reloRecord);

         TR::VirtualMethodFromOffsetRecord *svmRecord = reinterpret_cast<TR::VirtualMethodFromOffsetRecord *>(relocation->getTargetAddress());

         TR_ASSERT_FATAL((svmRecord->_virtualCallOffset & 1) == 0, "virtualCallOffset must be even");
         TR_ASSERT_FATAL(
            svmRecord->_virtualCallOffset == (int32_t)(int16_t)svmRecord->_virtualCallOffset,
            "virtualCallOffset must fit in a 16-bit signed integer");

         uint16_t ignoreRtResolve = static_cast<uint16_t>(svmRecord->_ignoreRtResolve);
         uint16_t virtualCallOffset = static_cast<uint16_t>(svmRecord->_virtualCallOffset);

         vmfoRecord->setMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_method));
         vmfoRecord->setDefiningClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_definingClass));
         vmfoRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         vmfoRecord->setVirtualCallOffsetAndIgnoreRtResolve(reloTarget, (virtualCallOffset | ignoreRtResolve));
         }
         break;

      case TR_ValidateInterfaceMethodFromCP:
         {
         TR_RelocationRecordValidateInterfaceMethodFromCP *imfcpRecord = reinterpret_cast<TR_RelocationRecordValidateInterfaceMethodFromCP *>(reloRecord);

         TR::InterfaceMethodFromCPRecord *svmRecord = reinterpret_cast<TR::InterfaceMethodFromCPRecord *>(relocation->getTargetAddress());

         imfcpRecord->setMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_method));
         imfcpRecord->setDefiningClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_definingClass));
         imfcpRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         imfcpRecord->setLookupID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_lookup));
         imfcpRecord->setCpIndex(reloTarget, static_cast<uint16_t>(svmRecord->_cpIndex));
         }
         break;

      case TR_ValidateImproperInterfaceMethodFromCP:
         {
         TR_RelocationRecordValidateImproperInterfaceMethodFromCP *iimfcpRecord = reinterpret_cast<TR_RelocationRecordValidateImproperInterfaceMethodFromCP *>(reloRecord);

         TR::ImproperInterfaceMethodFromCPRecord *svmRecord = reinterpret_cast<TR::ImproperInterfaceMethodFromCPRecord *>(relocation->getTargetAddress());

         iimfcpRecord->setMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_method));
         iimfcpRecord->setDefiningClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_definingClass));
         iimfcpRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         iimfcpRecord->setCpIndex(reloTarget, static_cast<uint16_t>(svmRecord->_cpIndex));
         }
         break;

      case TR_ValidateMethodFromClassAndSig:
         {
         TR_RelocationRecordValidateMethodFromClassAndSig *mfcsRecord = reinterpret_cast<TR_RelocationRecordValidateMethodFromClassAndSig *>(reloRecord);

         TR::MethodFromClassAndSigRecord *svmRecord = reinterpret_cast<TR::MethodFromClassAndSigRecord *>(relocation->getTargetAddress());

         // Store rom method to get name of method
         J9Method *methodToValidate = reinterpret_cast<J9Method *>(svmRecord->_method);
         J9ROMMethod *romMethod = static_cast<TR_J9VM *>(fej9)->getROMMethodFromRAMMethod(methodToValidate);
         uintptr_t romMethodOffsetInSharedCache = self()->offsetInSharedCacheFromROMMethod(sharedCache, romMethod);

         mfcsRecord->setMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_method));
         mfcsRecord->setDefiningClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_definingClass));
         mfcsRecord->setBeholderID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_beholder));
         mfcsRecord->setLookupClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_lookupClass));
         mfcsRecord->setRomMethodOffsetInSCC(reloTarget, romMethodOffsetInSharedCache);
         }
         break;

      case TR_ValidateStackWalkerMaySkipFramesRecord:
         {
         TR_RelocationRecordValidateStackWalkerMaySkipFrames *swmsfRecord = reinterpret_cast<TR_RelocationRecordValidateStackWalkerMaySkipFrames *>(reloRecord);

         TR::StackWalkerMaySkipFramesRecord *svmRecord = reinterpret_cast<TR::StackWalkerMaySkipFramesRecord *>(relocation->getTargetAddress());

         swmsfRecord->setMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_method));
         swmsfRecord->setMethodClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_methodClass));
         swmsfRecord->setSkipFrames(reloTarget, svmRecord->_skipFrames);
         }
         break;

      case TR_ValidateClassInfoIsInitialized:
         {
         TR_RelocationRecordValidateClassInfoIsInitialized *ciiiRecord = reinterpret_cast<TR_RelocationRecordValidateClassInfoIsInitialized *>(reloRecord);

         TR::ClassInfoIsInitialized *svmRecord = reinterpret_cast<TR::ClassInfoIsInitialized *>(relocation->getTargetAddress());

         ciiiRecord->setClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_class));
         ciiiRecord->setIsInitialized(reloTarget, svmRecord->_isInitialized);
         }
         break;

      case TR_ValidateMethodFromSingleImplementer:
         {
         TR_RelocationRecordValidateMethodFromSingleImpl *mfsiRecord = reinterpret_cast<TR_RelocationRecordValidateMethodFromSingleImpl *>(reloRecord);

         TR::MethodFromSingleImplementer *svmRecord = reinterpret_cast<TR::MethodFromSingleImplementer *>(relocation->getTargetAddress());

         mfsiRecord->setMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_method));
         mfsiRecord->setDefiningClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_definingClass));
         mfsiRecord->setThisClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_thisClass));
         mfsiRecord->setCpIndexOrVftSlot(reloTarget, svmRecord->_cpIndexOrVftSlot);
         mfsiRecord->setCallerMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_callerMethod));
         mfsiRecord->setUseGetResolvedInterfaceMethod(reloTarget, svmRecord->_useGetResolvedInterfaceMethod);
         }
         break;

      case TR_ValidateMethodFromSingleInterfaceImplementer:
         {
         TR_RelocationRecordValidateMethodFromSingleInterfaceImpl *mfsiiRecord = reinterpret_cast<TR_RelocationRecordValidateMethodFromSingleInterfaceImpl *>(reloRecord);

         TR::MethodFromSingleInterfaceImplementer *svmRecord = reinterpret_cast<TR::MethodFromSingleInterfaceImplementer *>(relocation->getTargetAddress());

         mfsiiRecord->setMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_method));
         mfsiiRecord->setDefiningClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_definingClass));
         mfsiiRecord->setThisClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_thisClass));
         mfsiiRecord->setCallerMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_callerMethod));
         mfsiiRecord->setCpIndex(reloTarget, static_cast<uint16_t>(svmRecord->_cpIndex));
         }
         break;

      case TR_ValidateMethodFromSingleAbstractImplementer:
         {
         TR_RelocationRecordValidateMethodFromSingleAbstractImpl *mfsaiRecord = reinterpret_cast<TR_RelocationRecordValidateMethodFromSingleAbstractImpl *>(reloRecord);

         TR::MethodFromSingleAbstractImplementer *svmRecord = reinterpret_cast<TR::MethodFromSingleAbstractImplementer *>(relocation->getTargetAddress());

         mfsaiRecord->setMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_method));
         mfsaiRecord->setDefiningClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_definingClass));
         mfsaiRecord->setThisClassID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_thisClass));
         mfsaiRecord->setCallerMethodID(reloTarget, symValManager->getIDFromSymbol(svmRecord->_callerMethod));
         mfsaiRecord->setVftSlot(reloTarget, svmRecord->_vftSlot);
         }
         break;

      case TR_SymbolFromManager:
         {
         TR_RelocationRecordSymbolFromManager *sfmRecord = reinterpret_cast<TR_RelocationRecordSymbolFromManager *>(reloRecord);

         uint8_t *symbol = relocation->getTargetAddress();
         uint16_t symbolID = comp->getSymbolValidationManager()->getIDFromSymbol(static_cast<void *>(symbol));

         uint16_t symbolType = (uint16_t)(uintptr_t)relocation->getTargetAddress2();

         sfmRecord->setSymbolID(reloTarget, symbolID);
         sfmRecord->setSymbolType(reloTarget, static_cast<TR::SymbolType>(symbolType));
         }
         break;

      case TR_ResolvedTrampolines:
         {
         TR_RelocationRecordResolvedTrampolines *rtRecord = reinterpret_cast<TR_RelocationRecordResolvedTrampolines *>(reloRecord);

         uint8_t *symbol = relocation->getTargetAddress();
         uint16_t symbolID = comp->getSymbolValidationManager()->getIDFromSymbol(static_cast<void *>(symbol));

         rtRecord->setSymbolID(reloTarget, symbolID);
         }
         break;

      case TR_MethodObject:
         {
         TR_RelocationRecordMethodObject *moRecord = reinterpret_cast<TR_RelocationRecordMethodObject *>(reloRecord);

         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(relocation->getTargetAddress());

         moRecord->setInlinedSiteIndex(reloTarget, reinterpret_cast<uintptr_t>(relocation->getTargetAddress2()));
         moRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(symRef->getOwningMethod(comp)->constantPool()));
         }
         break;

      case TR_DataAddress:
         {
         TR_RelocationRecordDataAddress *daRecord = reinterpret_cast<TR_RelocationRecordDataAddress *>(reloRecord);

         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(relocation->getTargetAddress());
         uintptr_t inlinedSiteIndex = reinterpret_cast<uintptr_t>(relocation->getTargetAddress2());

         void *constantPool = symRef->getOwningMethod(comp)->constantPool();
         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(constantPool, inlinedSiteIndex);

         daRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         daRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(constantPool));
         daRecord->setCpIndex(reloTarget, symRef->getCPIndex());
         daRecord->setOffset(reloTarget, symRef->getOffset());
         }
         break;

      case TR_FixedSequenceAddress2:
      case TR_RamMethodSequence:
         {
         TR_RelocationRecordWithOffset *rwoRecord = reinterpret_cast<TR_RelocationRecordWithOffset *>(reloRecord);

         TR_ASSERT_FATAL(relocation->getTargetAddress(), "target address is NULL");
         uintptr_t offset = relocation->getTargetAddress()
                  ? static_cast<uintptr_t>(relocation->getTargetAddress() - aotMethodCodeStart)
                  : 0x0;

         rwoRecord->setOffset(reloTarget, offset);
         }
         break;

      case TR_ArbitraryClassAddress:
         {
         TR_RelocationRecordArbitraryClassAddress *acaRecord = reinterpret_cast<TR_RelocationRecordArbitraryClassAddress *>(reloRecord);

         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(relocation->getTargetAddress());
         TR::StaticSymbol *sym = symRef->getSymbol()->castToStaticSymbol();
         TR_OpaqueClassBlock *j9class = reinterpret_cast<TR_OpaqueClassBlock *>(sym->getStaticAddress());
         uintptr_t inlinedSiteIndex = reinterpret_cast<uintptr_t>(relocation->getTargetAddress2());

         void *constantPool = symRef->getOwningMethod(comp)->constantPool();
         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(constantPool, inlinedSiteIndex);

         uintptr_t classChainIdentifyingLoaderOffsetInSharedCache = sharedCache->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(j9class);
         uintptr_t classChainOffsetInSharedCache = self()->getClassChainOffset(j9class);

         acaRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         acaRecord->setClassChainIdentifyingLoaderOffsetInSharedCache(reloTarget, classChainIdentifyingLoaderOffsetInSharedCache);
         acaRecord->setClassChainForInlinedMethod(reloTarget, classChainOffsetInSharedCache);
         }
         break;

      case TR_GlobalValue:
      case TR_HCR:
         {
         TR_RelocationRecordWithOffset *rwoRecord = reinterpret_cast<TR_RelocationRecordWithOffset *>(reloRecord);

         uintptr_t gv = reinterpret_cast<uintptr_t>(relocation->getTargetAddress());

         rwoRecord->setOffset(reloTarget, gv);
         }
         break;

      case TR_DebugCounter:
         {
         TR_RelocationRecordDebugCounter *dcRecord = reinterpret_cast<TR_RelocationRecordDebugCounter *>(reloRecord);

         TR::DebugCounterBase *counter = reinterpret_cast<TR::DebugCounterBase *>(relocation->getTargetAddress());
         if (!counter || !counter->getReloData() || !counter->getName())
            comp->failCompilation<TR::CompilationException>("Failed to generate debug counter relo data");

         TR::DebugCounterReloData *counterReloData = counter->getReloData();

         uintptr_t offsetOfNameString = fej9->sharedCache()->rememberDebugCounterName(counter->getName());
         uint8_t flags = counterReloData->_seqKind;

         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         dcRecord->setReloFlags(reloTarget, flags);
         dcRecord->setInlinedSiteIndex(reloTarget, static_cast<uintptr_t>(counterReloData->_callerIndex));
         dcRecord->setBCIndex(reloTarget, counterReloData->_bytecodeIndex);
         dcRecord->setDelta(reloTarget, counterReloData->_delta);
         dcRecord->setFidelity(reloTarget, counterReloData->_fidelity);
         dcRecord->setStaticDelta(reloTarget, counterReloData->_staticDelta);
         dcRecord->setOffsetOfNameString(reloTarget, offsetOfNameString);
         }
         break;

      case TR_BlockFrequency:
         {
         TR_RelocationRecordBlockFrequency *bfRecord = reinterpret_cast<TR_RelocationRecordBlockFrequency *>(reloRecord);
         TR_RelocationRecordInformation *recordInfo = reinterpret_cast<TR_RelocationRecordInformation *>(relocation->getTargetAddress());
         TR::SymbolReference *tempSR = reinterpret_cast<TR::SymbolReference *>(recordInfo->data1);
         TR::StaticSymbol *staticSym = tempSR->getSymbol()->getStaticSymbol();

         uint8_t flags = (uint8_t) recordInfo->data2;
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");
         bfRecord->setReloFlags(reloTarget, flags);

         TR_PersistentProfileInfo *profileInfo = comp->getRecompilationInfo()->getProfileInfo();
         TR_ASSERT(NULL != profileInfo, "PersistentProfileInfo not found when creating relocation record for block frequency\n");
         if (NULL == profileInfo)
            {
            comp->failCompilation<J9::AOTRelocationRecordGenerationFailure>("AOT header initialization can't find profile info");
            }

         TR_BlockFrequencyInfo *blockFrequencyInfo = profileInfo->getBlockFrequencyInfo();
         TR_ASSERT(NULL != blockFrequencyInfo, "BlockFrequencyInfo not found when creating relocation record for block frequency\n");
         if (NULL == blockFrequencyInfo)
            {
            comp->failCompilation<J9::AOTRelocationRecordGenerationFailure>("AOT header initialization can't find block frequency info");
            }

         uintptr_t frequencyArrayBase = reinterpret_cast<uintptr_t>(blockFrequencyInfo->getFrequencyArrayBase());
         uintptr_t frequencyPtr = reinterpret_cast<uintptr_t>(staticSym->getStaticAddress());

         bfRecord->setFrequencyOffset(reloTarget, frequencyPtr - frequencyArrayBase);
         }
         break;

      case TR_Breakpoint:
         {
         TR_RelocationRecordBreakpointGuard *bpgRecord = reinterpret_cast<TR_RelocationRecordBreakpointGuard *>(reloRecord);

         int32_t inlinedSiteIndex     = static_cast<int32_t>(reinterpret_cast<uintptr_t>(relocation->getTargetAddress()));
         uintptr_t destinationAddress = reinterpret_cast<uintptr_t>(relocation->getTargetAddress2());

         bpgRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         bpgRecord->setDestinationAddress(reloTarget, destinationAddress);
         }
         break;

      default:
         TR_ASSERT(false, "Unknown relo type %d!\n", kind);
         comp->failCompilation<J9::AOTRelocationRecordGenerationFailure>("Unknown relo type %d!\n", kind);
         break;
      }
   }

uint8_t *
J9::AheadOfTimeCompile::dumpRelocationHeaderData(uint8_t *cursor, bool isVerbose)
   {
   TR::Compilation *comp = TR::comp();
   TR_RelocationRuntime *reloRuntime = comp->reloRuntime();
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();

   TR_RelocationRecord storage;
   TR_RelocationRecord *reloRecord = TR_RelocationRecord::create(&storage, reloRuntime, reloTarget, reinterpret_cast<TR_RelocationRecordBinaryTemplate *>(cursor));

   TR_ExternalRelocationTargetKind kind = reloRecord->type(reloTarget);

   int32_t offsetSize = reloRecord->wideOffsets(reloTarget) ? 4 : 2;

   uint8_t *startOfOffsets = cursor + self()->getSizeOfAOTRelocationHeader(kind);
   uint8_t *endOfCurrentRecord = cursor + reloRecord->size(reloTarget);

   bool orderedPair = isOrderedPair(kind);

   traceMsg(self()->comp(), "%16x  ", cursor);
   traceMsg(self()->comp(), "%-5d", reloRecord->size(reloTarget));
   traceMsg(self()->comp(), "%-31s", TR::ExternalRelocation::getName(kind));
   traceMsg(self()->comp(), "%-6d", offsetSize);
   traceMsg(self()->comp(), "%s", (reloRecord->flags(reloTarget) & RELOCATION_TYPE_EIP_OFFSET) ? "Rel " : "Abs ");

   // Print out the correct number of spaces when no index is available
   if (kind != TR_HelperAddress && kind != TR_AbsoluteHelperAddress)
      traceMsg(self()->comp(), "      ");

   switch (kind)
      {
      case TR_ConstantPool:
      case TR_Thunks:
      case TR_Trampolines:
         {
         TR_RelocationRecordConstantPool * cpRecord = reinterpret_cast<TR_RelocationRecordConstantPool *>(reloRecord);

         self()->traceRelocationOffsets(startOfOffsets, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nInlined site index = %d, Constant pool = %x",
                                     cpRecord->inlinedSiteIndex(reloTarget),
                                     cpRecord->constantPool(reloTarget));
            }
         }
         break;

      case TR_MethodObject:
         {
         TR_RelocationRecordConstantPool * cpRecord = reinterpret_cast<TR_RelocationRecordConstantPool *>(reloRecord);

         self()->traceRelocationOffsets(startOfOffsets, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nInlined site index = %d, Constant pool = %x, flags = %x",
                                     cpRecord->inlinedSiteIndex(reloTarget),
                                     cpRecord->constantPool(reloTarget),
                                     (uint32_t)cpRecord->reloFlags(reloTarget));
            }
         }
         break;

      case TR_HelperAddress:
         {
         TR_RelocationRecordHelperAddress *haRecord = reinterpret_cast<TR_RelocationRecordHelperAddress *>(reloRecord);

         uint32_t helperID = haRecord->helperID(reloTarget);

         traceMsg(self()->comp(), "%-6d", helperID);
         self()->traceRelocationOffsets(startOfOffsets, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            TR::SymbolReference *symRef = self()->comp()->getSymRefTab()->getSymRef(helperID);
            traceMsg(self()->comp(), "\nHelper method address of %s(%d)", self()->getDebug()->getName(symRef), helperID);
            }
         }
         break;

      case TR_RelativeMethodAddress:
      case TR_AbsoluteMethodAddress:
      case TR_BodyInfoAddress:
      case TR_RamMethod:
      case TR_ClassUnloadAssumption:
      case TR_AbsoluteMethodAddressOrderedPair:
      case TR_ArrayCopyHelper:
      case TR_ArrayCopyToc:
      case TR_BodyInfoAddressLoad:
      case TR_RecompQueuedFlag:
         {
         self()->traceRelocationOffsets(startOfOffsets, offsetSize, endOfCurrentRecord, orderedPair);
         }
         break;

      case TR_MethodCallAddress:
         {
         TR_RelocationRecordMethodCallAddress *mcaRecord = reinterpret_cast<TR_RelocationRecordMethodCallAddress *>(reloRecord);
         traceMsg(self()->comp(), "\n Method Call Address: address=" POINTER_PRINTF_FORMAT, mcaRecord->address(reloTarget));
         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         }
         break;

      case TR_AbsoluteHelperAddress:
         {
         TR_RelocationRecordAbsoluteHelperAddress *ahaRecord = reinterpret_cast<TR_RelocationRecordAbsoluteHelperAddress *>(reloRecord);

         uint32_t helperID = ahaRecord->helperID(reloTarget);

         traceMsg(self()->comp(), "%-6d", helperID);
         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            TR::SymbolReference *symRef = self()->comp()->getSymRefTab()->getSymRef(helperID);
            traceMsg(self()->comp(), "\nHelper method address of %s(%d)", self()->getDebug()->getName(symRef), helperID);
            }
         }
         break;

      case TR_JNIVirtualTargetAddress:
      case TR_JNIStaticTargetAddress:
      case TR_JNISpecialTargetAddress:
      case TR_StaticRamMethodConst:
      case TR_SpecialRamMethodConst:
      case TR_VirtualRamMethodConst:
         {
         TR_RelocationRecordConstantPoolWithIndex *cpiRecord = reinterpret_cast<TR_RelocationRecordConstantPoolWithIndex *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Address Relocation (%s): inlinedIndex = %d, constantPool = %p, CPI = %d",
                                     getNameForMethodRelocation(kind),
                                     cpiRecord->inlinedSiteIndex(reloTarget),
                                     cpiRecord->constantPool(reloTarget),
                                     cpiRecord->cpIndex(reloTarget));
            }
         }
         break;

      case TR_ClassAddress:
         {
         TR_RelocationRecordConstantPoolWithIndex *cpiRecord = reinterpret_cast<TR_RelocationRecordConstantPoolWithIndex *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Address Relocation (TR_ClassAddress): inlinedIndex = %d, constantPool = %p, CPI = %d, flags = %x",
                                     cpiRecord->inlinedSiteIndex(reloTarget),
                                     cpiRecord->constantPool(reloTarget),
                                     cpiRecord->cpIndex(reloTarget),
                                     (uint32_t)cpiRecord->reloFlags(reloTarget));
            }
         }
         break;

      case TR_CheckMethodEnter:
      case TR_CheckMethodExit:
         {
         TR_RelocationRecordMethodTracingCheck *mtRecord = reinterpret_cast<TR_RelocationRecordMethodTracingCheck *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nDestination address %x", mtRecord->destinationAddress(reloTarget));
            }
         }
         break;

      case TR_VerifyClassObjectForAlloc:
         {
         TR_RelocationRecordVerifyClassObjectForAlloc *allocRecord = reinterpret_cast<TR_RelocationRecordVerifyClassObjectForAlloc *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nVerify Class Object for Allocation: InlineCallSite index = %d, Constant pool = %x, index = %d, binaryEncode = %x, size = %d",
                                     allocRecord->inlinedSiteIndex(reloTarget),
                                     allocRecord->constantPool(reloTarget),
                                     allocRecord->cpIndex(reloTarget),
                                     allocRecord->branchOffset(reloTarget),
                                     allocRecord->allocationSize(reloTarget));
            }
         }
         break;

      case TR_VerifyRefArrayForAlloc:
         {
         TR_RelocationRecordVerifyRefArrayForAlloc *allocRecord = reinterpret_cast<TR_RelocationRecordVerifyRefArrayForAlloc *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nVerify Class Object for Allocation: InlineCallSite index = %d, Constant pool = %x, index = %d, binaryEncode = %x",
                                     allocRecord->inlinedSiteIndex(reloTarget),
                                     allocRecord->constantPool(reloTarget),
                                     allocRecord->cpIndex(reloTarget),
                                     allocRecord->branchOffset(reloTarget));
            }
         }
         break;

      case TR_ValidateInstanceField:
         {
         TR_RelocationRecordValidateInstanceField *fieldRecord = reinterpret_cast<TR_RelocationRecordValidateInstanceField *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nValidation Relocation: InlineCallSite index = %d, Constant pool = %x, cpIndex = %d, Class Chain offset = %x",
                                     fieldRecord->inlinedSiteIndex(reloTarget),
                                     fieldRecord->constantPool(reloTarget),
                                     fieldRecord->cpIndex(reloTarget),
                                     fieldRecord->classChainOffsetInSharedCache(reloTarget));
            }
         }
         break;

      case TR_InlinedStaticMethodWithNopGuard:
      case TR_InlinedSpecialMethodWithNopGuard:
      case TR_InlinedVirtualMethodWithNopGuard:
      case TR_InlinedInterfaceMethodWithNopGuard:
      case TR_InlinedAbstractMethodWithNopGuard:
         {
         TR_RelocationRecordNopGuard *inlinedMethod = reinterpret_cast<TR_RelocationRecordNopGuard *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nInlined Method: Inlined site index = %d, Constant pool = %x, cpIndex = %x, romClassOffsetInSharedCache=%p, destinationAddress = %p",
                                     inlinedMethod->inlinedSiteIndex(reloTarget),
                                     inlinedMethod->constantPool(reloTarget),
                                     inlinedMethod->cpIndex(reloTarget),
                                     inlinedMethod->romClassOffsetInSharedCache(reloTarget),
                                     inlinedMethod->destinationAddress(reloTarget));
            }
         }
         break;

      case TR_ValidateStaticField:
         {
         TR_RelocationRecordValidateStaticField *vsfRecord = reinterpret_cast<TR_RelocationRecordValidateStaticField *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nValidation Relocation: InlineCallSite index = %d, Constant pool = %x, cpIndex = %d, ROM Class offset = %x",
                                       vsfRecord->inlinedSiteIndex(reloTarget),
                                       vsfRecord->constantPool(reloTarget),
                                       vsfRecord->cpIndex(reloTarget),
                                       vsfRecord->romClassOffsetInSharedCache(reloTarget));
            }
         }
         break;

      case TR_ValidateClass:
         {
         TR_RelocationRecordValidateClass *vcRecord = reinterpret_cast<TR_RelocationRecordValidateClass *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nValidation Relocation: InlineCallSite index = %d, Constant pool = %x, cpIndex = %d, Class Chain offset = %x",
                                       vcRecord->inlinedSiteIndex(reloTarget),
                                       vcRecord->constantPool(reloTarget),
                                       vcRecord->cpIndex(reloTarget),
                                       vcRecord->classChainOffsetInSharedCache(reloTarget));
            }
         }
         break;

      case TR_ProfiledMethodGuardRelocation:
      case TR_ProfiledClassGuardRelocation:
      case TR_ProfiledInlinedMethodRelocation:
         {
         TR_RelocationRecordProfiledInlinedMethod *pRecord = reinterpret_cast<TR_RelocationRecordProfiledInlinedMethod *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nProfiled Class Guard: Inlined site index = %d, Constant pool = %x, cpIndex = %x, romClassOffsetInSharedCache=%p, classChainIdentifyingLoaderOffsetInSharedCache=%p, classChainForInlinedMethod %p, methodIndex %d",
                                       pRecord->inlinedSiteIndex(reloTarget),
                                       pRecord->constantPool(reloTarget),
                                       pRecord->romClassOffsetInSharedCache(reloTarget),
                                       pRecord->classChainIdentifyingLoaderOffsetInSharedCache(reloTarget),
                                       pRecord->classChainForInlinedMethod(reloTarget),
                                       pRecord->methodIndex(reloTarget));
            }
         }
         break;

      case TR_MethodPointer:
         {
         TR_RelocationRecordMethodPointer *mpRecord = reinterpret_cast<TR_RelocationRecordMethodPointer *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nMethod Pointer: Inlined site index = %d, classChainIdentifyingLoaderOffsetInSharedCache=%p, classChainForInlinedMethod %p, vTableOffset %x",
                                      mpRecord->inlinedSiteIndex(reloTarget),
                                      mpRecord->classChainIdentifyingLoaderOffsetInSharedCache(reloTarget),
                                      mpRecord->classChainForInlinedMethod(reloTarget),
                                      mpRecord->vTableSlot(reloTarget));
            }
         }
         break;

      case TR_InlinedMethodPointer:
         {
         TR_RelocationRecordInlinedMethodPointer *impRecord = reinterpret_cast<TR_RelocationRecordInlinedMethodPointer *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nInlined Method Pointer: Inlined site index = %d", impRecord->inlinedSiteIndex(reloTarget));
            }
         }
         break;

      case TR_ClassPointer:
      case TR_ArbitraryClassAddress:
         {
         TR_RelocationRecordClassPointer *cpRecord = reinterpret_cast<TR_RelocationRecordClassPointer *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nClass Pointer: Inlined site index = %d, classChainIdentifyingLoaderOffsetInSharedCache=%p, classChainForInlinedMethod %p",
                                      cpRecord->inlinedSiteIndex(reloTarget),
                                      cpRecord->classChainIdentifyingLoaderOffsetInSharedCache(reloTarget),
                                      cpRecord->classChainForInlinedMethod(reloTarget));
            }
         }
         break;

      case TR_ValidateArbitraryClass:
         {
         TR_RelocationRecordValidateArbitraryClass *vacRecord = reinterpret_cast<TR_RelocationRecordValidateArbitraryClass *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nValidateArbitraryClass Relocation: classChainOffsetForClassToValidate = %p, classChainIdentifyingClassLoader = %p",
                                      vacRecord->classChainOffsetForClassBeingValidated(reloTarget),
                                      vacRecord->classChainIdentifyingLoaderOffset(reloTarget));
            }
         }
         break;

      case TR_InlinedInterfaceMethod:
      case TR_InlinedVirtualMethod:
         {
         TR_RelocationRecordInlinedMethod *imRecord = reinterpret_cast<TR_RelocationRecordInlinedMethod *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Removed Guard inlined method: Inlined site index = %d, Constant pool = %x, cpIndex = %x, romClassOffsetInSharedCache=%p",
                                      imRecord->inlinedSiteIndex(reloTarget),
                                      imRecord->constantPool(reloTarget),
                                      imRecord->cpIndex(reloTarget),
                                      imRecord->romClassOffsetInSharedCache(reloTarget));
            }
         }
         break;

      case TR_J2IVirtualThunkPointer:
         {
         TR_RelocationRecordJ2IVirtualThunkPointer *vtpRecord = reinterpret_cast<TR_RelocationRecordJ2IVirtualThunkPointer *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nInlined site index %lld, constant pool 0x%llx, offset to j2i thunk pointer 0x%llx",
                                     vtpRecord->inlinedSiteIndex(reloTarget),
                                     vtpRecord->constantPool(reloTarget),
                                     vtpRecord->getOffsetToJ2IVirtualThunkPointer(reloTarget));
            }
         } 
         break;

      case TR_ValidateClassByName:
         {
         TR_RelocationRecordValidateClassByName *cbnRecord = reinterpret_cast<TR_RelocationRecordValidateClassByName *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Class By Name: classID=%d beholderID=%d classChainOffsetInSCC=%p ",
                                      (uint32_t)cbnRecord->classID(reloTarget),
                                      (uint32_t)cbnRecord->beholderID(reloTarget),
                                      (void *)cbnRecord->classChainOffset(reloTarget));
            }
         }
         break;

      case TR_ValidateProfiledClass:
         {
         TR_RelocationRecordValidateProfiledClass *pcRecord = reinterpret_cast<TR_RelocationRecordValidateProfiledClass *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Profiled Class: classID=%d classChainOffsetInSCC=%p classChainOffsetForCLInScc=%p ",
                                      (uint32_t)pcRecord->classID(reloTarget),
                                      (void *)pcRecord->classChainOffset(reloTarget),
                                      (void *)pcRecord->classChainOffsetForClassLoader(reloTarget));
            }
         }
         break;

      case TR_ValidateClassFromCP:
      case TR_ValidateStaticClassFromCP:
      case TR_ValidateClassFromITableIndexCP:
      case TR_ValidateDeclaringClassFromFieldOrStatic:
         {
         TR_RelocationRecordValidateClassFromCP *cpRecord = reinterpret_cast<TR_RelocationRecordValidateClassFromCP *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            const char *recordType;
            if (kind == TR_ValidateClassFromCP)
               recordType = "Class From CP";
            else if (kind == TR_ValidateStaticClassFromCP)
               recordType = "Static Class FromCP";
            else if (kind == TR_ValidateClassFromITableIndexCP)
               recordType = "Class From ITable Index CP";
            else if (kind == TR_ValidateDeclaringClassFromFieldOrStatic)
               recordType = "Declaring Class From Field or Static";
            else
               TR_ASSERT_FATAL(false, "Unknown relokind %d!\n", kind);

            traceMsg(self()->comp(), "\n Validate %s: classID=%d, beholderID=%d, cpIndex=%d ",
                                     recordType,
                                     (uint32_t)cpRecord->classID(reloTarget),
                                     (uint32_t)cpRecord->beholderID(reloTarget),
                                     cpRecord->cpIndex(reloTarget));
            }
         }
         break;

      case TR_ValidateDefiningClassFromCP:
         {
         TR_RelocationRecordValidateDefiningClassFromCP *dcpRecord = reinterpret_cast<TR_RelocationRecordValidateDefiningClassFromCP *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Defining Class From CP: classID=%d, beholderID=%d, cpIndex=%d, isStatic=%s ",
                                      (uint32_t)dcpRecord->classID(reloTarget),
                                      (uint32_t)dcpRecord->beholderID(reloTarget),
                                      dcpRecord->cpIndex(reloTarget),
                                      dcpRecord->isStatic(reloTarget) ? "true" : "false");
            }
         }
         break;

      case TR_ValidateArrayClassFromComponentClass:
         {
         TR_RelocationRecordValidateArrayClassFromComponentClass *acRecord = reinterpret_cast<TR_RelocationRecordValidateArrayClassFromComponentClass *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Array Class From Component: arrayClassID=%d, componentClassID=%d ",
                                      (uint32_t)acRecord->arrayClassID(reloTarget),
                                      (uint32_t)acRecord->componentClassID(reloTarget));
            }
         }
         break;

      case TR_ValidateSuperClassFromClass:
      case TR_ValidateConcreteSubClassFromClass:
         {
         TR_RelocationRecordValidateSuperClassFromClass *scRecord = reinterpret_cast<TR_RelocationRecordValidateSuperClassFromClass *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            const char *recordType;
            if (kind == TR_ValidateSuperClassFromClass)
               recordType = "Super Class From Class";
            else if (kind == TR_ValidateConcreteSubClassFromClass)
               recordType = "Concrete SubClass From Class";
            else
               TR_ASSERT_FATAL(false, "Unknown relokind %d!\n", kind);

            traceMsg(self()->comp(), "\n Validate %s: superClassID=%d, childClassID=%d ",
                                      recordType,
                                      (uint32_t)scRecord->superClassID(reloTarget),
                                      (uint32_t)scRecord->childClassID(reloTarget));
            }
         }
         break;

      case TR_ValidateClassInstanceOfClass:
         {
         TR_RelocationRecordValidateClassInstanceOfClass *cicRecord = reinterpret_cast<TR_RelocationRecordValidateClassInstanceOfClass *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if(isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Class InstanceOf Class: classOneID=%d, classTwoID=%d, objectTypeIsFixed=%s, castTypeIsFixed=%s, isInstanceOf=%s ",
                                     (uint32_t)cicRecord->classOneID(reloTarget),
                                     (uint32_t)cicRecord->classTwoID(reloTarget),
                                     cicRecord->objectTypeIsFixed(reloTarget) ? "true" : "false",
                                     cicRecord->castTypeIsFixed(reloTarget) ? "true" : "false",
                                     cicRecord->isInstanceOf(reloTarget) ? "true" : "false");
            }
         }
         break;

      case TR_ValidateSystemClassByName:
         {
         TR_RelocationRecordValidateSystemClassByName *scmRecord = reinterpret_cast<TR_RelocationRecordValidateSystemClassByName *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate System Class By Name: systemClassID=%d classChainOffsetInSCC=%p ",
                     (uint32_t)scmRecord->systemClassID(reloTarget),
                     (void *)scmRecord->classChainOffset(reloTarget));
            }
         }
         break;

      case TR_ValidateClassChain:
         {
         TR_RelocationRecordValidateClassChain *ccRecord = reinterpret_cast<TR_RelocationRecordValidateClassChain *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Class Chain: classID=%d classChainOffsetInSCC=%p ",
                     (uint32_t)ccRecord->classID(reloTarget),
                     (void *)ccRecord->classChainOffset(reloTarget));
            }
         }
         break;

      case TR_ValidateMethodFromClass:
         {
         TR_RelocationRecordValidateMethodFromClass *mfcRecord = reinterpret_cast<TR_RelocationRecordValidateMethodFromClass *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Method By Name: methodID=%d, beholderID=%d, index=%d ",
                     (uint32_t)mfcRecord->methodID(reloTarget),
                     (uint32_t)mfcRecord->beholderID(reloTarget),
                     mfcRecord->index(reloTarget));
            }
         }
         break;

      case TR_ValidateStaticMethodFromCP:
      case TR_ValidateSpecialMethodFromCP:
      case TR_ValidateVirtualMethodFromCP:
      case TR_ValidateImproperInterfaceMethodFromCP:
         {
         TR_RelocationRecordValidateMethodFromCP *mfcpRecord = reinterpret_cast<TR_RelocationRecordValidateMethodFromCP *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            const char *recordType;
            if (kind == TR_ValidateStaticMethodFromCP)
               recordType = "Static";
            else if (kind == TR_ValidateSpecialMethodFromCP)
               recordType = "Special";
            else if (kind == TR_ValidateVirtualMethodFromCP)
               recordType = "Virtual";
            else if (kind == TR_ValidateImproperInterfaceMethodFromCP)
               recordType = "Improper Interface";
            else
               TR_ASSERT_FATAL(false, "Unknown relokind %d!\n", kind);

            traceMsg(self()->comp(), "\n Validate %s Method From CP: methodID=%d, definingClassID=%d, beholderID=%d, cpIndex=%d ",
                     recordType,
                     (uint32_t)mfcpRecord->methodID(reloTarget),
                     (uint32_t)mfcpRecord->definingClassID(reloTarget),
                     (uint32_t)mfcpRecord->beholderID(reloTarget),
                     (uint32_t)mfcpRecord->cpIndex(reloTarget));
            }
         }
         break;

      case TR_ValidateVirtualMethodFromOffset:
         {
         TR_RelocationRecordValidateVirtualMethodFromOffset *vmfoRecord = reinterpret_cast<TR_RelocationRecordValidateVirtualMethodFromOffset *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Virtual Method From Offset: methodID=%d, definingClassID=%d, beholderID=%d, virtualCallOffset=%d, ignoreRtResolve=%s ",
                     (uint32_t)vmfoRecord->methodID(reloTarget),
                     (uint32_t)vmfoRecord->definingClassID(reloTarget),
                     (uint32_t)vmfoRecord->beholderID(reloTarget),
                     (uint32_t)(vmfoRecord->virtualCallOffsetAndIgnoreRtResolve(reloTarget) & ~1),
                     (vmfoRecord->virtualCallOffsetAndIgnoreRtResolve(reloTarget) & 1) ? "true" : "false");
            }
         }
         break;

      case TR_ValidateInterfaceMethodFromCP:
         {
         TR_RelocationRecordValidateInterfaceMethodFromCP *imfcpRecord = reinterpret_cast<TR_RelocationRecordValidateInterfaceMethodFromCP *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Interface Method From CP: methodID=%d, definingClassID=%d, beholderID=%d, lookupID=%d, cpIndex=%d ",
                     (uint32_t)imfcpRecord->methodID(reloTarget),
                     (uint32_t)imfcpRecord->definingClassID(reloTarget),
                     (uint32_t)imfcpRecord->beholderID(reloTarget),
                     (uint32_t)imfcpRecord->lookupID(reloTarget),
                     (uint32_t)imfcpRecord->cpIndex(reloTarget));
            }
         }
         break;

      case TR_ValidateMethodFromClassAndSig:
         {
         TR_RelocationRecordValidateMethodFromClassAndSig *mfcsRecord = reinterpret_cast<TR_RelocationRecordValidateMethodFromClassAndSig *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Method From Class and Sig: methodID=%d, definingClassID=%d, lookupClassID=%d, beholderID=%d, romMethodOffsetInSCC=%p ",
                     (uint32_t)mfcsRecord->methodID(reloTarget),
                     (uint32_t)mfcsRecord->definingClassID(reloTarget),
                     (uint32_t)mfcsRecord->lookupClassID(reloTarget),
                     (uint32_t)mfcsRecord->beholderID(reloTarget),
                     (void *)mfcsRecord->romMethodOffsetInSCC(reloTarget));
            }
         }
         break;

      case TR_ValidateStackWalkerMaySkipFramesRecord:
         {
         TR_RelocationRecordValidateStackWalkerMaySkipFrames *swmsfRecord = reinterpret_cast<TR_RelocationRecordValidateStackWalkerMaySkipFrames *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Stack Walker May Skip Frames: methodID=%d, methodClassID=%d, beholderID=%d, skipFrames=%s ",
                     (uint32_t)swmsfRecord->methodID(reloTarget),
                     (uint32_t)swmsfRecord->methodClassID(reloTarget),
                     swmsfRecord->skipFrames(reloTarget) ? "true" : "false");
            }
         }
         break;

      case TR_ValidateClassInfoIsInitialized:
         {
         TR_RelocationRecordValidateClassInfoIsInitialized *ciiiRecord = reinterpret_cast<TR_RelocationRecordValidateClassInfoIsInitialized *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Class Info Is Initialized: classID=%d, isInitialized=%s ",
                     (uint32_t)ciiiRecord->classID(reloTarget),
                     ciiiRecord->isInitialized(reloTarget) ? "true" : "false");
            }
         }
         break;

      case TR_ValidateMethodFromSingleImplementer:
         {
         TR_RelocationRecordValidateMethodFromSingleImpl *mfsiRecord = reinterpret_cast<TR_RelocationRecordValidateMethodFromSingleImpl *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Method From Single Implementor: methodID=%d, definingClassID=%d, thisClassID=%d, cpIndexOrVftSlot=%d, callerMethodID=%d, useGetResolvedInterfaceMethod=%d ",
                     (uint32_t)mfsiRecord->methodID(reloTarget),
                     (uint32_t)mfsiRecord->definingClassID(reloTarget),
                     (uint32_t)mfsiRecord->thisClassID(reloTarget),
                     (uint32_t)mfsiRecord->cpIndexOrVftSlot(reloTarget),
                     (uint32_t)mfsiRecord->callerMethodID(reloTarget),
                     (uint32_t)mfsiRecord->useGetResolvedInterfaceMethod(reloTarget));
            }
         }
         break;

      case TR_ValidateMethodFromSingleInterfaceImplementer:
         {
         TR_RelocationRecordValidateMethodFromSingleInterfaceImpl *mfsiiRecord = reinterpret_cast<TR_RelocationRecordValidateMethodFromSingleInterfaceImpl *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Method From Single Interface Implementor: methodID=%u, definingClassID=%u, thisClassID=%u, cpIndex=%u, callerMethodID=%u ",
                     (uint32_t)mfsiiRecord->methodID(reloTarget),
                     (uint32_t)mfsiiRecord->definingClassID(reloTarget),
                     (uint32_t)mfsiiRecord->thisClassID(reloTarget),
                     mfsiiRecord->cpIndex(reloTarget),
                     (uint32_t)mfsiiRecord->callerMethodID(reloTarget));
            }
         }
         break;

      case TR_ValidateMethodFromSingleAbstractImplementer:
         {
         TR_RelocationRecordValidateMethodFromSingleAbstractImpl *mfsaiRecord = reinterpret_cast<TR_RelocationRecordValidateMethodFromSingleAbstractImpl *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Validate Method From Single Abstract Implementor: methodID=%d, definingClassID=%d, thisClassID=%d, vftSlot=%d, callerMethodID=%d ",
                     (uint32_t)mfsaiRecord->methodID(reloTarget),
                     (uint32_t)mfsaiRecord->definingClassID(reloTarget),
                     (uint32_t)mfsaiRecord->thisClassID(reloTarget),
                     mfsaiRecord->vftSlot(reloTarget),
                     (uint32_t)mfsaiRecord->callerMethodID(reloTarget));
            }
         }
         break;

      case TR_SymbolFromManager:
      case TR_DiscontiguousSymbolFromManager:
         {
         TR_RelocationRecordSymbolFromManager *sfmRecord = reinterpret_cast<TR_RelocationRecordSymbolFromManager *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n %sSymbol From Manager: symbolID=%d symbolType=%d flags=%x",
                     kind == TR_DiscontiguousSymbolFromManager ? "Discontiguous " : "",
                     (uint32_t)sfmRecord->symbolID(reloTarget),
                     (uint32_t)sfmRecord->symbolType(reloTarget),
                     (uint32_t)sfmRecord->reloFlags(reloTarget));
            }
         }
         break;

      case TR_ResolvedTrampolines:
         {
         TR_RelocationRecordResolvedTrampolines *rtRecord = reinterpret_cast<TR_RelocationRecordResolvedTrampolines *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Resolved Trampoline: symbolID=%u ",
                     (uint32_t)rtRecord->symbolID(reloTarget));
            }
         }
         break;

      case TR_DataAddress:
         {
         TR_RelocationRecordDataAddress *daRecord = reinterpret_cast<TR_RelocationRecordDataAddress *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nTR_DataAddress: InlinedCallSite index = %d, Constant pool = %x, cpIndex = %d, offset = %x, flags = %x",
                                     daRecord->inlinedSiteIndex(reloTarget),
                                     daRecord->constantPool(reloTarget),
                                     daRecord->cpIndex(reloTarget),
                                     daRecord->offset(reloTarget),
                                     (uint32_t)daRecord->reloFlags(reloTarget));
            }
         }
         break;

      case TR_FixedSequenceAddress:
      case TR_FixedSequenceAddress2:
      case TR_RamMethodSequence:
      case TR_GlobalValue:
      case TR_HCR:
         {
         TR_RelocationRecordWithOffset *rwoRecord = reinterpret_cast<TR_RelocationRecordWithOffset *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            const char *recordType;
            if (kind == TR_FixedSequenceAddress)
               recordType = "Fixed Sequence Relo";
            else if (kind == TR_FixedSequenceAddress2)
               recordType = "Load Address Relo";
            else if (kind == TR_RamMethodSequence)
               recordType = "Ram Method Sequence Relo";
            else if (kind == TR_GlobalValue)
               recordType = "Global Value";
            else if (kind == TR_HCR)
               recordType = "HCR";
            else
               TR_ASSERT_FATAL(false, "Unknown relokind %d!\n", kind);

            traceMsg(self()->comp(),"%s: patch location offset = %p", recordType, (void *)rwoRecord->offset(reloTarget));
            }
         }
         break;

      case TR_EmitClass:
         {
         TR_RelocationRecordEmitClass *ecRecord = reinterpret_cast<TR_RelocationRecordEmitClass *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nTR_EmitClass: InlinedCallSite index = %d, bcIndex = %d",
                                     ecRecord->inlinedSiteIndex(reloTarget),
                                     ecRecord->bcIndex(reloTarget));
            }
         }
         break;

      case TR_PicTrampolines:
         {
         TR_RelocationRecordPicTrampolines *ptRecord = reinterpret_cast<TR_RelocationRecordPicTrampolines *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nTR_PicTrampolines: num trampolines = %d", ptRecord->numTrampolines(reloTarget));
            }
         }
         break;

      case TR_DebugCounter:
         {
         TR_RelocationRecordDebugCounter *dcRecord = reinterpret_cast<TR_RelocationRecordDebugCounter *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Debug Counter: Inlined site index = %d, bcIndex = %d, delta = %d, fidelity = %d, staticDelta = %d, offsetOfNameString = %p",
                                     dcRecord->inlinedSiteIndex(reloTarget),
                                     dcRecord->bcIndex(reloTarget),
                                     dcRecord->delta(reloTarget),
                                     dcRecord->fidelity(reloTarget),
                                     dcRecord->staticDelta(reloTarget),
                                     (void *)dcRecord->offsetOfNameString(reloTarget));
            }
         }
         break;

      case TR_BlockFrequency:
         {
         TR_RelocationRecordBlockFrequency *bfRecord = reinterpret_cast<TR_RelocationRecordBlockFrequency *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Frequency offset %lld", bfRecord->frequencyOffset(reloTarget));
            }
         }
         break;

      case TR_Breakpoint:
         {
         TR_RelocationRecordBreakpointGuard *bpgRecord = reinterpret_cast<TR_RelocationRecordBreakpointGuard *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\n Breakpoint Guard: Inlined site index = %d, destinationAddress = %p",
                     bpgRecord->inlinedSiteIndex(reloTarget),
                     bpgRecord->destinationAddress(reloTarget));
            }
         }
         break;

      default:
         return cursor;
      }

   traceMsg(self()->comp(), "\n");

   return endOfCurrentRecord;
   }

void
J9::AheadOfTimeCompile::dumpRelocationData()
   {
   // Don't trace unless traceRelocatableDataCG or traceRelocatableDataDetailsCG
   if (!self()->comp()->getOption(TR_TraceRelocatableDataCG) && !self()->comp()->getOption(TR_TraceRelocatableDataDetailsCG))
      {
      return;
      }

   bool isVerbose = self()->comp()->getOption(TR_TraceRelocatableDataDetailsCG);

   uint8_t *cursor = self()->getRelocationData();

   if (!cursor)
      {
      traceMsg(self()->comp(), "No relocation data allocated\n");
      return;
      }

   // Output the method
   traceMsg(self()->comp(), "%s\n", self()->comp()->signature());

   if (self()->comp()->getOption(TR_TraceRelocatableDataCG))
      {
      traceMsg(self()->comp(), "\n\nRelocation Record Generation Info\n");
      traceMsg(self()->comp(), "%-35s %-32s %-5s %-9s %-10s %-8s\n", "Type", "File", "Line","Offset(M)","Offset(PC)", "Node");

      TR::list<TR::Relocation*>& aotRelocations = self()->comp()->cg()->getExternalRelocationList();
      //iterate over aotRelocations
      if (!aotRelocations.empty())
         {
         for (auto relocation = aotRelocations.begin(); relocation != aotRelocations.end(); ++relocation)
            {
            if (*relocation)
               {
               (*relocation)->trace(self()->comp());
               }
            }
         }
      if (!self()->comp()->getOption(TR_TraceRelocatableDataCG) && !self()->comp()->getOption(TR_TraceRelocatableDataDetailsCG))
         {
         return;//otherwise continue with the other options
         }
      }

   if (isVerbose)
      {
      traceMsg(self()->comp(), "Size of relocation data in AOT object is %d bytes\n", self()->getSizeOfAOTRelocations());
      }

   uint8_t *endOfData;
   if (self()->comp()->target().is64Bit())
      {
      endOfData = cursor + *(uint64_t *)cursor;
      traceMsg(self()->comp(), "Size field in relocation data is %d bytes\n\n", *(uint64_t *)cursor);
      cursor += 8;
      }
   else
      {
      endOfData = cursor + *(uint32_t *)cursor;
      traceMsg(self()->comp(), "Size field in relocation data is %d bytes\n\n", *(uint32_t *)cursor);
      cursor += 4;
      }

   if (self()->comp()->getOption(TR_UseSymbolValidationManager))
      {
      traceMsg(
         self()->comp(),
         "SCC offset of class chain offsets of well-known classes is: 0x%llx\n\n",
         (unsigned long long)*(uintptr_t *)cursor);
      cursor += sizeof (uintptr_t);
      }

   traceMsg(self()->comp(), "Address           Size %-31s", "Type");
   traceMsg(self()->comp(), "Width EIP Index Offsets\n"); // Offsets from Code Start

   while (cursor < endOfData)
      {
      cursor = self()->dumpRelocationHeaderData(cursor, isVerbose);
      }
   }

void J9::AheadOfTimeCompile::interceptAOTRelocation(TR::ExternalRelocation *relocation)
   {
   OMR::AheadOfTimeCompile::interceptAOTRelocation(relocation);

   TR_ExternalRelocationTargetKind kind = relocation->getTargetKind();

   if (kind == TR_ClassAddress)
      {
      TR::SymbolReference *symRef = NULL;
      void *p = relocation->getTargetAddress();
      if (TR::AheadOfTimeCompile::classAddressUsesReloRecordInfo())
         symRef = (TR::SymbolReference*)((TR_RelocationRecordInformation*)p)->data1;
      else
         symRef = (TR::SymbolReference*)p;

      if (symRef->getCPIndex() == -1)
         relocation->setTargetKind(TR_ArbitraryClassAddress);
      }
   else if (kind == TR_MethodPointer)
      {
      TR::Node *aconstNode = reinterpret_cast<TR::Node *>(relocation->getTargetAddress());

      TR_OpaqueMethodBlock *j9method = reinterpret_cast<TR_OpaqueMethodBlock *>(aconstNode->getAddress());
      if (aconstNode->getOpCodeValue() == TR::loadaddr)
         {
         j9method =
               reinterpret_cast<TR_OpaqueMethodBlock *>(
                  aconstNode->getSymbolReference()->getSymbol()->castToStaticSymbol()->getStaticAddress());
         }

      uintptr_t inlinedSiteIndex = static_cast<uintptr_t>(aconstNode->getInlinedSiteIndex());
      TR_InlinedCallSite & ics = TR::comp()->getInlinedCallSite(inlinedSiteIndex);
      TR_ResolvedMethod *inlinedMethod = ((TR_AOTMethodInfo *)ics._methodInfo)->resolvedMethod;
      TR_OpaqueMethodBlock *inlinedJ9Method = inlinedMethod->getPersistentIdentifier();

      /* If the j9method from the aconst node is the same as the j9method at
       * inlined call site at inlinedSiteIndex, switch the relo kind to
       * TR_InlinedMethodPointer; at relo time, the inlined site index is
       * sufficient to materialize the j9method pointer
       */
      if (inlinedJ9Method == j9method)
         {
         relocation->setTargetKind(TR_InlinedMethodPointer);
         relocation->setTargetAddress(reinterpret_cast<uint8_t *>(inlinedSiteIndex));
         }
      }
   }

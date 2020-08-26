/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

uint8_t *
J9::AheadOfTimeCompile::emitClassChainOffset(uint8_t* cursor, TR_OpaqueClassBlock* classToRemember)
   {
   uintptr_t classChainForInlinedMethodOffsetInSharedCache = self()->getClassChainOffset(classToRemember);
   *pointer_cast<uintptr_t *>(cursor) = classChainForInlinedMethodOffsetInSharedCache;
   return cursor + SIZEPOINTER;
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
J9::AheadOfTimeCompile::initializeCommonAOTRelocationHeader(TR::IteratedExternalRelocation *relocation, TR_RelocationRecord *reloRecord)
   {
   uint8_t *cursor = relocation->getRelocationData();

   TR::Compilation *comp = TR::comp();
   TR_RelocationRuntime *reloRuntime = comp->reloRuntime();
   TR_RelocationTarget *reloTarget = reloRuntime->reloTarget();
   TR::SymbolValidationManager *symValManager = comp->getSymbolValidationManager();
   TR_J9VMBase *fej9 = comp->fej9();
   TR_SharedCache *sharedCache = fej9->sharedCache();

   TR_ExternalRelocationTargetKind kind = relocation->getTargetKind();

   // initializeCommonAOTRelocationHeader is currently in the process
   // of becoming the canonical place to initialize the platform agnostic
   // relocation headers; new relocation records' header should be
   // initialized here.
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
         {
         TR_RelocationRecordInlinedMethod *imRecord = reinterpret_cast<TR_RelocationRecordInlinedMethod *>(reloRecord);
         uintptr_t destinationAddress = reinterpret_cast<uintptr_t>(relocation->getTargetAddress());
         TR_VirtualGuard *guard = reinterpret_cast<TR_VirtualGuard *>(relocation->getTargetAddress2());

         uint8_t flags = 0;
         // Setup flags field with type of method that needs to be validated at relocation time
         if (guard->getSymbolReference()->getSymbol()->getMethodSymbol()->isStatic())
            flags = inlinedMethodIsStatic;
         else if (guard->getSymbolReference()->getSymbol()->getMethodSymbol()->isSpecial())
            flags = inlinedMethodIsSpecial;
         else if (guard->getSymbolReference()->getSymbol()->getMethodSymbol()->isVirtual())
            flags = inlinedMethodIsVirtual;
         TR_ASSERT((flags & RELOCATION_CROSS_PLATFORM_FLAGS_MASK) == 0,  "reloFlags bits overlap cross-platform flags bits\n");

         int32_t inlinedSiteIndex = guard->getCurrentInlinedSiteIndex();

         TR_ResolvedMethod *resolvedMethod;
         if (kind == TR_InlinedInterfaceMethodWithNopGuard ||
             kind == TR_InlinedInterfaceMethod ||
             kind == TR_InlinedAbstractMethodWithNopGuard)
            {
            TR_InlinedCallSite *inlinedCallSite = &comp->getInlinedCallSite(inlinedSiteIndex);
            TR_AOTMethodInfo *aotMethodInfo = (TR_AOTMethodInfo *)inlinedCallSite->_methodInfo;
            resolvedMethod = aotMethodInfo->resolvedMethod;
            }
         else
            {
            resolvedMethod = guard->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod();
            }

         // Ugly; this will be cleaned up in a future PR
         uintptr_t cpIndexOrData = 0;
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            TR_OpaqueMethodBlock *method = resolvedMethod->getPersistentIdentifier();
            TR_OpaqueClassBlock *thisClass = guard->getThisClass();
            uint16_t methodID = symValManager->getIDFromSymbol(static_cast<void *>(method));
            uint16_t receiverClassID = symValManager->getIDFromSymbol(static_cast<void *>(thisClass));

            cpIndexOrData = (((uintptr_t)receiverClassID << 16) | (uintptr_t)methodID);
            }
         else
            {
            cpIndexOrData = static_cast<uintptr_t>(guard->getSymbolReference()->getCPIndex());
            }

         TR_OpaqueClassBlock *inlinedMethodClass = resolvedMethod->containingClass();
         J9ROMClass *romClass = reinterpret_cast<J9ROMClass *>(fej9->getPersistentClassPointerFromClassPointer(inlinedMethodClass));
         uintptr_t romClassOffsetInSharedCache = self()->offsetInSharedCacheFromROMClass(sharedCache, romClass);

         imRecord->setReloFlags(reloTarget, flags);
         imRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         imRecord->setConstantPool(reloTarget, reinterpret_cast<uintptr_t>(guard->getSymbolReference()->getOwningMethod(comp)->constantPool()));
         imRecord->setCpIndex(reloTarget, cpIndexOrData);
         imRecord->setRomClassOffsetInSharedCache(reloTarget, romClassOffsetInSharedCache);

         if (kind != TR_InlinedInterfaceMethod && kind != TR_InlinedVirtualMethod)
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

         TR_VirtualGuard *guard = reinterpret_cast<TR_VirtualGuard *>(relocation->getTargetAddress2());

         int32_t inlinedSiteIndex = guard->getCurrentInlinedSiteIndex();

         TR::SymbolReference *callSymRef = guard->getSymbolReference();
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

      default:
         return cursor;
      }

   reloRecord->setSize(reloTarget, relocation->getSizeOfRelocationData());
   reloRecord->setType(reloTarget, kind);

   uint8_t wideOffsets = relocation->needsWideOffsets() ? RELOCATION_TYPE_WIDE_OFFSET : 0;
   reloRecord->setFlag(reloTarget, wideOffsets);

   cursor += TR_RelocationRecord::getSizeOfAOTRelocationHeader(kind);

   return cursor;
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

   uint8_t *startOfOffsets = cursor + TR_RelocationRecord::getSizeOfAOTRelocationHeader(kind);
   uint8_t *endOfCurrentRecord = cursor + reloRecord->size(reloTarget);

   bool orderedPair = isOrderedPair(kind);

   // dumpRelocationHeaderData is currently in the process of becoming the
   // the canonical place to dump the relocation header data; new relocation
   // records' header data should be printed here.
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

      case TR_ClassPointer:
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
      bool is64BitTarget = self()->comp()->target().is64Bit();
      if (is64BitTarget)
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
      void *ep1, *ep2, *ep3, *ep4, *ep5, *ep6, *ep7;
      TR::SymbolReference *tempSR = NULL;

      uint8_t *origCursor = cursor;

      traceMsg(self()->comp(), "%16x  ", cursor);

      // Relocation size
      traceMsg(self()->comp(), "%-5d", *(uint16_t*)cursor);

      uint8_t *endOfCurrentRecord = cursor + *(uint16_t *)cursor;
      cursor += 2;

      TR_ExternalRelocationTargetKind kind = (TR_ExternalRelocationTargetKind)(*cursor);
      // Relocation type
      traceMsg(self()->comp(), "%-31s", TR::ExternalRelocation::getName(kind));

      // Relocation offset width
      uint8_t *flagsCursor = cursor + 1;
      int32_t offsetSize = (*flagsCursor & RELOCATION_TYPE_WIDE_OFFSET) ? 4 : 2;
      traceMsg(self()->comp(), "%-6d", offsetSize);

      // Ordered-paired or non-paired
      bool orderedPair = isOrderedPair(*cursor); //(*cursor & RELOCATION_TYPE_ORDERED_PAIR) ? true : false;

      // Relative or Absolute offset type
      traceMsg(self()->comp(), "%s", (*flagsCursor & RELOCATION_TYPE_EIP_OFFSET) ? "Rel " : "Abs ");

      // Print out the correct number of spaces when no index is available
      if (kind != TR_HelperAddress && kind != TR_AbsoluteHelperAddress)
         traceMsg(self()->comp(), "      ");

      cursor++;

      // dumpRelocationHeaderData is currently in the process of becoming the
      // the canonical place to dump the relocation header data; new relocation
      // records' header data should be printed here.
      uint8_t *newCursor = self()->dumpRelocationHeaderData(origCursor, isVerbose);
      if (origCursor != newCursor)
         {
         cursor = newCursor;
         continue;
         }

      switch (kind)
         {
         case TR_ConstantPoolOrderedPair:
            // constant pool address is placed as the last word of the header
            //traceMsg(self()->comp(), "\nConstant pool %x\n", *(uint32_t *)++cursor);
            cursor++;        // unused field
            if (is64BitTarget)
               {
               cursor +=4;      // padding
               ep1 = cursor;
               ep2 = cursor+8;
               cursor += 16;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\nInlined site index = %d, Constant pool = %x", *(uint32_t *)ep1, *(uint64_t *)ep2);
                  }
               }
            else
               {
               ep1 = cursor;
               ep2 = cursor+4;
               cursor += 8;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  // ep1 is same as self()->comp()->getCurrentMethod()->constantPool())
                  traceMsg(self()->comp(), "\nInlined site index = %d, Constant pool = %x", *(uint32_t *)ep1, *(uint32_t *)(ep2));
                  }
               }
            break;
         case TR_AbsoluteMethodAddressOrderedPair:
            // Reference to the current method, no other information
            cursor++;
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            break;
         case TR_DataAddress:
            {
            cursor++;        //unused field
            if (is64BitTarget)
               cursor += 4;     // padding
            ep1 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            ep2 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            ep3 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            ep4 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            if (isVerbose)
               {
               if (is64BitTarget)
                  {
                  traceMsg(self()->comp(), "\nTR_DataAddress: InlinedCallSite index = %d, Constant pool = %x, cpIndex = %d, offset = %x",
                          *(uint64_t *)ep1, *(uint64_t *)ep2, *(uint64_t *)ep3, *(uint64_t *)ep4);
                  }
               else
                  {
                  traceMsg(self()->comp(), "\nTR_DataAddress: InlineCallSite index = %d, Constant pool = %x, cpIndex = %d, offset = %x",
                          *(uint32_t *)ep1, *(uint32_t *)ep2, *(uint32_t *)ep3, *(uint32_t *)ep4);
                  }
               }
            }
            break;
         case TR_ClassAddress:
            {
            cursor++;        //unused field
            if (is64BitTarget)
               cursor += 4;     // padding
            ep1 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            ep2 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            ep3 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            if (isVerbose)
               {
               if (is64BitTarget)
                  {
                  traceMsg(self()->comp(), "\nTR_ClassAddress: InlinedCallSite index = %d, Constant pool = %x, cpIndex = %d",
                          *(uint64_t *)ep1, *(uint64_t *)ep2, *(uint64_t *)ep3);
                  }
               else
                  {
                  traceMsg(self()->comp(), "\nTR_ClassAddress: InlineCallSite index = %d, Constant pool = %x, cpIndex = %d",
                          *(uint32_t *)ep1, *(uint32_t *)ep2, *(uint32_t *)ep3);
                  }
               }
            }
            break;
         case TR_MethodObject:
            // next word is the address of the constant pool to which the index refers
            // second word is the index in the above stored constant pool that indicates the particular
            // relocation target
            cursor++;        // unused field
            if (is64BitTarget)
               {
               cursor += 4;     // padding
               ep1 = cursor;
               ep2 = cursor+8;
               cursor += 16;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\nMethod Object: Constant pool = %x, index = %d", *(uint64_t *)ep1, *(uint64_t *)ep2);
                  }
               }
            else
               {
               ep1 = cursor;
               ep2 = cursor+4;
               cursor += 8;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\nMethod Object: Constant pool = %x, index = %d", *(uint32_t *)ep1, *(uint32_t *)ep2);
                  }
               }
            break;
         case TR_FixedSequenceAddress:
            cursor++;        // unused field
            if (is64BitTarget)
               {
               cursor += 4;     // padding
               ep1 = cursor;
               cursor += 8;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(),"Fixed Sequence Relo: patch location offset = %p", *(uint64_t *)ep1);
                  }
               }
            break;
         case TR_FixedSequenceAddress2:
            cursor++;        // unused field
            if (is64BitTarget)
               {
               cursor += 4;     // padding
               ep1 = cursor;
               cursor += 8;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(),"Load Address Relo: patch location offset = %p", *(uint64_t *)ep1);
                  }
               }
            break;
         case TR_BodyInfoAddressLoad:
         case TR_ArrayCopyHelper:
         case TR_ArrayCopyToc:
            cursor++;
            if (is64BitTarget)
               {
               cursor +=4;      // padding
               }
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            break;
         case TR_ResolvedTrampolines:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordResolvedTrampolinesBinaryTemplate *binaryTemplate =
               reinterpret_cast<TR_RelocationRecordResolvedTrampolinesBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Resolved Trampoline: symbolID=%d ",
                        (uint32_t)binaryTemplate->_symbolID);
               }
            cursor += sizeof(TR_RelocationRecordResolvedTrampolinesBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;
         case TR_PicTrampolines:
            cursor++;        // unused field
            if (is64BitTarget)
               {
               //cursor +=4;      // no padding for this one
               ep1 = cursor;
               cursor += 4; // 32 bit entry
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\nnum trampolines %x", *(uint64_t *)ep1);
                  }
               }
            else
               {
               ep1 = cursor;
               cursor += 4;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  // ep1 is same as self()->comp()->getCurrentMethod()->constantPool())
                  traceMsg(self()->comp(), "num trampolines\n %x", *(uint32_t *)ep1);
                  }
               }
            break;
         case TR_RamMethodSequence:
         case TR_RamMethodSequenceReg:
            cursor++;        // unused field
            if (is64BitTarget)
               {
               cursor +=4;      // padding
               ep1 = cursor;
               cursor += 8;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\nDestination address %x", *(uint64_t *)ep1);
                  }
               }
            else
               {
               cursor += 4;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               }
            break;

         case TR_ArbitraryClassAddress:
            cursor++;           // unused field
            if (is64BitTarget)
               cursor += 4;     // padding
            ep1 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            ep2 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            ep3 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);

            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);

            if (isVerbose)
               {
               if (is64BitTarget)
                  traceMsg(self()->comp(), "\nClass Pointer: Inlined site index = %d, classChainIdentifyingLoaderOffsetInSharedCache=%p, classChainForInlinedMethod %p",
                                  *(uint64_t *)ep1, *(uint64_t *)ep2, *(uint64_t *)ep3);
               else
                  traceMsg(self()->comp(), "\nClass Pointer: Inlined site index = %d, classChainIdentifyingLoaderOffsetInSharedCache=%p, classChainForInlinedMethod %p",
                                  *(uint32_t *)ep1, *(uint32_t *)ep2, *(uint32_t *)ep3);
               }
            break;

         case TR_GlobalValue:
            cursor++;        //unused field
            if (is64BitTarget)
               cursor += 4;     // padding
            ep1 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            if (isVerbose)
               {
               if (is64BitTarget)
                  traceMsg(self()->comp(), "\nGlobal value: %s", TR::ExternalRelocation::nameOfGlobal(*(uint64_t *)ep1));
               else
                  traceMsg(self()->comp(), "\nGlobal value: %s", TR::ExternalRelocation::nameOfGlobal(*(uint32_t *)ep1));
               }
            break;
         case TR_J2IThunks:
            {
            cursor++;        //unused field
            if (is64BitTarget)
               cursor += 4;     // padding

            ep1 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            ep2 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            ep3 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);

            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            if (isVerbose)
               {
               if (is64BitTarget)
                  {
                  traceMsg(self()->comp(), "\nTR_J2IThunks Relocation: InlineCallSite index = %d, ROM Class offset = %x, cpIndex = %d",
                                  *(uint64_t *)ep1, *(uint64_t *)ep2, *(uint64_t *)ep3);
                  }
               else
                  {
                  traceMsg(self()->comp(), "\nTR_J2IThunks Relocation: InlineCallSite index = %d, ROM Class offset = %x, cpIndex = %d",
                                  *(uint32_t *)ep1, *(uint32_t *)ep2, *(uint32_t *)ep3);
                  }
               }
            break;
            }
         case TR_HCR:
            cursor++;        // unused field
            if (is64BitTarget)
               {
               cursor +=4;      // padding
               ep1 = cursor;
               cursor += 8;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\nFirst address %x", *(uint64_t *)ep1);
                  }
               }
            else
               {
               ep1 = cursor;
               cursor += 4;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  // ep1 is same as self()->comp()->getCurrentMethod()->constantPool())
                  traceMsg(self()->comp(), "\nFirst address %x", *(uint32_t *)ep1);
                  }
               }
            break;
         case TR_DebugCounter:
            cursor ++;
            if (is64BitTarget)
               {
               cursor += 4;     // padding
               ep1 = cursor;    // inlinedSiteIndex
               ep2 = cursor+8;  // bcIndex
               ep3 = cursor+16; // offsetOfNameString
               ep4 = cursor+24; // delta
               ep5 = cursor+40; // staticDelta
               cursor += 48;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\n Debug Counter: Inlined site index = %d, bcIndex = %d, offsetOfNameString = %p, delta = %d, staticDelta = %d",
                                   *(int64_t *)ep1, *(int32_t *)ep2, *(UDATA *)ep3, *(int32_t *)ep4, *(int32_t *)ep5);
                  }
               }
            else
               {
               ep1 = cursor;    // inlinedSiteIndex
               ep2 = cursor+4;  // bcIndex
               ep3 = cursor+8;  // offsetOfNameString
               ep4 = cursor+12; // delta
               ep5 = cursor+20; // staticDelta
               cursor += 24;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\n Debug Counter: Inlined site index = %d, bcIndex = %d, offsetOfNameString = %p, delta = %d, staticDelta = %d",
                                   *(int32_t *)ep1, *(int32_t *)ep2, *(UDATA *)ep3, *(int32_t *)ep4, *(int32_t *)ep5);
                  }
               }
            break;

         case TR_ValidateImproperInterfaceMethodFromCP:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Improper Interface Method From CP: methodID=%d, definingClassID=%d, beholderID=%d, cpIndex=%d ",
                        (uint32_t)binaryTemplate->_methodID,
                        (uint32_t)binaryTemplate->_definingClassID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_cpIndex);
               }
            cursor += sizeof(TR_RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateStackWalkerMaySkipFramesRecord:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Stack Walker May Skip Frames: methodID=%d, methodClassID=%d, beholderID=%d, skipFrames=%s ",
                        (uint32_t)binaryTemplate->_methodID,
                        (uint32_t)binaryTemplate->_methodClassID,
                        binaryTemplate->_skipFrames ? "true" : "false");
               }
            cursor += sizeof(TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateClassInfoIsInitialized:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Class Info Is Initialized: classID=%d, isInitialized=%s ",
                        (uint32_t)binaryTemplate->_classID,
                        binaryTemplate->_isInitialized ? "true" : "false");
               }
            cursor += sizeof(TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateMethodFromSingleImplementer:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Method From Single Implementor: methodID=%d, definingClassID=%d, thisClassID=%d, cpIndexOrVftSlot=%d, callerMethodID=%d, useGetResolvedInterfaceMethod=%d ",
                        (uint32_t)binaryTemplate->_methodID,
                        (uint32_t)binaryTemplate->_definingClassID,
                        (uint32_t)binaryTemplate->_thisClassID,
                        binaryTemplate->_cpIndexOrVftSlot,
                        (uint32_t)binaryTemplate->_callerMethodID,
                        binaryTemplate->_useGetResolvedInterfaceMethod);
               }
            cursor += sizeof(TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateMethodFromSingleInterfaceImplementer:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Method From Single Interface Implementor: methodID=%d, definingClassID=%d, thisClassID=%d, cpIndex=%d, callerMethodID=%d ",
                        (uint32_t)binaryTemplate->_methodID,
                        (uint32_t)binaryTemplate->_definingClassID,
                        (uint32_t)binaryTemplate->_thisClassID,
                        binaryTemplate->_cpIndex,
                        (uint32_t)binaryTemplate->_callerMethodID);
               }
            cursor += sizeof(TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateMethodFromSingleAbstractImplementer:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Method From Single Interface Implementor: methodID=%d, definingClassID=%d, thisClassID=%d, vftSlot=%d, callerMethodID=%d ",
                        (uint32_t)binaryTemplate->_methodID,
                        (uint32_t)binaryTemplate->_definingClassID,
                        (uint32_t)binaryTemplate->_thisClassID,
                        binaryTemplate->_vftSlot,
                        (uint32_t)binaryTemplate->_callerMethodID);
               }
            cursor += sizeof(TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;


         case TR_SymbolFromManager:
         case TR_DiscontiguousSymbolFromManager:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordSymbolFromManagerBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordSymbolFromManagerBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Symbol From Manager: symbolID=%d symbolType=%d ",
                        (uint32_t)binaryTemplate->_symbolID, (uint32_t)binaryTemplate->_symbolType);

               if (kind == TR_DiscontiguousSymbolFromManager)
                  {
                  traceMsg(self()->comp(), "isDiscontiguous ");
                  }
               }
            cursor += sizeof(TR_RelocationRecordSymbolFromManagerBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         default:
            traceMsg(self()->comp(), "Unknown Relocation type = %d\n", kind);
            TR_ASSERT_FATAL(false, "Unknown Relocation type = %d", kind);
            break;
         }

      traceMsg(self()->comp(), "\n");
      }
   }

void J9::AheadOfTimeCompile::interceptAOTRelocation(TR::ExternalRelocation *relocation)
   {
   OMR::AheadOfTimeCompile::interceptAOTRelocation(relocation);

   if (relocation->getTargetKind() == TR_ClassAddress)
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
   }

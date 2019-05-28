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

#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "env/SharedCache.hpp"
#include "env/jittypes.h"
#include "exceptions/PersistenceFailure.hpp"
#include "il/DataTypes.hpp"
#include "env/VMJ9.h"
#include "codegen/AheadOfTimeCompile.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationRecord.hpp"

extern bool isOrderedPair(uint8_t reloType);

uint8_t *
J9::AheadOfTimeCompile::emitClassChainOffset(uint8_t* cursor, TR_OpaqueClassBlock* classToRemember)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(self()->comp()->fe());
   TR_SharedCache * sharedCache(fej9->sharedCache());
   void *classChainForInlinedMethod = sharedCache->rememberClass(classToRemember);
   if (!classChainForInlinedMethod)
      self()->comp()->failCompilation<J9::ClassChainPersistenceFailure>("classChainForInlinedMethod == NULL");
   uintptrj_t classChainForInlinedMethodOffsetInSharedCache = reinterpret_cast<uintptrj_t>( sharedCache->offsetInSharedCacheFromPointer(classChainForInlinedMethod) );
   *pointer_cast<uintptrj_t *>(cursor) = classChainForInlinedMethodOffsetInSharedCache;
   return cursor + SIZEPOINTER;
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
      if (comp->compileRelocatableCode())
         {
         TR_AOTMethodInfo *callSiteMethodInfo = (TR_AOTMethodInfo *)inlinedCallSite._methodInfo;
         constantPoolForSiteIndex = (uintptr_t)callSiteMethodInfo->resolvedMethod->constantPool();
         }
      else
         {
         TR_OpaqueMethodBlock *callSiteJ9Method = inlinedCallSite._methodInfo;
         constantPoolForSiteIndex = comp->fej9()->getConstantPoolFromMethod(callSiteJ9Method);
         }
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
         if (comp->compileRelocatableCode())
            {
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
         else
            {
            for (uintptr_t i = 0; i < comp->getNumInlinedCallSites(); i++)
               {
               TR_InlinedCallSite &inlinedCallSite = comp->getInlinedCallSite(i);
               TR_OpaqueMethodBlock *callSiteJ9Method = inlinedCallSite._methodInfo;

               if ((uintptr_t)constantPool == comp->fej9()->getConstantPoolFromMethod(callSiteJ9Method))
                  {
                  matchFound = true;
                  inlinedSiteIndex = i;
                  break;
                  }
               }
            }
         }
      }

   TR_ASSERT(matchFound, "Couldn't find this CP in inlined site list");
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

         cpRecord->setConstantPool(reloTarget, reinterpret_cast<uintptrj_t>(relocation->getTargetAddress()));
         cpRecord->setInlinedSiteIndex(reloTarget, reinterpret_cast<uintptrj_t>(relocation->getTargetAddress2()));
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
      case TR_StaticRamMethodConst:
         {
         TR_RelocationRecordConstantPoolWithIndex *cpiRecord = reinterpret_cast<TR_RelocationRecordConstantPoolWithIndex *>(reloRecord);
         TR::SymbolReference *symRef = reinterpret_cast<TR::SymbolReference *>(relocation->getTargetAddress());
         uintptrj_t inlinedSiteIndex = reinterpret_cast<uintptrj_t>(relocation->getTargetAddress2());

         void *constantPool = symRef->getOwningMethod(comp)->constantPool();
         inlinedSiteIndex = self()->findCorrectInlinedSiteIndex(constantPool, inlinedSiteIndex);

         cpiRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         cpiRecord->setConstantPool(reloTarget, reinterpret_cast<uintptrj_t>(constantPool));
         cpiRecord->setCpIndex(reloTarget, symRef->getCPIndex());
         }
         break;

      case TR_CheckMethodEnter:
         {
         TR_RelocationRecordMethodEnterCheck *mcRecord = reinterpret_cast<TR_RelocationRecordMethodEnterCheck *>(reloRecord);

         mcRecord->setDestinationAddress(reloTarget, reinterpret_cast<uintptrj_t>(relocation->getTargetAddress()));
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

         allocRecord->setInlinedSiteIndex(reloTarget, static_cast<uintptrj_t>(recordInfo->data2));
         allocRecord->setConstantPool(reloTarget, reinterpret_cast<uintptrj_t>(classSymRef->getOwningMethod(comp)->constantPool()));
         allocRecord->setBranchOffset(reloTarget, static_cast<uintptrj_t>(branchOffset));
         allocRecord->setAllocationSize(reloTarget, static_cast<uintptrj_t>(recordInfo->data1));

         /* Temporary, will be cleaned up in a future PR */
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            TR_OpaqueClassBlock *classOfMethod = reinterpret_cast<TR_OpaqueClassBlock *>(recordInfo->data5);
            uint16_t classID = symValManager->getIDFromSymbol(static_cast<void *>(classOfMethod));
            allocRecord->setCpIndex(reloTarget, static_cast<uintptrj_t>(classID));
            }
         else
            {
            allocRecord->setCpIndex(reloTarget, static_cast<uintptrj_t>(classSymRef->getCPIndex()));
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

         allocRecord->setInlinedSiteIndex(reloTarget, static_cast<uintptrj_t>(recordInfo->data2));
         allocRecord->setConstantPool(reloTarget, reinterpret_cast<uintptrj_t>(classSymRef->getOwningMethod(comp)->constantPool()));
         allocRecord->setBranchOffset(reloTarget, static_cast<uintptrj_t>(branchOffset));

         /* Temporary, will be cleaned up in a future PR */
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            TR_OpaqueClassBlock *classOfMethod = reinterpret_cast<TR_OpaqueClassBlock *>(recordInfo->data5);
            uint16_t classID = symValManager->getIDFromSymbol(static_cast<void *>(classOfMethod));
            allocRecord->setCpIndex(reloTarget, static_cast<uintptrj_t>(classID));
            }
         else
            {
            allocRecord->setCpIndex(reloTarget, static_cast<uintptrj_t>(classSymRef->getCPIndex()));
            }
         }
         break;

      case TR_ValidateInstanceField:
         {
         TR_RelocationRecordValidateInstanceField *fieldRecord = reinterpret_cast<TR_RelocationRecordValidateInstanceField *>(reloRecord);
         uintptrj_t inlinedSiteIndex = reinterpret_cast<uintptrj_t>(relocation->getTargetAddress());
         TR::AOTClassInfo *aotCI = reinterpret_cast<TR::AOTClassInfo*>(relocation->getTargetAddress2());
         void *classChainOffsetInSharedCache = sharedCache->offsetInSharedCacheFromPointer(aotCI->_classChain);

         fieldRecord->setInlinedSiteIndex(reloTarget, inlinedSiteIndex);
         fieldRecord->setConstantPool(reloTarget, reinterpret_cast<uintptrj_t>(aotCI->_constantPool));
         fieldRecord->setCpIndex(reloTarget, static_cast<uintptrj_t>(aotCI->_cpIndex));
         fieldRecord->setClassChainOffsetInSharedCache(reloTarget, reinterpret_cast<uintptrj_t>(classChainOffsetInSharedCache));
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
      case TR_StaticRamMethodConst:
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
         {
         TR_RelocationRecordMethodEnterCheck *mcRecord = reinterpret_cast<TR_RelocationRecordMethodEnterCheck *>(reloRecord);

         self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
         if (isVerbose)
            {
            traceMsg(self()->comp(), "\nDestination address %x", mcRecord->destinationAddress(reloTarget));
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
   if (!self()->comp()->getOption(TR_TraceRelocatableDataCG) && !self()->comp()->getOption(TR_TraceRelocatableDataDetailsCG) && !self()->comp()->getOption(TR_TraceReloCG))
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

   if (self()->comp()->getOption(TR_TraceReloCG))
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
      bool is64BitTarget = TR::Compiler->target.is64Bit();
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
         (unsigned long long)*(uintptrj_t *)cursor);
      cursor += sizeof (uintptrj_t);
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
         case TR_J2IVirtualThunkPointer:
            cursor++;        // unused field
            cursor += is64BitTarget ? 4 : 0;
            ep1 = cursor;
            ep2 = cursor + sizeof(uintptrj_t);
            ep3 = cursor + 2 * sizeof(uintptrj_t);
            cursor += 3 * sizeof(uintptrj_t);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            if (isVerbose)
               {
               traceMsg(
                  self()->comp(),
                  "\nInlined site index %lld, constant pool 0x%llx, offset to j2i thunk pointer 0x%llx",
                  (int64_t)*(uintptrj_t*)ep1,
                  (uint64_t)*(uintptrj_t*)ep2,
                  (uint64_t)*(uintptrj_t*)ep3);
               }
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
         case TR_CheckMethodExit:
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
               ep1 = cursor;
               cursor += 4;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  // ep1 is same as self()->comp()->getCurrentMethod()->constantPool())
                  traceMsg(self()->comp(), "\nDestination address %x", *(uint32_t *)ep1);
                  }
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
         case TR_InlinedStaticMethodWithNopGuard:
         case TR_InlinedSpecialMethodWithNopGuard:
         case TR_InlinedVirtualMethodWithNopGuard:
         case TR_InlinedInterfaceMethodWithNopGuard:
         case TR_InlinedAbstractMethodWithNopGuard:
         case TR_InlinedHCRMethod:
            cursor++;        // unused field
            if (is64BitTarget)
               {
               cursor += 4;           // padding
               ep1 = cursor;          // inlinedSiteIndex
               ep2 = cursor+8;        // constantPool
               ep3 = cursor+16;       // cpIndex
               ep4 = cursor+24;       // romClassOffsetInSharedCache
               ep5 = cursor+32; // destination address
               cursor +=40;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\nInlined Method: Inlined site index = %d, Constant pool = %x, cpIndex = %x, romClassOffsetInSharedCache=%p, destinationAddress = %p",
                                  *(uint64_t *)ep1, *(uint64_t *)ep2, *(uint64_t *)ep3, *(uint64_t *)ep4, *(uint64_t *)ep5);
                  }
               }
            else
               {
               ep1 = cursor;          // inlinedSiteIndex
               ep2 = cursor+4;        // constantPool
               ep3 = cursor+8;        // cpIndex
               ep4 = cursor+12;       // romClassOffsetInSharedCache
               ep5 = cursor+16; // destinationAddress
               cursor += 20;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\nInlined Method: Inlined site index = %d, Constant pool = %x, cpIndex = %x, romClassOffsetInSharedCache=%p, destinationAddress = %p",
                                  *(uint32_t *)ep1, *(uint32_t *)ep2, *(uint32_t *)ep3, *(uint32_t *)ep4, *(uint32_t *)ep5);
                  }
               }
            break;

         case TR_ArbitraryClassAddress:
         case TR_ClassPointer:
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

         case TR_MethodPointer:
            cursor++;           // unused field
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
                  traceMsg(self()->comp(), "\nMethod Pointer: Inlined site index = %d, classChainIdentifyingLoaderOffsetInSharedCache=%p, classChainForInlinedMethod %p, vTableOffset %x",
                                  *(uint64_t *)ep1, *(uint64_t *)ep2, *(uint64_t *)ep3, *(uint64_t *)ep4);
               else
                  traceMsg(self()->comp(), "\nMethod Pointer: Inlined site index = %d, classChainIdentifyingLoaderOffsetInSharedCache=%p, classChainForInlinedMethod %p, vTableOffset %x",
                                  *(uint32_t *)ep1, *(uint32_t *)ep2, *(uint32_t *)ep3, *(uint32_t *)ep4);
               }
            break;

         case TR_ProfiledInlinedMethodRelocation:
         case TR_ProfiledClassGuardRelocation:
         case TR_ProfiledMethodGuardRelocation:
            cursor++;        // unused field
            if (is64BitTarget)
               {
               cursor += 4;     // padding
               ep1 = cursor;    // inlinedSiteIndex
               ep2 = cursor+8;  // constantPool
               ep3 = cursor+16; // cpIndex
               ep4 = cursor+24; // romClassOffsetInSharedCache
               ep5 = cursor+32; // classChainIdentifyingLoaderOffsetInSharedCache
               ep6 = cursor+40; // classChainForInlinedMethod
               ep7 = cursor+48; // vTableSlot
               cursor +=56;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  traceMsg(self()->comp(), "\nProfiled Class Guard: Inlined site index = %d, Constant pool = %x, cpIndex = %x, romClassOffsetInSharedCache=%p, classChainIdentifyingLoaderOffsetInSharedCache=%p, classChainForInlinedMethod %p, methodIndex %d",
                                  *(uint64_t *)ep1, *(uint64_t *)ep2, *(uint64_t *)ep3, *(uint64_t *)ep4, *(uint64_t *)ep5, *(uint64_t *)ep6, *(uint64_t *)ep7);
               }
            else
               {
               ep1 = cursor;    // inlinedSiteIndex
               ep2 = cursor+4;  // constantPool
               ep3 = cursor+8;  // cpIndex
               ep4 = cursor+12; // romClassOffsetInSharedCache
               ep5 = cursor+16; // classChainIdentifyingLoaderOffsetInSharedCache
               ep6 = cursor+20; // classChainForInlinedMethod
               ep7 = cursor+24; // vTableSlot
               cursor += 28;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\nInlined Interface Method: Inlined site index = %d, Constant pool = %x, cpIndex = %x, romClassOffsetInSharedCache=%p, classChainIdentifyingLoaderOffsetInSharedCache=%p, classChainForInlinedMethod %p, vTableSlot %d",
                                  *(uint32_t *)ep1, *(uint32_t *)ep2, *(uint32_t *)ep3, *(uint32_t *)ep4, *(uint32_t *)ep5, *(uint32_t *)ep6, *(uint32_t *)ep7);
                  }
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
         case TR_ValidateClass:
         case TR_ValidateStaticField:
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
                  traceMsg(self()->comp(), "\nValidation Relocation: InlineCallSite index = %d, Constant pool = %x, cpIndex = %d, ROM Class offset = %x",
                                  *(uint64_t *)ep1, *(uint64_t *)ep2, *(uint64_t *)ep3, *(uint64_t *)ep4);
                  }
               else
                  {
                  traceMsg(self()->comp(), "\nValidation Relocation: InlineCallSite index = %d, Constant pool = %x, cpIndex = %d, ROM Class offset = %x",
                                  *(uint32_t *)ep1, *(uint32_t *)ep2, *(uint32_t *)ep3, *(uint32_t *)ep4);
                  }
               }
            break;
            }
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
         case TR_ValidateArbitraryClass:
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            ep1 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            ep2 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);

            if (is64BitTarget)
            {
            traceMsg(self()->comp(), "\nValidateArbitraryClass Relocation: classChainOffsetForClassToValidate = %p, classChainIdentifyingClassLoader = %p",
                            *(uint64_t *)ep1, *(uint64_t *)ep2);
            }
            else
            {
            traceMsg(self()->comp(), "\nValidateArbitraryClass Relocation: classChainOffsetForClassToValidate = %p, classChainIdentifyingClassLoader = %p",
                            *(uint32_t *)ep1, *(uint32_t *)ep2);
            }

            break;
         case TR_JNISpecialTargetAddress:
         case TR_VirtualRamMethodConst:
         case TR_SpecialRamMethodConst:
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            ep1 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            ep2 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            ep3 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);

            if (is64BitTarget)
            {
            traceMsg(self()->comp(), "\n Address Relocation (%s): inlinedIndex = %d, constantPool = %p, CPI = %d",
                            getNameForMethodRelocation(kind), *(uint64_t *)ep1, *(uint64_t *)ep2, *(uint64_t *)ep3);
            }
            else
            {
            traceMsg(self()->comp(), "\n Address Relocation (%s): inlinedIndex = %d, constantPool = %p, CPI = %d",
                            getNameForMethodRelocation(kind), *(uint32_t *)ep1, *(uint32_t *)ep2, *(uint32_t *)ep3);
            }
            break;
         case TR_InlinedInterfaceMethod:
         case TR_InlinedVirtualMethod:
            cursor++;
            if (is64BitTarget)
               {
               cursor += 4;     // padding
               ep1 = cursor;    // inlinedSiteIndex
               ep2 = cursor+8;  // constantPool
               ep3 = cursor+16; // cpIndex
               ep4 = cursor+24; // romClassOffsetInSharedCache
               cursor += 32;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\n Removed Guard inlined method: Inlined site index = %d, Constant pool = %x, cpIndex = %x, romClassOffsetInSharedCache=%p",
                                   *(uint64_t *)ep1, *(uint64_t *)ep2, *(uint64_t *)ep3, *(uint64_t *)ep4);
                  }
               }
            else
               {
               ep1 = cursor;          // inlinedSiteIndex
               ep2 = cursor+4;        // constantPool
               ep3 = cursor+8;        // cpIndex
               ep4 = cursor+12;       // romClassOffsetInSharedCache
               cursor += 16;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\n Removed Guard inlined method: Inlined site index = %d, Constant pool = %x, cpIndex = %x, romClassOffsetInSharedCache=%p",
                                   *(uint32_t *)ep1, *(uint32_t *)ep2, *(uint32_t *)ep3, *(uint32_t *)ep4);
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
         case TR_ClassUnloadAssumption:
            cursor++;        // unused field
            if (is64BitTarget)
               {
               cursor +=4; 
               }
            traceMsg(self()->comp(), "\n ClassUnloadAssumption \n");
            break;

         case TR_ValidateClassByName:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateClassByNameBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateClassByNameBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Class By Name: classID=%d beholderID=%d classChainOffsetInSCC=%p ",
                        (uint32_t)binaryTemplate->_classID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_classChainOffsetInSCC);
               }
            cursor += sizeof(TR_RelocationRecordValidateClassByNameBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateProfiledClass:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateProfiledClassBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateProfiledClassBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Profiled Class: classID=%d classChainOffsetInSCC=%d classChainOffsetForCLInScc=%p ",
                        (uint32_t)binaryTemplate->_classID,
                        binaryTemplate->_classChainOffsetInSCC,
                        binaryTemplate->_classChainOffsetForCLInScc);
               }
            cursor += sizeof(TR_RelocationRecordValidateProfiledClassBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateClassFromCP:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateClassFromCPBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateClassFromCPBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Class From CP: classID=%d, beholderID=%d, cpIndex=%d ",
                        (uint32_t)binaryTemplate->_classID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_cpIndex);
               }
            cursor += sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateDefiningClassFromCP:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Defining Class From CP: classID=%d, beholderID=%d, cpIndex=%d, isStatic=%s ",
                        (uint32_t)binaryTemplate->_classID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_cpIndex,
                        binaryTemplate->_isStatic ? "true" : "false");
               }
            cursor += sizeof(TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateStaticClassFromCP:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateStaticClassFromCPBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateStaticClassFromCPBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Static Class From CP: classID=%d, beholderID=%d, cpIndex=%d ",
                        (uint32_t)binaryTemplate->_classID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_cpIndex);
               }
            cursor += sizeof(TR_RelocationRecordValidateStaticClassFromCPBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateArrayClassFromComponentClass:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateArrayFromCompBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateArrayFromCompBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Array Class From Component: arrayClassID=%d, componentClassID=%d ",
                        (uint32_t)binaryTemplate->_arrayClassID,
                        (uint32_t)binaryTemplate->_componentClassID);
               }
            cursor += sizeof(TR_RelocationRecordValidateArrayFromCompBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateSuperClassFromClass:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Super Class From Class: superClassID=%d, childClassID=%d ",
                        (uint32_t)binaryTemplate->_superClassID,
                        (uint32_t)binaryTemplate->_childClassID);
               }
            cursor += sizeof(TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateClassInstanceOfClass:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Class InstanceOf Class: classOneID=%d, classTwoID=%d, objectTypeIsFixed=%s, castTypeIsFixed=%s, isInstanceOf=%s ",
                        (uint32_t)binaryTemplate->_classOneID,
                        (uint32_t)binaryTemplate->_classTwoID,
                        binaryTemplate->_objectTypeIsFixed ? "true" : "false",
                        binaryTemplate->_castTypeIsFixed ? "true" : "false",
                        binaryTemplate->_isInstanceOf ? "true" : "false");
               }
            cursor += sizeof(TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateSystemClassByName:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateSystemClassByNameBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateSystemClassByNameBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate System Class By Name: systemClassID=%d classChainOffsetInSCC=%p ",
                        (uint32_t)binaryTemplate->_systemClassID,
                        binaryTemplate->_classChainOffsetInSCC);
               }
            cursor += sizeof(TR_RelocationRecordValidateSystemClassByNameBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateClassFromITableIndexCP:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateClassFromITableIndexCPBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateClassFromITableIndexCPBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Class From ITableIndex CP: classID=%d beholderID=%d cpIndex=%d ",
                        (uint32_t)binaryTemplate->_classID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_cpIndex);
               }
            cursor += sizeof(TR_RelocationRecordValidateClassFromITableIndexCPBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateDeclaringClassFromFieldOrStatic:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Declaring Class From Field Or Static: classID=%d, beholderID=%d, cpIndex=%d ",
                        (uint32_t)binaryTemplate->_classID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_cpIndex);
               }
            cursor += sizeof(TR_RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateConcreteSubClassFromClass:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateConcreteSubFromClassBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateConcreteSubFromClassBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Concrete SubClass From Class: childClassID=%d, superClassID=%d ",
                        (uint32_t)binaryTemplate->_childClassID,
                        (uint32_t)binaryTemplate->_superClassID);
               }
            cursor += sizeof(TR_RelocationRecordValidateConcreteSubFromClassBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateClassChain:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateClassChainBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateClassChainBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Class Chain: classID=%d classChainOffsetInSCC=%p ",
                        (uint32_t)binaryTemplate->_classID,
                        binaryTemplate->_classChainOffsetInSCC);
               }
            cursor += sizeof(TR_RelocationRecordValidateClassChainBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateMethodFromClass:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateMethodFromClassBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateMethodFromClassBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Method By Name: methodID=%d, beholderID=%d, index=%d ",
                        (uint32_t)binaryTemplate->_methodID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_index);
               }
            cursor += sizeof(TR_RelocationRecordValidateMethodFromClassBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateStaticMethodFromCP:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateStaticMethodFromCPBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateStaticMethodFromCPBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Static Method From CP: methodID=%d, definingClassID=%d, beholderID=%d, cpIndex=%d ",
                        (uint32_t)binaryTemplate->_methodID,
                        (uint32_t)binaryTemplate->_definingClassID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_cpIndex);
               }
            cursor += sizeof(TR_RelocationRecordValidateStaticMethodFromCPBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateSpecialMethodFromCP:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateSpecialMethodFromCPBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateSpecialMethodFromCPBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Special Method From CP: methodID=%d, definingClassID=%d, beholderID=%d, cpIndex=%d ",
                        (uint32_t)binaryTemplate->_methodID,
                        (uint32_t)binaryTemplate->_definingClassID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_cpIndex);
               }
            cursor += sizeof(TR_RelocationRecordValidateSpecialMethodFromCPBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateVirtualMethodFromCP:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateVirtualMethodFromCPBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateVirtualMethodFromCPBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Virtual Method From CP: methodID=%d, definingClassID=%d, beholderID=%d, cpIndex=%d ",
                        (uint32_t)binaryTemplate->_methodID,
                        (uint32_t)binaryTemplate->_definingClassID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_cpIndex);
               }
            cursor += sizeof(TR_RelocationRecordValidateVirtualMethodFromCPBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateVirtualMethodFromOffset:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Virtual Method From Offset: methodID=%d, definingClassID=%d, beholderID=%d, virtualCallOffset=%d, ignoreRtResolve=%s ",
                        (uint32_t)binaryTemplate->_methodID,
                        (uint32_t)binaryTemplate->_definingClassID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_virtualCallOffsetAndIgnoreRtResolve & ~1,
                        (binaryTemplate->_virtualCallOffsetAndIgnoreRtResolve & 1) ? "true" : "false");
               }
            cursor += sizeof(TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            }
            break;

         case TR_ValidateInterfaceMethodFromCP:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Interface Method From CP: methodID=%d, definingClassID=%d, beholderID=%d, lookupID=%d, cpIndex=%d ",
                        (uint32_t)binaryTemplate->_methodID,
                        (uint32_t)binaryTemplate->_definingClassID,
                        (uint32_t)binaryTemplate->_beholderID,
                        (uint32_t)binaryTemplate->_lookupID,
                        binaryTemplate->_cpIndex);
               }
            cursor += sizeof(TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
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

         case TR_ValidateMethodFromClassAndSig:
            {
            cursor++;
            if (is64BitTarget)
               cursor += 4;     // padding
            cursor -= sizeof(TR_RelocationRecordBinaryTemplate);
            TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *binaryTemplate =
                  reinterpret_cast<TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate *>(cursor);
            if (isVerbose)
               {
               traceMsg(self()->comp(), "\n Validate Method From Class and Sig: methodID=%d, definingClassID=%d, lookupClassID=%d, beholderID=%d, romMethodOffsetInSCC=%p ",
                        (uint32_t)binaryTemplate->_methodID,
                        (uint32_t)binaryTemplate->_definingClassID,
                        (uint32_t)binaryTemplate->_lookupClassID,
                        (uint32_t)binaryTemplate->_beholderID,
                        binaryTemplate->_romMethodOffsetInSCC);
               }
            cursor += sizeof(TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate);
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
            TR_ASSERT(false, "should be unreachable");
            printf("Unknown Relocation type = %d\n", kind);
            fflush(stdout);
            exit(0);

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

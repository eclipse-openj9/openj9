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

#include "codegen/CodeGenerator.hpp"
#include "env/SharedCache.hpp"
#include "env/jittypes.h"
#include "exceptions/PersistenceFailure.hpp"
#include "il/DataTypes.hpp"
#include "env/VMJ9.h"
#include "codegen/AheadOfTimeCompile.hpp"

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

   traceMsg(self()->comp(), "Address           Size %-31s", "Type");
   traceMsg(self()->comp(), "Width EIP Index Offsets\n"); // Offsets from Code Start

   while (cursor < endOfData)
      {
      void *ep1, *ep2, *ep3, *ep4, *ep5, *ep6, *ep7;
      TR::SymbolReference *tempSR = NULL;

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

      switch (kind)
         {
         case TR_ConstantPool:
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
         case TR_AbsoluteHelperAddress:
         case TR_HelperAddress:
            // final byte of header is the index which indicates the particular helper being relocated to
            cursor++;
            ep1 = cursor;
            cursor += 4;
            traceMsg(self()->comp(), "%-6d", *(uint32_t *)ep1);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            if (isVerbose)
               {
               tempSR = self()->comp()->getSymRefTab()->getSymRef(*(int32_t*)ep1);
               traceMsg(self()->comp(), "\nHelper method address of %s(%d)", self()->getDebug()->getName(tempSR), *(int32_t*)ep1);
               }
            break;
         case TR_RelativeMethodAddress:
            // next word is the address of the constant pool to which the index refers
            // second word is the index in the above stored constant pool that indicates the particular
            // relocation target
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
                  traceMsg(self()->comp(), "\nRelative Method Address: Constant pool = %x, index = %d", *(uint64_t *)ep1, *(uint64_t *)ep2);
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
                  traceMsg(self()->comp(), "\nRelative Method Address: Constant pool = %x, index = %d", *(uint32_t *)ep1, *(uint32_t *)ep2);
                  }
               }
            break;
         case TR_AbsoluteMethodAddress:
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
         case TR_ClassObject:
            // next word is the address of the constant pool to which the index refers
            // second word is the index in the above stored constant pool that indicates the particular
            // relocation target
            cursor++;        //unused field
            if (is64BitTarget)
               {
               cursor += 4;     // padding
               ep1 = cursor;
               ep2 = cursor+8;
               cursor += 16;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\nClass Object: Constant pool = %x, index = %d", *(uint64_t *)ep1, *(uint64_t *)ep2);
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
                  traceMsg(self()->comp(), "\nClass Object: Constant pool = %x, index = %d", *(uint32_t *)ep1, *(uint32_t *)ep2);
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
         case TR_InterfaceObject:
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
                  traceMsg(self()->comp(),"Interface Object: Constant pool = %x, index = %d", *(uint64_t *)ep1, *(uint64_t *)ep2);
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
                  traceMsg(self()->comp(),"Interface Object: Constant pool = %x, index = %d", *(uint32_t *)ep1, *(uint32_t *)ep2);
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
         case TR_BodyInfoAddress:
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
         case TR_Thunks:
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
                  traceMsg(self()->comp(), "\nInlined site index %d, constant pool %x", *(int64_t*)ep1, *(uint64_t *)ep2);
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
                  traceMsg(self()->comp(), "\nInlined site index %d, constant pool %x", *(int32_t*)ep1, *(uint32_t *)ep2);
                  }
               }
            break;
         case TR_Trampolines:
            // constant pool address is placed as the last word of the header
            //traceMsg(self()->comp(), "\nConstant pool %x\n", *(uint32_t *)++cursor);
            cursor++;        // unused field
            if (is64BitTarget)
               {
               cursor +=4;      // padding
               ep1 = cursor;
               cursor += 8;
               ep2 = cursor;
               cursor += 8;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  traceMsg(self()->comp(), "\nInlined site index %d, constant pool %x", *(int64_t*)ep1, *(uint64_t *)ep2);
                  }
               }
            else
               {
               ep1 = cursor;
               cursor += 4;
               ep2 = cursor;
               cursor += 4;
               self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
               if (isVerbose)
                  {
                  // ep1 is same as self()->comp()->getCurrentMethod()->constantPool())
                  traceMsg(self()->comp(), "\nInlined site index %d, constant pool %x", *(int32_t*)ep1, *(uint32_t *)ep2);
                  }
               }
            break;
         case TR_CheckMethodEnter:
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
         case TR_RamMethod:
            cursor++;
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
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
         case TR_VerifyClassObjectForAlloc:
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
            ep5 = (uintptr_t *) cursor;
            cursor += sizeof(uintptr_t);
            self()->traceRelocationOffsets(cursor, offsetSize, endOfCurrentRecord, orderedPair);
            if (isVerbose)
               {
               if (is64BitTarget)
                  {
                  traceMsg(self()->comp(), "\nVerify Class Object for Allocation: InlineCallSite index = %d, Constant pool = %x, index = %d, binaryEncode = %x, size = %d",
                                  *(uint64_t *)ep1, *(uint64_t *)ep2, *(uint64_t *)ep3, *(uint64_t *)ep4, *(uint64_t *)ep5);
                  }
               else
                  {
                  traceMsg(self()->comp(), "\nVerify Class Object for Allocation: InlineCallSite index = %d, Constant pool = %x, index = %d, binaryEncode = %x, size = %d",
                                  *(uint32_t *)ep1, *(uint32_t *)ep2, *(uint32_t *)ep3, *(uint32_t *)ep4, *(uint32_t *)ep5);
                  }
               }
            }
            break;
         case TR_InlinedStaticMethodWithNopGuard:
         case TR_InlinedSpecialMethodWithNopGuard:
         case TR_InlinedVirtualMethodWithNopGuard:
         case TR_InlinedInterfaceMethodWithNopGuard:
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
                  traceMsg(self()->comp(), "\nProfiled Class Guard: Inlined site index = %d, Constant pool = %x, cpIndex = %x, romClassOffsetInSharedCache=%p, classChainIdentifyingLoaderOffsetInSharedCache=%p, classChainForInlinedMethod %p, vTableSlot %d",
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

         case TR_VerifyRefArrayForAlloc:
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
                  traceMsg(self()->comp(), "\nVerify Class Object for Allocation: InlineCallSite index = %d, Constant pool = %x, index = %d, binaryEncode = %x",
                                  *(uint64_t *)ep1, *(uint64_t *)ep2, *(uint64_t *)ep3, *(uint64_t *)ep4);
                  }
               else
                  {
                  traceMsg(self()->comp(), "\nVerify Class Object for Allocation: InlineCallSite index = %d, Constant pool = %x, index = %d, binaryEncode = %x",
                                  *(uint32_t *)ep1, *(uint32_t *)ep2, *(uint32_t *)ep3, *(uint32_t *)ep4);
                  }
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
         case TR_ValidateInstanceField:
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
         case TR_JNIVirtualTargetAddress:
         case TR_JNIStaticTargetAddress:
         case TR_JNISpecialTargetAddress:
         case TR_VirtualRamMethodConst:
         case TR_SpecialRamMethodConst:
         case TR_StaticRamMethodConst:
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

         default:
            traceMsg(self()->comp(), "Unknown Relocation type = %d\n", kind);
            TR_ASSERT(false, "should be unreachable");
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

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

#include "x/codegen/AllocPrefetchSnippet.hpp"

#include "codegen/Relocation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "runtime/CodeRuntime.hpp"
#include "runtime/J9CodeCache.hpp"
#include "env/VMJ9.h"

uint8_t *TR::X86AllocPrefetchSnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();
   if (TR::Options::getCmdLineOptions()->realTimeGC())
      return 0;

   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);

   TR::SymbolReference *helperSymRef = NULL;

   bool useSharedCodeCacheSnippet = fej9->supportsCodeCacheSnippets();

   bool prefetchThunkGenerated = (fej9->getAllocationPrefetchCodeSnippetAddress(comp) != 0);
#ifdef J9VM_GC_NON_ZERO_TLH
   if (isNonZeroTLH())
      {
      prefetchThunkGenerated = (fej9->getAllocationNoZeroPrefetchCodeSnippetAddress(comp) != 0);
      }
#endif

   TR_ASSERT(prefetchThunkGenerated, "Invalid prefetch snippet.");

   // CALL [32-bit relative]
   //
   *buffer++ = 0xe8;

   int32_t disp32;
   uintptrj_t helperAddress = 0;

   if (useSharedCodeCacheSnippet)
      {
#ifdef J9VM_GC_NON_ZERO_TLH
      if(!isNonZeroTLH())
         {
         helperAddress = (uintptr_t)(fej9->getAllocationPrefetchCodeSnippetAddress(comp));
         }
      else
         {
         helperAddress = (uintptr_t)(fej9->getAllocationNoZeroPrefetchCodeSnippetAddress(comp));
         }
#else
      helperAddress = (uintptr_t)(fej9->getAllocationPrefetchCodeSnippetAddress(comp));
#endif
      }

   if (helperAddress && IS_32BIT_RIP(helperAddress, (buffer + 4) ) )
      {
      disp32 = (int32_t)(helperAddress - (uintptr_t)(buffer+4));
      }
   else
      {
      TR_RuntimeHelper helper = (comp->getOption(TR_EnableNewX86PrefetchTLH)) ? TR_X86newPrefetchTLH : TR_X86prefetchTLH;
      helperSymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(helper, false, false, false);
      disp32 = cg()->branchDisplacementToHelperOrTrampoline(buffer+4, helperSymRef);
      if (fej9->helpersNeedRelocation())
         {
         cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(buffer,
                                                                                (uint8_t *)helperSymRef,
                                                                                TR_HelperAddress,
                                                                                cg()),
                                   __FILE__, __LINE__, getNode());
         }
      }

   *(int32_t *)buffer = disp32;
   buffer += 4;

   return genRestartJump(buffer);
   }

uint32_t TR::X86AllocPrefetchSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 10 + estimateRestartJumpLength(estimatedSnippetStart + 2);
   }

TR_X86AllocPrefetchGeometry
TR::X86AllocPrefetchSnippet::generatePrefetchGeometry()
   {

   if (TR::Options::_TLHPrefetchSize <= 0)
      TR::Options::_TLHPrefetchSize = 384;

   // These parameters were experimentally determined to be optimal for
   // Woodcrest hardware for small applications.

   if (TR::Options::_TLHPrefetchLineSize <= 0)
      TR::Options::_TLHPrefetchLineSize = 64;

   if (TR::Options::_TLHPrefetchLineCount <= 0)
      TR::Options::_TLHPrefetchLineCount = 8;

   if (TR::Options::_TLHPrefetchStaggeredLineCount <= 0)
      TR::Options::_TLHPrefetchStaggeredLineCount = 4;

   if (TR::Options::_TLHPrefetchBoundaryLineCount <= 0)
      TR::Options::_TLHPrefetchBoundaryLineCount = 6;

   if (TR::Options::_TLHPrefetchTLHEndLineCount <= 0)
      TR::Options::_TLHPrefetchTLHEndLineCount = 6;

   return TR_X86AllocPrefetchGeometry(
      TR::Options::_TLHPrefetchLineSize,
      TR::Options::_TLHPrefetchLineCount,
      TR::Options::_TLHPrefetchStaggeredLineCount,
      TR::Options::_TLHPrefetchBoundaryLineCount,
      TR::Options::_TLHPrefetchTLHEndLineCount
      );
   }

template <TR::HeapTypes::Type> struct vmThreadHeapOffsets;

template <> struct vmThreadHeapOffsets<TR::HeapTypes::ZeroedHeap>
   {
   static const int32_t offsetOfHeapAlloc = offsetof(J9VMThread, heapAlloc);
   static const int32_t offsetOfHeapTop   = offsetof(J9VMThread, heapTop);
   static const int32_t offsetOfTLHPrefetchCount = offsetof(J9VMThread, tlhPrefetchFTA);
   };

template <> struct vmThreadHeapOffsets<TR::HeapTypes::NonZeroedHeap>
   {
   static const int32_t offsetOfHeapAlloc = offsetof(J9VMThread, nonZeroHeapAlloc);
   static const int32_t offsetOfHeapTop   = offsetof(J9VMThread, nonZeroHeapTop);
   static const int32_t offsetOfTLHPrefetchCount = offsetof(J9VMThread, nonZeroTlhPrefetchFTA);
   };

template <TR::HeapTypes::Type HEAP_TYPE>
class HeapProperties
   {
   private:

   typedef vmThreadHeapOffsets<HEAP_TYPE> HeapOffsets;

   public:

   static int32_t offsetOfHeapAlloc() { return HeapOffsets::offsetOfHeapAlloc; }
   static int32_t offsetOfHeapTop() { return HeapOffsets::offsetOfHeapTop; }
   static int32_t offsetOfTLHPrefetchCount() { return HeapOffsets::offsetOfTLHPrefetchCount; }
   static bool needWideDisplacementForHeapAlloc() { return (offsetOfHeapAlloc() > 127 || offsetOfHeapAlloc() < -128); }
   static bool needWideDisplacementForHeapTop() { return (offsetOfHeapTop() > 127 || offsetOfHeapTop() < -128); }
   static bool needWideDisplacementForTLHPrefetchCount() { return (offsetOfTLHPrefetchCount() > 127 || offsetOfTLHPrefetchCount() < -128); }
   };

template <TR::HeapTypes::Type HEAP_TYPE, bool is64Bit>
uint8_t* TR::X86AllocPrefetchSnippet::emitSharedBody(uint8_t* prefetchSnippetBuffer, TR_X86ProcessorInfo &cpuInfo)
   {

   typedef HeapProperties<HEAP_TYPE> HeapTraits;

   static char * printCodeCacheSnippetAddress = feGetEnv("TR_printCodeCacheSnippetAddress");
   if (printCodeCacheSnippetAddress)
      {
      fprintf(stdout, "%s Allocation snippet is at address %p, size=%d\n", TR::HeapTypes::getPrefix(HEAP_TYPE), prefetchSnippetBuffer, sizeOfSharedBody<HEAP_TYPE, is64Bit>());
      fflush(stdout);
      }

   const TR_X86AllocPrefetchGeometry &prefetchGeometry = generatePrefetchGeometry();

   int32_t lineSize               = prefetchGeometry.getPrefetchLineSize();
   int32_t numLines               = prefetchGeometry.getPrefetchLineCount();
   int32_t staggerLines           = prefetchGeometry.getPrefetchStaggeredLineCount();
   int32_t boundaryLines          = prefetchGeometry.getPrefetchBoundaryLineCount();

   // PUSH rcx
   //
   *prefetchSnippetBuffer++ = 0x51;

   // MOV rcx, qword ptr [rbp + heapAlloc]
   //
   if (is64Bit)
      {
      // REX
      //
      *prefetchSnippetBuffer++ = 0x48;
      }

   prefetchSnippetBuffer[0] = 0x8B;

   if (HeapTraits::needWideDisplacementForHeapAlloc())
      {
      prefetchSnippetBuffer[1] = 0x8d;
      prefetchSnippetBuffer += 2;
      *((int32_t *)prefetchSnippetBuffer) = HeapTraits::offsetOfHeapAlloc();
      prefetchSnippetBuffer += 4;
      }
   else
      {
      prefetchSnippetBuffer[1] = 0x4d;
      prefetchSnippetBuffer[2] = (uint8_t) HeapTraits::offsetOfHeapAlloc();
      prefetchSnippetBuffer += 3;
      }

   // PREFETCHNTA [rcx + distance]
   // PREFETCHNTA [rcx + distance + lineSize]
   // ...
   // PREFETCHNTA [rcx + distance + n*lineSize]
   //
   for (int32_t lineOffset = 0; lineOffset < numLines; ++lineOffset)
      {
      prefetchSnippetBuffer[0] = 0x0F;
      if (cpuInfo.isAMD15h())
         prefetchSnippetBuffer[1] = 0x0D;
      else
         prefetchSnippetBuffer[1] = 0x18;
      prefetchSnippetBuffer[2] = 0x81;
      prefetchSnippetBuffer += 3;
      *(int32_t *)prefetchSnippetBuffer = (staggerLines + lineOffset) * lineSize;
      prefetchSnippetBuffer += 4;
      }

   // MOV dword ptr [rbp + TLH_PREFETCH_COUNT], "size"
   //
   *prefetchSnippetBuffer++ = 0xC7;

   if (HeapTraits::needWideDisplacementForTLHPrefetchCount())
      {
      *prefetchSnippetBuffer++ = 0x85;
      *(int32_t *)prefetchSnippetBuffer = HeapTraits::offsetOfTLHPrefetchCount();
      prefetchSnippetBuffer += 4;
      }
   else
      {
      *prefetchSnippetBuffer++ = 0x45;
      *prefetchSnippetBuffer++ = (uint8_t)HeapTraits::offsetOfTLHPrefetchCount();
      }

   *(uint32_t *)prefetchSnippetBuffer = (uint32_t)(boundaryLines*lineSize);
   prefetchSnippetBuffer += 4;

   // POP rcx
   //
   *prefetchSnippetBuffer++ = 0x59;

   // RET
   //
   *prefetchSnippetBuffer++ = 0xC3;

   return prefetchSnippetBuffer;
   }

template <TR::HeapTypes::Type HEAP_TYPE, bool is64Bit>
int32_t TR::X86AllocPrefetchSnippet::sizeOfSharedBody()
   {
   typedef HeapProperties<HEAP_TYPE> HeapTraits;

   const TR_X86AllocPrefetchGeometry &prefetchGeometry = generatePrefetchGeometry();

   int32_t prefetchSnippetSize = (is64Bit ? 14 : 13) + prefetchGeometry.getPrefetchLineCount() * 7;

   if (HeapTraits::needWideDisplacementForHeapAlloc())
      {
      prefetchSnippetSize += 3;
      }


   if (HeapTraits::needWideDisplacementForTLHPrefetchCount())
      {
      prefetchSnippetSize += 3;
      }

   /*
    * TODO: Refactor the alignment value to use a common definition either from the code cache or from some form of target query.
    */
   int32_t alignedSize = TR::alignAllocationSize<32>(prefetchSnippetSize);

   return alignedSize;
   }

uint32_t TR::getCCPreLoadedCodeSize()
   {
#if defined(TR_TARGET_64BIT)
   uint32_t sizeOfZeroedPrefetchBody = TR::X86AllocPrefetchSnippet::sizeOfSharedBody<TR::HeapTypes::ZeroedHeap, true>();
   uint32_t sizeOfNonZeroedPrefetchBody = TR::X86AllocPrefetchSnippet::sizeOfSharedBody<TR::HeapTypes::NonZeroedHeap, true>();
#else
   uint32_t sizeOfZeroedPrefetchBody = TR::X86AllocPrefetchSnippet::sizeOfSharedBody<TR::HeapTypes::ZeroedHeap, false>();
   uint32_t sizeOfNonZeroedPrefetchBody = TR::X86AllocPrefetchSnippet::sizeOfSharedBody<TR::HeapTypes::NonZeroedHeap, false>();
#endif
   return sizeOfZeroedPrefetchBody + sizeOfNonZeroedPrefetchBody;
   }

void TR::createCCPreLoadedCode(uint8_t *CCPreLoadedCodeBase, uint8_t *CCPreLoadedCodeTop, void ** CCPreLoadedCodeTable, TR::CodeGenerator *cg)
   {
   uint8_t *cursor = CCPreLoadedCodeBase;

   CCPreLoadedCodeTable[TR_CCPreLoadedCode::TR_AllocPrefetch] = static_cast<void *>(cursor);
   if (TR::Compiler->target.is64Bit())
      cursor = TR::X86AllocPrefetchSnippet::emitSharedBody<TR::HeapTypes::ZeroedHeap, true>(cursor, cg->getX86ProcessorInfo());
   else
      cursor = TR::X86AllocPrefetchSnippet::emitSharedBody<TR::HeapTypes::ZeroedHeap, false>(cursor, cg->getX86ProcessorInfo());

   cursor = static_cast<uint8_t *>( TR::alignAllocation<32>(cursor) );

   CCPreLoadedCodeTable[TR_CCPreLoadedCode::TR_NonZeroAllocPrefetch] = static_cast<void *>(cursor);
   if (TR::Compiler->target.is64Bit())
      cursor = TR::X86AllocPrefetchSnippet::emitSharedBody<TR::HeapTypes::NonZeroedHeap, true>(cursor, cg->getX86ProcessorInfo());
   else
      cursor = TR::X86AllocPrefetchSnippet::emitSharedBody<TR::HeapTypes::NonZeroedHeap, false>(cursor, cg->getX86ProcessorInfo());

   cursor = static_cast<uint8_t *>( TR::alignAllocation<32>(cursor) );

   TR_ASSERT(cursor == CCPreLoadedCodeTop, "The expected and actual sizes of the emitted code differ. cursor = %p, CCPreLoadedCodeTop = %p", cursor, CCPreLoadedCodeTop);
   }

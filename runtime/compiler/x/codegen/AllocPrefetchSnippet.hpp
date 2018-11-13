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

#ifndef IA32ALLOCPREFETCHSNIPPET_INCL
#define IA32ALLOCPREFETCHSNIPPET_INCL

#include "x/codegen/RestartSnippet.hpp"

namespace TR {
void createCCPreLoadedCode(uint8_t *CCPreLoadedCodeBase, uint8_t *CCPreLoadedCodeTop, void ** CCPreLoadedCodeTable, TR::CodeGenerator *cg);
uint32_t getCCPreLoadedCodeSize();
}


struct TR_X86AllocPrefetchGeometry
   {
   public:

   TR_X86AllocPrefetchGeometry(int32_t prefetchLineSize, int32_t prefetchLineCount, int32_t prefetchStaggeredLineCount, int32_t prefetchBoundaryLineCount, int32_t prefetchTLHEndLineCount) :
      _prefetchLineSize(prefetchLineSize),
      _prefetchLineCount(prefetchLineCount),
      _prefetchStaggeredLineCount(prefetchStaggeredLineCount),
      _prefetchBoundaryLineCount(prefetchBoundaryLineCount),
      _prefetchTLHEndLineCount(prefetchTLHEndLineCount)
      {}

   int32_t getPrefetchLineSize() const             { return _prefetchLineSize; }
   int32_t getPrefetchLineCount() const            { return _prefetchLineCount; }
   int32_t getPrefetchStaggeredLineCount() const   { return _prefetchStaggeredLineCount; }
   int32_t getPrefetchBoundaryLineCount() const    { return _prefetchBoundaryLineCount; }
   int32_t getPrefetchTLHEndLineCount() const      { return _prefetchTLHEndLineCount; }

   int32_t sizeOfTLHEndCount() const               { return getPrefetchTLHEndLineCount() * getPrefetchLineSize(); }
   bool needWideDisplacementForTLHEndCount() const { return (sizeOfTLHEndCount() > 127 || sizeOfTLHEndCount() < -128); }

   private:
   int32_t _prefetchLineSize;
   int32_t _prefetchLineCount;
   int32_t _prefetchStaggeredLineCount;
   int32_t _prefetchBoundaryLineCount;
   int32_t _prefetchTLHEndLineCount;
   };

namespace TR {

struct HeapTypes
   {
   enum Type
      {
      ZeroedHeap,
      NonZeroedHeap
      };
   static const char * const getPrefix(size_t index)
      { static const char * const prefixes[] =
         {
         "Zeroed",
         "Non-Zeroed"
         };
        return prefixes[index];
      }
   };

class X86AllocPrefetchSnippet  : public TR::X86RestartSnippet
   {
   public:

   X86AllocPrefetchSnippet(TR::CodeGenerator *cg,
                              TR::Node          *node,
                              int32_t           prefetchSize,
                              TR::LabelSymbol    *restartlab,
                              TR::LabelSymbol    *snippetlab,
                              bool              nonZeroTLH)
      : TR::X86RestartSnippet(cg, node, restartlab, snippetlab, false)
      {
      _prefetchSize = prefetchSize;
      _nonZeroTLH = nonZeroTLH;
      }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   
   int32_t getPrefetchSize() { return _prefetchSize; }

   bool isNonZeroTLH() { return _nonZeroTLH; }

   private:

   static TR_X86AllocPrefetchGeometry generatePrefetchGeometry();
   template <TR::HeapTypes::Type HEAP_TYPE, bool is64Bit> static int32_t sizeOfSharedBody();
   template <TR::HeapTypes::Type HEAP_TYPE, bool is64Bit> static uint8_t* emitSharedBody(uint8_t *, TR_X86ProcessorInfo &);

   friend void TR::createCCPreLoadedCode(uint8_t *CCPreLoadedCodeBase, uint8_t *CCPreLoadedCodeTop, void ** CCPreLoadedCodeTable, TR::CodeGenerator *cg);
   friend uint32_t TR::getCCPreLoadedCodeSize();

   int32_t _prefetchSize;
   bool _nonZeroTLH;

   };

}

#endif

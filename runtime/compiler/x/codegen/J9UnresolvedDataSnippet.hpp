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

#ifndef J9_X86UNRESOLVEDDATASNIPPET_INCL
#define J9_X86UNRESOLVEDDATASNIPPET_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_UNRESOLVEDDATASNIPPET_CONNECTOR
#define J9_UNRESOLVEDDATASNIPPET_CONNECTOR
namespace J9 { namespace X86 { class UnresolvedDataSnippet; } }
namespace J9 { typedef J9::X86::UnresolvedDataSnippet UnresolvedDataSnippetConnector; }
#else
#error J9::X86::UnresolvedDataSnippet expected to be a primary connector, but a J9 connector is already defined
#endif


#include "compiler/codegen/J9UnresolvedDataSnippet.hpp"

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "runtime/J9Runtime.hpp"
#include "infra/Flags.hpp"
#include "env/CompilerEnv.hpp"

namespace TR { class Node; }


namespace J9
{

namespace X86
{

class UnresolvedDataSnippet : public J9::UnresolvedDataSnippet
   {

   public:

   UnresolvedDataSnippet(TR::CodeGenerator *cg, TR::Node *node, TR::SymbolReference *symRef, bool isStore, bool isGCSafePoint);

   Kind getKind() { return TR::Compiler->target.is64Bit() ? IsUnresolvedDataAMD64 : IsUnresolvedDataIA32; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   TR_RuntimeHelper getHelper();

   uint8_t *emitResolveHelperCall(uint8_t *cursor);
   uint8_t *emitUnresolvedInstructionDescriptor(uint8_t *cursor);
   uint32_t getUnresolvedStaticStoreDeltaWithMemBarrier();
   uint8_t *emitConstantPoolAddress(uint8_t *cursor);
   uint8_t *emitConstantPoolIndex(uint8_t *cursor);
   uint8_t *fixupDataReferenceInstruction(uint8_t *cursor);

   TR::SymbolReference * getGlueSymRef() { return _glueSymRef; }
   void setGlueSymRef(TR::SymbolReference * glueSymRef) { _glueSymRef = glueSymRef; }

   TR::Symbol *getDataSymbol() {return getDataSymbolReference()->getSymbol();}

   bool resolveMustPatch8Bytes()
      {
      // On 64-bit, only unresolved fields require 4-byte patching.
      //
      return TR::Compiler->target.is64Bit() && !getDataSymbol()->isShadow();
      }

   uint8_t setNumLiveX87Registers(uint8_t live)
      {
      // If the unresolved data snippet call is for a floating point
      // load then the register assigner will free up one register on
      // the fp stack, however the live count isn't changed at that point
      // in which case we'll still have 8 live FPR, which we'll spill and reload
      // in PicBuilder. However, if we are doing floating point load and the
      // live count is 8 we'll do total of 9 pushes on the fp stack, 8 in the
      // runtime code + 1 for the load, which will cause fp stack overflow and
      // incorrect load. The fix is to adjust the number of live registers to 7
      // when we are doing floating load in the unresolved snippet.
      // See JCK: api.java.awt.geom.AffineTransform.CreateInverseTest
      //
      if (!isUnresolvedStore() && isFloatData() && (live == 8))
         {
         live--;
         }

      return (_numLiveX87Registers = live);
      }

   uint8_t getNumLiveX87Registers() {return _numLiveX87Registers;}

   /*
    * Flags to be passed to the resolution runtime.  These flags piggyback on
    * the unused bits of the cpIndex.
    *
    * Move to J9 FE.
    */
   enum
      {
      cpIndex_hasLiveXMMRegisters    = 0x10000000,
      cpIndex_doNotPatchSnippet      = 0x00200000,
      cpIndex_longPushTag            = 0x00800000,
      cpIndex_isStaticResolution     = 0x20000000,
      cpIndex_patch8ByteResolution   = 0x40000000, // OVERLOADED: 64-BIT only
      cpIndex_checkVolatility        = 0x00080000,
      cpIndex_procAsLongLow          = 0x00040000,
      cpIndex_procAsLongHigh         = 0x00020000,
      cpIndex_isHighWordOfLongPair   = 0x40000000,  // OVERLOADED: 32-BIT only
      cpIndex_extremeStaticMemBarPos = 0x80000000,
      cpIndex_genStaticMemBarPos     = 0x00000000,
      cpIndex_isFloatStore           = 0x00040000,  // OVERLOADED: 64-BIT only
      cpIndex_isCompressedPointer    = 0x40000000,  // OVERLOADED: multiTenancy perform load/store
      cpIndex_isOwningObjectNeeded   = 0x00040000,  // OVERLOADED: multiTenancy refrence type store needs owning object register
      };

   bool isFloatData() {return _flags.testAll(TO_MASK32(IsFloatData));}
   void setIsFloatData() {_flags.set(TO_MASK32(IsFloatData));}
   void resetFloatData() {_flags.reset(TO_MASK32(IsFloatData));}

   bool hasLiveXMMRegisters() {return _flags.testAll(TO_MASK32(HasLiveXMMRegisters));}
   void setHasLiveXMMRegisters(bool b) {_flags.set(TO_MASK32(HasLiveXMMRegisters), b);}
   void resetHasLiveXMMRegisters() {_flags.reset(TO_MASK32(HasLiveXMMRegisters));}

   protected:

   enum
      {
      IsFloatData = J9::UnresolvedDataSnippet::NextSnippetFlag,
      HasLiveXMMRegisters,
      DoNotPatchMainline,
      NextSnippetFlag
      };

   static_assert((int32_t)NextSnippetFlag <= (int32_t)J9::UnresolvedDataSnippet::MaxSnippetFlag,
      "J9::X86::UnresolvedDataSnippet too many flag bits for flag width");


   private:

   TR::SymbolReference *_glueSymRef;

   uint8_t _numLiveX87Registers;

   };

}

}

#endif

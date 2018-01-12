/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef J9_INSTRUCTIONBASE_INCL
#define J9_INSTRUCTIONBASE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_INSTRUCTION_CONNECTOR
#define J9_INSTRUCTION_CONNECTOR
   namespace J9 { class Instruction; }
   namespace J9 { typedef J9::Instruction InstructionConnector; }
#endif

#include "codegen/OMRInstruction.hpp"

class TR_BitVector;
class TR_GCStackMap;
namespace TR { class CodeGenerator; }
namespace TR { class Node; }

namespace J9
{

class OMR_EXTENSIBLE Instruction : public OMR::InstructionConnector
   {
   protected:

   Instruction(
      TR::CodeGenerator *cg,
      TR::InstOpCode::Mnemonic op,
      TR::Node *node = 0);

   Instruction(
      TR::CodeGenerator *cg,
      TR::Instruction *precedingInstruction,
      TR::InstOpCode::Mnemonic op,
      TR::Node *node = 0);


   public:

   typedef uint32_t TCollectableReferenceMask;

   bool needsGCMap() { return (_index & TO_MASK(NeedsGCMapBit)) != 0; }
   void setNeedsGCMap(TCollectableReferenceMask regMask = 0xFFFFFFFF) { _gc._GCRegisterMask = regMask; _index |= TO_MASK(NeedsGCMapBit); }
   bool needsAOTRelocation() { return (_index & TO_MASK(NeedsAOTRelocation)) != 0; }
   void setNeedsAOTRelocation(bool v = true) { v ? _index |= TO_MASK(NeedsAOTRelocation) : _index &= ~TO_MASK(NeedsAOTRelocation); }

   TR_GCStackMap *getGCMap() { return _gc._GCMap; }
   TR_GCStackMap *setGCMap(TR_GCStackMap *map) { return (_gc._GCMap = map); }
   TCollectableReferenceMask getGCRegisterMask() { return _gc._GCRegisterMask; }

   TR_BitVector *getLiveLocals() { return _liveLocals; }
   TR_BitVector *setLiveLocals(TR_BitVector *v) { return (_liveLocals = v); }
   TR_BitVector *getLiveMonitors() { return _liveMonitors; }
   TR_BitVector *setLiveMonitors(TR_BitVector *v) { return (_liveMonitors = v); }

   int32_t getRegisterSaveDescription() { return _registerSaveDescription; }
   int32_t setRegisterSaveDescription(int32_t v) { return (_registerSaveDescription = v); }

   private:

   TR_BitVector *_liveLocals;
   TR_BitVector *_liveMonitors;
   int32_t _registerSaveDescription;

   union TR_GCInfo
      {
      TCollectableReferenceMask _GCRegisterMask;
      TR_GCStackMap *_GCMap;
      TR_GCInfo() { _GCMap = 0; } // Clear the largest member, thus clearing the whole union
      } _gc;

   };

}

#endif /* TRJ9_INSTRUCTIONBASE_INCL */

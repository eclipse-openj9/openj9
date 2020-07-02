/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#ifndef J9_ARM64_MEMORY_REFERENCE_INCL
#define J9_ARM64_MEMORY_REFERENCE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_MEMORY_REFERENCE_CONNECTOR
#define J9_MEMORY_REFERENCE_CONNECTOR

namespace J9 { namespace ARM64 { class MemoryReference; } }
namespace J9 { typedef J9::ARM64::MemoryReference MemoryReferenceConnector; }
#else
#error J9::ARM64::MemoryReference expected to be a primary connector, but a J9 connector is already defined
#endif

#include "codegen/OMRMemoryReference.hpp"

namespace J9
{

namespace ARM64
{

class OMR_EXTENSIBLE MemoryReference : public OMR::MemoryReferenceConnector
   {
public:
   TR_ALLOC(TR_Memory::MemoryReference)

   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(TR::CodeGenerator *cg)
      : OMR::MemoryReferenceConnector(cg) {}

   /**
    * @brief Constructor
    * @param[in] br : base register
    * @param[in] ir : index register
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(
         TR::Register *br,
         TR::Register *ir,
         TR::CodeGenerator *cg)
      : OMR::MemoryReferenceConnector(br, ir, cg) {}

   /**
    * @brief Constructor
    * @param[in] br : base register
    * @param[in] ir : index register
    * @param[in] scale : scale of index
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(
         TR::Register *br,
         TR::Register *ir,
         uint8_t scale,
         TR::CodeGenerator *cg)
      : OMR::MemoryReferenceConnector(br, ir, scale, cg) {}

   /**
    * @brief Constructor
    * @param[in] br : base register
    * @param[in] disp : displacement
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(
         TR::Register *br,
         int32_t disp,
         TR::CodeGenerator *cg)
      : OMR::MemoryReferenceConnector(br, disp, cg) {}

   /**
    * @brief Constructor
    * @param[in] node : load or store node
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(TR::Node *node, TR::CodeGenerator *cg);

   /**
    * @brief Constructor
    * @param[in] node : node
    * @param[in] symRef : symbol reference
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(TR::Node *node, TR::SymbolReference *symRef, TR::CodeGenerator *cg);

   /**
    * @brief Adjustment for resolution
    * @param[in] cg : CodeGenerator
    */
   void adjustForResolution(TR::CodeGenerator *cg);

   /**
    * @brief Assigns registers
    * @param[in] currentInstruction : current instruction
    * @param[in] cg : CodeGenerator
    */
   void assignRegisters(TR::Instruction *currentInstruction, TR::CodeGenerator *cg);

   /**
    * @brief Estimates the length of generated binary
    * @param[in] op : opcode of the instruction to attach this memory reference to
    * @return estimated binary length
    */
   uint32_t estimateBinaryLength(TR::InstOpCode op);

   /**
    * @brief Generates binary encoding
    * @param[in] ci : current instruction
    * @param[in] cursor : instruction cursor
    * @param[in] cg : CodeGenerator
    * @return instruction cursor after encoding
    */
   uint8_t *generateBinaryEncoding(TR::Instruction *ci, uint8_t *cursor, TR::CodeGenerator *cg);
   };

} // ARM64

} // J9

#endif

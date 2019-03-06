/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef ARM64AOTRELOCATION_INCL
#define ARM64AOTRELOCATION_INCL

#include <stdint.h>
#include "codegen/Relocation.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }

namespace TR {

class ARM64Relocation
   {
   public:
   //TR_ALLOC(TR_Memory::ARM64Relocation)

   /**
    * @brief Constructor
    */
   ARM64Relocation(TR::Instruction *src,
                   uint8_t *trg,
                   TR_ExternalRelocationTargetKind k):
      _srcInstruction(src), _relTarget(trg), _kind(k)
      {}

   /**
    * @brief Answers source instruction
    * @return source instruction
    */
   TR::Instruction *getSourceInstruction() { return _srcInstruction; }
   /**
    * @brief Sets source instruction
    * @param[in] i : source instruction
    */
   void setSourceInstruction(TR::Instruction *i) { _srcInstruction = i; }

   /**
    * @brief Answers relocation target
    * @return relocation target
    */
   uint8_t *getRelocationTarget() { return _relTarget; }
   /**
    * @brief Sets relocation target
    * @param[in] t : relocation target
    */
   void setRelocationTarget(uint8_t *t) { _relTarget = t; }

   /**
    * @brief Answers relocation target kind
    * @return relocation target kind
    */
   TR_ExternalRelocationTargetKind getKind() { return _kind; }
   /**
    * @brief Sets relocation target kind
    * @param[in] k : relocation target kind
    */
   void setKind(TR_ExternalRelocationTargetKind k) { _kind = k; }

   /**
    * @brief Maps relocation
    * @param[in] cg : CodeGenerator
    */
   virtual void mapRelocation(TR::CodeGenerator *cg) = 0;

   private:
   TR::Instruction *_srcInstruction;
   uint8_t *_relTarget;
   TR_ExternalRelocationTargetKind _kind;
   };

} // TR

#endif

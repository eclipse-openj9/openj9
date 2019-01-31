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

#ifndef J9AOTRELOCATION_INCL
#define J9AOTRELOCATION_INCL

#include "codegen/Relocation.hpp"

#include "codegen/Instruction.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }

namespace TR {

class S390Relocation
   {
   public:
   TR_ALLOC(TR_Memory::S390Relocation)

   S390Relocation(TR::Instruction *src,
                  uint8_t           *trg,
       TR_ExternalRelocationTargetKind k):
      _srcInstruction(src), _relTarget(trg), _kind(k)
      {}

   TR::Instruction *getSourceInstruction() {return _srcInstruction;}
   void setSourceInstruction(TR::Instruction *i) {_srcInstruction = i;}

   uint8_t *getRelocationTarget() {return _relTarget;}
   void setRelocationTarget(uint8_t *t) {_relTarget = t;}

   TR_ExternalRelocationTargetKind getKind() {return _kind;}
   void setKind(TR_ExternalRelocationTargetKind k) {_kind = k;}

   virtual void mapRelocation(TR::CodeGenerator *cg) = 0;

   private:
   TR::Instruction              *_srcInstruction;
   uint8_t                         *_relTarget;
   TR_ExternalRelocationTargetKind  _kind;
   };

class S390PairedRelocation: public TR::S390Relocation
   {
   public:
   S390PairedRelocation(TR::Instruction *src1,
                        TR::Instruction *src2,
                        uint8_t           *trg,
                        TR_ExternalRelocationTargetKind k) :
      TR::S390Relocation(src1, trg, k), _src2Instruction(src2)
      {}

   TR::Instruction *getSource2Instruction() {return _src2Instruction;}
   void setSource2Instruction(TR::Instruction *src) {_src2Instruction = src;}

   virtual void mapRelocation(TR::CodeGenerator *cg);

   private:
   TR::Instruction              *_src2Instruction;
   };

class S390EncodingRelocation
   {
   public:
   TR_ALLOC(TR_Memory::S390EncodingRelocation)
   S390EncodingRelocation(uint32_t             rt,
                          TR::SymbolReference  *sr,
                          uintptr_t inlinedSiteIndex = -1)
      : _reloType(rt),
        _symbolReference(sr),
        _inlinedSiteIndex(inlinedSiteIndex) {}

   uint32_t                      _reloType;
   TR::SymbolReference*           _symbolReference;
   uintptr_t                     _inlinedSiteIndex;

   TR::SymbolReference* getSymbolReference() { return _symbolReference;}
   TR::SymbolReference* setSymbolReference(TR::SymbolReference* sr)
      {
      return _symbolReference = sr;
      }

   uint32_t            getReloType() { return _reloType;}
   uint32_t            setReloType(uint32_t rt)
      {
      return _reloType = rt;
      }

   uintptr_t            getInlinedSiteIndex() { return _inlinedSiteIndex;}
   uintptr_t            setInlinedSiteIndex(uintptr_t index)
      {
      return _inlinedSiteIndex = index;
      }

   virtual void addRelocation(TR::CodeGenerator *codeGen, uint8_t *cursor, char* file, uintptr_t line, TR::Node* node);
   };

}

#endif

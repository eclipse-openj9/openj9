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

#ifndef J9PPCWATCHEDSTATICFIELDSNIPPET_INCL
#define J9PPCWATCHEDSTATICFIELDSNIPPET_INCL

#include "codegen/J9WatchedStaticFieldSnippet.hpp"
#include "codegen/Instruction.hpp"


namespace TR {

class J9PPCWatchedStaticFieldSnippet: public TR::J9WatchedStaticFieldSnippet
   {
   private:
   TR::Instruction *_upperInstruction, *_lowerInstruction;
   int32_t _tocOffset;
   bool isloaded;

   public:
   J9PPCWatchedStaticFieldSnippet(TR::CodeGenerator *cg, TR::Node *node, J9Method *m, UDATA loc, void *fieldAddress, J9Class *fieldClass)
      : TR::J9WatchedStaticFieldSnippet(cg, node, m, loc, fieldAddress, fieldClass), _upperInstruction(NULL), _lowerInstruction(NULL), isloaded(false) {}
    
   int32_t getTOCOffset() {return _tocOffset;}
   void setTOCOffset(int32_t offset) {_tocOffset = offset;}

   TR::Instruction *getUpperInstruction() {return _upperInstruction;}
   TR::Instruction *getLowerInstruction() {return _lowerInstruction;}

   void setUpperInstruction(TR::Instruction *pi) {_upperInstruction = pi;}
   void setLowerInstruction(TR::Instruction *pi) {_lowerInstruction = pi;}

   void setLoadSnippet() {isloaded = true;}
   bool isSnippetLoaded() {return isloaded;}

   virtual uint8_t *emitSnippetBody();
   };
}

#endif

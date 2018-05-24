/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef PPCINTERFACECASTSNIPPET_INCL
#define PPCINTERFACECASTSNIPPET_INCL

#include "codegen/Snippet.hpp"
#include "p/codegen/PPCInstruction.hpp"

namespace TR {

class PPCInterfaceCastSnippet : public TR::Snippet
   {
   TR::Instruction     *_upperInstruction, *_lowerInstruction;
   TR::LabelSymbol         *_restartLabel;
   TR::LabelSymbol         *_trueLabel;
   TR::LabelSymbol         *_falseLabel;
   TR::LabelSymbol         *_doneLabel;
   TR::LabelSymbol         *_callLabel;
   bool                  _testCastClassIsSuper;
   bool                  _checkCast;
   bool                  _needsResult;
   int32_t               _offsetClazz;
   int32_t               _offsetCastClassCache;

   public:

   PPCInterfaceCastSnippet(TR::CodeGenerator *cg, TR::Node *n, TR::LabelSymbol *restartLabel, TR::LabelSymbol *snippetLabel, TR::LabelSymbol *trueLabel, TR::LabelSymbol *falseLabel, TR::LabelSymbol *doneLabel, TR::LabelSymbol *callLabel, bool testCastClassIsSuper, bool checkcast, int32_t offsetClazz, int32_t offsetCastClassCache, bool needsResult);

   bool isCheckCast() { return _checkCast;};

   bool getTestCastClassIsSuper()                     {return _testCastClassIsSuper;}
   bool getNeedsResult()                              {return _needsResult;}

   TR::LabelSymbol *getDoneLabel()                     {return _doneLabel;}
   TR::LabelSymbol *getTrueLabel()                     {return _trueLabel;}
   TR::LabelSymbol *getFalseLabel()                    {return _falseLabel;}
   TR::LabelSymbol *getReStartLabel()                  {return _restartLabel;}
   TR::LabelSymbol *setReStartLabel(TR::LabelSymbol *l) {return (_restartLabel = l);}

   TR::Instruction *getUpperInstruction() {return _upperInstruction;}
   TR::Instruction *setUpperInstruction(TR::Instruction *pi) { return (_upperInstruction = pi);}

   TR::Instruction *getLowerInstruction() {return _lowerInstruction;}
   TR::Instruction *setLowerInstruction(TR::Instruction *pi) { return (_lowerInstruction = pi);}

   virtual Kind getKind() { return IsInterfaceCastSnippet; }
   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

}

#endif


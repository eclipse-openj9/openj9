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

#ifndef S390STACKCHECKFAILURESNIPPET_INCL
#define S390STACKCHECKFAILURESNIPPET_INCL

#include "codegen/Snippet.hpp"

namespace TR {

class S390StackCheckFailureSnippet : public TR::Snippet
   {
   TR::LabelSymbol           *_reStartLabel;
   TR::SymbolReference      *_destination;
   uint32_t                _frameSize;
   uint16_t                _argSize;

   public:

   S390StackCheckFailureSnippet(TR::CodeGenerator        *cg,
                                TR::Node                 *node,
                                TR::LabelSymbol           *restartlab,
                                TR::LabelSymbol           *snippetlab,
                                TR::SymbolReference      *helper,
                                uint32_t                 frameSize);

   virtual Kind getKind() { return IsStackCheckFailure; }

   int16_t getSizeOfArguments()          {return _argSize;}
   int16_t setSizeOfArguments(int32_t s) {return _argSize = s;}     

   TR::SymbolReference *getDestination()             {return _destination;}
   TR::SymbolReference *setDestination(TR::SymbolReference *s) {return _destination = s;}

   TR::LabelSymbol *getReStartLabel()                  {return _reStartLabel;}
   TR::LabelSymbol *setReStartLabel(TR::LabelSymbol *l) {return _reStartLabel = l;}

   void setStackAtlasHelper();

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t);
   uint32_t getFrameSize()             {return _frameSize;}
   };

}

#endif

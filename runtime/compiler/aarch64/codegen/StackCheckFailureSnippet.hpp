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

#ifndef ARM64STACKCHECKFAILURESNIPPET_INCL
#define ARM64STACKCHECKFAILURESNIPPET_INCL

#include "codegen/Snippet.hpp"

#include <stdint.h>

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace TR {

class ARM64StackCheckFailureSnippet : public TR::Snippet
   {
   TR::LabelSymbol *reStartLabel;

   public:

   ARM64StackCheckFailureSnippet(TR::CodeGenerator *cg,
                                 TR::Node *node,
                                 TR::LabelSymbol *restartlab,
                                 TR::LabelSymbol *snippetlab)
      : TR::Snippet(cg, node, snippetlab), reStartLabel(restartlab)
      {
      }

   /**
    * @brief Answers the Snippet kind
    * @return Snippet kind
    */
   virtual Kind getKind() { return IsStackCheckFailure; }

   /**
    * @brief Answers the restart label
    * @return restart label
    */
   TR::LabelSymbol *getReStartLabel() { return reStartLabel; }
   /**
    * @brief Sets the restart label
    * @return restart label
    */
   TR::LabelSymbol *setReStartLabel(TR::LabelSymbol *l) { return (reStartLabel = l); }

   /**
    * @brief Emits the Snippet body
    * @return instruction cursor
    */
   virtual uint8_t *emitSnippetBody();

   /**
    * @brief Answers the Snippet length
    * @return Snippet length
    */
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

}

#endif

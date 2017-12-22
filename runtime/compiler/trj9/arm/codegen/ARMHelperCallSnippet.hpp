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

#ifndef ARMHELPERCALLSNIPPET_INCL
#define ARMHELPERCALLSNIPPET_INCL

#include "codegen/Snippet.hpp"

#include <stdint.h>
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "il/SymbolReference.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace TR {

class ARMHelperCallSnippet : public TR::Snippet
   {
   TR::SymbolReference      *_destination;
   TR::LabelSymbol          *_restartLabel;

   public:

   ARMHelperCallSnippet(TR::CodeGenerator    *cg,
                           TR::Node             *node,
                           TR::LabelSymbol      *snippetlab,
                           TR::SymbolReference  *helper,
                           TR::LabelSymbol      *restartLabel=NULL)
      : TR::Snippet(cg, node, snippetlab, (restartLabel!=NULL && helper->canCauseGC())),
        _destination(helper),
        _restartLabel(restartLabel)
      {
      }

   virtual Kind getKind() { return IsHelperCall; }

   TR::SymbolReference *getDestination()             {return _destination;}
   TR::SymbolReference *setDestination(TR::SymbolReference *s) {return (_destination = s);}

   TR::LabelSymbol      *getRestartLabel() {return _restartLabel;}
   TR::LabelSymbol      *setRestartLabel(TR::LabelSymbol *l) {return (_restartLabel=l);}

   uint8_t *genHelperCall(uint8_t *cursor);
   uint32_t getHelperCallLength();

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

}

#endif

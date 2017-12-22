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

#ifndef S390FORCERECOMPILATIONSNIPPET_INCL
#define S390FORCERECOMPILATIONSNIPPET_INCL

#include "codegen/Snippet.hpp"
#include "z/codegen/ConstantDataSnippet.hpp"
#include "codegen/CodeGenerator.hpp"

namespace TR { class LabelSymbol; }

namespace TR {

class S390ForceRecompilationDataSnippet : public TR::S390ConstantDataSnippet
   {
   // Label of Return Address in Main Line Code.
   TR::LabelSymbol *_restartLabel;

   public:

   S390ForceRecompilationDataSnippet(TR::CodeGenerator *,
                                     TR::Node *,
                                     TR::LabelSymbol *);

   virtual Kind getKind() { return IsForceRecompData; }

   TR::LabelSymbol *getRestartLabel()                  {return _restartLabel;}
   TR::LabelSymbol *setRestartLabel(TR::LabelSymbol *l) {return _restartLabel = l;}

   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };


class S390ForceRecompilationSnippet : public TR::Snippet
   {
   // Label of Return Address in Main Line Code.
   TR::LabelSymbol *_restartLabel;
   TR::S390ForceRecompilationDataSnippet *_dataSnippet;
   public:

   S390ForceRecompilationSnippet(TR::CodeGenerator        *cg,
                                 TR::Node                 *node,
                                 TR::LabelSymbol           *restartlab,
                                 TR::LabelSymbol           *snippetlab)
      : TR::Snippet(cg, node, snippetlab, false),
        _restartLabel(restartlab)
      {
      _dataSnippet = new (cg->trHeapMemory()) TR::S390ForceRecompilationDataSnippet(cg,node,restartlab);
      cg->addDataConstantSnippet(_dataSnippet);
      }

   virtual Kind getKind() { return IsForceRecomp; }

   TR::LabelSymbol *getRestartLabel()                  {return _restartLabel;}
   TR::LabelSymbol *setRestartLabel(TR::LabelSymbol *l) {return _restartLabel = l;}

   TR::S390ForceRecompilationDataSnippet *getDataConstantSnippet() { return _dataSnippet; }
   TR::S390ForceRecompilationDataSnippet *setDataConstantSnippet(TR::S390ForceRecompilationDataSnippet *snippet)
      {
      return _dataSnippet = snippet;
      }

   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t);
   };

}

#endif

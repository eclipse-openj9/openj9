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

#ifndef J9_SNIPPET_INCL
#define J9_SNIPPET_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_SNIPPET_CONNECTOR
#define J9_SNIPPET_CONNECTOR
namespace J9 { class Snippet; }
namespace J9 { typedef J9::Snippet SnippetConnector; }
#endif

#include "codegen/OMRSnippet.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace J9
{
   class OMR_EXTENSIBLE Snippet : public OMR::SnippetConnector
      {
      public:

      Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label, bool isGCSafePoint) :
         OMR::SnippetConnector(cg, node, label, isGCSafePoint)
         {
         }

      Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label) :
         OMR::SnippetConnector(cg, node, label)
         {
         }

      virtual uint8_t *emitSnippet();
      };
}

#endif

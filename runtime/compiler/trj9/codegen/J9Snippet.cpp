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

#include "codegen/Snippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "il/Block.hpp"

uint8_t *
J9::Snippet::emitSnippet()
   {
   uint32_t startOffset = self()->cg()->getBinaryBufferCursor() - self()->cg()->getCodeStart();
   uint8_t *endPC = self()->emitSnippetBody();

   if (self()->needsExceptionTableEntry() && _block && !_block->getExceptionSuccessors().empty())
      {
      uint32_t endOffset = endPC - self()->cg()->getCodeStart();
      _block->addExceptionRangeForSnippet(startOffset, endOffset);
      }

   return endPC;
   }


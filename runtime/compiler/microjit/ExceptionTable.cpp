/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#include <stdint.h>
#include "microjit/ExceptionTable.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"

class TR_ResolvedMethod;

// TODO: MicroJIT: This should create an exception table using only information MicroJIT has
MJIT::ExceptionTableEntryIterator::ExceptionTableEntryIterator(TR::Compilation *comp)
   : _compilation(comp)
   {
   int32_t i = 1;
   _tableEntries = (TR_Array<List<TR_ExceptionTableEntry> > *)comp->trMemory()->allocateHeapMemory(sizeof(TR_Array<List<TR_ExceptionTableEntry> >)*i);
   for (int32_t j = 0; j < i; ++j)
      _tableEntries[j].init(comp->trMemory());

   // lookup catch blocks
   //

   // for each exception block:
   //    create exception ranges from the list of exception predecessors

   }

// TODO: MicroJIT: This should create an exception table entry using only information MicroJIT has
void
MJIT::ExceptionTableEntryIterator::addSnippetRanges(
      List<TR_ExceptionTableEntry> & tableEntries,
      TR::Block *snippetBlock,
      TR::Block *catchBlock,
      uint32_t catchType,
      TR_ResolvedMethod *method,
      TR::Compilation *comp)
   {
   // TODO: MicroJIT: Create a snippet for each exception and add it to the correct place
   }

TR_ExceptionTableEntry *
MJIT::ExceptionTableEntryIterator::getFirst()
   {
   return NULL;
   }

TR_ExceptionTableEntry *
MJIT::ExceptionTableEntryIterator::getNext()
   {
   return NULL;
   }

uint32_t
MJIT::ExceptionTableEntryIterator::size()
   {
   return 0;
   }

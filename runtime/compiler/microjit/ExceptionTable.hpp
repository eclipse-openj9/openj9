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

#ifndef MJIT_EXCEPTIONTABLE_INCL
#define MJIT_EXCEPTIONTABLE_INCL

#include <stdint.h>
#include "infra/Array.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/List.hpp"
#include "env/ExceptionTable.hpp"

class TR_ResolvedMethod;
namespace TR { class Block; }
namespace TR { class Compilation; }
template <class T> class TR_Array;

namespace MJIT
{

struct ExceptionTableEntryIterator
   {
   ExceptionTableEntryIterator(TR::Compilation *comp);

   TR_ExceptionTableEntry *getFirst();
   TR_ExceptionTableEntry *getNext();

   uint32_t size();

   private:
      void addSnippetRanges(List<TR_ExceptionTableEntry> &, TR::Block *, TR::Block *, uint32_t, TR_ResolvedMethod *, TR::Compilation *);

   TR::Compilation *                         _compilation;
   TR_Array<List<TR_ExceptionTableEntry> > * _tableEntries;
   ListIterator<TR_ExceptionTableEntry>      _entryIterator;
   int32_t                                   _inlineDepth;
   uint32_t                                  _handlerIndex;
   };

} // namespace MJIT

#endif /* MJIT_EXCEPTIONTABLE_INCL */

/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#include <map>
#include <functional>
#include <utility>
#include "compile/Compilation.hpp"
#include "env/Region.hpp"
#include "il/Node.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Stack.hpp"

namespace TR { class Block; }
namespace TR { class CFGNode; }
namespace TR { class SymbolReference; }

namespace J9
{

enum MonitorInBlock
   {
   NoMonitor = 0,
   MonitorEnter,
   MonitorExit
   };


class SetMonitorStateOnBlockEntry
   {
public:
   typedef TR::typed_allocator<std::pair<int32_t const, TR_Stack<TR::SymbolReference *>*>, TR::Region&> LiveMonitorStacksAllocator;
   typedef std::less<int32_t> LiveMonitorStacksComparator;
   typedef std::map<int32_t, TR_Stack<TR::SymbolReference *>*, LiveMonitorStacksComparator, LiveMonitorStacksAllocator> LiveMonitorStacks;

   SetMonitorStateOnBlockEntry(TR::Compilation * c, LiveMonitorStacks *liveMonitorStacks)
      : _blocksToVisit(c->trMemory(), 8, false, stackAlloc)
      {
      _comp = c;
      _visitCount = c->incVisitCount();
      _liveMonitorStacks = liveMonitorStacks;
      }

   void set(bool& lmmdFailed, bool traceIt = false);

private:
   int32_t addSuccessors(TR::CFGNode * cfgNode, TR_Stack<TR::SymbolReference *> *, bool traceIt, bool dontPropagateMonitor = false, MonitorInBlock monitorType = NoMonitor, int32_t callerIndex = -1, bool walkOnlyExceptionSuccs = false);
   bool isMonitorStateConsistentForBlock(TR::Block *block, TR_Stack<TR::SymbolReference *> *newMonitorStack, bool popMonitor);

   TR_Memory *trMemory() { return comp()->trMemory(); }
   TR_HeapMemory trHeapMemory() { return trMemory(); }

   TR::Compilation *comp() { return _comp; }

   TR::Compilation *_comp;
   vcount_t _visitCount;
   TR_Stack<TR::Block *> _blocksToVisit;
   LiveMonitorStacks *_liveMonitorStacks;
   };

}

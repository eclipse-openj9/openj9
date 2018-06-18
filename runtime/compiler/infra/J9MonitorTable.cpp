/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "infra/MonitorTable.hpp"
#include "infra/Monitor.hpp"
#include "infra/CriticalSection.hpp"
#include "control/CompilationRuntime.hpp"

#include "j9.h"
#include "j9cfg.h"
#include "j9thread.h"
#include "j9port.h"

extern TR::Monitor *memoryAllocMonitor;

// allocated during codert_onload
TR::MonitorTable *
J9::MonitorTable::init(
      J9PortLibrary *portLib,
      J9JavaVM *javaVM)
   {
   if (_instance)
      {
      return _instance;
      }

   PORT_ACCESS_FROM_PORT(portLib);
   void *tableMem = j9mem_allocate_memory(sizeof(TR::MonitorTable), J9MEM_CATEGORY_JIT);
   if (!tableMem) return 0;
   TR::MonitorTable *table = new (tableMem) TR::MonitorTable();

   table->_portLib = portLib;
   table->_monitors.setFirst(0);

   // Initialize the Monitors
   if (!table->_tableMonitor.init      ("JIT-MonitorTableMonitor")) return 0;
   if (!table->_j9MemoryAllocMonitor.init("JIT-MemoryAllocMonitor")) return 0;
   if (!table->_j9ScratchMemoryPoolMonitor.init("JIT-ScratchMemoryPoolMonitor")) return 0;
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
   if (!table->_classUnloadMonitor.initFromVMMutex(javaVM->classUnloadMutex)) return 0;
#else
   if (!table->_classUnloadMonitor.init("JIT-ClassUnloadMonitor")) return 0;
#endif
   if (!table->_iprofilerPersistenceMonitor.init("JIT-IProfilerPersistenceMonitor")) return 0;

   // Setup a wrapper for VM's monitors that the JIT can acquire
   if (!table->_classTableMutex.initFromVMMutex(javaVM->classTableMutex)) return 0;

   memoryAllocMonitor = table->_memoryAllocMonitor = &table->_j9MemoryAllocMonitor;
   table->_scratchMemoryPoolMonitor = &table->_j9ScratchMemoryPoolMonitor; // export this value

   // Allocate and initialize the array of classUnloadMonitorHolders
   table->_classUnloadMonitorHolders = (int32_t*)j9mem_allocate_memory(sizeof(*(table->_classUnloadMonitorHolders))*MAX_TOTAL_COMP_THREADS, J9MEM_CATEGORY_JIT);
   if (!table->_classUnloadMonitorHolders)
      return 0;
   for (int32_t i = 0; i < MAX_TOTAL_COMP_THREADS; i++)
      table->_classUnloadMonitorHolders[i] = 0;

   OMR::MonitorTable::_instance = table;
   return table;
   }


TR::Monitor *
J9::MonitorTable::create(char *name)
   {
   PORT_ACCESS_FROM_PORT(_portLib);

   void *monMem = j9mem_allocate_memory(sizeof(TR::Monitor), J9MEM_CATEGORY_JIT);
   if (!monMem)
      {
      return 0;
      }

   TR::Monitor *m = new (monMem) TR::Monitor();
   if (!m->init(name))
      {
      return 0;
      }

   //fprintf(stderr, "ADDING MONITOR: %p, %s (first is %p)\n", m, name, _monitors.getFirst());
   self()->insert(m);
   return m;
   }


void
J9::MonitorTable::insert(TR::Monitor *monitor)
   {
   OMR::CriticalSection insertingNewMonitor(&_tableMonitor);
   monitor->setNext(_monitors.getFirst());
   _monitors.setFirst(monitor);
   }


void
J9::MonitorTable::removeAndDestroy(TR::Monitor *monitor)
   {
   TR::MonitorTable *table = TR::MonitorTable::get();
   if (!table) return;
   PORT_ACCESS_FROM_PORT(table->_portLib);
   // search the table for my monitor
   {
   OMR::CriticalSection searchTableAndRemoveMonitor(&_tableMonitor);
   TR::Monitor *prevMonitor = NULL;
   TR::Monitor *crtMonitor = (TR::Monitor *)table->_monitors.getFirst();
   while (crtMonitor)
      {
      if (crtMonitor == monitor)
         break;
      prevMonitor = crtMonitor;
      crtMonitor = crtMonitor->getNext();
      }
   if (crtMonitor)
      {
      // I found my monitor, so take it out from the link list
      if (prevMonitor)
         prevMonitor->setNext(crtMonitor->getNext());
      else
         table->_monitors.setFirst(crtMonitor->getNext());
      crtMonitor->destroy();
      // free the memory
      j9mem_free_memory(monitor);
      }
   }
   }


// Note that the following method does not destroys the actual monitors
// If we do, we must make sure that the monitors created from VM mutex are not destroyed
void
J9::MonitorTable::free()
   {
   TR::MonitorTable *table = TR::MonitorTable::get();
   if (!table) return;

   PORT_ACCESS_FROM_PORT(table->_portLib);

   TR::Monitor *monitor = (TR::Monitor *)table->_monitors.getFirst();
   while (monitor)
      {
      TR::Monitor *next = monitor->getNext();
      j9mem_free_memory(monitor);
      monitor = next;
      }
   j9mem_free_memory(table->_classUnloadMonitorHolders);
   _instance = 0;
   j9mem_free_memory(table);
   };


// Used to check if current thread holds any known monitors
bool
J9::MonitorTable::isThreadInSafeMonitorState(J9VMThread *vmThread)
   {
   // If we hold any of the following locks, return failure.
   if (_tableMonitor.owned_by_self()               ||
       _j9MemoryAllocMonitor.owned_by_self()       ||
       _j9ScratchMemoryPoolMonitor.owned_by_self() ||
       _classTableMutex.owned_by_self()            ||
       _iprofilerPersistenceMonitor.owned_by_self()
       )
      return false;
   {
   OMR::CriticalSection walkingMonitorTable(&_tableMonitor);
   for (TR::Monitor *monitor = (TR::Monitor *)_monitors.getFirst(); monitor; monitor = monitor->getNext())
      {
      if (monitor->owned_by_self())
         {
         return false;
         }
      }
   return true;
   }
   }


// Return the address of the first monitor that is found locked
TR::Monitor *
J9::MonitorTable::monitorHeldByCurrentThread()
   {
   // If we hold any of the following locks, return failure.
   if (_tableMonitor.owned_by_self())
      return &_tableMonitor;
   if (_j9MemoryAllocMonitor.owned_by_self())
      return &_j9MemoryAllocMonitor;
   if (_j9ScratchMemoryPoolMonitor.owned_by_self())
      return &_j9ScratchMemoryPoolMonitor;
   if (_classTableMutex.owned_by_self())
      return &_classTableMutex;
   if (_iprofilerPersistenceMonitor.owned_by_self())
      return &_iprofilerPersistenceMonitor;
   {
   OMR::CriticalSection walkMonitorTable(&_tableMonitor);
   for (TR::Monitor *monitor = (TR::Monitor *)_monitors.getFirst(); monitor; monitor = monitor->getNext())
      {
      if (monitor->owned_by_self())
         {
         return monitor;
         }
      }
   }
   return NULL;
   }


int32_t
J9::MonitorTable::readAcquireClassUnloadMonitor(int32_t compThreadIndex)
   {
   _classUnloadMonitor.enter_read();
   // Need to determine the identity of the compilation thread
   TR_ASSERT(compThreadIndex < MAX_TOTAL_COMP_THREADS, "compThreadIndex is too high");
   return ++(_classUnloadMonitorHolders[compThreadIndex]);
   }


int32_t
J9::MonitorTable::readReleaseClassUnloadMonitor(int32_t compThreadIndex)
   {
   TR_ASSERT(compThreadIndex < MAX_TOTAL_COMP_THREADS, "compThreadIndex is too high");
   if (_classUnloadMonitorHolders[compThreadIndex] > 0)
      {
      _classUnloadMonitorHolders[compThreadIndex]--;
      _classUnloadMonitor.exit_read();
      return _classUnloadMonitorHolders[compThreadIndex];
      }
   else
      {
      TR_ASSERT(false, "comp thread %d does not have classUnloadMonitor", compThreadIndex);
      return -1; // could not release monitor
      }
   }

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

#ifndef TRJ9_MONITORTABLE_INCL
#define TRJ9_MONITORTABLE_INCL

#ifndef TRJ9_MONITORTABLE_CONNECTOR
#define TRJ9_MONITORTABLE_CONNECTOR
namespace J9 { class MonitorTable; }
namespace J9 { typedef MonitorTable MonitorTableConnector; }
#endif


#include "infra/OMRMonitorTable.hpp"
#include "infra/Link.hpp"
#include "infra/Monitor.hpp"
#include "infra/RWMonitor.hpp"

class TR_DebugExt;
struct J9PortLibrary;
struct J9JavaVM;
struct J9VMThread;
namespace TR { class MonitorTable; }

namespace J9
{

class OMR_EXTENSIBLE MonitorTable : public OMR::MonitorTableConnector
   {
   friend class ::TR_DebugExt;

   public:

   void *operator new(size_t size, void *p) {return p;}

   static TR::MonitorTable *init(J9PortLibrary *portLib, J9JavaVM *javaVM);

   void free();

   J9::RWMonitor *getClassUnloadMonitor() { return &_classUnloadMonitor; }
   TR::Monitor *getClassTableMutex() { return &_classTableMutex; }
   TR::Monitor *getIProfilerPersistenceMonitor() { return &_iprofilerPersistenceMonitor; }
   void removeAndDestroy(TR::Monitor *monitor);

   bool isThreadInSafeMonitorState(J9VMThread *vmThread);
   TR::Monitor *monitorHeldByCurrentThread();
   int32_t readAcquireClassUnloadMonitor(int32_t compThreadIndex);
   int32_t readReleaseClassUnloadMonitor(int32_t compThreadIndex);
   int32_t getClassUnloadMonitorHoldCount(int32_t i) const { return _classUnloadMonitorHolders[i]; }


   private:

   friend class TR::Monitor;
   friend class J9::Monitor;
   friend class J9::RWMonitor;
   friend class TR::MonitorTable;

   TR::Monitor *create(char *name);
   void insert(TR::Monitor *monitor);

   J9PortLibrary *_portLib;
   TR_LinkHead0<TR::Monitor> _monitors;

   TR::Monitor _tableMonitor;
   TR::Monitor _j9MemoryAllocMonitor;
   TR::Monitor _j9ScratchMemoryPoolMonitor;
   J9::RWMonitor _classUnloadMonitor;
   TR::Monitor _classTableMutex;  // JavaVM's class table mutex
   TR::Monitor _iprofilerPersistenceMonitor;

   // To detect which thread has locked the classUnloadMonitor we will keep an array of integers
   // Each entry corresponds to a determined compilation thread. This way we avoid any
   // concurrency issues. Each number represents how many times the compilation thread has entered
   // the classUnloadmonitor. Normally it should not be more than 1
   //
   int32_t *_classUnloadMonitorHolders;
   };

}

#endif

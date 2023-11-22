/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef J9_MONITOR_INCL
#define J9_MONITOR_INCL

#ifndef J9_MONITOR_CONNECTOR
#define J9_MONITOR_CONNECTOR
namespace J9 { class Monitor; }
namespace J9 { typedef J9::Monitor MonitorConnector; }
#endif

#include "env/TRMemory.hpp"
#include "infra/Link.hpp"

struct J9PortLibrary;
struct J9ThreadMonitor;
struct J9JavaVM;
struct J9VMThread;
namespace TR { class MonitorTable; }
namespace TR { class Monitor; }
namespace J9 { class MonitorTable; }

namespace J9
{

class Monitor : public TR_Link0<TR::Monitor>
   {
   public:

   static TR::Monitor *create(const char *name);
   static void destroy(TR::Monitor *monitor);

   void enter();

   int32_t try_enter();

   int32_t exit();

   void destroy();

   void wait();

   intptr_t wait_timed(int64_t millis, int32_t nanos);

   void notify();

   void notifyAll();

   int32_t num_waiting();

   int32_t owned_by_self(); // returns 1 if current thread owns the monitor, 0 otherwise

   // Dangerous: do not use this routine, except for thread exit
   void *getVMMonitor() { return (void*)_monitor; }

   private:

   friend class J9::MonitorTable;

   void *operator new(size_t size, void *p) { return p; }
   void operator delete(void *p);
   void operator delete(void *p, void *) {}

   bool init(const char *name);

   bool initFromVMMutex(void *mutex);

   J9ThreadMonitor *_monitor;
   };

}

#endif

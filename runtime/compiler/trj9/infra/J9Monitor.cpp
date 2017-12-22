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

#include "infra/J9Monitor.hpp"

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "j9thread.h"
#include "infra/Monitor.hpp"
#include "infra/MonitorTable.hpp"

TR::MonitorTable *OMR::MonitorTable::_instance = 0;

TR::Monitor *memoryAllocMonitor = NULL;

TR::Monitor *
J9::Monitor::create(char *name)
   {
   return TR::MonitorTable::get()->create(name);
   }

void
J9::Monitor::destroy(TR::Monitor *monitor)
   {
   // The monitor will be destroyed when the monitor table is destroyed
   }

bool
J9::Monitor::init(char *name)
   {
   setNext(0);
   if (j9thread_monitor_init_with_name((J9ThreadMonitor**)&_monitor, 0, name))
      return false;
   else
      return true;
   }

bool
J9::Monitor::initFromVMMutex(void *mutex)
   {
   _monitor = (J9ThreadMonitor*)mutex;
   return true;
   }

void
J9::Monitor::enter()
   {
   TR_ASSERT(_monitor != TR::MonitorTable::get()->getClassTableMutex()->getVMMonitor(), "Use TR::ClassTableCriticalSection instead");
   j9thread_monitor_enter(_monitor);
   }

void
J9::Monitor::destroy()
   {
   j9thread_monitor_destroy(_monitor);
   }  //FIXME: remove from the table as well

void
J9::Monitor::wait()
   {
   j9thread_monitor_wait(_monitor);
   }

intptr_t
J9::Monitor::wait_timed(int64_t millis, int32_t nanos)
   {
   return j9thread_monitor_wait_timed(_monitor, millis, nanos);
   }

void
J9::Monitor::notify()
   {
   j9thread_monitor_notify(_monitor);
   }

void
J9::Monitor::notifyAll()
   {
   j9thread_monitor_notify_all(_monitor);
   }

int32_t
J9::Monitor::exit()
   {
   return (int32_t)j9thread_monitor_exit(_monitor);
   }

int32_t
J9::Monitor::try_enter()
   {
   return (int32_t)j9thread_monitor_try_enter(_monitor);
   }

int32_t
J9::Monitor::num_waiting()
   {
   return (int32_t)j9thread_monitor_num_waiting(_monitor);
   }

int32_t
J9::Monitor::owned_by_self()
   {
   return (int32_t)j9thread_monitor_owned_by_self(_monitor);
   }

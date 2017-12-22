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

#include "infra/RWMonitor.hpp"
#include "j9cfg.h"
#include "j9thread.h"
#include "j9port.h"

#ifdef J9VM_JIT_CLASS_UNLOAD_RWMONITOR
void
J9::RWMonitor::enter_read()
   {
   j9thread_rwmutex_enter_read(_monitor);
   }


void
J9::RWMonitor::enter_write()
   {
   j9thread_rwmutex_enter_write(_monitor);
   }


void
J9::RWMonitor::exit_read()
   {
   j9thread_rwmutex_exit_read(_monitor);
   }


void
J9::RWMonitor::exit_write()
   {
   j9thread_rwmutex_exit_write(_monitor);
   }


void
J9::RWMonitor::destroy()
   {
   j9thread_rwmutex_destroy(_monitor);
   }


bool
J9::RWMonitor::init(char *name)
   {
   return j9thread_rwmutex_init(&_monitor, 0, name) == 0 ? true : false;
   }


bool
J9::RWMonitor::initFromVMMutex(void *mutex)
   {
   _monitor = (RWMutex*)mutex;
   return true;
   }

#else

void
J9::RWMonitor::enter_read()
   {
   j9thread_monitor_enter(_monitor);
   }


void
J9::RWMonitor::enter_write()
   {
   j9thread_monitor_enter(_monitor);
   }


void
J9::RWMonitor::exit_read()
   {
   j9thread_monitor_exit(_monitor);
   }


void
J9::RWMonitor::exit_write()
   {
   j9thread_monitor_exit(_monitor);
   }


void
J9::RWMonitor::destroy()
   {
   j9thread_monitor_destroy(_monitor);
   }


bool
J9::RWMonitor::init(char *name)
   {
   return j9thread_monitor_init_with_name((J9ThreadMonitor**)&_monitor, 0, name) == 0 ? true : false;
   }


bool
J9::RWMonitor::initFromVMMutex(void *mutex)
   {
   _monitor = (J9ThreadMonitor*)mutex; return true;
   }

#endif

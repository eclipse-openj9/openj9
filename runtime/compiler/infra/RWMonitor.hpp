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

#ifndef TRJ9_RWMONITOR_INCL
#define TRJ9_RWMONITOR_INCL

#include "j9cfg.h"

struct J9ThreadMonitor;
struct RWMutex;

namespace J9
{

class MonitorTable;

// This is not going to be linked to other monitors for now

class RWMonitor
   {
   public:

   void enter_read();
   void enter_write();
   void exit_read();
   void exit_write();
   void destroy();
   void *getVMMonitor() {return (void*)_monitor;}

   private:

   friend class J9::MonitorTable;
   bool init(char *name);
   bool initFromVMMutex(void *mutex);

#ifdef J9VM_JIT_CLASS_UNLOAD_RWMONITOR
   RWMutex *_monitor;
#else
   J9ThreadMonitor *_monitor;
#endif
   };

}

#endif

/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#include "runtime/CompileService.hpp"
#include "infra/CriticalSection.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/MethodToBeCompiled.hpp"

// Routine called when a new connection request has been received at the server
// Executed by the listener thread
void J9CompileDispatcher::compile(JITServer::ServerStream *stream)
   {
   TR::CompilationInfo * compInfo = getCompilationInfo(_jitConfig);

   TR_MethodToBeCompiled *entry = NULL;
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Server received request for stream %p", stream);
      {
      // Grab the compilation monitor to queue this entry and notify a compilation thread
      OMR::CriticalSection compilationMonitorLock(compInfo->getCompilationMonitor());
      if (compInfo->addOutOfProcessMethodToBeCompiled(stream))
         {
         // successfully queued the new entry, so notify a thread
         compInfo->getCompilationMonitor()->notifyAll();
         return;
         }
      } // end critical section
   // If we reached this point there was a memory allocation failure
   stream->writeError(compilationLowPhysicalMemory);
   }

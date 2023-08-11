/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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

#include "runtime/CompileService.hpp"
#include "infra/CriticalSection.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "env/VerboseLog.hpp"

// Routine called when a new connection request has been received at the server
// Executed by the listener thread
void J9CompileDispatcher::compile(JITServer::ServerStream *stream)
   {
   TR::CompilationInfo * compInfo = getCompilationInfo(_jitConfig);
   TR_MethodToBeCompiled *entry = NULL;
   bool disableFurtherCompilation = false;
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Server received request for stream %p", stream);
      {
      // Grab the compilation monitor to queue this entry and notify a compilation thread
      OMR::CriticalSection compilationMonitorLock(compInfo->getCompilationMonitor());
      if (!(disableFurtherCompilation = compInfo->getPersistentInfo()->getDisableFurtherCompilation()))
         {
         if (compInfo->addOutOfProcessMethodToBeCompiled(stream))
            {
            // successfully queued the new entry, so notify a thread
            compInfo->getCompilationMonitor()->notifyAll();
            return;
            }
         }
      } // end critical section
      
   // If we reached this point, either there was a memory allocation failure
   // or compilations are disabled
   if (disableFurtherCompilation)
      {
      // Server disabled further compilations but client sent a compilation request anyway.
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Server rejected compilation request for stream %p because compilations are disabled", stream);
      stream->writeError(compilationStreamFailure);
      }
   else
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Server rejected compilation request for stream %p because of lack of memory", stream);
      stream->writeError(compilationLowPhysicalMemory, (uint64_t) JITServer::ServerMemoryState::VERY_LOW);
      }
   }

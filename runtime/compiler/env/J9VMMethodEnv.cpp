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

#define J9_EXTERNAL_TO_VM

#include "env/CompilerEnv.hpp"
#include "env/VMMethodEnv.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "control/JITaaSCompilationThread.hpp"
#include "j9.h"
#include "j9cfg.h"
#include "jilconsts.h"
#include "j9protos.h"

J9ROMMethod *romMethodOfRamMethod(J9Method* method)
   {
   if (!TR::CompilationInfo::getStream()) // If not JITaaS server
      return J9_ROM_METHOD_FROM_RAM_METHOD((J9Method *)method);

   // else, JITaaS
   auto clientData = TR::compInfoPT->getClientData();
   J9ROMMethod *romMethod = nullptr;

   // check if the method is already cached
      {
      OMR::CriticalSection romCache(clientData->getROMMapMonitor());
      auto &map = clientData->getJ9MethodMap();
      auto it = map.find((J9Method*) method);
      if (it != map.end())
         romMethod = it->second._romMethod;
      }

   // if not, go cache the associated ROM class and get the ROM method from it
   if (!romMethod)
      {
      auto stream = TR::CompilationInfo::getStream();
      stream->write(JITaaS::J9ServerMessageType::VM_getClassOfMethod, (TR_OpaqueMethodBlock*) method);
      J9Class *clazz = (J9Class*) std::get<0>(stream->read<TR_OpaqueClassBlock *>());
      TR::compInfoPT->getAndCacheRemoteROMClass(clazz);
         {
         OMR::CriticalSection romCache(clientData->getROMMapMonitor());
         auto &map = clientData->getJ9MethodMap();
         auto it = map.find((J9Method *) method);
         if (it != map.end())
            romMethod = it->second._romMethod;
         }
      }
   TR_ASSERT(romMethod, "Should have acquired romMethod");
   return romMethod;
   }


bool
J9::VMMethodEnv::hasBackwardBranches(TR_OpaqueMethodBlock *method)
   {
   J9ROMMethod * romMethod = romMethodOfRamMethod((J9Method *)method);
   return (romMethod->modifiers & J9AccMethodHasBackwardBranches) != 0;
   }


bool
J9::VMMethodEnv::isCompiledMethod(TR_OpaqueMethodBlock *method)
   {
   if (TR::Compiler->isCodeTossed())
      {
      return false;
      }

   return TR::CompilationInfo::isCompiled((J9Method *)method);
   }


uintptr_t
J9::VMMethodEnv::startPC(TR_OpaqueMethodBlock *method)
   {
   J9Method *j9method = reinterpret_cast<J9Method *>(method);
   return reinterpret_cast<uintptr_t>(TR::CompilationInfo::getJ9MethodStartPC(j9method));
   }

uintptr_t
J9::VMMethodEnv::bytecodeStart(TR_OpaqueMethodBlock *method)
   {
   J9ROMMethod *romMethod = romMethodOfRamMethod((J9Method*) method);
   return (uintptr_t)(J9_BYTECODE_START_FROM_ROM_METHOD(romMethod));
   }


uint32_t
J9::VMMethodEnv::bytecodeSize(TR_OpaqueMethodBlock *method)
   {
   J9ROMMethod *romMethod = romMethodOfRamMethod((J9Method*) method);
   return (uint32_t)(J9_BYTECODE_END_FROM_ROM_METHOD(romMethod) -
                     J9_BYTECODE_START_FROM_ROM_METHOD(romMethod));
   }


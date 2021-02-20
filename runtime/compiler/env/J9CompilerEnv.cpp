/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#include "env/CompilerEnv.hpp"
#include "env/RawAllocator.hpp"
#include "env/defines.h"
#include "env/CPU.hpp"
#include "j9.h"
#if defined(J9VM_OPT_JITSERVER)
#include "control/CompilationRuntime.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */

J9::CompilerEnv::CompilerEnv(J9JavaVM *vm, TR::RawAllocator raw, const TR::PersistentAllocatorKit &persistentAllocatorKit) :
#if defined(TR_HOST_ARM) || defined(TR_HOST_ARM64)
   OMR::CompilerEnvConnector(raw, persistentAllocatorKit),
#else
   OMR::CompilerEnvConnector(raw, persistentAllocatorKit, OMRPORT_FROM_J9PORT(vm->portLibrary)),
#endif
   portLib(vm->portLibrary),
   javaVM(vm)
   {
   }

void
J9::CompilerEnv::initializeTargetEnvironment()
   {
   OMR::CompilerEnvConnector::initializeTargetEnvironment();
   }

/**
 * \brief Determines whether methods are compiled and the generated code simply 
 *        "tossed" without execution.
 * \return true if compiles have been requested to be tossed; false otherwise.
 */
bool
J9::CompilerEnv::isCodeTossed()
   {
   J9JITConfig *jitConfig = javaVM->jitConfig;

   if (jitConfig == NULL)
      {
      return false;
      }

   if ((jitConfig->runtimeFlags & J9JIT_TOSS_CODE) ||
       (jitConfig->runtimeFlags & J9JIT_TESTMODE))
      {
      return true;
      }

   return false;
   }

TR::PersistentAllocator &
J9::CompilerEnv::persistentAllocator()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (J9::PersistentInfo::_remoteCompilationMode == JITServer::SERVER)
      {
      auto compInfoPT = TR::compInfoPT;
      if (compInfoPT && compInfoPT->getPerClientPersistentMemory())
         {
         // Returns per-client allocator after enterPerClientPersistentAllocator() is called
         return compInfoPT->getPerClientPersistentMemory()->_persistentAllocator.get();
         }
      }
#endif
   return OMR::CompilerEnv::persistentAllocator();
   }

TR_PersistentMemory *
J9::CompilerEnv::persistentMemory()
   {
#if defined(J9VM_OPT_JITSERVER)
   if (J9::PersistentInfo::_remoteCompilationMode == JITServer::SERVER)
      {
      auto compInfoPT = TR::compInfoPT;
      if (compInfoPT && compInfoPT->getPerClientPersistentMemory())
         {
         // Returns per-client persistent memory after enterPerClientPersistentAllocator() is called
         return compInfoPT->getPerClientPersistentMemory();
         }
      }
#endif
   return OMR::CompilerEnv::persistentMemory();
   }

#if defined(J9VM_OPT_JITSERVER)
TR::PersistentAllocator &
J9::CompilerEnv::persistentGlobalAllocator()
   {
   return OMR::CompilerEnv::persistentAllocator();
   }
#endif

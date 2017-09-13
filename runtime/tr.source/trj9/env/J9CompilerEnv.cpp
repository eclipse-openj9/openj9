/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "env/CompilerEnv.hpp"
#include "env/RawAllocator.hpp"
#include "env/defines.h"
#include "env/CPU.hpp"
#include "j9.h"

J9::CompilerEnv::CompilerEnv(J9JavaVM *vm, TR::RawAllocator raw, const TR::PersistentAllocatorKit &persistentAllocatorKit) :
   OMR::CompilerEnvConnector(raw, persistentAllocatorKit),
   portLib(vm->portLibrary),
   javaVM(vm)
   {
   }


void
J9::CompilerEnv::initializeTargetEnvironment()
   {

   TR::CPU *cpu = NULL;

   // Host processor bitness
   //
#ifdef TR_TARGET_64BIT
   target.setBitness(TR::bits_64);
#elif TR_TARGET_32BIT
   target.setBitness(TR::bits_32);
#else
   target.setBitness(TR::bits_unknown);
#endif

   // Initialize the target CPU by querying the host processor
   //
   target.cpu.initializeByHostQuery();

   // Host major operating system
   //
#if HOST_OS == OMR_LINUX
   target.setMajorOS(TR::os_linux);
#elif HOST_OS == OMR_AIX
   target.setMajorOS(TR::os_aix);
#elif HOST_OS == OMR_WINDOWS
   target.setMajorOS(TR::os_windows);
#elif HOST_OS == OMR_ZOS
   target.setMajorOS(TR::os_zos);
#else
   target.setMajorOS(TR::os_unknown);
#endif

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

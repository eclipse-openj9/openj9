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

#include "env/RawAllocator.hpp"

#include <memory.h>
#include <stdint.h>
#include "j9.h"
#include "j9jitnls.h"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "control/CompilationThread.hpp"
#include "env/J9JitMemory.hpp"

J9JITConfig *jitConfig;


TR_PersistentMemory * initializePersistentMemory(J9JITConfig * jitConfig)
   {
   TR_PersistentMemory * persistentMemory = (TR_PersistentMemory *)jitConfig->scratchSegment;
   if (!persistentMemory)
      {
      TR::RawAllocator rawAllocator(jitConfig->javaVM);
      try
         {
         persistentMemory = new (rawAllocator) TR_PersistentMemory(
            jitConfig,
            TR::Compiler->persistentAllocator()
            );
         ::trPersistentMemory = persistentMemory;
         jitConfig->scratchSegment = pointer_cast<J9MemorySegment *>(persistentMemory);
         }
      catch (const std::exception &e)
         {
         }
      }
   return persistentMemory;
   }



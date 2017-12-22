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

#ifndef LMGUARDEDSTORAGE_HPP
#define LMGUARDEDSTORAGE_HPP

#include <stdint.h>
#include "j9.h"
#include "compile/Compilation.hpp"
#include "env/jittypes.h"

#define J9PORT_GS_ENABLED ((uint32_t) 0x1)
#define J9PORT_GS_INITIALIZED ((uint32_t) 0x2)

#define IS_THREAD_GS_INITIALIZED(vmThread) ((vmThread->gsParameters.flags & J9PORT_GS_INITIALIZED) != 0)
#define IS_THREAD_GS_ENABLED(vmThread) ((vmThread->gsParameters.flags & J9PORT_GS_ENABLED) != 0)

/**
 * Guarded Storage Base Class. Each platform that supports Guarded Storage extends this
 * for platform specific purposes. This class is used to encapsulate the code
 * that is common between the different platforms.
 */
class TR_LMGuardedStorage
   {
public:

   /**
    * Constructor.
    * @param jitConfig the J9JITConfig
    */
   TR_LMGuardedStorage (J9JITConfig *jitConfig);

   /**
    * Pure virtual method to initialize guarded storage on given app thread.
    * @param vmThread The VM thread to initialize guarded storage.
    * @return true if initialization is successful; false otherwise.
    */
   virtual bool initializeThread(J9VMThread *vmThread) {return false;}

   /**
    * Pure virtual method to deinitialize guarded storage on given app thread
    * @param vmThread The VM thread to deinitialize guarded storage.
    * @return true if deinitialization is successful; false otherwise.
    */
   virtual bool deinitializeThread(J9VMThread *vmThread) {return false;}

   // JIT Config Cache
   J9JITConfig                    *_jitConfig;

   // TR_Compilation cache
   TR::CompilationInfo             *_compInfo;
   };
#endif


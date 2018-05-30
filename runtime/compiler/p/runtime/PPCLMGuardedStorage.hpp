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

#ifndef PPCLMGUARDEDSTORAGE_INCL
#define PPCLMGUARDEDSTORAGE_INCL

#include "runtime/LMGuardedStorage.hpp"

#include <stdint.h>
#include "env/jittypes.h"

class TR_J9VMBase;
struct TR_PPCHWProfilerEBBContext;
int32_t lmEventHandler(TR_PPCHWProfilerEBBContext *context);

class TR_PPCLMGuardedStorage : public TR_LMGuardedStorage
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::PPCLMGuardedStorage);
   
   /**
    * Constructor.
    * @param jitConfig the J9JITConfig
    */
   TR_PPCLMGuardedStorage(J9JITConfig *jitConfig);
   
   
   // --------------------------------------------------------------------------------------
   // HW Profiler Management Methods
   
   /**
    * Static method used to allocate the HW Profiler
    * @param jitConfig The J9JITConfig
    * @return pointer to the HWPRofiler
    */
   static TR_PPCLMGuardedStorage* allocate(J9JITConfig *jitConfig, bool ebbSetupDone);
   
   /**
    * Initialize hardware profiling on given app thread.
    * @param vmThread The VM thread to initialize profiling.
    * @return true if initialization is successful; false otherwise.
    */
   virtual bool initializeThread(J9VMThread *vmThread);
   
   /**
    * Deinitialize hardware profiling on given app thread
    * @param vmThread The VM thread to deinitialize profiling.
    * @return true if deinitialization is successful; false otherwise.
    */
   virtual bool deinitializeThread(J9VMThread *vmThread);
   };
#endif /* PPCHWPROFILER_INCL */


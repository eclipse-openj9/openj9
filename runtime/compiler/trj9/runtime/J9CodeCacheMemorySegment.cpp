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

#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include "j9.h"
#include "j9protos.h"
#include "j9thread.h"
#include "jitprotos.h"
#include "j9cp.h"
#define J9_EXTERNAL_TO_VM
#include "runtime/J9VMAccess.hpp"
#include "vmaccess.h"
#include "infra/Monitor.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "codegen/FrontEnd.hpp"
#ifdef CODECACHE_STATS
#include "infra/Statistics.hpp"
#endif
#include "env/ut_j9jit.h"
#include "infra/CriticalSection.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/CodeCacheMemorySegment.hpp"
#include "env/VMJ9.h"
#include "runtime/ArtifactManager.hpp"
#include "env/IO.hpp"


// CodeCacheMemorySegment
//
J9::CodeCacheMemorySegment::CodeCacheMemorySegment(J9MemorySegment *segment) :
   OMR::CodeCacheMemorySegmentConnector(segment->heapAlloc, segment->heapTop),
   _segment(segment)
   { }


void
J9::CodeCacheMemorySegment::free(TR::CodeCacheManager *manager)
   {
   J9JavaVM *javaVM = manager->javaVM();
   javaVM->internalVMFunctions->freeMemorySegment(javaVM, _segment, 1);
   new (reinterpret_cast<TR::CodeCacheMemorySegment *>(this)) TR::CodeCacheMemorySegment();
   }

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

#include <stddef.h>                   // for NULL
#include <stdint.h>                   // for int32_t, int64_t, uint32_t
#include <assert.h>
#include "compile/Compilation.hpp"
#include "env/DebugEnv.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"             // for uintptrj_t, intptrj_t
#include "env/VMJ9.h"
#include "infra/Assert.hpp"           // for TR_ASSERT
#include "thrtypes.h"


#if defined(LINUX) || defined(AIXPPC)
#include <signal.h>
#endif

void
J9::DebugEnv::breakPoint()
   {

#if defined(LINUX) || defined(AIXPPC)
   raise(SIGTRAP);
#elif defined(_MSC_VER)
   DebugBreak();
#else
   assert(0);
#endif

   }


char *
J9::DebugEnv::extraAssertMessage(TR::Compilation *comp)
   {
   return comp->fej9()->printAdditionalInfoOnAssertionFailure(comp);
   }

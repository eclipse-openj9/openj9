/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "ras/InternalFunctions.hpp"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "env/defines.h"
#include "env/IO.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"


TR_Memory *
TR_InternalFunctions::trMemory()
   {
   TR_ASSERT(_trMemory, "assertion failure");
   return _trMemory;
   }


TR_PersistentMemory *
TR_InternalFunctions::persistentMemory()
   {
   TR_ASSERT(_persistentMemory, "assertion failure");
   return _persistentMemory;
   }


void
TR_InternalFunctions::fprintf(TR::FILE *file, const char *format, ...)
   {
   va_list args;
   va_start(args, format);
   TR::IO::vfprintf(file, format, args);
   va_end(args);
   }

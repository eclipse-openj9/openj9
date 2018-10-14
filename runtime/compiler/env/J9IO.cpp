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

#include <stdio.h>
#include <stdarg.h>
#include "env/IO.hpp"
#include "env/VMJ9.h"
#include "infra/Assert.hpp"

TR::FILE *
J9::IO::fopen(char * fileName, const char * mode, bool encrypt)
   {
   return j9jit_fopen((char*)fileName, mode, false, encrypt);
   }


void
J9::IO::fclose(TR::FILE *fileId)
   {
   j9jit_fclose(fileId);
   }


void
J9::IO::fseek(TR::FILE *fileId, intptr_t offset, int32_t whence)
   {
   ::fseek(fileId->_stream, (long)offset, whence);
   }


long
J9::IO::ftell(TR::FILE *fileId)
   {
   return ::ftell(fileId->_stream);
   }


void
J9::IO::fflush(TR::FILE *fileId)
   {
   j9jit_fflush(fileId);
   }


int32_t
J9::IO::fprintf(TR::FILE *fileId, const char * format, ...)
   {
   va_list args;
   va_start(args, format);
   int32_t length = j9jit_vfprintf(fileId, (char*)format, args);
   va_end(args);
   return length;
   }


int32_t
J9::IO::vfprintf(TR::FILE *fileId, const char *format, va_list args)
   {
   return j9jit_vfprintf(fileId, (char*)format, args);
   }

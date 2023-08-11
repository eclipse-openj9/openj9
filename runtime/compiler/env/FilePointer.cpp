/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(J9ZOS390)
extern "C"
{
#include "atoe.h"
}
#undef fwrite
#endif

#include "env/FilePointer.hpp"
#include "j9.h"
#include "env/IO.hpp"

extern char *feGetEnv(const char *);

namespace TR
{

FILE TR::FilePointer::_null   = FilePointer(NULL);
FILE TR::FilePointer::_stdin  = FilePointer(stdin);
FILE TR::FilePointer::_stdout = FilePointer(stdout);
FILE TR::FilePointer::_stderr = FilePointer(stderr);


FilePointer::FilePointer(::FILE *stream)
   {
   initialize(stream);
   }


void
FilePointer::initialize(::FILE *stream)
   {
   _stream  = stream;
   _useJ9IO = false;
   }


void
FilePointer::initialize(J9PortLibrary *portLib, int32_t fileId)
   {
   PORT_ACCESS_FROM_PORT(portLib);
   _fileId  = fileId;
   _useJ9IO = true;
   }


int32_t
FilePointer::write(J9PortLibrary *portLib, char *buf, int32_t length)
   {
   PORT_ACCESS_FROM_PORT(portLib);
   if (length > 0)
      {
      if (_useJ9IO)
         {
         return j9file_write(_fileId, buf, length);
         }
      else
         length = fwrite(buf, 1, length, _stream);
      }

   return length;
   }


void
FilePointer::close(J9PortLibrary *portLib)
   {
   PORT_ACCESS_FROM_PORT(portLib);
   if (_useJ9IO)
      {
      flush(portLib);
      j9file_sync(_fileId);
      j9file_close(_fileId);
      }
   else
      {
      fclose(_stream);
      }
   }


void
FilePointer::flush(J9PortLibrary *portLib)
   {
   PORT_ACCESS_FROM_PORT(portLib);
   if (!_useJ9IO)
      fflush(_stream);
   }

}

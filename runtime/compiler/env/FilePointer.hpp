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

#ifndef TR_FILEPOINTER_INCL
#define TR_FILEPOINTER_INCL

#include <stdio.h>
#include <stdint.h>
#include "env/FilePointerDecl.hpp"

extern "C" { struct J9PortLibrary; }

namespace TR
{
   struct FilePointer
      {

      FilePointer(::FILE *stream);

      void initialize(J9PortLibrary *portLib, int32_t fileId);
      void initialize(::FILE *stream);

      int32_t write(J9PortLibrary *portLib, char *buf, int32_t length);

      void close(J9PortLibrary *portLib);

      void flush(J9PortLibrary *portLib);

      union
         {
         int32_t _fileId;
         ::FILE *_stream;
         };

      static FILE *Null()   { return &_null; }
      static FILE *Stdin()  { return &_stdin; }
      static FILE *Stdout() { return &_stdout; }
      static FILE *Stderr() { return &_stderr; }

      private:

      static FILE _null;
      static FILE _stdin;
      static FILE _stdout;
      static FILE _stderr;

      void swap();
      uint8_t cipher(uint8_t c);

      bool _useJ9IO;
      };

}


#endif

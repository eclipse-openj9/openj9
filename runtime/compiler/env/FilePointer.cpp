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

const int32_t TR::FilePointer::BUFFER_SIZE = 0;

namespace TR
{

FILE TR::FilePointer::_null   = FilePointer(NULL);
FILE TR::FilePointer::_stdin  = FilePointer(stdin);
FILE TR::FilePointer::_stdout = FilePointer(stdout);
FILE TR::FilePointer::_stderr = FilePointer(stderr);


FilePointer::FilePointer(::FILE *stream)
   {
   initialize(stream, false);
   }


void
FilePointer::initialize(::FILE *stream, bool encrypt)
   {
   _stream  = stream;
   _useJ9IO = false;
   _buffer  = 0;

   initialize(encrypt);
   }


void
FilePointer::initialize(J9PortLibrary *portLib, int32_t fileId, bool encrypt)
   {
   PORT_ACCESS_FROM_PORT(portLib);
   _fileId  = fileId;
   _useJ9IO = true;
   if (BUFFER_SIZE)
      _buffer  = (uint8_t*)j9mem_allocate_memory(BUFFER_SIZE, J9MEM_CATEGORY_JIT);
   else
      _buffer = 0;

   initialize(encrypt);
   }


void
FilePointer::initialize(bool enc)
   {
   _pos = 0;
   _encrypt = enc;
   _i = 0;
   _j = 0;
   if (enc)
      {
      // key - morphed simply by swapping the nibbles of each char
      // to avoid plaintext in the binary
      //
      static const uint8_t key[]
         = { 0xa6, 0x93, 0xa6, 0x96, 0x47, 0xc6, 0x03, 0x76, 0x66, 0x13, 0xc6, 0x33};
      static const int32_t keyLen = sizeof(key);

      for (_i = 0; _i < 256; ++_i)
         _s[_i] = _i;
      for (_i = 0; _i < 256; ++_i)
         {
         uint8_t c = key[_i % keyLen];
         c = (c>>4)|((c&0x0f)<<4);
         _j = (_j + _s[_i] + c) % 256;
         swap();
         }

      _i = _j = 0;
      }
   }


int32_t
FilePointer::write(J9PortLibrary *portLib, char *buf, int32_t length)
   {
   PORT_ACCESS_FROM_PORT(portLib);
   if (length > 0)
      {
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
      static char *code  = feGetEnv("TR_CodedLogs");
#else
      bool code = true;
#endif
      if (_encrypt && code)
         encrypt(buf, length);

      if (_useJ9IO)
         {
         if ((BUFFER_SIZE == 0) ||
             (_pos == 0 && length > BUFFER_SIZE))
            return j9file_write(_fileId, buf, length);

         int32_t available = BUFFER_SIZE - _pos;

         if (length < available) available = length;

         if (available > 0)
            {
            memcpy(_buffer+_pos, buf, available);
            _pos += available;
            }

         if (_pos == BUFFER_SIZE)
            {
            j9file_write(_fileId, _buffer, BUFFER_SIZE);
            _pos = 0;

            if (length > available)
               {
               int32_t offset = available;
               available = length - available;
               if (available >= BUFFER_SIZE)
                  j9file_write(_fileId, buf+offset, available);
               else
                  {
                  memcpy(_buffer+_pos, buf+offset, available);
                  _pos += available;
                  }
               }
            }
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
      if (_buffer)
         j9mem_free_memory(_buffer);
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
   if (_useJ9IO)
      {
      if (_pos > 0)
         {
         j9file_write(_fileId, _buffer, _pos);
         _pos = 0;
         }
      }
   else
      fflush(_stream);
   }


uint8_t
FilePointer::cipher(uint8_t data)
   {
   _i = (_i+1) % 256;
   _j = (_j+_s[_i]) % 256;
   swap();
   return _s[(_s[_i] + _s[_j]) % 256] ^ data;
   }


void
FilePointer::swap()
   {
   uint8_t t = _s[_i];
   _s[_i] = _s[_j];
   _s[_j] = t;
   }


void
FilePointer::encrypt(char *buf, int32_t len)
   {
   // FIXME: unroll and convert to integer operations to speed this up
   for (int32_t x = 0; x < len; ++x)
      buf[x] = cipher(buf[x]);
   }

}

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
extern "C"{
#include "atoe.h"
}
#undef fwrite
#endif

#include "j9.h"
#include "j9protos.h"
#include "j9list.h"
#include "j9port_generated.h"
#include "jitprotos.h"
#include "control/CompilationRuntime.hpp"
#include "env/VMJ9.h"
#include "env/IO.hpp"
#include "env/CompilerEnv.hpp"

extern char *feGetEnv(const char *);


extern "C" {

I_64 j9jit_time_current_time_millis()
   {
   I_64 time_millis;
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);

   time_millis = j9time_usec_clock();

   return time_millis;
   }


I_32 j9jit_fseek(I_32 fd, I_32 whence)
   {
   I_32 fileId=0;
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);

   j9file_seek(fd,0,whence);
   return fileId;
   }


I_32 j9jit_fread(I_32 fd, void * buf, IDATA nbytes)
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   I_32 fileId;
   I_32 bytesRead;

   bytesRead = j9file_read(fd, buf, nbytes);

   return bytesRead;
   }


I_32 j9jit_fmove(char * pathExist, char * pathNew)
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   I_32 fileId;

   if(j9file_unlink(pathNew))
      j9tty_printf(privatePortLibrary, "Non-Fatal Error: Unable to delete file (%s)\n", pathNew);
   fileId = j9file_move(pathExist, pathNew);
   if (fileId == -1)
      j9tty_printf(privatePortLibrary, "Non-Fatal Error: Unable to rename file (%s)\n", pathExist);
   return fileId;
   }


I_32 j9jit_fopen_existing(char *fileName)
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   I_32 fileId;

   fileId = j9file_open(fileName, EsOpenWrite | EsOpenRead | EsOpenAppend, 0660);
   if (fileId == -1)
      j9tty_printf(privatePortLibrary, "Non-Fatal Error: Unable to open file (%s)\n", fileName);
   return fileId;
   }


void j9jit_fcloseId(I_32 fileId)
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   if (fileId != -1)
      j9file_close(fileId);
   }


I_32 j9jit_fopenName(char *fileName)
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   I_32 fileId;

   j9file_unlink(fileName);
   fileId = j9file_open(fileName, EsOpenRead | EsOpenWrite | EsOpenCreate , 0660);
   if (fileId == -1)
      j9tty_printf(privatePortLibrary, "Non-Fatal Error: Unable to open file (%s)\n", fileName);
   return fileId;
   }


TR::FILE *
j9jit_fopen(char *fileName, const char *mode, bool useJ9IO, bool encrypt)
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   TR::FILE *pFile;

   if (useJ9IO)
      {
      I_32 fileId;

      // FIXME: this will nuke the file if it was being opened for reading!!
      j9file_unlink(fileName);
      fileId = j9file_open(fileName, EsOpenRead | EsOpenWrite | EsOpenCreate , 0660);
      if (fileId == -1)
         {
         j9tty_printf(privatePortLibrary, "Non-Fatal Error: Unable to open file (%s)\n", fileName);
         return NULL;
         }

      pFile = (TR::FILE *)j9mem_allocate_memory(sizeof(TR::FILE), J9MEM_CATEGORY_JIT);
      if (!pFile)
         {
         j9tty_printf(privatePortLibrary, "Non-Fatal Error: Unable to open file (%s)\n", fileName);
         return NULL;
         }
      pFile->initialize(privatePortLibrary, fileId, encrypt);
      }
   else
      {
      FILE *f = fopen(fileName, mode);
      if (!f)
         {
         j9tty_printf(privatePortLibrary, "Non-Fatal Error: Unable to open file (%s)\n", fileName);
         return NULL;
         }

      pFile = (TR::FILE *)j9mem_allocate_memory(sizeof(TR::FILE), J9MEM_CATEGORY_JIT);
      if (!pFile)
         {
         j9tty_printf(privatePortLibrary, "Non-Fatal Error: Unable to open file (%s)\n", fileName);
         return NULL;
         }

      pFile->initialize(f, encrypt);
      }

   return pFile;
   }


void j9jit_fclose(TR::FILE *pFile)
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   if (pFile && pFile != TR::IO::Stdout && pFile != TR::IO::Stderr)
      {
      pFile->close(privatePortLibrary);
      j9mem_free_memory(pFile);
      }
   }


void j9jit_fflush(TR::FILE *pFile)
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   if (pFile)
      {
      if (pFile == TR::IO::Stdout || pFile == TR::IO::Stderr)
         ;
      else
         pFile->flush(privatePortLibrary);
      }
   }


void j9jit_lock_vlog(void *voidConfig)
   {
   J9JITConfig *config = (J9JITConfig *) voidConfig;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   compInfo->vlogAcquire();
   }


void j9jit_unlock_vlog(void *voidConfig)
   {
   J9JITConfig *config = (J9JITConfig *) voidConfig;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   compInfo->vlogRelease();
   }


void j9jitrt_lock_log(void *voidConfig)
   {
   J9JITConfig *config = (J9JITConfig *) voidConfig;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   compInfo->rtlogAcquire();
   }


void j9jitrt_unlock_log(void *voidConfig)
   {
   J9JITConfig *config = (J9JITConfig *) voidConfig;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   compInfo->rtlogRelease();
   }


I_32 j9jit_vfprintfId(I_32 fileId, char *format, ...)
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   char buf[512];
   I_32 length;
   va_list args;
   va_start(args, format);

   length = j9str_vprintf(buf, 512, format, args);
   va_end(args);
   if (fileId != -1)
      {
       char * bufPtr = buf;
#if defined(J9ZOS390)
       bufPtr = a2e(bufPtr,length);
#endif
      if ((U_32)j9file_write(fileId, bufPtr, length) == length)
         {
         static char *forceFlush = feGetEnv("TR_ForceFileFlush");
         if (forceFlush)
            j9file_sync(fileId);
         }
#if defined(J9ZOS390)
      free(bufPtr);
#endif
      }
   else
      j9tty_printf(privatePortLibrary, "%s", buf);
   return length;
   }


I_32 j9jit_fprintfId(I_32 fileId, char *format, ...)
   {
   va_list args;
   va_start(args, format);
   I_32 length=j9jit_vfprintfId(fileId, format, args);
   va_end(args);
   return length;
   }


I_32 j9jit_vfprintf(TR::FILE *pFile, char *format, va_list args)
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   const int32_t BUFSIZE = 640;
   char buffer[BUFSIZE];
   char *buf = buffer;
   I_32 length;
   bool grewBuf = false;

   va_list copyOfArgs;
   memcpy(VA_PTR(copyOfArgs), VA_PTR(args), sizeof(copyOfArgs));
   length = j9str_vprintf(buf, BUFSIZE, format, copyOfArgs);
   va_end(copyOfArgs);

   if (length >= BUFSIZE)
      {
      grewBuf = true;
      buf = (char*)j9mem_allocate_memory(length+1, J9MEM_CATEGORY_JIT);
      if (!buf)
         return length;
      length = j9str_vprintf(buf, length+1, format, args);
      }

   if (!pFile || pFile == TR::IO::Stdout)
      j9tty_printf(privatePortLibrary, "%s", buf);
   else if (pFile == TR::IO::Stderr)
      j9tty_err_printf(privatePortLibrary, "%s", buf);
   else
      {
      char *bufPtr = buf;
#if defined(J9ZOS390)
      bufPtr = a2e(bufPtr, length);
#endif
      if (pFile->write(privatePortLibrary, bufPtr, length) == length)
         {
         static char *forceFlush = feGetEnv("TR_ForceFileFlush");
         if (forceFlush)
            pFile->flush(privatePortLibrary);
         }
#if defined(J9ZOS390)
      free(bufPtr);
#endif
      }
   if (grewBuf)
      j9mem_free_memory(buf);

   return length;
   }


I_32 j9jit_fprintf(TR::FILE *pFile, char *format, ...)
   {
   va_list args;
   va_start(args, format);
   I_32 length = j9jit_vfprintf(pFile, format, args);
   va_end(args);
   return length;
   }


static I_32 vlog_vprintf(J9JITConfig *config, char *format, va_list args)
   {
   return j9jit_vfprintf(((TR_JitPrivateConfig*)config->privateConfig)->vLogFile, format, args);
   }


static I_32 vlog_printf(J9JITConfig *config, char *format, ...)
   {
   va_list args;
   va_start(args, format);
   I_32 length = vlog_vprintf(config, format, args);
   va_end(args);
   return length;
   }


I_32 j9jit_vprintf(void *voidConfig, char *format, va_list args)
   {
   J9JITConfig *config = (J9JITConfig *) voidConfig;
   return vlog_vprintf(config, format, args);
   }


void j9jit_printf(void *voidConfig, char *format, ...)
   {
   va_list args;
   va_start(args, format);
   j9jit_vprintf(voidConfig, format, args);
   va_end(args);
   }


static I_32 rtlog_vprintf(J9JITConfig *config, char *format, va_list args)
   {
   return j9jit_vfprintf(((TR_JitPrivateConfig*)config->privateConfig)->rtLogFile, format, args);
   }


I_32 j9jitrt_printf(void *voidConfig, char *format, ...)
   {
   va_list args;
   va_start(args, format);
   I_32 length=rtlog_vprintf((J9JITConfig *)voidConfig, format, args);
   va_end(args);
   return length;
   }

}


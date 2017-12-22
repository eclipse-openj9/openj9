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

#ifndef J9_IO_INCL
#define J9_IO_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_IO_CONNECTOR
#define J9_IO_CONNECTOR
   namespace J9 { class IO; }
   namespace J9 { typedef J9::IO IOConnector; }
#endif

#include <stdint.h>
#include "infra/Annotations.hpp"
#include "env/OMRIO.hpp"


namespace J9
{

class OMR_EXTENSIBLE IO : public OMR::IOConnector
   {
   public:

   static TR::FILE *fopen(char *fileName, const char *attrs, bool encrypt);
   static void fclose(TR::FILE *fileId);
   static void fseek(TR::FILE *fileId, intptr_t offset, int32_t whence);
   static long ftell(TR::FILE *fileID);
   static void fflush(TR::FILE *fileId);

   static int32_t fprintf(TR::FILE *fileId, const char * format, ...);
   static int32_t vfprintf(TR::FILE *fileId, const char *format, va_list args);
   };

}

#endif

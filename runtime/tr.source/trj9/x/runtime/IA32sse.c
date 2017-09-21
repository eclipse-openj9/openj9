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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "j9comp.h"
#include "j9port.h"

#ifndef TR_CROSS_COMPILING_ONLY

#ifdef WINDOWS
#include <excpt.h>
#else
#include <signal.h>
#endif

#if !defined(WINDOWS) && !defined(J9HAMMER)
static volatile BOOLEAN osSupportsSSE = FALSE;
static void handleSIGILLForSSE(int signal, struct sigcontext context)
   {
   /* ONLY USED BY 32-BIT LINUX
    * The STMXCSR instruction is four bytes long.
    * The instruction pointer must be incremented manually to avoid
    * repeating the exception-throwing instruction.
    */
   context.eip += 4;
   osSupportsSSE = FALSE;
   }
#endif

#endif // TR_CROSS_COMPILING_ONLY

/* Detects if the SSE/SSE2 Instructions are supported by the OS. The lack
 * of such support can only be detected by catching the Illegal Instruction
 * exception (or SIGILL on UNIX), since the CR4 register is only readable at
 * ring 0.
 */
BOOLEAN jitTestOSForSSESupport()
   {
#if defined(TR_CROSS_COMPILING_ONLY)
    /* FIXME: The VM should figure out whether to generate SSE/SSE2 instructions. */
    return FALSE;
#elif defined(J9HAMMER)
   /* 64-Bit OS always supports SSE/SSE2 */
   return TRUE;
#elif defined(WINDOWS)
   __try
      {
      _mm_getcsr();
      return TRUE;
      }
   __except(EXCEPTION_EXECUTE_HANDLER)
      {
      return FALSE;
      }
#else
   /* Use POSIX signals when building on Linux. If an "illegal instruction"
    * signal is encountered, the signal handler will set osSupportsSSE to FALSE.
    */
   struct sigaction oldHandler;
   OMRSIG_SIGACTION(SIGILL, NULL, &oldHandler);
   OMRSIG_SIGNAL(SIGILL, (void (*)(int)) handleSIGILLForSSE);
   osSupportsSSE = TRUE;
   U_32 mxcsr;
   asm("stmxcsr %0"::"m"(mxcsr):);
   OMRSIG_SIGACTION(SIGILL, &oldHandler, NULL);
   return osSupportsSSE;
#endif
   }


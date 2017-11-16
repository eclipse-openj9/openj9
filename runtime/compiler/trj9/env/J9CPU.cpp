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

#pragma csect(CODE,"J9J9CPU#C")
#pragma csect(STATIC,"J9J9CPU#S")
#pragma csect(TEST,"J9J9CPU#T")

#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"
#include "env/VMJ9.h"
#include "j9.h"
#include "j9port.h"


J9ProcessorDesc*
J9::CPU::TO_PORTLIB_getJ9ProcessorDesc()
   {
   static int processorTypeInitialized = 0;
   static J9ProcessorDesc  processorDesc;

   if(processorTypeInitialized == 0)
      {
      PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);

      //Just make sure we initialize the J9ProcessorDesc at least once.
      j9sysinfo_get_processor_description(&processorDesc);

      processorTypeInitialized = 1;
      }
   return &processorDesc;
   }


bool
J9::CPU::getPPCSupportsVMX()
   {
#if defined(J9OS_I5) && defined(J9OS_I5_V5R4)
   return FALSE;
#else
   J9ProcessorDesc   *processorDesc       = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   J9PortLibrary     *privatePortLibrary  = TR::Compiler->portLib;
   BOOLEAN feature = j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_HAS_ALTIVEC);
   return (TRUE == feature);
#endif
   }

bool
J9::CPU::getPPCSupportsVSX()
   {
#if defined(J9OS_I5) && defined(J9OS_I5_V5R4)
   return FALSE;
#else
   J9ProcessorDesc   *processorDesc       = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   J9PortLibrary     *privatePortLibrary  = TR::Compiler->portLib;
   BOOLEAN feature = j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_HAS_VSX);
   return (TRUE == feature);
#endif
   }

bool
J9::CPU::getPPCSupportsAES()
   {
#if defined(J9OS_I5) && defined(J9OS_I5_V5R4)
   return FALSE;
#else
   J9ProcessorDesc   *processorDesc       = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   J9PortLibrary     *privatePortLibrary  = TR::Compiler->portLib;
   BOOLEAN hasVMX  = j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_HAS_ALTIVEC);
   BOOLEAN hasVSX  = j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_HAS_VSX);
   BOOLEAN isP8    = (processorDesc->processor >= PROCESSOR_PPC_P8);
   return (isP8 && hasVMX && hasVSX);
#endif
   }

bool
J9::CPU::getPPCSupportsTM()
   {
#if defined(J9OS_I5) && defined(J9OS_I5_V5R4)
   return FALSE;
#else
   J9ProcessorDesc   *processorDesc       = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   J9PortLibrary     *privatePortLibrary  = TR::Compiler->portLib;
   BOOLEAN feature = j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_HTM);
   return (TRUE == feature);
#endif
   }
   
bool
J9::CPU::getPPCSupportsLM()
  {
#if defined(J9OS_I5) && defined(J9OS_I5_V5R4)
  return FALSE;
#else
  J9ProcessorDesc   *processorDesc       = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
  BOOLEAN isP9    = (processorDesc->processor >= PROCESSOR_PPC_P9);
  return FALSE;
#endif
   }

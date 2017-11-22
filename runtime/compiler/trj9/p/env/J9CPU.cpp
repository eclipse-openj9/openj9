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

#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"
#include "j9.h"
#include "j9port.h"

namespace J9
{

namespace Power
{

// Double check with os400 team to see if we can enable popcnt on I
//
bool CPU::hasPopulationCountInstruction()
   {
#if defined(J9OS_I5)
   return false;
#else
   return TR::Compiler->target.cpu.isAtLeast(TR_PPCp7);
#endif
   }


bool CPU::supportsDecimalFloatingPoint()
   {
#if !defined(TR_HOST_POWER) || (defined(J9OS_I5) && defined(J9OS_I5_V5R4))
   return FALSE;
#else
   J9ProcessorDesc *processorDesc = TR::Compiler->target.cpu.TO_PORTLIB_getJ9ProcessorDesc();
   J9PortLibrary *privatePortLibrary = TR::Compiler->portLib;
   BOOLEAN feature = j9sysinfo_processor_has_feature(processorDesc, J9PORT_PPC_FEATURE_HAS_DFP);
   return (TRUE == feature);
#endif
   }


}

}

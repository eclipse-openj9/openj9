/*******************************************************************************
 * Copyright IBM Corp. and others 2013
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

/**
 * @file
 * @ingroup Port
 * @brief Processor Detection helper functions common to i386 platforms
 */

#include "j9sysinfo_helpers.h"

#include "j9port.h"
#include "j9porterror.h"
#include "portnls.h"
#include "ut_j9prt.h"

#include <string.h>
#if defined(WIN32)
#include <intrin.h>
#endif /* defined(WIN32) */

/**
 * Assembly code to get the register data from CPUID instruction.
 * This function executes the CPUID instruction based on which we can detect
 * if the environment is virtualized or not, and also get the Hypervisor Vendor
 * Name based on the same instruction. The leaf value specifies what information
 * to return.
 *
 * @param[in] 	leaf		The leaf value to the CPUID instruction and the value
 * 							in EAX when CPUID is called. A value of 0x1 returns basic
 * 							information including feature support.
 * @param[out]	cpuInfo 	Reference to the an integer array which holds the data
 * 							of EAX, EBX, ECX and EDX registers.
 *              cpuInfo[0]	To hold the EAX register data, value in this register at
 *							the time of CPUID tells what information to return
 *							EAX = 0x1, returns the processor Info and feature bits
 *							in EBX, ECX, EDX registers.
 *							EAX = 0x40000000 returns the Hypervisor Vendor Names
 * 							in the EBX, ECX, EDX registers.
 *              cpuInfo[1]  For EAX = 0x40000000 hold first 4 characters of the
 *							Hypervisor Vendor String
 *              cpuInfo[2] 	For EAX = 0x1, the 31st bit of ECX tells if its
 *              			running on Hypervisor or not,For EAX = 0x40000000 holds the second
 *							4 characters of the Hypervisor Vendor String
 *              cpuInfo[3]	For EAX = 0x40000000 hold the last 4 characters of the
 *              			Hypervisor Vendor String
 */
void
getX86CPUID(uint32_t leaf, uint32_t *cpuInfo)
{
	cpuInfo[0] = leaf;

/* Implement for x86 & x86_64 platforms. */
#if defined(WIN32)
	/* Specific CPUID instruction available on Windows. */
	__cpuid(cpuInfo, cpuInfo[0]);
#elif defined(LINUX) || defined(OSX) /* defined(WIN32) */
#if defined(J9X86)
	__asm volatile(
			"mov %%ebx, %%edi;"
			"cpuid;"
			"mov %%ebx, %%esi;"
			"mov %%edi, %%ebx;"
			: "+a" (cpuInfo[0]), "=S" (cpuInfo[1]), "=c" (cpuInfo[2]), "=d" (cpuInfo[3])
			:
			: "edi");

#elif defined(J9HAMMER) /* defined(J9X86) */
	__asm volatile(
			"cpuid;"
			: "+a" (cpuInfo[0]), "=b" (cpuInfo[1]), "=c" (cpuInfo[2]), "=d" (cpuInfo[3])
	);
#endif /* defined(J9X86) */
#endif /* defined(WIN32) */
}

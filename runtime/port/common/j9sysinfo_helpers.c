/*******************************************************************************
 * Copyright (c) 2013, 2017 IBM Corp. and others
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

/* defines for the CPUID instruction */
#define CPUID_VENDOR_INFO	0
#define CPUID_FAMILY_INFO	1

#define CPUID_VENDOR_INTEL		"GenuineIntel"
#define CPUID_VENDOR_AMD		"AuthenticAMD"
#define CPUID_VENDOR_LENGTH		12

#define CPUID_SIGNATURE_FAMILY			0x00000F00
#define CPUID_SIGNATURE_MODEL			0x000000F0
#define CPUID_SIGNATURE_EXTENDEDMODEL	0x000F0000

#define CPUID_SIGNATURE_FAMILY_SHIFT		8
#define CPUID_SIGNATURE_MODEL_SHIFT			4
#define CPUID_SIGNATURE_EXTENDEDMODEL_SHIFT	12

#define CPUID_FAMILYCODE_INTELPENTIUM	0x05
#define CPUID_FAMILYCODE_INTELCORE		0x06
#define CPUID_FAMILYCODE_INTELPENTIUM4	0x0F

#define CPUID_MODELCODE_INTELHASWELL	0x3A
#define CPUID_MODELCODE_SANDYBRIDGE		0x2A
#define CPUID_MODELCODE_INTELWESTMERE	0x25
#define CPUID_MODELCODE_INTELNEHALEM	0x1E
#define CPUID_MODELCODE_INTELCORE2		0x0F

#define CPUID_FAMILYCODE_AMDKSERIES		0x05
#define CPUID_FAMILYCODE_AMDATHLON		0x06
#define CPUID_FAMILYCODE_AMDOPTERON		0x0F

#define CPUID_MODELCODE_AMDK5			0x04
/**
 * @internal
 * Populates J9ProcessorDesc *desc on Windows and Linux (x86)
 *
 * @param[in] desc pointer to the struct that will contain the CPU type and features.
 *
 * @return 0 on success, -1 on failure
 */
intptr_t
getX86Description(struct J9PortLibrary *portLibrary, J9ProcessorDesc *desc)
{
	uint32_t CPUInfo[4] = {0};
	char vendor[12];
	uint32_t familyCode = 0;
	uint32_t processorSignature = 0;

	desc->processor = PROCESSOR_X86_UNKNOWN;

	/* vendor */
	getX86CPUID(CPUID_VENDOR_INFO, CPUInfo);
	memcpy(vendor + 0, &CPUInfo[1], sizeof(uint32_t));
	memcpy(vendor + 4, &CPUInfo[3], sizeof(uint32_t));
	memcpy(vendor + 8, &CPUInfo[2], sizeof(uint32_t));

	/* family and model */
	getX86CPUID(CPUID_FAMILY_INFO, CPUInfo);
	processorSignature = CPUInfo[0];
	familyCode = (processorSignature & CPUID_SIGNATURE_FAMILY) >> CPUID_SIGNATURE_FAMILY_SHIFT;
	if (0 == strncmp(vendor, CPUID_VENDOR_INTEL, CPUID_VENDOR_LENGTH)) {
		switch (familyCode) {
		case CPUID_FAMILYCODE_INTELPENTIUM:
			desc->processor = PROCESSOR_X86_INTELPENTIUM;
			break;
		case CPUID_FAMILYCODE_INTELCORE:
		{
			uint32_t modelCode  = (processorSignature & CPUID_SIGNATURE_MODEL) >> CPUID_SIGNATURE_MODEL_SHIFT;
			uint32_t extendedModelCode = (processorSignature & CPUID_SIGNATURE_EXTENDEDMODEL) >> CPUID_SIGNATURE_EXTENDEDMODEL_SHIFT;
			uint32_t totalModelCode = modelCode + extendedModelCode;

			if (totalModelCode > CPUID_MODELCODE_INTELHASWELL) {
				desc->processor = PROCESSOR_X86_INTELHASWELL;
			} else if (totalModelCode >= CPUID_MODELCODE_SANDYBRIDGE) {
				desc->processor = PROCESSOR_X86_INTELSANDYBRIDGE;
			} else if (totalModelCode >= CPUID_MODELCODE_INTELWESTMERE) {
				desc->processor = PROCESSOR_X86_INTELWESTMERE;
			} else if (totalModelCode >= CPUID_MODELCODE_INTELNEHALEM) {
				desc->processor = PROCESSOR_X86_INTELNEHALEM;
			} else if (totalModelCode == CPUID_MODELCODE_INTELCORE2) {
				desc->processor = PROCESSOR_X86_INTELCORE2;
			} else {
				desc->processor = PROCESSOR_X86_INTELP6;
			}
			break;
		}
		case CPUID_FAMILYCODE_INTELPENTIUM4:
			desc->processor = PROCESSOR_X86_INTELPENTIUM4;
			break;
		}
	} else if (0 == strncmp(vendor, CPUID_VENDOR_AMD, CPUID_VENDOR_LENGTH)) {
		switch (familyCode) {
		case CPUID_FAMILYCODE_AMDKSERIES:
		{
			uint32_t modelCode  = (processorSignature & CPUID_SIGNATURE_FAMILY) >> CPUID_SIGNATURE_MODEL_SHIFT;
			if (modelCode < CPUID_MODELCODE_AMDK5) {
				desc->processor = PROCESSOR_X86_AMDK5;
			}
			desc->processor = PROCESSOR_X86_AMDK6;
			break;
		}
		case CPUID_FAMILYCODE_AMDATHLON:
			desc->processor = PROCESSOR_X86_AMDATHLONDURON;
			break;
		case CPUID_FAMILYCODE_AMDOPTERON:
			desc->processor = PROCESSOR_X86_AMDOPTERON;
			break;
		}
	}

	desc->physicalProcessor = desc->processor;

	/* features */
	desc->features[0] = CPUInfo[3];
	desc->features[1] = CPUInfo[2];
	desc->features[2] = 0; /* reserved for future expansion */

	return 0;
}

/**
 * Assembly code to get the register data from CPUID instruction
 * This function executes the CPUID instruction based on which we can detect
 * if the environment is virtualized or not, and also get the Hypervisor Vendor
 * Name based on the same instruction. The leaf value specifies what information
 * to return.
 *
 * @param[in] 	leaf The leaf value to the CPUID instruction.
 * @param[out]	cpuInfo 	Reference to the an integer array which holds the data
 * 							of EAX,EBX,ECX and EDX registers.
 *              cpuInfo[0] To hold the EAX register data, value in this register at
 *							the time of CPUID tells what information to return
 *							EAX=0x1,returns the processor Info and feature bits
 *							in EBX,ECX,EDX registers.
 *							EAX=0x40000000 returns the Hypervisor Vendor Names
 * 							in the EBX,ECX,EDX registers.
 *              cpuInfo[1]  For EAX = 0x40000000 hold first 4 characters of the
 *							Hypervisor Vendor String
 *              cpuInfo[2] 	For EAX = 0x1, the 31st bit of ECX tells if its
 *              			running on Hypervisor or not,For EAX = 0x40000000 holds the second
 *							4 characters of the the Hypervisor Vendor String
 *              cpuInfo[3]	For EAX = 0x40000000 hold the last 4 characters of the
 *              			Hypervisor Vendor String
 *
 */
void
getX86CPUID(uint32_t leaf, uint32_t *cpuInfo)
{
	cpuInfo[0] = leaf;

/* Implemented for x86 & x86_64 bit platforms */
#if defined(WIN32)
	/* Specific CPUID instruction available in Windows */
	__cpuid(cpuInfo, cpuInfo[0]);

#elif defined(LINUX) || defined(OSX)
#if defined(J9X86)
	__asm volatile
	("mov %%ebx, %%edi;"
			"cpuid;"
			"mov %%ebx, %%esi;"
			"mov %%edi, %%ebx;"
			:"+a" (cpuInfo[0]), "=S" (cpuInfo[1]), "=c" (cpuInfo[2]), "=d" (cpuInfo[3])
			 : :"edi");

#elif defined(J9HAMMER)
  __asm volatile(
     "cpuid;"
     :"+a" (cpuInfo[0]), "=b" (cpuInfo[1]), "=c" (cpuInfo[2]), "=d" (cpuInfo[3])
        );
#endif
#endif
}

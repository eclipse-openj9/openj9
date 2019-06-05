/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "omr.h"
#include "omrcfg.h"

#include <string.h>

#include "EnvironmentBase.hpp"
#include "ModronAssertions.h"
#include "ProcessorInfo.hpp"

/**
 * Initialization.
 */
MM_ProcessorInfo*
MM_ProcessorInfo::newInstance(MM_EnvironmentBase *env)
{
	MM_ProcessorInfo *processorInfo = (MM_ProcessorInfo *)env->getForge()->allocate(sizeof(MM_ProcessorInfo), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (processorInfo) {
		new(processorInfo) MM_ProcessorInfo();
		if (!processorInfo->initialize(env)) {
			processorInfo->kill(env);	
			processorInfo = NULL;   			
		}
	}
	return processorInfo;
}

/**
 * Initialization.
 */
bool
MM_ProcessorInfo::initialize(MM_EnvironmentBase *env)
{
#if defined(AIXPPC) || defined(WIN32)
	/* on AIX and Windows we don't use tick based timer so no need to calculate CPU clock frequency */
	return true;
#else
	_freq = readFrequency();
	return (_freq != 0);
#endif
}

/**
 * Initialization.
 */
void
MM_ProcessorInfo::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Teardown
 */
void
MM_ProcessorInfo::tearDown(MM_EnvironmentBase *env)
{
}

U_64
MM_ProcessorInfo::secondToTick(double s)
{
	return (U_64) (s * _freq);
}

double
MM_ProcessorInfo::tickToSecond(U_64 t)
{
	return t / _freq;
}

double
MM_ProcessorInfo::readFrequency()
{
	double freq = 0;
#if (defined(LINUX) && (defined(J9X86) || defined(J9HAMMER) || defined(LINUXPPC)))
	char buffer[256];
	FILE *file = fopen("/proc/cpuinfo", "r");
	if (file != NULL) {
		while (!feof(file)) {
			if (NULL == fgets(buffer, sizeof(buffer), file)) {
				break;
			}
#if (defined(J9X86) || defined(J9HAMMER))
			/* the RTC on x86 is synchronized with the core clock so find the frequency in MHz then multiply by 10^6 to determine the ticks in a second */
			if (sscanf(buffer, "cpu MHz : %lf", &freq) == 1) {
				freq *= 1e6;
				break;
			}
#elif defined(LINUXPPC)
			/* the timebase register on PPC is potentially lower frequency than the core clock so get the kernel's estimation of its frequency */
			U_64 tempBase = 0;
			if (sscanf(buffer, "timebase        : %llu", (unsigned long long*)&tempBase) == 1) {
				/* PPC timebase is already in ticks per second so just convert it to a double for consistency with x86 implementation */
				freq = (double)tempBase;
				break;
			}
#else
			/* missing linux proc parsing logic */
			Assert_MM_unimplemented();
#endif
        }
		fclose(file);
	}
#elif defined(WIN32)
	/* Should read this via Windows API */
#elif defined(AIXPPC)
	/* Figure out the right thing to do */
	freq = 1000;
#else
	/* Hi-res tick implementation required */
	Assert_MM_unimplemented();
#endif
	return freq;
}

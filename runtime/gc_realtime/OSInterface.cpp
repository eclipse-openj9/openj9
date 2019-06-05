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
#include "omrport.h"

#include <string.h>
#include <math.h>

#if defined(AIXPPC)
#include <builtins.h>
#endif

#if defined(LINUX)
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#endif /* #if defined(LINUX) */

#if defined(LINUX) && !defined(J9ZTPF)
#include <linux/rtc.h>
#include <sys/syscall.h>
#include <sys/signal.h>
#elif defined(J9ZTPF)
#include <signal.h>
#endif /* defined(LINUX) && !defined(J9ZTPF) */

#include "ProcessorInfo.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "modronnls.h"

#include "OSInterface.hpp"
/**
 * Initialization.
 */
MM_OSInterface*
MM_OSInterface::newInstance(MM_EnvironmentBase *env)
{
	MM_OSInterface *osInterface = (MM_OSInterface *)env->getForge()->allocate(sizeof(MM_OSInterface), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (osInterface) {
		new(osInterface) MM_OSInterface();
		if (!osInterface->initialize(env)) {
			osInterface->kill(env);		
			osInterface = NULL;   			
		}
	}
	return osInterface;
}

/**
 * Initialization.
 */
bool
MM_OSInterface::initialize(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	_vm = env->getOmrVM();
	_extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	_numProcessors = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE);
	_physicalMemoryBytes = omrsysinfo_get_physical_memory();

	_omrtime_hires_clock_nanoSecondMultiplyFactor = 1000000000 / omrtime_hires_frequency();
	_omrtime_hires_clock_nanoSecondDivideFactor = omrtime_hires_frequency() / 1000000000;

	if (NULL == (_processorInfo = MM_ProcessorInfo::newInstance(env))) {
		return false;
	}

	_ticksPerMicroSecond = (U_64)(_processorInfo->_freq / 1e6);

	if (_extensions->verbose >= 1) {
		if (0 == _ticksPerMicroSecond) {
			omrtty_printf("Use OS high resolution timer instead of CPU tick-based timer\n");
		} else {
			omrtty_printf("ticksPerMicro = %llu\n", _ticksPerMicroSecond);
		}
	}
	return true;
}

/**
 * Initialization.
 */
void
MM_OSInterface::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Teardown
 */
void
MM_OSInterface::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _processorInfo) {
		_processorInfo->kill(env);
	}
}

bool 
MM_OSInterface::rtcTimerAvailable() {
#if defined(LINUX)
	return true;
#else
	return false;
#endif
}

bool 
MM_OSInterface::itTimerAvailable() {
#if defined(WIN32)
	return true;
#else
	return false;
#endif
}	

/**
 * hiresTimerAvailable
 * On Linux or AIX, return true if the realtime clock resolution is better than
 * the high resolution timer period, false otherwise.
 * On Windows, return false.
 * Other platforms will fail to compile until they have an implementation.
 * @ingroup GC_Metronome methodGroup
 */
bool
MM_OSInterface::hiresTimerAvailable() {
#if (defined(LINUX) && !defined(J9ZTPF)) || defined (AIXPPC)
	OMRPORT_ACCESS_FROM_OMRVM(_vm);
	struct timespec ts;
	if (clock_getres(CLOCK_REALTIME, &ts)) {
		if (_extensions->verbose >= 2) {
			omrtty_printf("POSIX High Resolution Clock not available\n");
		}
		return false;
	} else {
		if (_extensions->verbose >= 2) {
			omrtty_printf("POSIX High Resolution Clock has resolution %d nanoseconds\n", ts.tv_nsec);
		}
		bool returnValue = ((ts.tv_sec == 0) && ((uintptr_t)ts.tv_nsec < (_extensions->hrtPeriodMicro * 1000)));
		if (!returnValue && _extensions->overrideHiresTimerCheck) {
			omrnls_printf(J9NLS_INFO, J9NLS_GC_IGNORE_OS_REPORTED_HIGHRES_VALUES);
			returnValue = true;
		}
		return returnValue;
	}
#elif defined(WIN32)
	/* On Win32, we don't have a HRT and it is safe to use our ITimer implementation so we can return false */
	return false;
#else
	/* No high res time available so try your luck with ITAlarm instead */
	return false;
#endif	/* #if (defined(LINUX) && !defined(J9ZTPF)) || defined (AIXPPC) */
}

U_64
MM_OSInterface::nanoTime()
{
	OMRPORT_ACCESS_FROM_OMRVM(_vm);
	U_64 hiresTime = omrtime_hires_clock();
	if (_omrtime_hires_clock_nanoSecondMultiplyFactor != 0) {
		return hiresTime * _omrtime_hires_clock_nanoSecondMultiplyFactor;
	}
	return hiresTime / _omrtime_hires_clock_nanoSecondDivideFactor;
}

void
MM_OSInterface::maskSignals()
{
#if defined(LINUX)
	/* mask any further signals - useful for signal handlers */
	sigset_t mask_set;/* used to set a signal masking set. */
	sigfillset(&mask_set);
	sigprocmask(SIG_SETMASK, &mask_set, NULL);
#endif /* LINUX */
}


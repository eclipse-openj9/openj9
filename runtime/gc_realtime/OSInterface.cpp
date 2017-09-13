/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "j9.h"
#include "j9socket.h"
#include "portsock.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "modronopt.h"
#include "j9port.h"
#include "modronnls.h"

#include <string.h>
#include <math.h>

#if defined(AIXPPC)
#include <builtins.h>
#endif

#if defined(LINUX)
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#endif /* #if defined(LINUX) */

#if defined(LINUX) && !defined(J9ZTPF)
#include <linux/rtc.h>
#include <sys/syscall.h>
#include <sys/signal.h>
#elif defined(J9ZTPF)
#include <signal.h>
#endif /* defined(LINUX) && !defined(J9ZTPF) */

#include "OSInterface.hpp"
#include "ProcessorInfo.hpp"
#include "AtomicOperations.hpp"
#include "ClassModel.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "MemoryPoolSegregated.hpp"
#include "MemorySubSpace.hpp"
#include "modronapi.hpp"
#include "ObjectModel.hpp"
#include "RootScanner.hpp"
#include "Task.hpp"
#include "OSInterface.hpp"
#include "RealtimeGC.hpp"

/**
 * Initialization.
 */
MM_OSInterface*
MM_OSInterface::newInstance(MM_EnvironmentBase *env)
{
	MM_OSInterface *osInterface = (MM_OSInterface *)env->getForge()->allocate(sizeof(MM_OSInterface), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
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
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	_vm = (J9JavaVM*)env->getOmrVM()->_language_vm;
	_extensions = MM_GCExtensions::getExtensions(_vm);
	_numProcessors = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
	_physicalMemoryBytes = j9sysinfo_get_physical_memory();

	_j9time_hires_clock_nanoSecondMultiplyFactor = 1000000000 / j9time_hires_frequency();
	_j9time_hires_clock_nanoSecondDivideFactor = j9time_hires_frequency() / 1000000000;
	
	struct j9NetworkInterfaceArray_struct networkInterfaceArray;
	I_32 result = j9sock_get_network_interfaces(&networkInterfaceArray, TRUE);
	if (result >= 0) {
		/* now loop through the interfaces and extract the information to be returned */
		for (UDATA i = 0;i < networkInterfaceArray.length;i++) {
			for (UDATA j=0; j < networkInterfaceArray.elements[i].numberAddresses; j ++) {
				if (0x7f000001 != j9sock_ntohl(networkInterfaceArray.elements[i].addresses[j].addr.inAddr.s_addr)) {
					j9sock_inetntoa(&_ipAddrString, networkInterfaceArray.elements[i].addresses[j].addr.inAddr.s_addr);
					break;
				}
			}
			if (NULL != _ipAddrString) {
				break;
			}
		}
		/* free the memory for the interfaces struct and return the new NetworkInterface List */
		j9sock_free_network_interface_struct(&networkInterfaceArray);
	}
	if (NULL == _ipAddrString) {
		j9sock_inetntoa(&_ipAddrString, j9sock_htonl(0x7f000001));
	}
	j9sock_gethostname(_hostname, sizeof(_hostname));
	
	if (NULL == (_processorInfo = MM_ProcessorInfo::newInstance(env))) {
		return false;
	}

	_ticksPerMicroSecond = (U_64)(_processorInfo->_freq / 1e6);

	if (_extensions->verbose >= 1) {
		if (0 == _ticksPerMicroSecond) {
			j9tty_printf(PORTLIB, "Use OS high resolution timer instead of CPU tick-based timer\n");
		} else {
			j9tty_printf(PORTLIB, "ticksPerMicro = %llu\n", _ticksPerMicroSecond);
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

UDATA
MM_OSInterface::getTid()
{
#if defined(LINUX) && (defined(J9X86) || defined(J9HAMMER))
	return syscall(SYS_gettid);
#else
	return 0;
#endif
}

IDATA
MM_OSInterface::maxPriority()
{
#if defined(LINUX)
	return 99;
#elif defined(AIXPPC)
	return 127;
#elif defined(WIN32)
	return 3;
#else
	return 0;
#endif
}

IDATA
MM_OSInterface::minPriority()
{
	return 0;
}

bool
MM_OSInterface::changePriority(IDATA policy, IDATA priority)
{
#if defined(LINUX)
	PORT_ACCESS_FROM_JAVAVM(_vm);
	pthread_t self = pthread_self();
	struct sched_param schedParam;
	schedParam.sched_priority = priority;
	IDATA rc = pthread_setschedparam(self, policy, &schedParam);
	if (rc) {
		if (_extensions->verbose >= 1) {
			j9tty_printf(PORTLIB, "Error code %d: Failed to change thread to %s mode with priority %d\n", 
				rc, (policy == SCHED_RR) ? "SCHED_RR" : ((policy == SCHED_FIFO) ? "FIFO" : "UNKNOWN"), priority);
		}
		return false;
	}
	return true;
	/* What about affinity????  No thread affinity - only process affinity. */
#elif defined(WIN32)
	PORT_ACCESS_FROM_JAVAVM(_vm);
	HANDLE hThread = GetCurrentThread();
	/* We ignore the policy, and just set priority. */
	UINT uPriority;
	switch(priority) {
		case 0: 
			uPriority = THREAD_PRIORITY_NORMAL;
			break;
		case 1:
			uPriority = THREAD_PRIORITY_ABOVE_NORMAL;
			break;
		case 2:	
			uPriority = THREAD_PRIORITY_HIGHEST;
			break;
		case 3:
			uPriority = THREAD_PRIORITY_TIME_CRITICAL;
			break;
		default:
			return false;
	}
	if (SetThreadPriority(hThread, uPriority) == 0) {
		if (_extensions->verbose >= 1) {
			j9tty_printf(PORTLIB, "Error code %d: Failed to change thread to priority %d\n", 
				GetLastError(), priority);
		}
		return false;
	}
	return true;
#else
	return false;
#endif
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
	PORT_ACCESS_FROM_JAVAVM(_vm);
	struct timespec ts;
	if (clock_getres(CLOCK_REALTIME, &ts)) {
		if (_extensions->verbose >= 2) {
			j9tty_printf(PORTLIB, "POSIX High Resolution Clock not available\n");
		}
		return false;
	} else {
		if (_extensions->verbose >= 2) {
			j9tty_printf(PORTLIB, "POSIX High Resolution Clock has resolution %d nanoseconds\n", ts.tv_nsec);
		}
		bool returnValue = ((ts.tv_sec == 0) && ((UDATA)ts.tv_nsec < (_extensions->hrtPeriodMicro * 1000)));
		if (!returnValue && _extensions->overrideHiresTimerCheck) {
			j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_GC_IGNORE_OS_REPORTED_HIGHRES_VALUES);
			returnValue = true;
		}
		return returnValue;
	}
#elif defined(WIN32)
	/* On Win32, we don't have a HRT  and it is safe to use our ITimer implementation so we can return false */
	return false;
#else
	/* No high res time available so try your luck with ITAlarm instead */
	return false;
#endif	/* #if (defined(LINUX) && !defined(J9ZTPF)) || defined (AIXPPC) */
}

U_64
MM_OSInterface::nanoTime()
{
	PORT_ACCESS_FROM_JAVAVM(_vm);
	U_64 hiresTime = j9time_hires_clock();
	if (_j9time_hires_clock_nanoSecondMultiplyFactor != 0) {
		return hiresTime * _j9time_hires_clock_nanoSecondMultiplyFactor;
	}
	return hiresTime / _j9time_hires_clock_nanoSecondDivideFactor;
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


UDATA
MM_OSInterface::getParameter(UDATA which, char *keyBuffer, I_32 keyBufferSize, char *valueBuffer, I_32 valueBufferSize)
{
	PORT_ACCESS_FROM_JAVAVM(_vm);
	switch (which) {
		case 0:	j9str_printf(PORTLIB, keyBuffer, keyBufferSize, "Number of Processors");
			j9str_printf(PORTLIB, valueBuffer, valueBufferSize, "%d", _numProcessors);
			return 1;
		case 1:	j9str_printf(PORTLIB, keyBuffer, keyBufferSize, "Physical Memory");
			j9str_printf(PORTLIB, valueBuffer, valueBufferSize, "%d Mb", _physicalMemoryBytes >> 20);
			return 1;
		case 2:	j9str_printf(PORTLIB, keyBuffer, keyBufferSize, "IP Address");
			j9str_printf(PORTLIB, valueBuffer, valueBufferSize, _ipAddrString);
			return 1;
		case 3:	j9str_printf(PORTLIB, keyBuffer, keyBufferSize, "OS");
			j9str_printf(PORTLIB, valueBuffer, valueBufferSize, "%s", j9sysinfo_get_OS_type());
			return 1;
		case 4:	j9str_printf(PORTLIB, keyBuffer, keyBufferSize, "OS Version");
			j9str_printf(PORTLIB, valueBuffer, valueBufferSize, "%s", j9sysinfo_get_OS_version());
			return 1;
		case 5:	j9str_printf(PORTLIB, keyBuffer, keyBufferSize, "CPU");
			j9str_printf(PORTLIB, valueBuffer, valueBufferSize, "%s", j9sysinfo_get_CPU_architecture());
			return 1;
		case 6:	j9str_printf(PORTLIB, keyBuffer, keyBufferSize, "Username");
			j9sysinfo_get_username(valueBuffer, valueBufferSize);
			return 1;
		case 7:	j9str_printf(PORTLIB, keyBuffer, keyBufferSize, "Hostname");
			j9str_printf(PORTLIB, valueBuffer, valueBufferSize, _hostname);
			return 1;
		case 8: j9str_printf(PORTLIB, keyBuffer, keyBufferSize, "Tick Frequency");
			j9str_printf(PORTLIB, valueBuffer, valueBufferSize, "1000000000");
			return 1;
	}
	UDATA numFixedProps = 9;
	UDATA whichProc = which - numFixedProps;
	if (whichProc < _numProcessors) {
 		j9str_printf(PORTLIB, keyBuffer, keyBufferSize, "Processor %d (GHz)", whichProc);
 		j9str_printf(PORTLIB, valueBuffer, valueBufferSize, "%.9f", 
			_processorInfo->secondToTick(1.0) / 1.0e9);
		return 1;
	}
	return 0;
}

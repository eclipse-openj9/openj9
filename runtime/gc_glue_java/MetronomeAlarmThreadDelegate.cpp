/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "MetronomeAlarmThreadDelegate.hpp"

#if defined(J9VM_GC_REALTIME)

#include "omr.h"
#include "EnvironmentRealtime.hpp"
#include "GCExtensions.hpp"
#include "MetronomeAlarmThread.hpp"

uintptr_t
MM_MetronomeAlarmThreadDelegate::signalProtectedFunction(J9PortLibrary *privatePortLibrary, void* userData)
{
	MM_MetronomeAlarmThread *alarmThread = (MM_MetronomeAlarmThread *)userData;
	J9JavaVM *javaVM = (J9JavaVM *)alarmThread->getScheduler()->_extensions->getOmrVM()->_language_vm;
	J9VMThread *vmThread = NULL;
	MM_EnvironmentRealtime *env = NULL;
	
	if (JNI_OK != (javaVM->internalVMFunctions->attachSystemDaemonThread(javaVM, &vmThread, "GC Alarm"))) {
		return 0;
	}
	
	env = MM_EnvironmentRealtime::getEnvironment(vmThread);
	
	alarmThread->run(env);
	
	javaVM->internalVMFunctions->DetachCurrentThread((JavaVM*)javaVM);
	
	return 0;
}

/**
 * C entrypoint for the newly created alarm thread.
 */
int J9THREAD_PROC
MM_MetronomeAlarmThreadDelegate::metronomeAlarmThreadWrapper(void* userData)
{
	MM_MetronomeAlarmThread *alarmThread = (MM_MetronomeAlarmThread *)userData;
	J9JavaVM *javaVM = (J9JavaVM *)alarmThread->getScheduler()->_extensions->getOmrVM()->_language_vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	uintptr_t rc;

	j9sig_protect(MM_MetronomeAlarmThreadDelegate::signalProtectedFunction, (void*)userData,
		javaVM->internalVMFunctions->structuredSignalHandlerVM, javaVM,
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
		&rc);
		
	omrthread_monitor_enter(alarmThread->_mutex);
	alarmThread->_alarmThreadActive = MM_MetronomeAlarmThread::AlarmThradActive::ALARM_THREAD_SHUTDOWN;
	omrthread_monitor_notify(alarmThread->_mutex);
	omrthread_exit(alarmThread->_mutex);
		
	return 0;
}

#endif /* defined(J9VM_GC_REALTIME) */


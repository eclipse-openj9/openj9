/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include "omrcfg.h"
#include "jni.h"
#include "omrthread.h"
#include "j9port.h"
#include "j9.h"

void JNICALL Java_com_ibm_oti_vm_thread_Tracing_reset(JNIEnv *env, jobject recv);


void JNICALL 
Java_com_ibm_oti_vm_thread_Tracing_reset(JNIEnv *env, jobject recv)
{
	J9VMThread* vmstruct = (J9VMThread*)env;

#ifndef OMR_THR_TRACING
	PORT_ACCESS_FROM_VMC(vmstruct);
	j9tty_err_printf(PORTLIB, "Warning: com.ibm.oti.vm.thread.Tracing.reset() called, but OMR_THR_TRACING is not enabled\n");
#else /* OMR_THR_TRACING */
	/* bring all threads to a safe point. This won't help for native threads, but it's better than nothing */
	vmstruct->javaVM->internalVMFunctions->internalAcquireVMAccess(vmstruct);
	vmstruct->javaVM->internalVMFunctions->acquireExclusiveVMAccess(vmstruct);

	omrthread_reset_tracing();

	vmstruct->javaVM->internalVMFunctions->releaseExclusiveVMAccess(vmstruct);
	vmstruct->javaVM->internalVMFunctions->internalExitVMToJNI(vmstruct);
#endif /* OMR_THR_TRACING */
	return ;
}


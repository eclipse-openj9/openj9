/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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

#include "j9.h"
#include "omrthread.h"
#include "util_api.h"

J9VMThread *
getVMThreadFromOMRThread(J9JavaVM *vm, omrthread_t omrthread)
{
	J9VMThread *vmThread = NULL;

	if ((NULL != omrthread) && (NULL != vm->omrVM) && (vm->omrVM->_vmThreadKey > 0)) {
		OMR_VMThread *omrVMThread = ((OMR_VMThread *)omrthread_tls_get(omrthread, vm->omrVM->_vmThreadKey));
		if (NULL != omrVMThread) {
			vmThread = (J9VMThread *)omrVMThread->_language_vmthread;
		}
	}
	return vmThread;
}

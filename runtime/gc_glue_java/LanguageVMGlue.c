/*******************************************************************************
 * Copyright (c) 2014, 2014 IBM Corp. and others
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
#include "omr.h"

omr_error_t
OMR_Glue_BindCurrentThread(OMR_VM *omrVM, const char *threadName, OMR_VMThread **omrVMThread)
{
	omr_error_t rc = OMR_ERROR_NONE;
	J9JavaVM *vm = (J9JavaVM *)omrVM->_language_vm;
	J9VMThread *tempVMThread = NULL;

	/* Pass in a temporary J9VMThread because the JVM's attach API handles initialization of omrVMThread->_language_vmthread.*/
	if (JNI_OK != vm->internalVMFunctions->attachSystemDaemonThread(vm, &tempVMThread, threadName)) {
		rc = OMR_ERROR_INTERNAL;
	} else {
		*omrVMThread = tempVMThread->omrVMThread;
	}
	return rc;
}

omr_error_t
OMR_Glue_UnbindCurrentThread(OMR_VMThread *omrVMThread)
{
	J9JavaVM *vm = (J9JavaVM *)omrVMThread->_vm->_language_vm;
	vm->internalVMFunctions->DetachCurrentThread((JavaVM *)vm);
	return OMR_ERROR_NONE;
}

omr_error_t
OMR_Glue_AllocLanguageThread(void *languageVM, void **languageThread)
{
	/* OMRTODO Unimplemented */
	*languageThread = NULL;
	return OMR_ERROR_NONE;
}

omr_error_t
OMR_Glue_FreeLanguageThread(void *languageThread)
{
	/* OMRTODO Unimplemented */
	return OMR_ERROR_NONE;
}

omr_error_t
OMR_Glue_LinkLanguageThreadToOMRThread(void *languageThread, OMR_VMThread *omrVMThread)
{
	/* OMRTODO Unimplemented */
	return OMR_ERROR_NONE;
}


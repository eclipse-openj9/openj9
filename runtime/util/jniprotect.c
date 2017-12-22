/*******************************************************************************
 * Copyright (c) 2002, 2014 IBM Corp. and others
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
#include "j9port.h"
#include "ut_j9util.h"

typedef struct J9NewSignalSupportArgs {
	protected_fn function;
	void* args;
} J9NewSignalSupportArgs;

/**
 * Calls the function passed in userData.
 *
 * @param portLibrary	
 * @param userData    Function and its args which is called in this function.
 * @return The result of the function call which is passed in userData.
 *
 */
static UDATA
signalProtectAndRunGlue(J9PortLibrary* portLibrary, void * userData)
{
	J9NewSignalSupportArgs* newArgs = userData;

	return newArgs->function(newArgs->args);
}

/**
 * Calls the function passed in args by making sure that it is signal protected. 
 * If it is not signal protected already, then it protects, calls the function and unprotects it again.
 *
 * @param function	Function to be called
 * @param env    J9VMThread *
 * @param args 	Function arguments
 * @return The result of the function call which is passed in args.
 * In the case when j9sig_protect() call fails, an assertion will occur. If assertions
 * are not enabled the return value will be zero. Its up to the caller to interpret this,
 * but it will basically result in undefined behavior, as the caller will be
 * interpreting the return value of the protected function, and there is
 * no common standard for these return codes.
 *
 */
UDATA
gpProtectAndRun(protected_fn function, JNIEnv * env, void *args)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	UDATA rc = 0;
	J9NewSignalSupportArgs newArgs;

	PORT_ACCESS_FROM_VMC(vmThread);

	Assert_Util_false(vmThread->gpProtected);

	vmThread->gpProtected = TRUE;

	newArgs.function = function;
	newArgs.args = args;
	if (j9sig_protect(signalProtectAndRunGlue, &newArgs,
		vmThread->javaVM->internalVMFunctions->structuredSignalHandler, vmThread,
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
		&rc)
	) {
		Assert_Util_signalProtectionFailed();
	}

	Assert_Util_true(vmThread->gpProtected);

	vmThread->gpProtected = FALSE;

	return rc;
}


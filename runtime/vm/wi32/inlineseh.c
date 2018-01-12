/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#include <windows.h>
#include <stddef.h> /* for offsetof */

#include "j9.h"
#include "j9protos.h"

/* these magic constants may not be defined in windows.h */
#ifndef DISPOSITION_CONTINUE_EXECUTION
#define DISPOSITION_CONTINUE_EXECUTION 0
#endif
#ifndef DISPOSITION_CONTINUE_SEARCH
#define DISPOSITION_CONTINUE_SEARCH 1
#endif
#ifndef EXCEPTION_UNWINDING
#define EXCEPTION_UNWINDING 2
#endif
#ifndef EXCEPTION_EXIT_UNWIND
#define EXCEPTION_EXIT_UNWIND 4
#endif

#if !defined(WIN64)

/**
 * This file provides support for fast structured exception handling (SEH) on Win32 
 * platforms.  Instead of calling through the port library for signal protection (via 
 * j9sig_protect), the VM installs a SEH handler itself on call-in. This avoids two
 * calls through function pointers and results in about a 10% improvement in call-in
 * speed (as measured by vich 12).
 *
 * This optimization should only be used if the port library is unmodified. That is, if
 * omrport_isFunctionOverridden(offsetof(J9PortLibrary, sig_protect)) returns zero.
 * The signal handler provided in this file validates that sig_protect has not been 
 * overridden, and effectively ignores the signal if it has been. This allows jstartup
 * to install the handler without checking.
 *
 * @see J9VMPlatformDependentWindows31#enterSEH:vmThreadWrapper:
 * @see j9portcontrol.c#j9port_control()
 * @see j9signal.c#structuredExceptionHandler()
 * @see gphandle.c#structuredSignalHandler()
 *
 * @see http://www.microsoft.com/msj/0197/exception/exception.aspx
 */
EXCEPTION_DISPOSITION __cdecl
win32ExceptionHandler(struct _EXCEPTION_RECORD *ExceptionRecord, void *EstablisherFrame, struct _CONTEXT *ContextRecord, void *DispatcherContext)
{
	/* EstablisherFrame points into the middle of the J9VMEntryLocalStorage structure */
	J9VMEntryLocalStorage* els = (J9VMEntryLocalStorage*)((U_8*)EstablisherFrame - offsetof(J9VMEntryLocalStorage, gpLink));
	J9VMThread* vmThread = els->currentVMThread;
	int (*portLibraryInternalHandler)(struct J9PortLibrary *, j9sig_handler_fn, void*, U_32, EXCEPTION_POINTERS *);
	int result;
	EXCEPTION_POINTERS pointers;
	PORT_ACCESS_FROM_VMC(vmThread);
	OMRPORT_ACCESS_FROM_J9VMTHREAD(vmThread);

	if (ExceptionRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)) {
		/* this is the second phase of the handler. Nothing to do. */
		return DISPOSITION_CONTINUE_SEARCH;
	}

	if (omrport_isFunctionOverridden(offsetof(OMRPortLibrary, sig_protect))) {
		/* we don't know how sig_protect is implemented. This exception is none of our business */
		return DISPOSITION_CONTINUE_SEARCH;
	}

	/* fortunately, we know that our handler will never return J9PORT_SIG_EXCEPTION_RETURN 
	 * so we don't have to handle the difficult cases
	 */

	/* peek into the port library implementation using j9port_control */
	if (j9port_control("SIG_INTERNAL_HANDLER", (UDATA)&portLibraryInternalHandler)) {
		/* this isn't supposed to happen */
		return DISPOSITION_CONTINUE_SEARCH;
	}

	pointers.ExceptionRecord = ExceptionRecord;
	pointers.ContextRecord = ContextRecord;

	result = portLibraryInternalHandler(PORTLIB, 
		structuredSignalHandler, 
		vmThread, 
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
		&pointers);

	if (result == EXCEPTION_CONTINUE_EXECUTION) {
		return DISPOSITION_CONTINUE_EXECUTION;
	} else {
		return DISPOSITION_CONTINUE_SEARCH;
	}
}

#endif /* !defined(WIN64) */

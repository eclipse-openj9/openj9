/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#include <stdio.h>
#include <stdarg.h>

#include "omrthread.h"
#include "rastrace_internal.h"
#include "omrutilbase.h"

#if defined(J9ZOS390)
#include "atoe.h"
#endif


#undef DEBUG
#if defined(DEBUG)
#define WRAPPER_DEBUG(x) printf x
#else
#define WRAPPER_DEBUG(x)
#endif


void
twFprintf(const char * template, ...)
{
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
	va_list arg_ptr;

	va_start(arg_ptr, template);
	j9tty_err_vprintf(template, arg_ptr);
	va_end(arg_ptr); 
}

UtThreadData ** twThreadSelf(void)
{
	omrthread_t self = omrthread_self();

	/*
	 * Retrieve slot from TLS - may be address of a field inside VM thread, a local var, or some other slot
	 */
	return (UtThreadData **) 	( self ? omrthread_tls_get(self, j9uteTLSKey) : NULL );
}

omr_error_t
twE2A(char * str)
{
#if defined(J9ZOS390)
	uintptr_t length = 0;

	WRAPPER_DEBUG(("<TW> E2A called\n"));
	length = strlen(str);
	if (length > 0){
		char *abuf;
		abuf = e2a(str, length);
		if (abuf) {
			strcpy(str, abuf);
			free(abuf);
		}
	}
#endif
	return OMR_ERROR_NONE;
}

int32_t
twCompareAndSwap32(volatile uint32_t *target, uint32_t old, uint32_t new32)
{
	return (compareAndSwapU32((uint32_t *)target, old, new32) == old)? TRUE : FALSE;
}

int32_t
twCompareAndSwapPtr(volatile uintptr_t *target, uintptr_t old, uintptr_t newptr)
{
	return (compareAndSwapUDATA((uintptr_t *)target, old, newptr) == old)? TRUE : FALSE;
}

omr_error_t
twThreadAttach(UtThreadData **thr, char *name)
{
	omr_error_t ret = OMR_ERROR_NONE;
	const OMRTraceLanguageInterface *languageIntf = &UT_GLOBAL(languageIntf);
	OMR_VMThread *currentThread = NULL;

	WRAPPER_DEBUG(("<TW> ThreadAttach called\n"));
	
	/* 
	 * Attach the thread to the VM. For the J9VM, this will generate the
	 * J9HOOK_THREAD_ABOUT_TO_START hook, which the trace engine
	 * catches and uses to call threadStart().
	 */
	if (NULL != languageIntf->AttachCurrentThreadToLanguageVM) {
		if (OMR_ERROR_NONE != languageIntf->AttachCurrentThreadToLanguageVM((OMR_VM *)UT_GLOBAL(vm), name, &currentThread)) {
			ret = OMR_ERROR_INTERNAL;
			goto out;
		}
	}
	
	/* 
	 * threadStart() will have allocated this thread's UtThreadData struct
	 * so we need to retrieve it and set our local `thr' accordingly.
	 */
	if (NULL == currentThread) {
		ret = OMR_ERROR_INTERNAL;
	} else {
		*thr = *UT_THREAD_FROM_OMRVM_THREAD(currentThread);
		if (NULL == *thr) {
			if (NULL != languageIntf->DetachCurrentThreadFromLanguageVM) {
				languageIntf->DetachCurrentThreadFromLanguageVM(currentThread);
			}
			ret = OMR_ERROR_INTERNAL;
		}
	}

out:
	WRAPPER_DEBUG(("<TW> ThreadAttach returned %d\n", ret));
	return ret;
}

omr_error_t
twThreadDetach(UtThreadData **thr)
{
	omrthread_t self = OS_THREAD_FROM_UT_THREAD(thr);
	omr_error_t ret = OMR_ERROR_NONE;
	const OMRTraceLanguageInterface *languageIntf = &UT_GLOBAL(languageIntf);
	OMR_VMThread *currentThread = OMR_VM_THREAD_FROM_UT_THREAD(thr);

	WRAPPER_DEBUG(("<TW> ThreadDetach called\n"));

	/* 
	 * Detach the thread from the VM. For the J9VM, this will generate the
	 * J9HOOK_VM_THREAD_END hook, which the trace engine
	 * catches and uses to call threadStop().
	 */
	if (NULL != languageIntf->DetachCurrentThreadFromLanguageVM) {
		languageIntf->DetachCurrentThreadFromLanguageVM(currentThread);
	}

	if (self) {
		omrthread_tls_set(self, j9uteTLSKey, NULL);	/* Wipe _slot_ address in TLS */
	}

	WRAPPER_DEBUG(("<TW> ThreadDetach returned %d\n", ret));
	return ret;
}

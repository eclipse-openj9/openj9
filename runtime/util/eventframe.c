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

#include "j9.h"
#include "j9consts.h"
#include "j9protos.h"
#include "util_internal.h"
#include "ut_j9vmutil.h"


void
popEventFrame(J9VMThread * currentThread, UDATA hadVMAccess)
{
	J9SFJNINativeMethodFrame * frame;
	UDATA * spAfterPop;

	Trc_VMUtil_popEventFrame_Entry(currentThread, hadVMAccess);

	/* Acquire VM access if the current thread does not already have it */

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	if (currentThread->inNative) {
		currentThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(currentThread);
	}
#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
	if (!(currentThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS)) {
		currentThread->javaVM->internalVMFunctions->internalAcquireVMAccess(currentThread);
	}
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

	/* Perform JNI cleanup */

	frame = (J9SFJNINativeMethodFrame *) (((U_8 *) currentThread->sp) + (UDATA) currentThread->literals);
	if (frame->specialFrameFlags & J9_SSF_JIT_JNI_FRAME_COLLAPSE_BITS) {
		currentThread->javaVM->internalVMFunctions->returnFromJNI(currentThread, &(frame->savedA0));
	}

	/* Toss the stack frame */

	spAfterPop = currentThread->arg0EA + 1;
	currentThread->arg0EA = (UDATA *) ((UDATA) frame->savedA0 & ~J9SF_A0_INVISIBLE_TAG);
	currentThread->literals = frame->savedCP;
	currentThread->pc = frame->savedPC;
	currentThread->sp = spAfterPop;

	/* Release VM access unless the current thread had it before reporting the event */

	if (!hadVMAccess) {
		currentThread->javaVM->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	Trc_VMUtil_popEventFrame_Exit(currentThread);
}



UDATA
pushEventFrame(J9VMThread * currentThread, UDATA wantVMAccess, UDATA jniRefSlots)
{
	J9SFJNINativeMethodFrame * frame;
	UDATA hadVMAccess;

	Trc_VMUtil_pushEventFrame_Entry(currentThread, wantVMAccess, jniRefSlots);

	/* Acquire VM access if the current thread does not already have it.  Remember the access state. */

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	Assert_VMUtil_false(currentThread->inNative);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
	if (currentThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) {
		hadVMAccess = TRUE;
	} else {
		hadVMAccess = FALSE;
		currentThread->javaVM->internalVMFunctions->internalAcquireVMAccess(currentThread);
	}

	/* Build the frame - it's a JNI native frame which has no method (no arguments pushed) and is hidden */

	frame = (J9SFJNINativeMethodFrame *) (((U_8 *) (currentThread->sp - jniRefSlots)) - sizeof(J9SFJNINativeMethodFrame));
	frame->method = NULL;
	frame->specialFrameFlags = 0;
	frame->savedCP = currentThread->literals;
	frame->savedPC = currentThread->pc;
	frame->savedA0 = (UDATA *) ((UDATA) currentThread->arg0EA | J9SF_A0_INVISIBLE_TAG);
	currentThread->arg0EA = ((UDATA *) &(frame->savedA0)) + jniRefSlots;
	currentThread->sp = (UDATA *) frame;
	currentThread->literals = NULL;
	currentThread->pc = (U_8 *) J9SF_FRAME_TYPE_JNI_NATIVE_METHOD;

	/* Release VM access unless the caller has requested that it be kept */

	if (!wantVMAccess) {
		Assert_VMUtil_true(0 == jniRefSlots);
		currentThread->javaVM->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	/* Return the state of VM access upon entry to this function */

	Trc_VMUtil_pushEventFrame_Exit(currentThread, hadVMAccess);

	return hadVMAccess;
}





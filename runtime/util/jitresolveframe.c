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

#include "j9protos.h"
#include "j9consts.h"
#include "stackwalk.h"
#include "util_internal.h"

#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
/**
 * @internal
 * 
 * Push a resolve frame on the stack so that it is walkable.
 *
 * This should be in jswalk.c, but it is in util for now so that is can be used by both jswalk.c and gphandle.c.
 * Once the JIT specific code in gphandle.c is moved into the JIT module this can be moved back to
 * jswalk.c where it belongs.
 */
J9SFJITResolveFrame* 
jitPushResolveFrame(J9VMThread* vmThread, UDATA* sp, U_8* pc) 
{
	J9SFJITResolveFrame * resolveFrame = (J9SFJITResolveFrame *) (((U_8 *) sp) - sizeof(J9SFJITResolveFrame));
	resolveFrame->savedJITException = NULL;
	resolveFrame->specialFrameFlags = J9_SSF_JIT_RESOLVE;
	resolveFrame->parmCount = 0;
	resolveFrame->returnAddress = pc;
	resolveFrame->taggedRegularReturnSP = (UDATA *) (((U_8 *) (resolveFrame + 1)) + J9SF_A0_INVISIBLE_TAG);

	vmThread->pc = (U_8 *) J9SF_FRAME_TYPE_JIT_RESOLVE;
	vmThread->arg0EA = (UDATA *) &(resolveFrame->taggedRegularReturnSP);
	vmThread->literals = NULL;
	vmThread->sp = (UDATA *) resolveFrame;

	return resolveFrame;
}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */

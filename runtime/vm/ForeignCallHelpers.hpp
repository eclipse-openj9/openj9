/*******************************************************************************
 * Copyright IBM Corp. and others 2026
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(FOREIGNCALLHELPERS_HPP_)
#define FOREIGNCALLHELPERS_HPP_

#include "j9cfg.h"

#if JAVA_SPEC_VERSION >= 21
#include <errno.h>
#if defined(WIN32)
/* Ignore the definition of UDATA as it is defined in <windows.h>. */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA /* This is safe because our UDATA is a typedef rather than a macro. */
#include <winsock2.h>
#endif /* defined(WIN32) */
#endif /* JAVA_SPEC_VERSION >= 21 */

#include "j9consts.h"

/* These bit values must match the corresponding values defined by
 * jdk.internal.foreign.abi.CapturableState.
 */
typedef enum {
	J9_CAPTURE_GET_LAST_ERROR = 1 << 0,
	J9_CAPTURE_WSA_LAST_ERROR = 1 << 1,
	J9_CAPTURE_ERRNO = 1 << 2,
#if defined(WIN32)
	J9_CAPTURE_ALL_STATES = J9_CAPTURE_GET_LAST_ERROR | J9_CAPTURE_WSA_LAST_ERROR | J9_CAPTURE_ERRNO
#else /* defined(WIN32) */
	J9_CAPTURE_ALL_STATES = J9_CAPTURE_ERRNO
#endif /* defined(WIN32) */
} DowncallCapturableState;

class ForeignCallHelpers
{
/*
 * Function members
 */
public:

#if JAVA_SPEC_VERSION >= 25
	/**
	 * Restore the selected native thread-local state values from the
	 * capture state buffer before a native downcall.
	 *
	 * @param returnState[in] pointer to the capture state buffer
	 * @param capturedCallStateMask[in] mask identifying the call state
	 * values to restore
	 */
	static VMINLINE void
	restoreCapturedCallState(I_32 *returnState, I_32 capturedCallStateMask)
	{
		if (NULL != returnState) {
#if defined(WIN32)
			if (J9_ARE_ANY_BITS_SET(capturedCallStateMask, J9_CAPTURE_GET_LAST_ERROR)) {
				SetLastError((DWORD)returnState[0]);
			}

			if (J9_ARE_ANY_BITS_SET(capturedCallStateMask, J9_CAPTURE_WSA_LAST_ERROR)) {
				WSASetLastError(returnState[1]);
			}

			if (J9_ARE_ANY_BITS_SET(capturedCallStateMask, J9_CAPTURE_ERRNO)) {
				errno = returnState[2];
			}
#else /* defined(WIN32) */
			if (J9_ARE_ANY_BITS_SET(capturedCallStateMask, J9_CAPTURE_ERRNO)) {
				errno = returnState[0];
			}
#endif /* defined(WIN32) */
		}
	}
#endif /* JAVA_SPEC_VERSION >= 25 */

#if JAVA_SPEC_VERSION >= 21
	/**
	 * Store the selected native thread-local state values to the
	 * capture state buffer after a native downcall.
	 *
	 * @param returnState[out] pointer to the capture state buffer
	 * @param capturedCallStateMask[in] mask identifying the call state
	 * values to save
	 */
	static VMINLINE void
	storeCapturedCallState(I_32 *returnState, I_32 capturedCallStateMask)
	{
		if (NULL != returnState) {
#if defined(WIN32)
			if (J9_ARE_ANY_BITS_SET(capturedCallStateMask, J9_CAPTURE_GET_LAST_ERROR)) {
				returnState[0] = (I_32)GetLastError();
			}

			if (J9_ARE_ANY_BITS_SET(capturedCallStateMask, J9_CAPTURE_WSA_LAST_ERROR)) {
				returnState[1] = WSAGetLastError();
			}

			if (J9_ARE_ANY_BITS_SET(capturedCallStateMask, J9_CAPTURE_ERRNO)) {
				returnState[2] = errno;
			}
#else /* defined(WIN32) */
			if (J9_ARE_ANY_BITS_SET(capturedCallStateMask, J9_CAPTURE_ERRNO)) {
				returnState[0] = errno;
			}
#endif /* defined(WIN32) */
		}
	}
#endif /* JAVA_SPEC_VERSION >= 21 */

};

#endif /* !defined(FOREIGNCALLHELPERS_HPP_) */

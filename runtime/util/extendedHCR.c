/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

UDATA
areExtensionsEnabled(J9JavaVM * vm)
{

	/* If -Xfuture is specified, adhere strictly to the specification */

	if (vm->runtimeFlags & J9_RUNTIME_XFUTURE) {
		return FALSE;
	}

#ifdef J9VM_INTERP_NATIVE_SUPPORT

#ifdef J9VM_JIT_FULL_SPEED_DEBUG

	/* Enable RedefineClass and RetransformClass extensions only if we are in full speed debug.
     * Currently this is always true since acquiring the redefine capability forces us into FSD.
	 */

    if (vm->jitConfig) {
		/* JIT is available and initialized for this platform */
		if (J9_FSD_ENABLED(vm)) {
			return TRUE;
		} else {
			/* but FSD is not enabled */
			return FALSE;
		}
	} else {

		/* JIT is not available, since we run in interpreted mode, no JIT fixups are necessary
		 * and we can allow extensions */

		return TRUE;
	}

#else

    /* We have JIT but no FSD, do not allow extensions */

    if (vm->jitConfig) {
    	return FALSE;
    }

#endif /* J9VM_JIT_FULL_SPEED_DEBUG */

#else

	/* No JIT, extensions are always allowed */

#endif /* J9VM_INTERP_NATIVE_SUPPORT */

    return TRUE;
}

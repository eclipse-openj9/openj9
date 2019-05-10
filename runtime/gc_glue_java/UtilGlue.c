/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

#include "j9cfg.h"
#include "j9lib.h"
#include "omr.h"
#include "j9vmutilnls.h"

#if defined(WIN32)
omr_error_t
OMR_Glue_GetVMDirectoryToken(void **token)
{
	/* Return the name of a DLL that resides in the VM directory. */
	*token = (void *)J9_PORT_DLL_NAME;
	return OMR_ERROR_NONE;
}
#endif /* defined(WIN32) */

/**
 * Provides the thread name to be used when no name is given.
 */
char *
OMR_Glue_GetThreadNameForUnamedThread(OMR_VMThread *vmThread)
{
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(vmThread);
	return (char *)omrnls_lookup_message(
			J9NLS_DO_NOT_APPEND_NEWLINE | J9NLS_DO_NOT_PRINT_MESSAGE_TAG,
			J9NLS_VMUTIL_THREAD_NAME_UNNAMED__MODULE,
			J9NLS_VMUTIL_THREAD_NAME_UNNAMED__ID,
			"(unnamed thread)");
}

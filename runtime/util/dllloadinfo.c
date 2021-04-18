/*******************************************************************************
 * Copyright (c) 2020, 2021 IBM Corp. and others
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
#include "jvminit.h"

/**
 * Retrieves the load info for the appropriate GC DLL based on reference mode.
 *
 * @param vm The Java VM
 * @returns J9VMDllLoadInfo for the GC DLL selected
 */
J9VMDllLoadInfo *
getGCDllLoadInfo(J9JavaVM *vm)
{
	J9VMDllLoadInfo *loadInfo = NULL;
	const char *gcDLLName = J9_GC_DLL_NAME;

#if defined(OMR_MIXED_REFERENCES_MODE_STATIC)
	if (!J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
		gcDLLName = J9_GC_FULL_DLL_NAME;
	}
#endif /* defined(OMR_MIXED_REFERENCES_MODE_STATIC) */

	loadInfo = FIND_DLL_TABLE_ENTRY(gcDLLName);

	return loadInfo;
}

/**
 * Retrieves the load info for the appropriate VERBOSE DLL based on reference mode.
 *
 * @param vm The Java VM
 * @returns J9VMDllLoadInfo for VERBOSE DLL selected
 */
J9VMDllLoadInfo *
getVerboseDllLoadInfo(J9JavaVM *vm)
{
	J9VMDllLoadInfo *loadInfo = NULL;
	const char *verboseDLLName = J9_VERBOSE_DLL_NAME;

#if defined(OMR_MIXED_REFERENCES_MODE_STATIC)
	if (!J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
		verboseDLLName = J9_VERBOSE_FULL_DLL_NAME;
	}
#endif /* defined(OMR_MIXED_REFERENCES_MODE_STATIC) */

	loadInfo = FIND_DLL_TABLE_ENTRY(verboseDLLName);

	return loadInfo;
}

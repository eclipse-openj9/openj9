/*******************************************************************************
 * Copyright IBM Corp. and others 2025
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

#include <windows.h>
#include "j9.h"

/**
 * Try to find the 'correct' windows temp directory.
 */
char *
getTmpDir(JNIEnv *env, char **tempdir)
{
	PORT_ACCESS_FROM_ENV(env);

	wchar_t unicodeBuffer[EsMaxPath];
	char *buffer = NULL;
	char *retVal = ".";
	DWORD rc = GetTempPathW(EsMaxPath, unicodeBuffer);

	if ((0 != rc) && (rc < EsMaxPath)) {
		/* convert */
		rc = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeBuffer, -1, NULL, 0, NULL, NULL);
		if (0 != rc) {
			buffer = j9mem_allocate_memory(rc, OMRMEM_CATEGORY_VM);
			if (NULL != buffer) {
				rc = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeBuffer, -1, buffer, rc, NULL, NULL);
				if (0 == rc) {
					j9mem_free_memory(buffer);
					buffer = NULL;
				} else {
					retVal = buffer;
				}
			}
		}
	}
	*tempdir = buffer;
	return retVal;
}

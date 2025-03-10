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

#include <stdio.h>
#include "j9.h"

/**
 * Try to find the 'correct' unix temp directory, as taken from the man page for tmpnam.
 * As a last resort, '.' representing the current directory is returned.
 */
char *
getTmpDir(JNIEnv *env, char **envSpace)
{
	PORT_ACCESS_FROM_ENV(env);
	I_32 envSize = j9sysinfo_get_env("TMPDIR", NULL, 0);
	if (envSize > 0) {
		*envSpace = j9mem_allocate_memory(envSize, OMRMEM_CATEGORY_VM); /* trailing null taken into account */
		if (NULL == *envSpace) {
			return ".";
		}
		j9sysinfo_get_env("TMPDIR", *envSpace, envSize);
		if (j9file_attr(*envSpace) > -1) {
			return *envSpace;
		}
		/* directory was not there, free up memory and continue */
		j9mem_free_memory(*envSpace);
		*envSpace = NULL;
	}
	if (j9file_attr(P_tmpdir) > -1) {
		return P_tmpdir;
	}
	if (j9file_attr("/tmp") > -1) {
		return "/tmp";
	}
	return ".";
}

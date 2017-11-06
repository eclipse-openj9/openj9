/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <string.h>

#include "j9.h"
#include "j9consts.h"
#include "jni.h"
#include "j9protos.h"

#define JIMAGE_EXTENSION "jimage"

/* this file is owned by the VM-team.  Please do not modify it without consulting the VM team */

#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT))


char* getDefaultBootstrapClassPath(J9JavaVM * vm, char* javaHome)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	char separator = (char) j9sysinfo_get_classpathSeparator();
	extern const char* const jclBootstrapClassPath[];
	extern char* jclBootstrapClassPathAllocated[];
	char **entry = NULL;
	char *path = NULL;
	char *lastEntry = NULL;
	UDATA pathLength = 0;
	UDATA javaHomeLength = strlen(javaHome);

	for (entry = GLOBAL_TABLE(jclBootstrapClassPath); NULL != *entry; ++entry)	{
		lastEntry = *entry;

		pathLength += strlen(lastEntry);

		/*
		 * Entries that start with ! don't need our attention:
		 * The space for the exclamation mark will be consumed
		 * by a path separator.
		 */
		if ('!' != lastEntry[0]) {
			pathLength += javaHomeLength;
			pathLength += 1; /* for separator or NUL-terminator */
			pathLength += sizeof("/lib/") - 1;
		}
	}

	/* Always create space enough for the NUL-terminator. */
	if (0 == pathLength) {
		pathLength = 1;
	}

	path = j9mem_allocate_memory(pathLength, J9MEM_CATEGORY_VM_JCL);
	if (NULL != path) {
		UDATA i = 0;
		char* subPath = path;

		/* Always NUL-terminate the path. */
		path[0] = '\0';

		for (entry = GLOBAL_TABLE(jclBootstrapClassPath), i = 0; NULL != *entry; ++entry, ++i) {
			UDATA subLength = 0;

			/* add a separator before the second and subsequent entries */
			if (0 != i) {
				*subPath = separator;
				subPath += 1;
				pathLength -= 1;
			}

			lastEntry = *entry;

			if ('!' == lastEntry[0]) {
				j9str_printf(PORTLIB, subPath, (U_32)pathLength, "%s", lastEntry + 1);
				j9mem_free_memory(*entry);
			} else {
				j9str_printf(PORTLIB, subPath, (U_32)pathLength,
								"%s" DIR_SEPARATOR_STR "lib" DIR_SEPARATOR_STR "%s",
								/* classes.zip */ javaHome, *entry);
				/* if this entry was allocated then free it as we will not use it
				 * after this point */
				if (NULL != jclBootstrapClassPathAllocated[i]) {
					j9mem_free_memory(jclBootstrapClassPathAllocated[i]);
				}
			}
			*entry = NULL;
			jclBootstrapClassPathAllocated[i] = NULL;

			subLength = strlen(subPath);
			subPath += subLength;
			pathLength -= subLength;
		}
	}

	return path;
}

#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */

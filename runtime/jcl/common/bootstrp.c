/*******************************************************************************
 * Copyright (c) 1998, 2016 IBM Corp. and others
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

#include <string.h>
#include <ctype.h>

#include "j9.h"
#include "j9consts.h"
#include "jni.h"
#include "j9protos.h"
#include "jvminit.h"

#include "ut_j9jcl.h"

/* this file is owned by the VM-team.  Please do not modify it without consulting the VM team */

#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT))

char* catPaths(J9PortLibrary* portLib, char* path1, char* path2) {
	char* newPath;
	UDATA newPathLength;
	PORT_ACCESS_FROM_PORT(portLib);

	newPathLength = strlen(path1) + strlen(path2) + 2;
	newPath = j9mem_allocate_memory(newPathLength, J9MEM_CATEGORY_VM_JCL);
	if (newPath) {
		j9str_printf(PORTLIB, newPath, (U_32)newPathLength, "%s%c%s", path1, (char) j9sysinfo_get_classpathSeparator(), path2);
	}
	return newPath;
}

#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */


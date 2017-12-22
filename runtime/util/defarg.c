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

#include <string.h>
#include "j9user.h"
#include "util_internal.h"
#include "ut_j9util.h"

/* parse arg to determine if it is of the form -Darg=foo, and return foo.
 * Returns an empty string for args of the form -Darg,
 * Returns NULL if the argument is not recognized
 */
char *
getDefineArgument(char* arg, char* key)
{
	Trc_Util_getDefineArgument_Entry(arg, key);
	if (arg[0] == '-' && arg[1] == 'D') {
		size_t keyLength = strlen(key);

		if (strncmp(&arg[2], key, keyLength) == 0) {
			switch (arg[2 + keyLength]) {
			case '=': 
				Trc_Util_getDefineArgument_Exit(&arg[3 + keyLength]);
				return &arg[3 + keyLength];
			case '\0': 
				Trc_Util_getDefineArgument_Empty();
				return "";
			}
		}
	}

	Trc_Util_getDefineArgument_NotFound();
	return NULL;
}



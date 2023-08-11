/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

#include <string.h>
#include "j9user.h"
#include "util_internal.h"
#include "ut_j9util.h"

static const char* getDefineArgument(const char *arg, const char *key, const size_t keyLength);

/**
 * Parse arg to determine if it is of the form -Darg=foo, and return foo.
 * Returns an empty string for args of the form -Darg.
 * Returns NULL if the argument is not recognized.
 *
 * @param arg the defined argument
 * @param key the argument key
 * @param keyLength the key length
 *
 * @return the argument value or NULL
 */
static const char*
getDefineArgument(const char *arg, const char *key, const size_t keyLength)
{
	const char *result = NULL;

	Trc_Util_getDefineArgument_Entry(arg, key);
	if (('-' == arg[0]) && ('D' == arg[1])) {
		if (0 == strncmp(&arg[2], key, keyLength)) {
			switch (arg[2 + keyLength]) {
			case '=':
				Trc_Util_getDefineArgument_Exit(&arg[3 + keyLength]);
				result = &arg[3 + keyLength];
				break;
			case '\0':
				Trc_Util_getDefineArgument_Empty();
				result = "";
				/* Fall through case !!! */
			default:
				break;
			}
		}
	}

	if (NULL == result) {
		Trc_Util_getDefineArgument_NotFound();
	}
	return result;
}

const char*
getDefinedArgumentFromJavaVMInitArgs(JavaVMInitArgs *vmInitArgs, const char *defArg)
{
	const char *result = NULL;
	const size_t defArgLength = strlen(defArg);
	jint count = 0;

	Trc_Util_getDefinedArgumentFromJavaVMInitArgs_Entry(vmInitArgs, defArg);
	for (count = (vmInitArgs->nOptions - 1); count >= 0; count--) {
		JavaVMOption *option = &(vmInitArgs->options[count]);
		result = getDefineArgument(option->optionString, defArg, defArgLength);
		if (NULL != result){
			break;
		}
	}
	Trc_Util_getDefinedArgumentFromJavaVMInitArgs_Exit(result);
	return result;
}

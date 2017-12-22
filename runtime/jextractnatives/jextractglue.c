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


#include "j9dbgext.h"
#include "j9comp.h"
#include "j9protos.h"
#include "jextractnatives_internal.h"
#include <string.h>
#include <ctype.h>

static char const *notVisibleMsg =
		"Error: J9SIZEOF_%s was not visible at compile time\n"
		"to fix this, add %s to cVisibleStructs (j9.h) and recompile %s\n"
		"\n"
		"(or if %s is in a different header, include that header in %s)";
#define PRINT_NOT_VISIBLE_MSG(x)  dbgPrint(notVisibleMsg, x, x, __FILE__, x, __FILE__)

void
run_command( const char *cmd )
{
	size_t len = 0;
	const char* options;

	while(cmd[len] && !isspace(cmd[len])) {
		len++;
	}

	options = cmd + len;
	while (isspace(*options)) {
		options++;
	}

	if( (len == sizeof("!j9help") - 1) && (memcmp( cmd, "!j9help", sizeof("!j9help") - 1 ) == 0) ) {
		dbgext_j9help(options);
		return;
	}

	if( (len == sizeof("!findvm") - 1) && (memcmp( cmd, "!findvm", sizeof("!findvm") - 1 ) == 0) ) {
		dbgext_findvm(options);
		return;
	}

	if( (len == sizeof("!trprint") - 1) && (memcmp( cmd, "!trprint", sizeof("!trprint") - 1 ) == 0) ) {
		dbgext_trprint(options);
		return;
	}

	if( (len == sizeof("!setvm") - 1) && (memcmp( cmd, "!setvm", sizeof("!setvm") - 1 ) == 0) ) {
		dbgext_setvm(options);
		return;
	}


	dbgPrint("Unknown J9 plugin command %s\n", cmd);

}

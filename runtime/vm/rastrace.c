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
#include "j9trace.h"
#include "jvminit.h"
#include <string.h>
#include "vm_internal.h"


#define VMOPT_XTRACE		 "-Xtrace"
#define VMOPT_XTRACE_NONE	 "-Xtrace:none"

#define VMOPT_XTRACE_DEFAULT VMOPT_XTRACE

IDATA 
configureRasTrace(J9JavaVM *vm, J9VMInitArgs *j9vm_args)
{
	IDATA xtraceIndex;
	char *optionString;

	/* things to decide:
	 * 		1) if there is no -Xtrace option on the command line, do the platform default.
	 * else, we have some -Xtrace options to parse:
	 * 		2) if -Xtrace:none is the rightmost trace option:
	 * 				consume all and
	 * 				do not load trace
	 * 		2) else
	 * 				consume the -Xtrace arguments
	 * 				load trace - we must have found a list of trace options
	 */ 
	xtraceIndex = FIND_ARG_IN_VMARGS(OPTIONAL_LIST_MATCH, VMOPT_XTRACE, NULL);
	
	if (xtraceIndex < 0){
		/* no trace options are specified, so use the platform default */
		optionString = VMOPT_XTRACE_DEFAULT;
	} else {
		/* we only need to check the right most argument for -Xtrace:none 
		 * xtraceIndex is currently pointing at the right most argument by the way */
		optionString = j9vm_args->actualVMArgs->options[xtraceIndex].optionString;	
	}
		
	if ( strcmp(optionString, VMOPT_XTRACE_NONE) != 0 ) {
		/* if that argument is not -Xtrace:none load trace */
		J9VMDllLoadInfo *entry = FIND_DLL_TABLE_ENTRY( J9_RAS_TRACE_DLL_NAME );
		entry->loadFlags |= EARLY_LOAD;
	} else {
		/* either the right most argument was -Xtrace:none or the platform default is
	 	 * -Xtrace:none, so simply fall through to consume any args without asking 
	 	 * the trace engine to load
	 	 */
	}
	
	while (xtraceIndex >= 0){
		/* consume the -Xtrace: command line arguments they will be parsed by ute if its active */
		CONSUME_ARG(j9vm_args, xtraceIndex);
		xtraceIndex = FIND_NEXT_ARG_IN_VMARGS(OPTIONAL_LIST_MATCH, VMOPT_XTRACE, NULL, xtraceIndex);
	}
	return JNI_OK;
}

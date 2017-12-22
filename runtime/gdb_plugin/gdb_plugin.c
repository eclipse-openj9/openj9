/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#include "gdb_plugin.h"
#include <string.h>
#include "plugin-interface.h"
#include "j9port.h"


/* add_commands() is defined in gdb_plugin_glue.c */
extern void add_commands(struct plugin_service_ops *plugin_service);


struct plugin_service_ops *plugin_service;

int
plugin_session_init(int session_id, struct plugin_service_ops *a_plugin_service)
{
	plugin_service = a_plugin_service;

	add_commands(plugin_service);

	plugin_service->print_output("J9 GDB plugin loaded\n");
	plugin_service->print_output("Type j9help for usage information\n");
	//dbgext_startddr(NULL);
	return 0;
}

int
plugin_get_version(void)
{
  return PLUGIN_CURRENT_VERSION;
}

const char *
plugin_get_name(void)
{
  return "j9gdb";
}



void 
dbgReadMemory (UDATA address, void *structure, UDATA size, UDATA *bytesRead)
{
	UDATA count = 0;

	if (plugin_service->read_inferior_memory((void*) address, structure, size) == 0) {
		*bytesRead = size;
		return;
	} else {
		*bytesRead = 0;
		return;
	}
}


UDATA dbgGetExpression (const char* args)
{
	UDATA result;
	
    result = (UDATA) plugin_service->parse_and_eval_address((char *) args);

	return result;
}


/*
 * See dbgFindPatternInRange
 */
void* 
dbgFindPattern(U_8* pattern, UDATA patternLength, UDATA patternAlignment, U_8* startSearchFrom, UDATA* bytesSearched) 
{
#if defined(J9VM_ENV_DATA64)
	*bytesSearched = 0;
	return NULL;
#else
	return dbgFindPatternInRange(pattern, patternLength, patternAlignment, startSearchFrom, (UDATA)-1 - (UDATA)startSearchFrom, bytesSearched);
#endif
}



/*
 * Prints a message to the debugger output
 */
void 
dbgWriteString (const char* message)
{
#ifdef IBM_ATOE
	char *conv = a2e_string( message );
	plugin_service->print_output("%s", conv);
	free(conv);
#else
	plugin_service->print_output("%s", message);
#endif
}

I_32
dbg_j9port_create_library(J9PortLibrary *portLib, J9PortLibraryVersion *version, UDATA size)
{
	return j9port_create_library(portLib, version, size);
}

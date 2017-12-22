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


#include "gdb_plugin.h"
#ifdef IBM_ATOE
#include <atoe.h>
#endif

#include "plugin-interface.h"


void
wrap_dbgext_j9help(char *args, int from_tty)
{
	dbgext_j9help(args ? args : "" );
}


void
wrap_dbgext_findvm(char *args, int from_tty)
{
	dbgext_findvm(args ? args : "" );
}


void
wrap_dbgext_trprint(char *args, int from_tty)
{
	dbgext_trprint(args ? args : "" );
}


void
wrap_dbgext_setvm(char *args, int from_tty)
{
	dbgext_setvm(args ? args : "" );
}


void
add_commands(struct plugin_service_ops *plugin_service)
{

	plugin_service->add_command("j9help", class_plugin, wrap_dbgext_j9help, "j9help");
	plugin_service->add_command("findvm", class_plugin, wrap_dbgext_findvm, "findvm");
	plugin_service->add_command("trprint", class_plugin, wrap_dbgext_trprint, "trprint");
	plugin_service->add_command("setvm", class_plugin, wrap_dbgext_setvm, "setvm");
}

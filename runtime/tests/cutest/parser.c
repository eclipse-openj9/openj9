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
#include "j9.h"
#include "j9port.h"
#include "CuTest.h"

/*
 * Search for a cutest optionson the command line. Elements in argv
 * are searched in reverse order. The first element is always ignored (typically
 * the first argv element is the program name, not a command line option).
 *
 * If one is found, return its index, otherwise return 0.
 */
UDATA
cutest_parseCmdLine( J9PortLibrary *portLibrary, UDATA lastLegalArg , char **argv )
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA i;

	for (i = lastLegalArg; i >= 1; i--)  {
		/* new style -Xcheck:memory options */
		if (0 == strcmp("-verbose", argv[i])) {
			verbose = 1;
			j9tty_err_printf(PORTLIB, "cutest: verbose output enabled.\n");
			return i;
		}
	}
	return 0;
}

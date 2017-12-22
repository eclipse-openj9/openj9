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


#include <stdlib.h>
#include "j9port.h"

UDATA
genericSignalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData)
{


	PORT_ACCESS_FROM_PORT(portLibrary);
	U_32 category;

	j9tty_printf(PORTLIB, "\nAn unhandled error (%d) has occurred.\n", gpType);
	for (category=0 ; category<J9PORT_SIG_NUM_CATEGORIES ; category++) {
		U_32 infoCount = j9sig_info_count(gpInfo, category) ;
		U_32 infoKind, index;
		void *value;
		const char* name;

		for (index=0 ; index < infoCount ; index++) {
			infoKind = j9sig_info(gpInfo, category, index, &name, &value);

			switch (infoKind) {
				case J9PORT_SIG_VALUE_32:
					j9tty_printf(PORTLIB, "%s=%08.8x\n", name, *(U_32 *)value);
					break;
				case J9PORT_SIG_VALUE_64:
				case J9PORT_SIG_VALUE_FLOAT_64:
					j9tty_printf(PORTLIB, "%s=%016.16llx\n", name, *(U_64 *)value);
					break;
				case J9PORT_SIG_VALUE_STRING:
					j9tty_printf(PORTLIB, "%s=%s\n", name, (const char *)value);
					break;
				case J9PORT_SIG_VALUE_ADDRESS:
					j9tty_printf(PORTLIB, "%s=%p\n", name, *(void**)value);
					break;
			}
		}
	}

	abort();
	
	/* UNREACHABLE */
	return 0;
}

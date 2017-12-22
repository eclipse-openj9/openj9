/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef LIBHLP_H
#define LIBHLP_H

/* @ddr_namespace: default */
#ifdef __cplusplus
extern "C" {
#endif

#include "j9port.h"

#define J9CMDLINE_OK 0
#define J9CMDLINE_PARSE_ERROR 1
#define J9CMDLINE_OUT_OF_MEMORY 2
#define J9CMDLINE_GENERIC_ERROR 3

struct j9cmdlineOptions {
	int argc;
	char** argv;
	char** envp;
	J9PortLibrary* portLibrary;
	BOOLEAN shutdownPortLib;
};

#ifdef __cplusplus
}
#endif

#endif /* LIBHLP_H */

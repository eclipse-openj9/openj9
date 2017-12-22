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
#ifndef j9portpg_h
#define j9portpg_h
/* @ddr_namespace: map_to_type=J9PortpgConstants */
typedef struct STFLEFacilities {
	uint64_t dw1;
	uint64_t dw2;
	uint64_t dw3;
} STFLEFacilities;

typedef struct J9STFLECache {
	uintptr_t lastDoubleWord;
	STFLEFacilities facilities;
} J9STFLECache;

typedef struct J9PortPlatformGlobals {
	J9STFLECache stfleCache;
	const void * j9csrsiSession;
	int32_t j9csrsiRetCode;
} J9PortPlatformGlobals;

#define PPG_stfleCache (portLibrary->portGlobals->platformGlobals.stfleCache)

#define PPG_j9csrsi_session (portLibrary->portGlobals->platformGlobals.j9csrsiSession)
#define PPG_j9csrsi_ret_code (portLibrary->portGlobals->platformGlobals.j9csrsiRetCode)

#endif /* j9portpg_h */


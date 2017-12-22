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
/* from windef.h */
typedef void *HANDLE;

typedef struct J9PortPlatformGlobals {
	BOOLEAN sock_IPv6_function_support;
	HANDLE shmem_creationMutex;
	HANDLE shsem_creationMutex;
#if defined(J9_USE_CONNECTION_MANAGER)
	HANDLE sock_connection;
#endif
	int si_l1DCacheLineSize;
} J9PortPlatformGlobals;

#define PPG_sock_IPv6_FUNCTION_SUPPORT (portLibrary->portGlobals->platformGlobals.sock_IPv6_function_support)
#define PPG_shmem_creationMutex (portLibrary->portGlobals->platformGlobals.shmem_creationMutex)
#define PPG_shsem_creationMutex (portLibrary->portGlobals->platformGlobals.shsem_creationMutex)
#define PPG_sock_connection (portLibrary->portGlobals->platformGlobals.sock_connection)
#define PPG_sysL1DCacheLineSize (portLibrary->portGlobals->platformGlobals.si_l1DCacheLineSize)
#endif /* j9portpg_h */

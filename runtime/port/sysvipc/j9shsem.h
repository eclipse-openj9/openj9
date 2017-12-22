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
#ifndef j9shsem_h
#define j9shsem_h

/* @ddr_namespace: default */
#include <sys/types.h>
#include <errno.h>

#include "j9comp.h"

/*j9shsem_baseFileFormat is only used for opening deprecated (aka old format) control file*/
typedef struct j9shsem_baseFileFormat {
	int32_t version;
	int32_t modlevel;
	int32_t timeout;
	int32_t proj_id;
	key_t ftok_key;
	int32_t semid;
	int32_t creator_pid;
	int32_t semsetSize;
} j9shsem_baseFileFormat;

typedef struct j9shsem_handle {
	int32_t semid;
	int32_t nsems;
	char* baseFile;
	int64_t timestamp;
 	uint32_t deleteBasefile : 1; /* delete the base file (used to generate the semaphore key) when destroying the semaphore */
	/* int32_t baseFilefd deleted: it is never assigned or used. */
} j9shsem_handle;

#define J9SHSEM_SEMFLAGS (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)
#define J9SHSEM_SEMFLAGS_GROUP (IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define J9SHSEM_SEMFLAGS_OPEN (S_IRUSR | S_IWUSR)
#define J9SHSEM_SEMFLAGS_GROUP_OPEN (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

/* for zOS we have a function to get extended error code - useful for debugging */
#if !defined(J9ZOS390)
#define __errno2() 0
#endif

#endif     /* j9shsem_h */



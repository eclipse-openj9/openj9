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
#ifndef j9_sysv_wrappers_h
#define j9_sysv_wrappers_h

#include <stddef.h>
#include "portpriv.h"
#include "j9port.h"
#include "portnls.h"
#include "ut_j9prt.h"
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#if defined (J9ZOS390)
#include <sys/__getipc.h>
#endif


typedef union {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
} semctlUnion;

/*ftok*/
int ftokWrapper(J9PortLibrary *portLibrary, const char *baseFile, int proj_id);

/*semaphores*/
int semgetWrapper(J9PortLibrary *portLibrary, key_t key, int nsems, int semflg);
int semctlWrapper(J9PortLibrary *portLibrary, BOOLEAN storeError, int semid, int semnum, int cmd, ...);
int semopWrapper(J9PortLibrary *portLibrary, int semid, struct sembuf *sops, size_t nsops);

/*memory*/
int shmgetWrapper(J9PortLibrary *portLibrary, key_t key, size_t size, int shmflg);
int shmctlWrapper(J9PortLibrary *portLibrary, BOOLEAN storeError, int shmid, int cmd, struct shmid_ds *buf);
void * shmatWrapper(J9PortLibrary *portLibrary, int shmid, const void *shmaddr, int shmflg);
int shmdtWrapper(J9PortLibrary *portLibrary, const void *shmaddr);

/*z/OS sysv helper function*/
#if defined (J9ZOS390)
int getipcWrapper(J9PortLibrary *portLibrary, int id, IPCQPROC *info, size_t length, int cmd);
#endif

#endif

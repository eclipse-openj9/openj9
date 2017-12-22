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

#if !defined(PROCESSHELPER_H_INCLUDED)
#define PROCESSHELPER_H_INCLUDED
#include "j9port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHRTEST_MAX_PATH 256
#define SHRTEST_MAX_CMD_OPTS 15

IDATA openLaunchSemaphore (J9PortLibrary* portLibrary, const char* name, UDATA nProcess);
IDATA ReleaseLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore, UDATA nProcess);
IDATA WaitForLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore);
IDATA CloseLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore);

J9ProcessHandle LaunchChildProcess (J9PortLibrary* portLibrary, const char* testname, char * newargv[SHRTEST_MAX_CMD_OPTS], UDATA newargc);
IDATA WaitForTestProcess (J9PortLibrary* portLibrary, J9ProcessHandle pid);

void SleepFor(IDATA second);

#ifdef __cplusplus
}
#endif
#endif /* !defined(PROCESSHELPER_H_INCLUDED) */

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
#if !defined(TESTPROCESSHELPER_H_INCLUDED)
#define TESTPROCESSHELPER_H_INCLUDED
#include "j9port.h"

IDATA openLaunchSemaphore (J9PortLibrary* portLibrary, const char* name, UDATA nProcess);
IDATA SetLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore, UDATA nProcess);
IDATA ReleaseLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore, UDATA nProcess);
IDATA WaitForLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore);
IDATA CloseLaunchSemaphore (J9PortLibrary* portLibrary, IDATA semaphore);

J9ProcessHandle launchChildProcess (J9PortLibrary* portLibrary, const char* testname, const char* argv0, const char* options);
IDATA waitForTestProcess (J9PortLibrary *portLibrary, J9ProcessHandle processHandle);
void SleepFor(IDATA second);

IDATA writeToProcess(J9PortLibrary *portLibrary, char *testName, J9ProcessHandle processHandle, char *message, UDATA lenByteMessage);
IDATA pollProcessForComplete(J9PortLibrary *portLibrary, char *testName, J9ProcessHandle processHandle, I_64 timeoutMillis);
IDATA readFromProcess(J9PortLibrary *portLibrary, char *testName, J9ProcessHandle processHandle, UDATA stream, char *buffer, UDATA lenBytesBuffer);
#endif /* !defined(TESTPROCESSHELPER_H_INCLUDED) */


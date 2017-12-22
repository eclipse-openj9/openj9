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

#include "j9protos.h"
#include "util_api.h"
#include "j9port.h"

static I_32 
testSignature(J9PortLibrary *portLib, const char* sig, UDATA expectedSlots, UDATA *passCount, UDATA *failCount);

I_32 
verifySendSlots(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount) 
{
	I_32 rc = 0;
	PORT_ACCESS_FROM_PORT(portLib);

	j9tty_printf(PORTLIB, "Testing getSendSlotsFromSignature()...\n");

	rc |= testSignature(portLib, "()V", 0, passCount, failCount);
	rc |= testSignature(portLib, "(B)V", 1, passCount, failCount);
	rc |= testSignature(portLib, "(Z)V", 1, passCount, failCount);
	rc |= testSignature(portLib, "(C)V", 1, passCount, failCount);
	rc |= testSignature(portLib, "(S)V", 1, passCount, failCount);
	rc |= testSignature(portLib, "(I)V", 1, passCount, failCount);
	rc |= testSignature(portLib, "(F)V", 1, passCount, failCount);
	rc |= testSignature(portLib, "(D)V", 2, passCount, failCount);
	rc |= testSignature(portLib, "(J)V", 2, passCount, failCount);
	rc |= testSignature(portLib, "(Ljava/lang/String;)V", 1, passCount, failCount);
	rc |= testSignature(portLib, "([Ljava/lang/String;)V", 1, passCount, failCount);
	rc |= testSignature(portLib, "([[Ljava/lang/String;)V", 1, passCount, failCount);
	rc |= testSignature(portLib, "(Ljava/lang/String;Z)V", 2, passCount, failCount);
	rc |= testSignature(portLib, "([Ljava/lang/String;Z)V", 2, passCount, failCount);
	rc |= testSignature(portLib, "([[Ljava/lang/String;Z)V", 2, passCount, failCount);
	rc |= testSignature(portLib, "(DZ)V", 3, passCount, failCount);
	rc |= testSignature(portLib, "([DZ)V", 2, passCount, failCount);
	rc |= testSignature(portLib, "([[DZ)V", 2, passCount, failCount);
	rc |= testSignature(portLib, "(Ljava/lang/String;IZ[[DD)Ljava/lang/StringBuffer;", 6, passCount, failCount);

	j9tty_printf(PORTLIB, "Finished getSendSlotsFromSignature().\n");

	return rc;
}



static I_32 
testSignature(J9PortLibrary *portLib, const char* sig, UDATA expectedSlots, UDATA *passCount, UDATA *failCount)
{
	PORT_ACCESS_FROM_PORT(portLib);
	UDATA result;

	result = getSendSlotsFromSignature((const U_8*)sig);

	if (result != expectedSlots) {
		j9tty_err_printf(PORTLIB, "Incorrect result for \"%s\". Expected %u, got %u\n", sig, expectedSlots, result);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	return 0;
}




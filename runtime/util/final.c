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

#include "j9.h"
#include <string.h>
#include "util_internal.h"


UDATA methodIsFinalInObject(UDATA nameLength, U_8* name, UDATA sigLength, U_8* sig) {

	const char names[] = "wait\0" "wait\0" "wait\0" "notify\0" "notifyAll\0" "getClass\0";
	const U_8 nameLengths[] = {
		sizeof("wait") - 1, sizeof("wait") - 1, sizeof("wait") - 1,
		sizeof("notify") - 1, sizeof("notifyAll") - 1, sizeof("getClass") - 1 };
	const char sigs[] = "()V\0" "(J)V\0" "(JI)V\0" "()V\0" "()V\0" "()Ljava/lang/Class;\0";
	const U_8 sigLengths[] = {
		sizeof("()V") - 1, sizeof("(J)V") - 1, sizeof("(JI)V") - 1,
		sizeof("()V") - 1, sizeof("()V") - 1, sizeof("()Ljava/lang/Class;") - 1 };
#define OBJECT_FINAL_COUNT 6
#define SHORTEST_METHOD_NAME (sizeof("wait") - 1)
#define LONGEST_METHOD_NAME (sizeof("notifyAll") - 1)

	int i;

	const char* thisName = names;
	const char* thisSig = sigs;

	if ((nameLength < SHORTEST_METHOD_NAME) || (nameLength > LONGEST_METHOD_NAME)) {
		return 0;
	}

	for (i = 0; i < OBJECT_FINAL_COUNT; i++) {
		UDATA nameLen = nameLengths[i];
		UDATA sigLen = sigLengths[i];
		/* we should really be doing UTF8 compares here, since the UTF8s may not be canonical */
		if ((nameLength == nameLen)
		&& (sigLength == sigLen)
		&& (memcmp(name, thisName, nameLen) == 0)
		&& (memcmp(sig, thisSig, sigLen) == 0)
		) {
			return 1;
		}
		thisName += nameLengths[i] + 1;
		thisSig += sigLengths[i] + 1;
	}

#undef SHORTEST_METHOD_NAME
#undef LONGEST_METHOD_NAME
#undef OBJECT_FINAL_COUNT
	return 0;
}




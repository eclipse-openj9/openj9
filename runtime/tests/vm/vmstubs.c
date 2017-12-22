/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#include "j9comp.h"
#include "j9.h"

void
setCurrentException(J9VMThread *currentThread, UDATA exceptionNumber, UDATA *detailMessage)
{
	/* NOTE: Stub function. */
}

void
setCurrentExceptionUTF(J9VMThread * vmThread, UDATA exceptionNumber, const char * detailUTF)
{
	/* NOTE: Stub function. */
}

struct J9Class *
internalFindClassUTF8(J9VMThread *currentThread, U_8 *className, UDATA classNameLength, J9ClassLoader *classLoader, UDATA options)
{
	/* NOTE: Stub function. */
	return NULL;
}

J9VMThread *
currentVMThread(J9JavaVM * vm)
{
	/* NOTE: Stub function. */
	return NULL;
}

UDATA
initializeMonitorTable(J9VMThread *vmThread)
{
	/* NOTE: Stub function. */
	return 0;
}

void
destroyMonitorTable(J9JavaVM* vm)
{
	/* NOTE: Stub function. */
}

void
destroyOMRVMThread(J9JavaVM *vm, J9VMThread *vmThread)
{
	/* NOTE: Stub function. */
}

char *
illegalAccessMessage(J9VMThread *currentThread, IDATA badMemberModifier, J9Class *senderClass, J9Class *targetClass, IDATA errorType)
{
	/* NOTE: Stub function. */
	return NULL;
}

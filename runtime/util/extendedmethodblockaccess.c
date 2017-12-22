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

#include "j9protos.h"
#include "j9cp.h"
#include "util_internal.h"


U_8 * 
fetchMethodExtendedFlagsPointer(J9Method* method)
{
	J9Class* clazz = J9_CLASS_FROM_METHOD(method);
	UDATA index = method - clazz->ramMethods;

	return ((U_8 *) clazz->ramMethods) - 1 - index;
}


void
setExtendedMethodFlags(J9JavaVM * vm, U_8 * mtFlag, U_8 flags)
{
	omrthread_monitor_enter(vm->extendedMethodFlagsMutex);
	*mtFlag |= flags;
	omrthread_monitor_exit(vm->extendedMethodFlagsMutex);
}


void
clearExtendedMethodFlags(J9JavaVM * vm, U_8 * mtFlag, U_8 flags)
{
	omrthread_monitor_enter(vm->extendedMethodFlagsMutex);
	*mtFlag &= ~flags;
	omrthread_monitor_exit(vm->extendedMethodFlagsMutex);
}


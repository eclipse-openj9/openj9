/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "j9.h"
#include "ut_j9jcl.h"



UDATA
triggerClassPreinitializeEvent(J9VMThread* vmThread, J9Class* clazz)
{
	UDATA failed = FALSE;

	TRIGGER_J9HOOK_VM_CLASS_PREINITIALIZE(vmThread->javaVM->hookInterface, vmThread, clazz, failed);

	return failed;
}

void
triggerClassInitializeFailedEvent(J9VMThread* vmThread, J9Class* clazz)
{
	J9ROMClass* romClass = clazz->romClass;
	J9UTF8* name = J9ROMCLASS_CLASSNAME(romClass);
	Trc_JCL_triggerClassInitializeFailedEvent(vmThread, clazz, (UDATA)name->length, name->data);

	TRIGGER_J9HOOK_VM_CLASS_INITIALIZE_FAILED(vmThread->javaVM->hookInterface, vmThread, clazz);
}

void
triggerClassInitializeEvent(J9VMThread* vmThread, J9Class* clazz)
{
	TRIGGER_J9HOOK_VM_CLASS_INITIALIZE(vmThread->javaVM->hookInterface, vmThread, clazz);
}

void
triggerClassLoaderInitializedEvent(J9VMThread* vmThread, J9ClassLoader* classLoader)
{
	TRIGGER_J9HOOK_VM_CLASS_LOADER_INITIALIZED(vmThread->javaVM->hookInterface, vmThread, classLoader);
}

void  
triggerClassPrepareEvent(J9VMThread* currentThread, J9Class* clazz)
{
	ALWAYS_TRIGGER_J9HOOK_VM_CLASS_PREPARE(currentThread->javaVM->hookInterface, currentThread, clazz);
}

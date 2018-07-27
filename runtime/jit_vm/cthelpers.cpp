/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
#include "j9protos.h"
#include "j9consts.h"
#include "rommeth.h"
#include "VMHelpers.hpp"
#include "AtomicSupport.hpp"

extern "C" {

void*
jitGetCountingSendTarget(J9VMThread *vmThread, J9Method *ramMethod)
{
	J9ROMMethod* romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
	J9JavaVM * vm = vmThread->javaVM;
	U_32 modifiers = romMethod->modifiers;

	if (VM_VMHelpers::calculateStackUse(romMethod) > 32) {
		return J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_LARGE);
	}


	if (modifiers & J9AccSynchronized) {
		if (modifiers & J9AccStatic) {
			return J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_SYNC_STATIC);
		}
		return J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_SYNC);
	}
	if (J9ROMMETHOD_IS_OBJECT_CONSTRUCTOR(romMethod)) {
		if (J9ROMMETHOD_IS_EMPTY(romMethod)) {
			return J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_EMPTY_OBJ_CTOR);
		}
		return J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_OBJ_CTOR);
	}
	return J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_NON_SYNC);
}

J9Class*
jitGetInterfaceITableIndexFromCP(J9VMThread *currentThread, J9ConstantPool *constantPool, UDATA cpIndex, UDATA* pITableIndex)
{
	J9RAMInterfaceMethodRef *ramMethodRef = (J9RAMInterfaceMethodRef*)constantPool + cpIndex;
	/* interfaceClass is used to indicate 'resolved'. Make sure to read it first */
	J9Class* volatile interfaceClass = (J9Class*)ramMethodRef->interfaceClass;
	VM_AtomicSupport::readBarrier();
	UDATA volatile methodIndexAndArgCount = ramMethodRef->methodIndexAndArgCount;
	/* If the ref is resolved, do not call the resolve helper, as it will currently fail if the interface class is not initialized */
	if (NULL == interfaceClass) {
		J9RAMInterfaceMethodRef localEntry;
		if (0 == currentThread->javaVM->internalVMFunctions->resolveInterfaceMethodRefInto(currentThread, constantPool, cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME, &localEntry)) {
			goto done;
		}
		interfaceClass = (J9Class*)localEntry.interfaceClass;
		methodIndexAndArgCount = localEntry.methodIndexAndArgCount;
	}
	*pITableIndex = methodIndexAndArgCount >> 8;
done:
	return interfaceClass;
}

UDATA
jitGetInterfaceVTableOffsetFromCP(J9VMThread *currentThread, J9ConstantPool *constantPool, UDATA cpIndex, J9Class* lookupClass)
{
	UDATA vTableOffset = 0;
	UDATA iTableIndex = 0;
	J9Class *interfaceClass = jitGetInterfaceITableIndexFromCP(currentThread, constantPool, cpIndex, &iTableIndex);
	if (NULL != interfaceClass) {
		J9ITable * iTable = lookupClass->lastITable;
		if (interfaceClass == iTable->interfaceClass) {
			goto foundITable;
		}
		iTable = (J9ITable*)lookupClass->iTable;
		while (NULL != iTable) {
			if (interfaceClass == iTable->interfaceClass) {
				lookupClass->lastITable = iTable;
foundITable:
				vTableOffset = ((UDATA*)(iTable + 1))[iTableIndex];
				break;
			}
			iTable = iTable->next;
		}
	}
	return vTableOffset;
}

J9Method*
jitGetInterfaceMethodFromCP(J9VMThread *currentThread, J9ConstantPool *constantPool, UDATA cpIndex, J9Class* lookupClass)
{
	UDATA vTableOffset = jitGetInterfaceVTableOffsetFromCP(currentThread, constantPool, cpIndex, lookupClass);
	J9Method *method = NULL;
	if (0 != vTableOffset) {
		method = *(J9Method**)((UDATA)lookupClass + vTableOffset);
		if (!J9_ARE_ANY_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers, J9AccPublic)) {
			method = NULL;
		}
	}
	return method;
}

}

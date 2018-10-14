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

/* Only returns non-null if the method is resolved and not to be dispatched by
 * itable, i.e. if it is:
 * - private, using direct dispatch;
 * - a final method of Object, using direct dispatch; or
 * - a non-final method of Object, using virtual dispatch.
 */
J9Method*
jitGetImproperInterfaceMethodFromCP(J9VMThread *currentThread, J9ConstantPool *constantPool, UDATA cpIndex)
{
	J9RAMInterfaceMethodRef *ramMethodRef = (J9RAMInterfaceMethodRef*)constantPool + cpIndex;
	J9Class* interfaceClass = (J9Class*)ramMethodRef->interfaceClass;
	UDATA methodIndexAndArgCount = ramMethodRef->methodIndexAndArgCount;
	J9Method *improperMethod = NULL;
	/* If the ref is resolved, do not call the resolve helper, as it will currently fail if the interface class is not initialized */
	if (!J9RAMINTERFACEMETHODREF_RESOLVED(interfaceClass, methodIndexAndArgCount)) {
		J9RAMInterfaceMethodRef localEntry;
		if (NULL == currentThread->javaVM->internalVMFunctions->resolveInterfaceMethodRefInto(currentThread, constantPool, cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME, &localEntry)) {
			goto done;
		}
		interfaceClass = (J9Class*)localEntry.interfaceClass;
		methodIndexAndArgCount = localEntry.methodIndexAndArgCount;
	}
	if (J9_ARE_ANY_BITS_SET(methodIndexAndArgCount, J9_ITABLE_INDEX_TAG_BITS)) {
		UDATA methodIndex = methodIndexAndArgCount >> J9_ITABLE_INDEX_SHIFT;
		J9Class *jlObject = J9VMJAVALANGOBJECT_OR_NULL(currentThread->javaVM);
		if (J9_ARE_ANY_BITS_SET(methodIndexAndArgCount, J9_ITABLE_INDEX_METHOD_INDEX)) {
			if (J9_ARE_ANY_BITS_SET(methodIndexAndArgCount, J9_ITABLE_INDEX_OBJECT)) {
				/* Object method not in the vTable */
				improperMethod = jlObject->ramMethods + methodIndex;
			} else {
				/* Private interface method */
				improperMethod = interfaceClass->ramMethods + methodIndex;
			}
		} else {
			/* Object method in the vTable. Return the Object method since we
			 * don't know the receiver. The JIT will find its vTable offset and
			 * generate a virtual dispatch.
			 */
			improperMethod = *(J9Method**)((UDATA)jlObject + methodIndex);
		}
	}
done:
	return improperMethod;
}

/* jitGetInterfaceITableIndexFromCP, jitGetInterfaceVTableOffsetFromCP and jitGetInterfaceMethodFromCP
 * apply only to normal interface invocations, any special-case invocation will cause the calls to fail
 * (i.e. special cases will be treated as unresolved).
 */

J9Class*
jitGetInterfaceITableIndexFromCP(J9VMThread *currentThread, J9ConstantPool *constantPool, UDATA cpIndex, UDATA* pITableIndex)
{
	J9RAMInterfaceMethodRef *ramMethodRef = (J9RAMInterfaceMethodRef*)constantPool + cpIndex;
	J9Class* interfaceClass = (J9Class*)ramMethodRef->interfaceClass;
	UDATA methodIndexAndArgCount = ramMethodRef->methodIndexAndArgCount;
	/* If the ref is resolved, do not call the resolve helper, as it will currently fail if the interface class is not initialized */
	if (!J9RAMINTERFACEMETHODREF_RESOLVED(interfaceClass, methodIndexAndArgCount)) {
		J9RAMInterfaceMethodRef localEntry;
		if (NULL == currentThread->javaVM->internalVMFunctions->resolveInterfaceMethodRefInto(currentThread, constantPool, cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME, &localEntry)) {
			goto done;
		}
		interfaceClass = (J9Class*)localEntry.interfaceClass;
		methodIndexAndArgCount = localEntry.methodIndexAndArgCount;
	}
	/* If any tag bits are set, this is a special-case invokeinterface so fail this call */
	if (J9_ARE_ANY_BITS_SET(methodIndexAndArgCount, J9_ITABLE_INDEX_TAG_BITS)) {
		interfaceClass = NULL;
	}
	*pITableIndex = methodIndexAndArgCount >> J9_ITABLE_INDEX_SHIFT;
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

J9UTF8*
jitGetConstantDynamicTypeFromCP(J9VMThread *currentThread, J9ConstantPool *constantPool, UDATA cpIndex)
{
	J9ROMConstantDynamicRef *romConstantRef = (J9ROMConstantDynamicRef*)J9_ROM_CP_FROM_CP(constantPool) + cpIndex;
	J9UTF8 *sigUTF = J9ROMNAMEANDSIGNATURE_SIGNATURE(NNSRP_GET(romConstantRef->nameAndSignature, struct J9ROMNameAndSignature*));

	return sigUTF;
}

}

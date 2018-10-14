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
#include "vm_internal.h"
#include "VMHelpers.hpp"
#include <string.h>

extern "C" {

static VMINLINE bool
initializeMethodRunAddressVarHandle(J9Method *method)
{
	void *encodedAccessMode = NULL;
	J9ROMMethod* romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	U_32 const modifiers = romMethod->modifiers;

	/* The methods that require the special send target are all native and non-public. */
	if (((J9AccNative | J9AccPublic) & modifiers) == J9AccNative) {
		J9UTF8 * classNameUTF = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(method)->romClass);
		if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(classNameUTF), J9UTF8_LENGTH(classNameUTF), "java/lang/invoke/VarHandleInternal")) {
			J9UTF8 * methodNameUTF = J9ROMMETHOD_NAME(romMethod);
			UDATA methodNameLength = J9UTF8_LENGTH(methodNameUTF);
			U_8 * methodName = J9UTF8_DATA(methodNameUTF);

			switch (methodNameLength) {
			case 8:
				if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "get_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "set_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_SET);
				}
				break;
			case 14:
				if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndSet_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_SET);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndAdd_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_ADD);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getOpaque_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_OPAQUE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "setOpaque_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_SET_OPAQUE);
				}
				break;
			case 15:
				if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAcquire_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_ACQUIRE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "setRelease_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_SET_RELEASE);
				}
				break;
			case 16:
				if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getVolatile_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_VOLATILE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "setVolatile_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_SET_VOLATILE);
				}
				break;
			case 21:
				if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndSetAcquire_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_SET_ACQUIRE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndSetRelease_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_SET_RELEASE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndAddAcquire_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_ADD_ACQUIRE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndAddRelease_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_ADD_RELEASE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndBitwiseAnd_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_BITWISE_AND);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndBitwiseXor_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_BITWISE_XOR);
				}
				break;
			case 27:
				if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndBitwiseOrAcquire_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_BITWISE_OR_ACQUIRE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndBitwiseOrRelease_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_BITWISE_OR_RELEASE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "weakCompareAndSetPlain_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_WEAK_COMPARE_AND_SET_PLAIN);
				}
				break;
			case 28:
				if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndBitwiseAndAcquire_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_BITWISE_AND_ACQUIRE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndBitwiseAndRelease_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_BITWISE_AND_RELEASE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndBitwiseXorAcquire_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_BITWISE_XOR_ACQUIRE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndBitwiseXorRelease_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_BITWISE_XOR_RELEASE);
				}
				break;
			case 29:
				if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "weakCompareAndSetAcquire_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_WEAK_COMPARE_AND_SET_ACQUIRE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "weakCompareAndSetRelease_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_WEAK_COMPARE_AND_SET_RELEASE);
				}
				break;
			case 30:
				if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "compareAndExchangeAcquire_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_COMPARE_AND_EXCHANGE_ACQUIRE);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "compareAndExchangeRelease_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_COMPARE_AND_EXCHANGE_RELEASE);
				}
				break;
			default:
				if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "compareAndSet_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_COMPARE_AND_SET);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "getAndBitwiseOr_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_GET_AND_BITWISE_OR);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "weakCompareAndSet_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_WEAK_COMPARE_AND_SET);
				} else if (J9UTF8_LITERAL_EQUALS(methodName, methodNameLength, "compareAndExchange_impl")) {
					encodedAccessMode = J9_VH_ENCODE_ACCESS_MODE(VH_COMPARE_AND_EXCHANGE);
				}
				break;
			}

			if (NULL != encodedAccessMode) {
				method->extra = encodedAccessMode;
				method->methodRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_VARHANDLE);
			}
		}
	}

	return NULL != encodedAccessMode;
}

void
initializeMethodRunAddress(J9VMThread *vmThread, J9Method *method)
{
	J9JavaVM* vm = vmThread->javaVM;

	method->extra = (void *) J9_STARTPC_NOT_TRANSLATED;

	if (initializeMethodRunAddressVarHandle(method)) {
		return;
	}

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_INITIALIZE_SEND_TARGET)) {
		method->methodRunAddress = NULL;
		ALWAYS_TRIGGER_J9HOOK_VM_INITIALIZE_SEND_TARGET(vm->hookInterface, vmThread, method);
		if (NULL != method->methodRunAddress) {
			return;
		}
	}

	initializeMethodRunAddressNoHook(vm, method);
}


void
initializeMethodRunAddressNoHook(J9JavaVM* vm, J9Method *method)
{
	J9ROMMethod* romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	U_32 const modifiers = romMethod->modifiers;

	if (modifiers & J9AccAbstract) {
		method->methodRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_UNSATISFIED_OR_ABSTRACT);
		return;
	}

	if (modifiers & J9AccNative) {
		method->methodRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_BIND_NATIVE);
		return;
	}

	/* Check for methods with large stack use.  Assign them the stack checking target. */
	if (VM_VMHelpers::calculateStackUse(romMethod) > 32) {
		method->methodRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_LARGE);
		return;
	}

	/* The zeroing target correctly handles all of the optional cases (except large stack) */
	if (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_DEBUG_MODE) {
		method->methodRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_ZEROING);
		return;
	}


	/* If the method is java/lang/Object.<init>()V, use J9_BCLOOP_SEND_TARGET_EMPTY_OBJ_CTOR or J9_BCLOOP_SEND_TARGET_OBJ_CTOR */
	if (J9ROMMETHOD_IS_OBJECT_CONSTRUCTOR(romMethod)) {
		if (J9ROMMETHOD_IS_EMPTY(romMethod) && !mustReportEnterStepOrBreakpoint(vm)) {
			method->methodRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_EMPTY_OBJ_CTOR);
		} else {
			method->methodRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_OBJ_CTOR);
		}
		return;
	}

	if (modifiers & J9AccSynchronized) {
		if (modifiers & J9AccStatic) {
			method->methodRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_SYNC_STATIC);
		} else {
			method->methodRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_SYNC);
		}
		return;
	}

	/* use the generic small stack send target */
	method->methodRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_NON_SYNC);
}

J9Method cInitialStaticMethod = { 0, 0, J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_INITIAL_STATIC), 0 };
J9Method cInitialSpecialMethod = { 0, 0, J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_INITIAL_SPECIAL), 0 };
J9Method cInitialVirtualMethod = { 0, 0, J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_INITIAL_VIRTUAL), 0 };
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
J9Method cInvokePrivateMethod  = { 0, 0, J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_INVOKE_PRIVATE), 0 };
#endif /* J9VM_OPT_VALHALLA_NESTMATES */

void
initializeInitialMethods(J9JavaVM *vm)
{
	vm->jniSendTarget = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_RUN_JNI_NATIVE);
	vm->initialMethods.initialStaticMethod = &cInitialStaticMethod;
	vm->initialMethods.initialSpecialMethod = &cInitialSpecialMethod;
	vm->initialMethods.initialVirtualMethod = &cInitialVirtualMethod;
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
	vm->initialMethods.invokePrivateMethod = &cInvokePrivateMethod;
#endif /* J9VM_OPT_VALHALLA_NESTMATES */
}

}

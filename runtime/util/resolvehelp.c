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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/


#include <string.h>
#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "util_internal.h"
#include "j9vmutilnls.h"
#include "rommeth.h"
#include "ut_j9util.h"

J9Method *
getMethodForSpecialSend(J9VMThread *vmStruct, J9Class *currentClass, J9Class *resolvedClass, J9Method *method, UDATA lookupOptions)
{
	/* See if this is meant to be a super send.  Super send requires:
	 *
	 *	A) ACC_SUPER set for current class
	 *	B) Resolved method class is a superclass of current class
	 *	C) Resolved method is not <init>
	 *	D) Skip checking vTables if resolved or current class is an interface
	 */
	if ((J9AccSuper == (currentClass->romClass->modifiers & J9AccSuper))
	|| J9_ARE_NO_BITS_SET(vmStruct->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_ALLOW_NON_VIRTUAL_CALLS)
	) {
		J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
		UDATA currentDepth = J9CLASS_DEPTH(currentClass);
		UDATA superDepth = J9CLASS_DEPTH(methodClass);
		BOOLEAN isInterfaceMethod = J9AccInterface == (methodClass->romClass->modifiers & J9AccInterface);

		if ((isInterfaceMethod || ((currentDepth > superDepth) && (currentClass->superclasses[superDepth] == methodClass)))
				&& (J9_ARE_NO_BITS_SET(resolvedClass->romClass->modifiers, J9AccInterface) && J9_ARE_NO_BITS_SET(currentClass->romClass->modifiers, J9AccInterface))
		) {
			/* <init> will never be in the vTable.  Also, any other method that
			 * is not in the vTable cannot be overridden, so we can just run it.
			 */
			J9InternalVMFunctions *vmFuncs = vmStruct->javaVM->internalVMFunctions;
			UDATA vTableIndex = vmFuncs->getVTableIndexForMethod(method, resolvedClass, vmStruct);

			if (vTableIndex != 0) {
				J9Class *superclass = currentClass->superclasses[currentDepth - 1];

				if (isInterfaceMethod) {
					/* CMVC 170457: Algorithm for invokespecial lookup is wrong.
					 * New Algorithm:
					 * 1) Find the vtable index for J9Method in the resolved class.
					 * 2) Using the vtable index to get the J9Method at that index in the current class.
					 * 3) Walk the current class vtable backwards to find the most "recent" override of that method
					 * 4) Lookup the method at vtableIndex in currentClass's super class
					 */
					UDATA superVTableSize = (*(UDATA*)(superclass + 1) * sizeof(UDATA)) + sizeof(J9Class);
					method = *(J9Method **)(((UDATA)currentClass) + vTableIndex);
					vTableIndex = vmFuncs->getVTableIndexForMethod(method, currentClass, vmStruct);
					/* We may have looked up a J9Method from an interface due to either defender methods
					 * or the method not being implemented in the class.  If that's the case, we need
					 * to ensure we don't read a non-existent vtable slot.  The found method is the correct one
					 */
					if ((vTableIndex > 0) && (vTableIndex <= superVTableSize)) {
						method = *(J9Method **)(((UDATA)superclass) + vTableIndex);
					}
				} else {
					/* In order to support non-vTable methods in a super invoke, perform the lookup rather than
					 * looking in the vTable.
					 */
					method = (J9Method *)vmFuncs->javaLookupMethod(vmStruct, superclass, J9ROMMETHOD_NAMEANDSIGNATURE(J9_ROM_METHOD_FROM_RAM_METHOD(method)), currentClass, lookupOptions);
				}
			}
		}
	}
	return method;
}

BOOLEAN
isDirectSuperInterface(J9VMThread *vmStruct, J9Class *resolvedClass, J9Class *currentClass) {
	/**
	 * JVMS 4.9.2: If resolvedClass is an interface, ensure that it is a DIRECT superinterface of currentClass (or resolvedClass == currentClass)
	 */
	BOOLEAN result = FALSE;
	J9ROMClass *currentROMClass = currentClass->romClass;
	if (J9ROMCLASS_IS_UNSAFE(currentROMClass)) {
		/* If this is an unsafe class, we skip the check so that a lambda "inherit" super interfaces from its host class. */
		result = TRUE;
	} else if (J9_ARE_ANY_BITS_SET(resolvedClass->romClass->modifiers, J9AccInterface) && (currentClass != resolvedClass)) {
		U_32 i;
		U_32 interfaceCount = currentROMClass->interfaceCount;
		J9SRP *interfaceNames = J9ROMCLASS_INTERFACES(currentROMClass);
		for (i = 0; i < interfaceCount; i++) {
			J9JavaVM * vm = vmStruct->javaVM;
			J9Class *interfaceClass = NULL;
			J9UTF8 *interfaceName = NNSRP_GET(interfaceNames[i], J9UTF8*);
			omrthread_monitor_enter(vm->classTableMutex);
			interfaceClass = vm->internalVMFunctions->hashClassTableAt(resolvedClass->classLoader, J9UTF8_DATA(interfaceName), J9UTF8_LENGTH(interfaceName));
			omrthread_monitor_exit(vm->classTableMutex);
			if (interfaceClass == resolvedClass) {
				/* Found resolvedClass in the currentClass ROMClass interface list */
				result = TRUE;
				break;
			}
		}
	} else {
		result = TRUE;
	}
	return result;
}

static char*
createErrorMessage(J9VMThread *vmStruct, J9Class *resolvedOrReceiverClass, J9Class *currentClass, const char* errorMsg) {
	PORT_ACCESS_FROM_VMC(vmStruct);
	char *buf = NULL;
	if (NULL != errorMsg) {
		J9UTF8 *resolvedOrReceiverName = J9ROMCLASS_CLASSNAME(resolvedOrReceiverClass->romClass);
		J9UTF8 *currentName = J9ROMCLASS_CLASSNAME(currentClass->romClass);
		UDATA bufLen = j9str_printf(PORTLIB, NULL, 0, errorMsg,
				J9UTF8_LENGTH(resolvedOrReceiverName), J9UTF8_DATA(resolvedOrReceiverName),
				J9UTF8_LENGTH(currentName), J9UTF8_DATA(currentName));
		if (bufLen > 0) {
			buf = j9mem_allocate_memory(bufLen, OMRMEM_CATEGORY_VM);
			if (NULL != buf) {
				j9str_printf(PORTLIB, buf, bufLen, errorMsg,
					J9UTF8_LENGTH(resolvedOrReceiverName), J9UTF8_DATA(resolvedOrReceiverName),
					J9UTF8_LENGTH(currentName), J9UTF8_DATA(currentName));
			}
		}
	}
	return buf;
}

void
setIllegalAccessErrorReceiverNotSameOrSubtypeOfCurrentClass(J9VMThread *vmStruct, J9Class *receiverClass, J9Class *currentClass) {
	PORT_ACCESS_FROM_VMC(vmStruct);
	J9JavaVM *vm = vmStruct->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	/* Construct error string */
	const char *errorMsg = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VMUTIL_RECEIVERCLASS_NOT_SAME_OR_SUBTYPE_OF_CURRENTCLASS, NULL);
	char *buf = createErrorMessage(vmStruct, receiverClass, currentClass, errorMsg);
	vmFuncs->setCurrentExceptionUTF(vmStruct, J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR, buf);
	j9mem_free_memory(buf);
}

void
setIncompatibleClassChangeErrorInvalidDefenderSupersend(J9VMThread *vmStruct, J9Class *resolvedClass, J9Class *currentClass) {
	PORT_ACCESS_FROM_VMC(vmStruct);
	J9JavaVM *vm = vmStruct->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	/* Construct error string */
	const char *errorMsg = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VMUTIL_DEFAULT_METHOD_INVALID_SUPERSEND, NULL);
	char *buf = createErrorMessage(vmStruct, resolvedClass, currentClass, errorMsg);
	vmFuncs->setCurrentExceptionUTF(vmStruct, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, buf);
	j9mem_free_memory(buf);
}

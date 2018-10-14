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

#include "j9consts.h"
#include "j9cp.h"
#include "j9protos.h"
#include "rommeth.h"
#include "j9.h"
#include "ut_j9vm.h"
#include "vm_internal.h"


J9Method* 
getForwardedMethod(J9VMThread* currentThread, J9Method* method)
{
	J9Method *candidateMethod;
	J9ROMNameAndSignature *nameAndSig;
	J9Class *methodClass;
	J9Class *currentClass;
	J9ROMMethod *currentROMMethod;
	j9object_t currentMethodClass = NULL;
	j9object_t currentMethodProtectionDomain = NULL;

	Trc_VM_getForwardedMethod_Entry(currentThread, method);

	/* Do not allow the forwarder optimization if method enter, single step or breakpoint events are being reported */

	if (mustReportEnterStepOrBreakpoint(currentThread->javaVM)) {
		Trc_VM_getForwardedMethod_Exit_Debug(currentThread, method);
		return method;
	}

	currentROMMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	nameAndSig = &(currentROMMethod->nameAndSignature);
	currentClass = J9_CLASS_FROM_METHOD(method);
	methodClass = currentClass;
	candidateMethod =  method;
	currentMethodClass = J9VM_J9CLASS_TO_HEAPCLASS(methodClass);
	currentMethodProtectionDomain = J9VMJAVALANGCLASS_PROTECTIONDOMAIN(currentThread, currentMethodClass);

	/* loop while method is a forwarder */
	while (J9ROMMETHOD_IS_FORWARDER(currentROMMethod)) {
		J9Method *currentMethod;
		J9Class *lookupClass;
		UDATA depth = J9CLASS_DEPTH(currentClass);
		j9object_t currentHeapClass = NULL;
		
		if (0 == depth) {
			/* Hit java/lang/Object.  Finished */
			break;
		}

		lookupClass = currentClass->superclasses[depth-1];
		currentMethod = (J9Method *)javaLookupMethod(currentThread, lookupClass, nameAndSig, currentClass, J9_LOOK_VIRTUAL | J9_LOOK_NO_JAVA);
		if (NULL == currentMethod) {
			/* No method found in superclasses */
			break;
		}
	
		currentROMMethod = J9_ROM_METHOD_FROM_RAM_METHOD(currentMethod);
		if (J9ROMMETHOD_IS_EMPTY(currentROMMethod)) {
			/* Found an empty method.  Finished */
			candidateMethod = currentMethod;
			break;
		}

		currentClass = J9_CLASS_FROM_METHOD(currentMethod);
		currentHeapClass = J9VM_J9CLASS_TO_HEAPCLASS(currentClass);

		/* In order to forward, the new method must be in the identical protection domain as the resolved method. */
		if (currentMethodProtectionDomain == J9VMJAVALANGCLASS_PROTECTIONDOMAIN(currentThread, currentHeapClass)) {
			/* Candidate method found */
			candidateMethod = currentMethod;			
		} 
	}

	Trc_VM_getForwardedMethod_Exit(currentThread, method, candidateMethod);
	return candidateMethod;
}


UDATA
mustReportEnterStepOrBreakpoint(J9JavaVM * vm)
{
	J9HookInterface** hookInterface = J9_HOOK_INTERFACE(vm->hookInterface);

	return
		((*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_METHOD_ENTER) != 0) ||
		((*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_METHOD_RETURN) != 0) ||
		((*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_SINGLE_STEP) != 0) ||
		((*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_BREAKPOINT) != 0);
}





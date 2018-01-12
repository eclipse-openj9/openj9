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
#include "j9cfg.h"
#include "j9protos.h"
#include "rommeth.h"

#if defined(J9VM_JIT_FULL_SPEED_DEBUG)
void
jitResetAllUntranslateableMethods(J9VMThread *vmThread)
{
	J9MemorySegment *currentSegment = vmThread->javaVM->classMemorySegments->nextSegment;
	while(NULL != currentSegment) {
		if(MEMORY_TYPE_RAM_CLASS == (currentSegment->type & MEMORY_TYPE_RAM_CLASS)) {
			J9Class *clazz = *(J9Class **)currentSegment->heapBase;
			while(NULL != clazz) {
				J9Method *method = clazz->ramMethods;
				UDATA methodCount = clazz->romClass->romMethodCount;
				while(0 != methodCount) {
					if(0 == ((J9_JAVA_NATIVE | J9_JAVA_ABSTRACT) & J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers)) {
						if(J9_JIT_NEVER_TRANSLATE == (IDATA)method->extra) {
							/* This call resets the methodRunAddress to the JIT counting send target, and reinitializes the count (extra) field. */
							vmThread->javaVM->internalVMFunctions->initializeMethodRunAddress(vmThread, method);
						}
					}
					method += 1;
					methodCount -= 1;
				}
				clazz = clazz->nextClassInSegment;
			}
		}
		currentSegment = currentSegment->nextSegment;
	}
}
#endif /* J9VM_JIT_FULL_SPEED_DEBUG */

/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

/* Stub methods called from files in the runtime/bcutil directory.
 *
 * These functions are called using the following macros which
 * 	* J9_VM_FUNCTION(currentThread, function)
 *	* J9_VM_FUNCTION_VIA_JAVAVM(javaVM, function)
 * allow them to be direct calls or indirect through the 
 * internalVMFunctions table.
 *
 * The bcutil directory is compiled as a static library with
 * `J9_INTERNAL_TO_VM` defined so the calls appear to be direct.
 *
 * cfdump needs to stub some of these functions to allow the
 * bcutil static library to be linked.  This avoids the following
 * link errors:
 *
 *
 * "_freeMemorySegment", referenced from:
 *    _internalDefineClass in libj9dyn.a(defineclass.o)
 * "_internalCreateRAMClassFromROMClass", referenced from:
 *    _internalDefineClass in libj9dyn.a(defineclass.o)
 * "_setCurrentException", referenced from:
 *    _internalDefineClass in libj9dyn.a(defineclass.o)
 * "_setCurrentExceptionUTF", referenced from:
 *    _internalDefineClass in libj9dyn.a(defineclass.o)
 */

void freeMemorySegment (J9JavaVM *javaVM, J9MemorySegment *segment, BOOLEAN freeDescriptor)
{
	printf("freeMemorySegment stub called!\n");
	return;
}

J9Class *   
internalCreateRAMClassFromROMClass(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass,
	UDATA options, J9Class* elementClass, j9object_t protectionDomain, J9ROMMethod ** methodRemapArray,
	IDATA entryIndex, I_32 locationType, J9Class *classBeingRedefined, J9Class *hostClass)
{
	printf("internalCreateRAMClassFromROMClass stub called!\n");
	return NULL;

}

void  
setCurrentExceptionUTF(J9VMThread * vmThread, UDATA exceptionNumber, const char * detailUTF)
{
	printf("setCurrentExceptionUTF stub called!\n");
	return;
}

void  
setCurrentException(J9VMThread *currentThread, UDATA exceptionNumber, UDATA *detailMessage)
{
	printf("setCurrentException stub called!\n");
	return;
}

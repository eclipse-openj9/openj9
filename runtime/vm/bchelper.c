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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "j9consts.h"
#include "omrgcconsts.h"
#include "j9port.h"
#include "j9protos.h"
#include "j9user.h"
#include "objhelp.h"
#include "ut_j9vm.h"
#include "vm_internal.h"

static j9object_t allocate_dimension (J9VMThread *vmStruct, J9ArrayClass *currentClass, U_32 dimensions, U_32 currentDimension, U_32 *dimensionArray, UDATA allocationType);

j9object_t  
helperMultiANewArray(J9VMThread *vmStruct, J9ArrayClass *classPtr, UDATA dimensionsUDATA, I_32 *dimensionArray, UDATA allocationType)
{
	U_32 dimensions = (U_32)dimensionsUDATA;
	J9JavaVM * vm = vmStruct->javaVM;
	J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
	j9object_t saveTable;
	j9object_t result;
	J9Class *elementClass;
	J9Class *arrayOfObject;
	U_32 i;

#if 0
	PORT_ACCESS_FROM_VMC(vmStruct);
	{
	j9tty_printf( PORTLIB, "alloc %d dimensional array, sizes: (",dimensions,0,0,0,0,0,0,0,0,0);
	for(i=0;i<dimensions;i++) {
		j9tty_printf( PORTLIB, "%d ", dimensionArray[i],0,0,0,0,0,0,0,0,0);
	}

	j9tty_printf( PORTLIB, ")\n",0,0,0,0,0,0,0,0,0,0);
	}
#endif

	/* Check all indices for negative sizes. This is performed prior to any allocations (0 is a valid size). */
	for(i=0;i<dimensions;i++) {
		if (dimensionArray[i] < 0) {
			setCurrentException( vmStruct, J9VMCONSTANTPOOL_JAVALANGNEGATIVEARRAYSIZEEXCEPTION, NULL );  /* Negative array size exception */
			return NULL;
		}
	}

	elementClass = J9VMJAVALANGOBJECT_OR_NULL(vm);
	arrayOfObject = elementClass->arrayClass;
	if (arrayOfObject == NULL) {
		/* the first class in vm->arrayROMClasses is the array class for Objects */
		arrayOfObject = internalCreateArrayClass(vmStruct,
			(J9ROMArrayClass *) J9ROMIMAGEHEADER_FIRSTCLASS(vm->arrayROMClasses), 
			elementClass);
		if (arrayOfObject == NULL) {
			/* exception is pending from the call */
			return NULL;
		}
	}

	saveTable = mmFuncs->J9AllocateIndexableObject(
		vmStruct, arrayOfObject, dimensions, allocationType);
	if (saveTable == NULL) {
		setHeapOutOfMemoryError(vmStruct);
		return NULL;
	}

	/* call the recursive routine to fill in the elements */
	PUSH_OBJECT_IN_SPECIAL_FRAME(vmStruct, saveTable);
	result = allocate_dimension(vmStruct, (J9ArrayClass*)(classPtr->componentType), dimensions, dimensions - 1, (U_32 *)dimensionArray, allocationType);
	DROP_OBJECT_IN_SPECIAL_FRAME(vmStruct);

	/* The resulting object may have been tenured (and possibly remembered)
	 * by this point. Call the special barrier for recently-allocated objects
	 * before it becomes visible.
	 */
	if(NULL != result) {
		mmFuncs->j9gc_objaccess_recentlyAllocatedObject(vmStruct, result);
	}

	return result;
}


static j9object_t 
allocate_dimension(J9VMThread *vmStruct, J9ArrayClass *currentClass, U_32 dimensions, U_32 currentDimension, U_32 *dimensionArray, UDATA allocationType)
{
	U_32 i;
	j9object_t parentResult;
	j9object_t childResult;
	j9object_t saveTable;

	/* allocate the current one */
#if 0
	PORT_ACCESS_FROM_VMC(vmStruct);
	j9tty_printf( PORTLIB, "alloc dim %d (size = %d)\n", currentDimension, dimensionArray[currentDimension], 0,0,0,0,0,0,0,0);
#endif
	parentResult = vmStruct->javaVM->memoryManagerFunctions->J9AllocateIndexableObject(
		vmStruct, (J9Class*)currentClass, dimensionArray[currentDimension], allocationType);

	if(!parentResult) {
		setHeapOutOfMemoryError(vmStruct);
		return NULL;
	}

	saveTable = PEEK_OBJECT_IN_SPECIAL_FRAME(vmStruct, 0);
	J9JAVAARRAYOFOBJECT_STORE(vmStruct, saveTable, currentDimension, parentResult); /* save the result to protect it from GCs */

	/* recurse if we have to */
	if (currentDimension > 0) {
		for(i = 0; i<dimensionArray[currentDimension]; i++) {
			if(NULL == (childResult = allocate_dimension(vmStruct, (J9ArrayClass*)(currentClass->componentType), dimensions, currentDimension - 1, dimensionArray, allocationType))) {
				return NULL;
			}
			saveTable = PEEK_OBJECT_IN_SPECIAL_FRAME(vmStruct, 0);
			parentResult = J9JAVAARRAYOFOBJECT_LOAD(vmStruct, saveTable, currentDimension); /* reload the result from the array in case a GC happened */
			J9JAVAARRAYOFOBJECT_STORE(vmStruct, parentResult, i, childResult); 
		}
	}

	/* Verify object sizes after potential GC points. */
	Assert_VM_true(J9INDEXABLEOBJECT_SIZE(vmStruct, saveTable) == dimensions);
	Assert_VM_true(J9INDEXABLEOBJECT_SIZE(vmStruct, parentResult) == dimensionArray[currentDimension]);

	return parentResult;
}

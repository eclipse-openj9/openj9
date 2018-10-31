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

#include "j9ddr.h"
#include "j9cfg.h"

/* @ddr_namespace: map_to_type=DDRAlgorithmVersions */

#define VM_MAJOR_VERSION EsVersionMajor
#define VM_MINOR_VERSION EsVersionMinor
#define VM_LINE_NUMBER_TABLE_VERSION 1
#define VM_LOCAL_VARIABLE_TABLE_VERSION 1
#define VM_CAN_ACCESS_LOCALS_VERSION 1
#define ALG_GC_ARRAYLET_OBJECT_MODEL_VERSION 1
#define VM_HASHTABLE_VERSION 1
#define ALG_VM_J9CLASS_VERSION 1
#define ALG_GC_OBJECT_HEAP_ITERATOR_SEGREGATED_ORDERED_LIST_VERSION 1
#define J9_OBJECT_FIELD_OFFSET_ITERATOR_VERSION 1
#define VM_STACK_GROW_VERSION 1
#define ALG_ROM_HELP_VERSION 1
#define FOUR_BYTE_OFFSETS_VERSION 1
#define ALG_VM_VTABLE_VERSION 1
#define ALG_VM_ITABLE_VERSION 1
#define ALG_VM_BYTECODE_VERSION 1

J9DDRConstantTableBegin(DDRAlgorithmVersions)
	J9DDRConstantTableEntryWithValue("VM_MAJOR_VERSION", VM_MAJOR_VERSION)
	J9DDRConstantTableEntryWithValue("VM_MINOR_VERSION", VM_MINOR_VERSION)
	J9DDRConstantTableEntryWithValue("VM_LINE_NUMBER_TABLE_VERSION", VM_LINE_NUMBER_TABLE_VERSION)
	J9DDRConstantTableEntryWithValue("VM_LOCAL_VARIABLE_TABLE_VERSION", VM_LOCAL_VARIABLE_TABLE_VERSION)
	J9DDRConstantTableEntryWithValue("VM_CAN_ACCESS_LOCALS_VERSION", VM_CAN_ACCESS_LOCALS_VERSION)
	J9DDRConstantTableEntryWithValue("ALG_GC_ARRAYLET_OBJECT_MODEL_VERSION", ALG_GC_ARRAYLET_OBJECT_MODEL_VERSION)
	J9DDRConstantTableEntryWithValue("VM_HASHTABLE_VERSION", VM_HASHTABLE_VERSION)
	J9DDRConstantTableEntryWithValue("ALG_VM_J9CLASS_VERSION", ALG_VM_J9CLASS_VERSION)
	J9DDRConstantTableEntryWithValue("ALG_GC_OBJECT_HEAP_ITERATOR_SEGREGATED_ORDERED_LIST_VERSION", ALG_GC_OBJECT_HEAP_ITERATOR_SEGREGATED_ORDERED_LIST_VERSION)
	J9DDRConstantTableEntryWithValue("J9_OBJECT_FIELD_OFFSET_ITERATOR_VERSION", J9_OBJECT_FIELD_OFFSET_ITERATOR_VERSION)
	J9DDRConstantTableEntryWithValue("VM_STACK_GROW_VERSION", VM_STACK_GROW_VERSION)
	J9DDRConstantTableEntryWithValue("ALG_ROM_HELP_VERSION", ALG_ROM_HELP_VERSION)
	J9DDRConstantTableEntryWithValue("FOUR_BYTE_OFFSETS_VERSION", FOUR_BYTE_OFFSETS_VERSION)
	J9DDRConstantTableEntryWithValue("ALG_VM_VTABLE_VERSION", ALG_VM_VTABLE_VERSION)
	J9DDRConstantTableEntryWithValue("ALG_VM_ITABLE_VERSION", ALG_VM_ITABLE_VERSION)
	J9DDRConstantTableEntryWithValue("ALG_VM_BYTECODE_VERSION", ALG_VM_BYTECODE_VERSION)
J9DDRConstantTableEnd

J9DDRStructTableBegin(AlgorithmVersions)
	J9DDREmptyStruct(DDRAlgorithmVersions, NULL)
J9DDRStructTableEnd

const J9DDRStructDefinition* getAlgorithmVersionStructTable()
{
	return J9DDR_AlgorithmVersions_structs;
}

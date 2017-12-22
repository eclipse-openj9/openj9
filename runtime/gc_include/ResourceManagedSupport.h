
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

/**
 * @file
 * @ingroup GC_Include
 */

#if !defined(RESOURCEMANAGEDSUPPORT_H_)
#define RESOURCEMANAGEDSUPPORT_H_
/* @ddr_namespace: map_to_type=ResourceManagedSupportConstants */
#include "j9.h"
#include "j9cfg.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @ingroup GC_Include
 * @name Return codes for memory space merge operation.
 * @{
 */
#define MM_RESMAN_MERGE_OK (UDATA)0
#define MM_RESMAN_MERGE_FAILED (UDATA)1
/**
 * @}
 */

/**
 * @ingroup GC_Include
 * @name Return codes for object move operation.
 * @{
 */
#define MM_RESMAN_OBJ_MOVE_OK (UDATA)0
#define MM_RESMAN_OBJ_MOVE_FAILED (UDATA)1
/**
 * @}
 */

/**
 * Merge a memory space into another.
 * This operation will move all live data from the source memory space into the destination memory space, expanding
 * if required.  The move is not partial, but an all-or-nothing operation.  If the move fails, there may be memory
 * consumed in the destination which is "dead", and will be reclaimed on the next garbage collect.
 * @return MM_RESMAN_MERGE_OK on success, otherwise fail.
 */
UDATA
mergeMemorySpaces(J9VMThread *vmThread, void *destinationMemorySpace, void *sourceMemorySpace);

/**
 * Move an object to the given memory space.
 * This operation will allocate the object pointer in the given memory space, fixing all references to the object
 * in the heap to point at the new location.  The old object is left intact, but is effectively dead.
 * @return MM_RESMAN_OBJ_MOVE_OK on success, otherwise fail.
 */
UDATA
moveObjectToMemorySpace(J9VMThread *vmThread, void *destinationMemorySpace, j9object_t objectPtr);

/**
 * Determine whether an object is in a given memory space.
 * @return non-zero if the object is in the memory space, 0 otherwise.
 */
UDATA
isObjectInMemorySpace(J9VMThread *vmThread, void *memorySpace, j9object_t objectPtr);

/**
 * Determine whether a memory space has any external references.
 * @return non-zero if there are any references to the space
 */
UDATA
isMemorySpaceReferenced(J9VMThread *vmThread, void *memorySpace);

#ifdef __cplusplus
} /* extern "C" { */
#endif /* __cplusplus */

#endif /* RESOURCEMANAGEDSUPPORT_H_ */

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

#ifndef simplepool_api_h
#define simplepool_api_h


/**
* @file simplepool_api.h
* @brief Public API for the SIMPLEPOOL module.
*
* This file contains public function prototypes and
* type definitions for the SIMPLEPOOL module.
*
*/

#include "j9comp.h"
#include "j9simplepool.h"

#ifdef __cplusplus
extern "C" {
#endif

#define J9SIMPLEPOOL_FREELIST(parm) SRP_GET((parm)->freeList, J9SimplePoolFreeList*)
#define J9SIMPLEPOOL_FIRSTFREESLOT(parm) SRP_GET((parm)->firstFreeSlot, uint8_t*)
#define J9SIMPLEPOOL_BLOCKEND(parm) SRP_GET((parm)->blockEnd, uint8_t*)
#define J9SIMPLEPOOLFREELIST_NEXT(parm) SRP_GET((parm)->next, J9SimplePoolFreeList*)
#define J9SIMPLEPOOLFREELIST_SIMPLEPOOL(parm) SRP_GET((parm)->simplePool, J9SimplePool*)

/* Simple Pool Constants */
#define SIMPLEPOOL_MIN_ELEMENT_SIZE		sizeof(J9SimplePoolFreeList) /* minimum size of an element in bytes => sizeof(J9SimplePoolFreeList) */
#define SIMPLEPOOL_MAX_MEMORY_SIZE		((uint32_t)2*1024*1024*1024) /* maximum supported memory size of a simple pool => max range of a J9SRP == 2^31 */


/* ---------------- simplepool.c ---------------- */

/**
* @brief
* @param[in] poolAddress
* @param[in] memorySize
* @param[in] elementSize
* @param[in] flags
* @return J9SimplePool*
*/
J9SimplePool *
simplepool_new(void *poolAddress, uint32_t memorySize, uint32_t elementSize, uint32_t flags);

/**
 * @brief
 * @param[in] simplePool
 * @return 	void*
 */
void *
simplepool_newElement(J9SimplePool *simplePool);

/**
 * @brief
 * @param[in] simplePool
 * @param[in] address
 * @return  BOOLEAN
 */
BOOLEAN
simplepool_isElement(J9SimplePool *simplePool, void *address);

/**
 * @brief
 * @param[in] simplePool
 * @param[in] element
 * @return  intptr_t
 */
intptr_t
simplepool_removeElement(J9SimplePool *simplePool, void *element);

/**
 * @brief
 * @param[in] simplePool
 * @return uintptr_t
 */
uintptr_t
simplepool_maxNumElements(J9SimplePool *simplePool);

/**
 * @brief
 * @param[in] entrySize
 * @param[in] numberOfElements
 * @return uintptr_t
 */
uint32_t
simplepool_totalSize(uint32_t  entrySize, uint32_t numberOfElements);

/**
 * @brief
 * @param[in] simplePool
 * @return uintptr_t
 */
uintptr_t
simplepool_numElements(J9SimplePool *simplePool);

/**
 * @brief
 * @param[in] simplePool
 * @param[in] portLib
 * @param[in] doFunction
 * @param[in] userData
 * @return BOOLEAN
 */
BOOLEAN
simplepool_do(J9SimplePool *simplePool, J9PortLibrary *portLib, BOOLEAN (*doFunction) (void *anElement, void *userData), void *userData);

/**
 * @brief
 * @param[in] simplePool
 * @param[in] memorySize
 * @param[in] elementSize
 * @return BOOLEAN
 *
 */
BOOLEAN
simplepool_verify(J9SimplePool *simplePool, uint32_t memorySize, uint32_t elementSize);

/**
 * @brief
 * @param[in] simplePool
 * @return void
 */
void
simplepool_clear(J9SimplePool *simplePool);

/**
 * @brief
 * @param[in] simplePool
 * @param[in] portLib
 * @param[in] doFunction
 * @param[in] userData
 * @param[in] skipCount
 * @return BOOLEAN
 */
BOOLEAN
simplepool_checkConsistency(J9SimplePool *simplePool, J9PortLibrary *portLib, BOOLEAN (*doFunction) (void *anElement, void *userData), void *userData, uintptr_t skipCount);
/**
 * @brief
 * @param void
 * @return uintptr_t
 *
 */
uint32_t
simplepool_headerSize(void);

#ifdef __cplusplus
}
#endif

#endif /* simplepool_api_h */

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

#ifndef stackmap_api_h
#define stackmap_api_h

/**
* @file stackmap_api.h
* @brief Public API for the STACKMAP module.
*
* This file contains public function prototypes and
* type definitions for the STACKMAP module.
*
*/

#include "j9.h"
#include "j9comp.h"

#define MAP_MEMORY_RESULTS_BUFFER_SIZE 8192
#define MAP_MEMORY_DEFAULT (180 * 1024)

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- maxmap.c ---------------- */

/**
* @brief
* @param vmThread
* @param romClass
* @return UDATA
*/
UDATA
j9maxmap_setMapMemoryBuffer(J9JavaVM * vm, J9ROMClass * romClass);

/* ---------------- mapmemorybuffer.c ---------------- */

/**
* @brief Callback function to access the global emergency buffer for stack/local mapping.
* @return A pointer to the guaranteed large enough global buffer.
*/
UDATA * j9mapmemory_GetBuffer(void * userData) ;

/**
* @brief Callback function to release the global emergency buffer for stack/local mapping.
* @return Void.
*/
void j9mapmemory_ReleaseBuffer(void * userData);

/**
* @brief Callback function to access the global emergency results buffer for stack/local mapping.
* @return A pointer to the guaranteed large enough global buffer.
*/
U_32 * j9mapmemory_GetResultsBuffer(void * userData) ;

/**
* @brief Callback function to release the global emergency results buffer for stack/local mapping.
* @return Void.
*/
void j9mapmemory_ReleaseResultsBuffer(void * userData);


/* ---------------- fixreturns.c ---------------- */

struct J9ROMClass;
/**
* @brief
* @param portLib
* @param romClass
* @return IDATA
*/
IDATA
fixReturnBytecodes(J9PortLibrary * portLib, struct J9ROMClass* romClass);


/**
* @brief
* @param romMethod
* @param returnSlots
* @return U_8
*/
U_8
getReturnBytecode(J9ROMClass *romClass, J9ROMMethod * romMethod, UDATA * returnSlots);


/* ---------------- localmap.c ---------------- */

/**
* @brief
* @param romMethod
* @param resultArrayBase
* @return void
*/
void
j9localmap_ArgBitsForPC0 (J9ROMClass * romClass, J9ROMMethod * romMethod, U_32 * resultArrayBase);


/**
* @brief
* @param portLibrary
* @param romClass
* @param romMethod
* @param pc
* @param resultArrayBase
* @param userData
* @param getBuffer function
* @param releaseBuffer function
* @return IDATA
*/
IDATA
j9localmap_LocalBitsForPC(J9PortLibrary * portLib, J9ROMClass * romClass, J9ROMMethod * romMethod, UDATA pc, U_32 * resultArrayBase, 
		void * userData, UDATA * (* getBuffer) (void * userData), void (* releaseBuffer) (void * userData));


/* ---------------- debuglocalmap.c ---------------- */

/**
* @brief
* @param portLibrary
* @param romClass
* @param romMethod
* @param pc
* @param resultArrayBase
* @param userData
* @param getBuffer function
* @param releaseBuffer function
* @return IDATA
*/
IDATA
j9localmap_DebugLocalBitsForPC(J9PortLibrary * portLib, J9ROMClass * romClass, J9ROMMethod * romMethod, UDATA pc, U_32 * resultArrayBase, 
		void * userData, UDATA * (* getBuffer) (void * userData), void (* releaseBuffer) (void * userData)); 


/**
* @brief
* @param *currentThread
* @param *ramMethod
* @param offsetPC
* @param slot
* @param slotSignature
* @param compressTypes
* @return UDATA
*/
UDATA
validateLocalSlot(J9VMThread *currentThread, J9Method *ramMethod, U_32 offsetPC, U_32 slot, char slotSignature, UDATA compressTypes);


/**
* @brief
* @param *vm
* @return void
*/
void
installDebugLocalMapper(J9JavaVM * vm);


/* ---------------- stackmap.c ---------------- */

/**
* @brief
* @param portLib
* @param pc
* @param romClass
* @param romMethod
* @param resultArrayBase
* @param resultArraySize
* @param userData
* @param getBuffer function
* @param releaseBuffer function
* @return IDATA
*/
IDATA
j9stackmap_StackBitsForPC(J9PortLibrary * portLib, UDATA pc, J9ROMClass * romClass, J9ROMMethod * romMethod,
		U_32 * resultArrayBase, UDATA resultArraySize,
		void * userData, 
		UDATA * (* getBuffer) (void * userData), 
		void (* releaseBuffer) (void * userData));

#ifdef __cplusplus
}
#endif

#endif /* stackmap_api_h */

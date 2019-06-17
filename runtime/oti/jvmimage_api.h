/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#ifndef jvmimage_api_h
#define jvmimage_api_h

#include "j9.h"
#include "j9comp.h"
#include "jvmimage.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
* Allocate memory in heap image
* 
* @param size[in] size to allocate
*
* @return pointer to allocated memory on success, NULL on failure 
*/
void* mem_allocate_memory(uintptr_t byteAmount);

/*
* Allocate memory in heap image
* 
* @param portLibrary[in] the default port library
* @param byteAmount[in] size to allocate
* @param callSite[in] location memory alloc is called from
* @param category[in] category of memory alloc
*
* @return pointer to allocated memory on success, NULL on failure 
*/
void* image_mem_allocate_memory(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite, uint32_t category);

/*
* Free memory in heap image
*
* @param memoryPointer[in] pointer to address for free
*/
void mem_free_memory(void *memoryPointer);

/*
* Free memory in heap image
*
* @param portLibrary[in] the default port library
* @param memoryPointer[in] pointer to address for free
*/
void image_mem_free_memory(struct OMRPortLibrary *portLibrary, void *memoryPointer);

/*
* Creates and allocates the jvm image and its' heap
*
* @param vm[in] the java vm 
*
* @return 0 on fail, 1 on success
*/
UDATA initializeJVMImage(J9JavaVM *vm);

/*
* Registers class loader in table
*
* @param classLoader[in] J9ClassLoader to register
*/
void registerClassLoader(J9ClassLoader *classLoader);

/*
* Registers class segment in table
*
* @param clazz[in] J9Class to register
*/
void registerClassSegment(J9Class *clazz);

/*
* Registers class path entry in table
*
* @param cpEntry[in] J9ClassPathEntry to register
*/
void registerCPEntry(J9ClassPathEntry *cpEntry);

/*
* Returns the port library used by the image
* Do not use directly but through macro defined in jvmimage.h
*
* @return pointer to port library of image on success, NULL on failure
*/
OMRPortLibrary* getImagePortLibrary();

/*
* Shut down sequence of JVMImage
* Frees memory of heap variables and jvmimage instance
*
* @param vm[in] the java vm 
*/
void shutdownJVMImage(J9JavaVM *vm);

#ifdef __cplusplus
}
#endif

#endif /* jvmimage_api_h */

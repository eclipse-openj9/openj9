
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

#if !defined(MIXEDOBJECTMODEL_HPP_)
#define MIXEDOBJECTMODEL_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "objectdescription.h"

#include "Bits.hpp"

class MM_GCExtensionsBase;

/**
 * Provides information for mixed objects.
 * @ingroup GC_Base
 */
class GC_MixedObjectModel
{

/*
* Data members
*/
private:
protected:
public:

/*
* Function members
*/
private:
protected:
public:
	/**
	 * Returns the size of a class, in bytes, excluding the header.
	 * @param clazzPtr Pointer to the class whose size is required
	 * @return Size of class in bytes excluding the header
	 */
	MMINLINE UDATA
	getSizeInBytesWithoutHeader(J9Class *clazz)
	{
		return clazz->totalInstanceSize;
	}

	/**
	 * Returns the size of a mixed object, in bytes, excluding the header.
	 * @param objectPtr Pointer to the object whose size is required
	 * @return Size of object in bytes excluding the header
	 */	
	MMINLINE UDATA
	getSizeInBytesWithoutHeader(J9Object *objectPtr)
	{
		return getSizeInBytesWithoutHeader(J9GC_J9OBJECT_CLAZZ(objectPtr));
	}

	/**
	 * Returns the size of a mixed object, in bytes, including the header.
	 * @param objectPtr Pointer to the object whose size is required
	 * @return Size of object in bytes including the header
	 */		
	MMINLINE UDATA
	getSizeInBytesWithHeader(J9Object *objectPtr)
	{
		return getSizeInBytesWithoutHeader(objectPtr) + sizeof(J9Object);
	}
	
	/**
	* Returns an address of first data slot of the object
	* @param objectPtr Pointer to the object
	* @return Address of first data slot of the object
	*/
	MMINLINE fomrobject_t *
	getHeadlessObject(J9Object *objectPtr)
	{
		return (fomrobject_t *)((uint8_t*)objectPtr + getHeaderSize(objectPtr));
	}
	
	/**
	 * Returns the header size of a given  object.
	 * @param arrayPtr Ptr to an array for which header size will be returned
	 * @return Size of header in bytes
	 */
	MMINLINE UDATA
	getHeaderSize(J9Object *objectPtr)
	{
		return sizeof(J9Object);
	}

	/**
	 * Returns the offset of the hashcode slot, in bytes, from the beginning of the header.
	 * @param clazzPtr Pointer to the class of the object
	 * @return offset of the hashcode slot
	 */
	MMINLINE UDATA
	getHashcodeOffset(J9Class *clazzPtr)
	{
		return clazzPtr->backfillOffset;
	}

	/**
	 * Returns the offset of the hashcode slot, in bytes, from the beginning of the header.
	 * @param arrayPtr Pointer to the object
	 * @return offset of the hashcode slot
	 */
	MMINLINE UDATA
	getHashcodeOffset(J9Object *objectPtr)
	{
		return getHashcodeOffset(J9GC_J9OBJECT_CLAZZ(objectPtr));
	}

	/**
	 * Initialize the receiver, a new instance of GC_ObjectModel
	 * 
	 * @return true on success, false on failure
	 */
	bool initialize(MM_GCExtensionsBase *extensions);
	
	/**
	 * Tear down the receiver
	 */
	void tearDown(MM_GCExtensionsBase *extensions);
};

#endif /* MIXEDOBJECTMODEL_HPP_ */

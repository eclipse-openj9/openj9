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

#if !defined(SCAVENGERFORWARDEDHEADER_HPP_)
#define SCAVENGERFORWARDEDHEADER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "ModronAssertions.h"

#include "AtomicOperations.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "ObjectAccessBarrier.hpp"

/* @ddr_namespace: map_to_type=MM_ScavengerForwardedHeader */

/* used to distinguish a forwarded object from a class pointer */
#define FORWARDED_TAG ((UDATA)0x4)
/* the grow tag is used by VLHGC and can only be set if the FORWARDED_TAG is already set.  It signifies that the object grew a hash field when moving */
#define GROW_TAG ((UDATA)0x2)
/* combine these flags into one mask which should be stripped from the pointer in order to remove all tags */
#define ALL_TAGS (FORWARDED_TAG | GROW_TAG)

/**
 * Used to construct a linked list of objects on the heap.
 * This structure provides the ability to restore the object, so
 * only data which can be restored from the class is destroyed. 
 * Because its fields must line up with J9Object no virtual methods
 * are permitted.
 * 
 * @ingroup GC_Base
 */
class MM_ScavengerForwardedHeader
{
public:
protected:

	struct MutableHeaderFields {
		/* class slot must be always aligned to UDATA */
		j9objectclass_t clazz;

#if defined (OMR_GC_COMPRESSED_POINTERS)
		/*
		 * this field is used indirectly by extending of clazz field (8 bytes starting from &MutableHeaderFields.clazz)
		 * must be here to reserve space if clazz field is 4 bytes long
		 */
		U_32 overlap;
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */
	};
	
	omrobjectptr_t _objectPtr; /**< the object on which to act */
	MutableHeaderFields _preserved; /**< a backup copy of the header fields which may be modified by this class */
private:

public:
	
	/**
	 * Called at the start of a copy-forward (minimally, must be called at least once after trace is enabled but before we use this class) to validate
	 * sizing and object geometry assumptions.
	 */
	static void validateAssumptions();
	
	/**
	 * Update this object to be forwarded to destinationObjectPtr using atomic operations.
	 * If the update fails (because the object has already been forwarded), read the forwarded
	 * header and return the forwarded object which was written into the header.
	 * 
	 * @parm[in] destinationObjectPtr the object to forward to
	 * 
	 * @return the winning forwarded object (either destinationObjectPtr or one written by another thread)
	 */
	omrobjectptr_t setForwardedObject(omrobjectptr_t destinationObjectPtr);

#if defined(J9VM_GC_VLHGC)
	/**
	 * Similar to setForwardedObject but optionally sets the "GROW_TAG" on the forwarding pointer so that we know the object
	 * grew a hashcode slot when it moved.
	 * @param destinationObjectPtr[in] The new location of the object
	 * @param isObjectGrowing[in] True if the object grew during the move
	 * @return The location of the new copy of the object (will be destinationObjectPtr if the calling thread succeeded in the copy)
	 */
	omrobjectptr_t setForwardedObjectGrowing(omrobjectptr_t destinationObjectPtr, bool isObjectGrowing);

	/**
	 * @return True if the underlying object (original location) resized when it was copied to its new location 
	 */
	bool didObjectGrowOnCopy();
#endif /* defined(J9VM_GC_VLHGC) */

	/**
	 * Determine if the current object is forwarded.
	 * 
	 * @note This function can safely be called for a free list entry. It will
	 * return false in that case.
	 * 
	 * @return true if the current object is forwarded, false otherwise
	 */
	MMINLINE bool
	isForwardedPointer()
	{
		return FORWARDED_TAG == ((UDATA)_preserved.clazz & FORWARDED_TAG);
	}
	
	/**
	 * Determine if the current object is Indexable
	 *
	 * @return true if object is indexable
	 */
	MMINLINE bool
	isIndexable()
	{
		J9Class* preservedClass = (J9Class*)((UDATA)_preserved.clazz & J9GC_J9OBJECT_CLAZZ_ADDRESS_MASK);
		return J9GC_CLASS_IS_ARRAY(preservedClass);
	}

	/**
	 * Determine if the current object has been moved
	 *
	 * @return true if object has been moved
	 */
	MMINLINE bool
	hasBeenMoved()
	{
		return (OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS == ((UDATA)_preserved.clazz & OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS));
	}

	/**
	 * Determine if the current object has been hashed
	 *
	 * @return true if object has been hashed
	 */
	MMINLINE bool
	hasBeenHashed()
	{
		return (0 != ((UDATA)_preserved.clazz & (OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS | OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS)));
	}

	/**
	 * Determine if the current object has been hashed
	 *
	 * @return true if object has been hashed
	 */
	MMINLINE bool
	isHasBeenHashedBitSet()
	{
		return (OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS == ((UDATA)_preserved.clazz & OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS));
	}

	/**
	 * Determine if the current object is a reverse forwarded object.
	 * 
	 * @note reverse forwarded objects are indistinguishable from holes. Only use this
	 * function if you can be sure the object is not a hole.
	 * 
	 * @return true if the current object is reverse forwarded, false otherwise
	 */
	MMINLINE bool
	isReverseForwardedPointer()
	{
		return J9_GC_MULTI_SLOT_HOLE == ((UDATA)_preserved.clazz & J9_GC_OBJ_HEAP_HOLE_MASK);
	}
	
	/**
	 * If the object has been forwarded, return the forwarded version of the
	 * object, otherwise return NULL.
	 * 
	 * @note This function can safely be called for a free list entry. It will
	 * return NULL in that case.
	 * 
	 * @return the forwarded version of this object or NULL
	 */
	MMINLINE omrobjectptr_t
	getForwardedObject()
	{
		if (isForwardedPointer()) {
			return getForwardedObjectNoCheck();
		} else {
			return NULL;
		}
	}
	
	/**
	 * @return the object pointer represented by the receiver
	 */
	MMINLINE omrobjectptr_t
	getObject()
	{
		return _objectPtr;
	}

	/**
	 * If the object has not been forwarded, return the class pointer
	 * of the object. Do not attempt to read the class pointer of
	 * the object directly, as it may have been updated asynchronously
	 * by another thread.
	 * 
	 * @return the class pointer of the object
	 */
	MMINLINE J9Class*
	getPreservedClass()
	{
		Assert_MM_false(isForwardedPointer());
		return (J9Class*)((UDATA)_preserved.clazz & J9GC_J9OBJECT_CLAZZ_ADDRESS_MASK);
	}
	
	/**
	 * Return the flags of the object. 
	 * If flags are stored in low bits of class slot
	 * the object can not be read directly (may be destroyed already).
	 *
	 * @return the flags of the object
	 */
	MMINLINE UDATA
	getPreservedFlags()
	{
		return (UDATA)_preserved.clazz & J9GC_J9OBJECT_CLAZZ_FLAGS_MASK;
	}

	/**
	 * Return back object's Age
	 */
	MMINLINE UDATA getPreservedAge()
	{
		return (UDATA)_preserved.clazz & OBJECT_HEADER_AGE_MASK;
	}
	/**
	 * Get the reverse forwarded pointer for this object.
	 * 
	 * @return the reverse forwarded value
	 */
	MMINLINE omrobjectptr_t
	getReverseForwardedPointer()
	{
		Assert_MM_true(isReverseForwardedPointer());
		return (omrobjectptr_t)MM_HeapLinkedFreeHeader::getHeapLinkedFreeHeader(_objectPtr)->getNext();
	}
	
	/**
	 * Construct a new ScavengerForwardedHeader for the specified object.
	 * Any fields which may be modified by forwarding are cached at this
	 * time.
	 * 
	 * @parm[in] object the object to be used
	 */
	MMINLINE
	MM_ScavengerForwardedHeader(omrobjectptr_t object) :
		_objectPtr(object)
	{
		volatile MutableHeaderFields* originalHeader = (volatile MutableHeaderFields *)_objectPtr;
		/* class slot is always aligned to UDATA (compressed or not)
		 * so forwarded pointer is stored in UDATA word starts class slot address
		 * (overlap next slot for compressed)
		 * So, for compressed this read fulfill clazz and overlap fields at the same time
		 */
		*(UDATA *)&_preserved.clazz = *((UDATA *)&originalHeader->clazz);
	}

	/**
	 * Update the new version of this object after it has been copied. This undoes any damaged
	 * caused by installing the forwarding pointer into the original prior to the copy.
	 * Minimally this will install the correct (i.e. unforwarded) class pointer and the
	 * updated flags.
	 *
	 * @parm[in] copiedObjectPtr the copied object to be fixed up
	 */
	MMINLINE void
	fixupCopiedObject(omrobjectptr_t copiedObjectPtr)
	{
		volatile MutableHeaderFields* newHeader = (volatile MutableHeaderFields *)copiedObjectPtr;

		/* class slot is always aligned to UDATA (compressed or not)
		 * so forwarded pointer is stored in UDATA word starts class slot address
		 * (overlap next slot for compressed)
		 */
		*(UDATA *)&newHeader->clazz = *(UDATA *)&_preserved.clazz;
	}
	/**
	 * Update the new version of this object after it has been copied. This undoes any damaged
	 * caused by installing the forwarding pointer into the original prior to the copy.
	 * Minimally this will install the correct (i.e. unforwarded) class pointer and the
	 * updated flags.
	 * 
	 * @parm[in] copiedObjectPtr the copied object to be fixed up
	 * @parm[in] newFlags the flags to set in the copied object
	 */
	MMINLINE void
	fixupCopiedObject(omrobjectptr_t copiedObjectPtr, UDATA newAge, bool setOldFlag)
	{
		volatile MutableHeaderFields* newHeader = (volatile MutableHeaderFields *)copiedObjectPtr;
		
		/* restore class slot from local copy, change age flags in low bits of class slot at the same time */
		newHeader->clazz = (j9objectclass_t)(((UDATA)_preserved.clazz & ~OBJECT_HEADER_AGE_MASK) | newAge);

#if defined (OMR_GC_COMPRESSED_POINTERS)
		/* first object slot is destroyed, must be restored from local copy as well */
		newHeader->overlap = _preserved.overlap;
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */
	}

	MMINLINE U_32
	getPreservedIndexableSize()
	{
#if defined (OMR_GC_COMPRESSED_POINTERS)
		/* in compressed headers, the size of the object is stored in the other half of the UDATA read when we read clazz
		 * so read it from there instead of the heap (since the heap copy would have been over-written by the forwarding
		 * pointer if another thread copied the object underneath us).
		 * In non-compressed, this field should still be readable out of the heap.
		 */
		U_32 size = _preserved.overlap;
#else /* defined (OMR_GC_COMPRESSED_POINTERS) */
		U_32 size = ((J9IndexableObjectContiguous *)_objectPtr)->size;
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */
#if defined(J9VM_GC_HYBRID_ARRAYLETS)
		if (0 == size) {
			/* Discontiguous */
			size = ((J9IndexableObjectDiscontiguous *)_objectPtr)->size;
		}
#endif
		return size;

	}

	MMINLINE I_32 computeObjectHash(J9VMThread* vmThread)
	{
		J9JavaVM* javaVM = vmThread->javaVM;
		return convertValueToHash(javaVM, (UDATA)_objectPtr);
	}

	MMINLINE UDATA
	getHashcodeOffset(J9JavaVM* javaVM, J9Object* dest)
	{
		MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
		UDATA hashOffset = 0;

		if (isIndexable()) {
			hashOffset = extensions->indexableObjectModel.getHashcodeOffset((J9IndexableObject *)dest);
		} else {
			hashOffset = extensions->mixedObjectModel.getHashcodeOffset(dest);
		}

		return hashOffset;
	}

protected:

	/**
	 * Return the forwarded version of the object
	 */
	MMINLINE omrobjectptr_t
	getForwardedObjectNoCheck()
	{
		Assert_MM_true(isForwardedPointer());

#if defined (OMR_GC_COMPRESSED_POINTERS) && !defined(J9VM_ENV_LITTLE_ENDIAN)
		/* compressed big endian - read two halves separately */
		U_32 low = _preserved.clazz & ~ALL_TAGS;
		U_32 high = _preserved.overlap;
		omrobjectptr_t forwardedObject = (omrobjectptr_t)((U_64)low | ((U_64)high << 32));

#else /* defined (OMR_GC_COMPRESSED_POINTERS) && !defined(J9VM_ENV_LITTLE_ENDIAN) */

		/* Little endian or not compressed - read all UDATA bytes at once */
		omrobjectptr_t forwardedObject = (omrobjectptr_t)(*(UDATA *)(&_preserved.clazz) & ~ALL_TAGS);

#endif /* defined (OMR_GC_COMPRESSED_POINTERS) && !defined(J9VM_ENV_LITTLE_ENDIAN) */

		return forwardedObject;
	}

private:

};
#endif /* SCAVENGERFORWARDEDHEADER_HPP_ */

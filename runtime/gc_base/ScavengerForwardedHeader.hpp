/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
	omrobjectptr_t _objectPtr; /**< the object on which to act */
	UDATA _preserved; /**< a backup copy of the header fields which may be modified by this class */
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
	bool _compressObjectReferences;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
private:

public:
	
	/**
	 * Return back true if object references are compressed
	 * @return true, if object references are compressed
	 */
	MMINLINE bool
	compressObjectReferences()
	{
#if defined(OMR_GC_COMPRESSED_POINTERS)
#if defined(OMR_GC_FULL_POINTERS)
#if defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES)
		return (bool)OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES;
#else /* defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES) */
		return _compressObjectReferences;
#endif /* defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES) */
#else /* defined(OMR_GC_FULL_POINTERS) */
		return true;
#endif /* defined(OMR_GC_FULL_POINTERS) */
#else /* defined(OMR_GC_COMPRESSED_POINTERS) */
		return false;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
	}

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
		return FORWARDED_TAG == (getPreservedClassAndTags() & FORWARDED_TAG);
	}
	
	/**
	 * Determine if the current object is Indexable
	 *
	 * @return true if object is indexable
	 */
	MMINLINE bool
	isIndexable()
	{
		J9Class* preservedClass = (J9Class*)(getPreservedClassAndTags() & J9GC_J9OBJECT_CLAZZ_ADDRESS_MASK);
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
		return (OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS == (getPreservedClassAndTags() & OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS));
	}

	/**
	 * Determine if the current object has been hashed
	 *
	 * @return true if object has been hashed
	 */
	MMINLINE bool
	hasBeenHashed()
	{
		return (0 != (getPreservedClassAndTags() & (OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS | OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS)));
	}

	/**
	 * Determine if the current object has been hashed
	 *
	 * @return true if object has been hashed
	 */
	MMINLINE bool
	isHasBeenHashedBitSet()
	{
		return (OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS == (getPreservedClassAndTags() & OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS));
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
		return J9_GC_MULTI_SLOT_HOLE == (getPreservedClassAndTags() & J9_GC_OBJ_HEAP_HOLE_MASK);
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
	 * Fetch the class portion of the preserved data (with any tags).
	 * 
	 * @return the class and tags
	 */
	MMINLINE UDATA
	getPreservedClassAndTags()
	{
		UDATA result = _preserved;
#if defined(OMR_GC_COMPRESSED_POINTERS)
		if (compressObjectReferences()) {
#if defined(J9VM_ENV_LITTLE_ENDIAN)
			result &= 0xFFFFFFFF;
#else /* defined(J9VM_ENV_LITTLE_ENDIAN) */
			result >>= 32;
#endif /* defined(J9VM_ENV_LITTLE_ENDIAN) */
		} 
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		return result;
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
		return (J9Class*)(getPreservedClassAndTags() & J9GC_J9OBJECT_CLAZZ_ADDRESS_MASK);
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
		return (UDATA)getPreservedClassAndTags() & J9GC_J9OBJECT_CLAZZ_FLAGS_MASK;
	}

	/**
	 * Return back object's Age
	 */
	MMINLINE UDATA getPreservedAge()
	{
		return (UDATA)getPreservedClassAndTags() & OBJECT_HEADER_AGE_MASK;
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
		return (omrobjectptr_t)MM_HeapLinkedFreeHeader::getHeapLinkedFreeHeader(_objectPtr)->getNext(compressObjectReferences());
	}
	
	/**
	 * Construct a new ScavengerForwardedHeader for the specified object.
	 * Any fields which may be modified by forwarding are cached at this
	 * time.
	 *
	 * The class slot is always aligned to UDATA (it is at offset 0, compressed or not)
	 * so forwarded pointer is stored in UDATA word starts class slot address
	 * (overlap next slot for compressed). So, for compressed this read fulfill clazz and
	 * overlap fields at the same time.
	 *
	 * @parm[in] object the object to be used
	 * @parm[in] extensions the current GC extensions
	 */
	MMINLINE
	MM_ScavengerForwardedHeader(omrobjectptr_t object, MM_GCExtensionsBase *extensions) :
		_objectPtr(object)
		, _preserved(*(volatile UDATA *)_objectPtr)
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
		, _compressObjectReferences(extensions->compressObjectReferences())
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
	{
	}

	/**
	 * Construct a new ScavengerForwardedHeader for the specified object.
	 * Any fields which may be modified by forwarding are cached at this
	 * time.
	 * 
	 * The class slot is always aligned to UDATA (it is at offset 0, compressed or not)
	 * so forwarded pointer is stored in UDATA word starts class slot address
	 * (overlap next slot for compressed). So, for compressed this read fulfill clazz and
	 * overlap fields at the same time.
	 *
	 * @parm[in] object the object to be used
	 * @parm[in] compress boolean indicating whether compressed references is in use
	 */
	MMINLINE
	MM_ScavengerForwardedHeader(omrobjectptr_t object, bool compress) :
		_objectPtr(object)
		, _preserved(*(volatile UDATA *)_objectPtr)
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
		, _compressObjectReferences(compress)
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
	{
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
		/* class slot is always aligned to UDATA (compressed or not)
		 * so forwarded pointer is stored in UDATA word starts class slot address
		 * (overlap next slot for compressed)
		 */
		*(UDATA*)copiedObjectPtr = _preserved;
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
		/* restore class slot from local copy, change age flags in low bits of class slot at the same time */
		UDATA clazz = (getPreservedClassAndTags() & ~OBJECT_HEADER_AGE_MASK) | newAge;
#if defined(OMR_GC_COMPRESSED_POINTERS) && !defined(J9VM_ENV_LITTLE_ENDIAN)
		if (compressObjectReferences()) {
			/* compressed big endian - classAndFlags in low memory (high order of the UDATA, overlap in high memory */
			clazz = (clazz << 32) | (_preserved & 0xFFFFFFFF);
		}
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && !defined(J9VM_ENV_LITTLE_ENDIAN) */
		*(UDATA*)copiedObjectPtr = clazz;
	}

	MMINLINE U_32
	getPreservedIndexableSize()
	{
		U_32 size = 0;

		/* in compressed headers, the size of the object is stored in the other half of the UDATA read when we read clazz
		 * so read it from there instead of the heap (since the heap copy would have been over-written by the forwarding
		 * pointer if another thread copied the object underneath us).
		 * In non-compressed, this field should still be readable out of the heap.
		 */
		if (compressObjectReferences()) {
			size = ((J9IndexableObjectContiguousCompressed *)&_preserved)->size;
			if (0 == size) {
				/* Discontiguous */
				size = ((J9IndexableObjectDiscontiguousCompressed *)_objectPtr)->size;
			}
		} else {
			size = ((J9IndexableObjectContiguousFull *)_objectPtr)->size;
			if (0 == size) {
				/* Discontiguous */
				size = ((J9IndexableObjectDiscontiguousFull *)_objectPtr)->size;
			}
		}

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
		UDATA forwardedObject = _preserved;
#if defined(OMR_GC_COMPRESSED_POINTERS) && !defined(J9VM_ENV_LITTLE_ENDIAN)
		if (compressObjectReferences()) {
			/* Forwarding pointer has been stored endian-flipped - flip it back
			 * (see MM_ScavengerForwardedHeader::setForwardedObject).
			 */
			forwardedObject = (forwardedObject >> 32) | (forwardedObject << 32);
		}
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && !defined(J9VM_ENV_LITTLE_ENDIAN) */

		return (omrobjectptr_t)(forwardedObject & ~ALL_TAGS);
	}

private:

};
#endif /* SCAVENGERFORWARDEDHEADER_HPP_ */

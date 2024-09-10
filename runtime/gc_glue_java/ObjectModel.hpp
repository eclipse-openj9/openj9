/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(OBJECTMODEL_HPP_)
#define OBJECTMODEL_HPP_

/* @ddr_namespace: default */
#include <assert.h>

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9nonbuilder.h"
#include "modron.h"
#include "j9modron.h"
#include "objectdescription.h"
#include "util_api.h"

#include "ArrayObjectModel.hpp"
#include "ArrayletObjectModel.hpp"
#include "AtomicOperations.hpp"
#include "ForwardedHeader.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "MixedObjectModel.hpp"
#include "ObjectModelBase.hpp"
#include "ObjectModelDelegate.hpp"

#if defined(OMR_GC_REALTIME)
/* this bit is set in the object header slot if an overflow condition is raised */
#define GC_OVERFLOW	 0x4
#endif /* defined(OMR_GC_REALTIME) */

/*
 * #defines representing the scope depth stored in each object header
 * use two low bits of Collector Bits for Depth
 */
#define OBJECT_HEADER_DEPTH_ZERO	0
#define OBJECT_HEADER_DEPTH_MAX		3
#define OBJECT_HEADER_DEPTH_SHIFT	OMR_OBJECT_METADATA_AGE_SHIFT
#define OBJECT_HEADER_DEPTH_MASK	(OBJECT_HEADER_DEPTH_MAX << OBJECT_HEADER_DEPTH_SHIFT)

/* check that we have enough collector bits to be used for Depth */
#if (0 != (OBJECT_HEADER_DEPTH_MASK & ~(OMR_OBJECT_METADATA_AGE_MASK)))
#error "Some OBJECT_HEADER_DEPTH_MASK bits are out of range of OMR_OBJECT_METADATA_AGE_MASK."
#endif /* (0 != (OBJECT_HEADER_DEPTH_MASK & ~(OMR_OBJECT_METADATA_AGE_MASK))) */

/*
 * #defines representing the 2 bits stored in immortal object header used as mark and overflow bits by ReferenceChainWalker.
 * Both bits occupy the arraylet layout bits which have been deprecated
 */
#define OBJECT_HEADER_REFERENCE_CHAIN_WALKER_IMMORTAL_MARKED 0x40
#define OBJECT_HEADER_REFERENCE_CHAIN_WALKER_IMMORTAL_OVERFLOW 0xC0

/* Object header flag bits used for lazy hashcode insertion on copy/move */
#define OMR_GC_DEFERRED_HASHCODE_INSERTION

class MM_AllocateInitialization;
class MM_EnvironmentBase;
class MM_GCExtensionsBase;

/**
 * Provides information for a given object.
 * @ingroup GC_Base
 */
class GC_ObjectModel : public GC_ObjectModelBase
{
/*
 * Member data and types
 */
private:
	J9JavaVM* _javaVM; /***< pointer to the Java VM */
	GC_MixedObjectModel *_mixedObjectModel; /**< pointer to the mixed object model in extensions (so that we can delegate to it) */
	GC_ArrayObjectModel *_indexableObjectModel; /**< pointer to the indexable object model in extensions (so that we can delegate to it) */
	J9Class *_classClass; /**< java.lang.Class class pointer for detecting special objects */
	J9Class *_classLoaderClass; /**< java.lang.ClassLoader class pointer for detecting special objects */
	J9Class *_continuationClass; /**< jdk/internal/vm/Continuation class pointer for detecting subclass of Continuation */
	J9Class *_atomicMarkableReferenceClass; /**< java.util.concurrent.atomic.AtomicMarkableReference class pointer for detecting special objects */

protected:
public:
	/**
 	* Return values for getScanType().
 	*/
	enum ScanType {
		SCAN_INVALID_OBJECT = 0,
		SCAN_MIXED_OBJECT = 1,
		SCAN_POINTER_ARRAY_OBJECT = 2,
		SCAN_PRIMITIVE_ARRAY_OBJECT = 3,
		SCAN_REFERENCE_MIXED_OBJECT = 4,
		SCAN_CLASS_OBJECT = 5,
		SCAN_CLASSLOADER_OBJECT = 6,
		SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT = 7,
		SCAN_OWNABLESYNCHRONIZER_OBJECT = 8,
		SCAN_MIXED_OBJECT_LINKED = 9,
		SCAN_FLATTENED_ARRAY_OBJECT = 10,
		SCAN_CONTINUATION_OBJECT = 11
	};

	/**
	 * Values for the 'state' field in java.lang.ref.Reference.
	 * Note that these values are mirrored in the Java code. Do not change them.
	 */
	enum ReferenceState {
		REF_STATE_INITIAL = 0, /**< indicates the initial (normal) state for a Reference object. Referent is weak. */
		REF_STATE_CLEARED = 1, /**< indicates that the Reference object has been cleared, either by the GC or by the Java clear() API. Referent is null or strong. */ 
		REF_STATE_ENQUEUED = 2, /**< indicates that the Reference object has been cleared and enqueued on its ReferenceQueue. Referent is null or strong. */
		REF_STATE_REMEMBERED = 3, /**< indicates that the Reference object was discovered by a global cycle and that the current local GC cycle must return it to that list and restore the state to INITIAL. */
	};
	
/*
 * Member functions
 */
private:
	/**
	 * Determine the scan type for an instant of the specified class.
	 * The class has the J9AccClassGCSpecial bit set.
	 * @param[in] objectClazz the class of the object to identify
	 * @return one of the ScanType constants 
	 */
	ScanType getSpecialClassScanType(J9Class *objectClazz);
	
	/**
	 * Examine all classes as they are loaded to determine if they require the J9AccClassGCSpecial bit.
	 * These classes are handled specially by GC_ObjectModel::getScanType()
	 */
	static void internalClassLoadHook(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);

	/**
	 * Update all of the  GC special class pointers to their most current version after a class
	 * redefinition has occurred.
	 */
	static void classesRedefinedHook(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
	
	/**
	 * Returns the shape of an class.
	 * @param objectPtr Pointer to object whose shape is required.
	 * @return The shape of the object
	 */
	MMINLINE uintptr_t
	getClassShape(J9Object *objectPtr)
	{
		J9Class* clazz = J9GC_J9OBJECT_CLAZZ(objectPtr, this);
		return J9GC_CLASS_SHAPE(clazz);
	}

public:
	/**
	 * Determine the ScanType code for objects of the specified class. This code determines how instances should be scanned.
	 * @param clazz[in] the class of the object to be scanned
	 * @return a ScanType code, SCAN_INVALID_OBJECT if the code cannot be determined due to an error
	 */
	MMINLINE ScanType
	getScanType(J9Class *clazz)
	{
		ScanType result = SCAN_INVALID_OBJECT;

		switch(J9GC_CLASS_SHAPE(clazz)) {
		case OBJECT_HEADER_SHAPE_MIXED:
		{
			uintptr_t classFlags = J9CLASS_FLAGS(clazz) & (J9AccClassReferenceMask | J9AccClassGCSpecial | J9AccClassOwnableSynchronizer | J9AccClassContinuation);
			if (0 == classFlags) {
				if (0 != clazz->selfReferencingField1) {
					result = SCAN_MIXED_OBJECT_LINKED;
				} else {
					result = SCAN_MIXED_OBJECT;
				}
			} else {
				if (0 != (classFlags & J9AccClassReferenceMask)) {
					result = SCAN_REFERENCE_MIXED_OBJECT;
				} else if (0 != (classFlags & J9AccClassGCSpecial)) {
					result = getSpecialClassScanType(clazz);
				} else if (0 != (classFlags & J9AccClassOwnableSynchronizer)) {
					result = SCAN_OWNABLESYNCHRONIZER_OBJECT;
				} else if (0 != (classFlags & J9AccClassContinuation)) {
					result = SCAN_CONTINUATION_OBJECT;
				} else {
					/* Assert_MM_unreachable(); */
					assert(false);
				}
			}
			break;
		}
		case OBJECT_HEADER_SHAPE_POINTERS:
			if (J9_IS_J9CLASS_FLATTENED(clazz)) {
				if (J9CLASS_HAS_REFERENCES(((J9ArrayClass *)clazz)->leafComponentType)) {
					result = SCAN_FLATTENED_ARRAY_OBJECT;
				} else {
					result = SCAN_PRIMITIVE_ARRAY_OBJECT;
				}
			} else {
				result = SCAN_POINTER_ARRAY_OBJECT;
			}
			break;
		case OBJECT_HEADER_SHAPE_DOUBLES:
		case OBJECT_HEADER_SHAPE_BYTES:
		case OBJECT_HEADER_SHAPE_WORDS:
		case OBJECT_HEADER_SHAPE_LONGS:
			/* Must be a primitive array*/
			result = SCAN_PRIMITIVE_ARRAY_OBJECT;
			break;
		default:
			result = SCAN_INVALID_OBJECT;
		}

		return result;
	}

	MMINLINE ScanType
	getScanType(J9Object *objectPtr)
	{
		J9Class *clazz = J9GC_J9OBJECT_CLAZZ(objectPtr, this);
		return getScanType(clazz);
	}
	
	/**
	 * Returns the depth of an object.
	 * @param objectPtr Pointer to object whose depth is required.
	 * @return The depth of the object
	 */
	MMINLINE uintptr_t
	getObjectDepth(J9Object *objectPtr)
	{
		return (getRememberedBits(objectPtr) & OBJECT_HEADER_DEPTH_MASK);
	}

	/**
	 * Returns TRUE if a class is indexable, FALSE otherwise.
	 * @param clazz Pointer to the class
	 * @return TRUE if a class is indexable, FALSE otherwise
	 */
	MMINLINE bool
	isIndexable(J9Class* clazz)
	{
		return J9GC_CLASS_IS_ARRAY(clazz);
	}

	using GC_ObjectModelBase::isIndexable;

	/**
	 * Returns TRUE if an object has a OBJECT_HEADER_SHAPE_POINTERS shape, FALSE otherwise.
	 * @param objectPtr Pointer to an object
	 * @return TRUE if an object has a OBJECT_HEADER_SHAPE_POINTERS shape, FALSE otherwise
	 */
	MMINLINE bool
	isObjectArray(J9Object *objectPtr)
	{
		J9Class* clazz = J9GC_J9OBJECT_CLAZZ(objectPtr, this);
		return (OBJECT_HEADER_SHAPE_POINTERS == J9GC_CLASS_SHAPE(clazz));
	}

	/**
	 * @see isObjectArray(J9Object *objectPtr)
	 */
	MMINLINE bool
	isObjectArray(J9IndexableObject *objectPtr)
	{
		return isObjectArray((J9Object*)objectPtr);
	}
	
	/**
	 * Returns TRUE if an object is primitive array, FALSE otherwise.
	 * @param objectPtr Pointer to an object
	 * @return TRUE if an object is primitive array, FALSE otherwise
	 */
	MMINLINE bool
	isPrimitiveArray(J9Object *objectPtr)
	{
		bool isPrimitiveArray = false;

		switch(getClassShape(objectPtr)) {
		case OBJECT_HEADER_SHAPE_BYTES:
		case OBJECT_HEADER_SHAPE_WORDS:
		case OBJECT_HEADER_SHAPE_LONGS:
		case OBJECT_HEADER_SHAPE_DOUBLES:
			isPrimitiveArray = true;
			break;
		default:
			isPrimitiveArray = false;
			break;
		}

		return isPrimitiveArray;
	}

	/**
	 * @see isPrimitiveArray(J9Object *objectPtr)
	 */
	MMINLINE bool
	isPrimitiveArray(J9IndexableObject *objectPtr)
	{
		return isPrimitiveArray((J9Object*)objectPtr);
	}
	
	/**
	 * Determine whether or not the given object is a double array
	 *
	 * @return true if objectPtr is a double array
	 * @return false otherwise
	 */
	MMINLINE bool
	isDoubleArray(J9Object* objectPtr)
	{
		return (OBJECT_HEADER_SHAPE_DOUBLES == getClassShape(objectPtr));
	}

	/**
	 * @see isDoubleArray(J9Object* objectPtr)
	 */
	MMINLINE bool
	isDoubleArray(J9IndexableObject *objectPtr)
	{
		return isDoubleArray((J9Object*)objectPtr);
	}
	
	/**
	 * Check is indexable bit set properly:
	 * must be set for arrays and primitive arrays
	 * must not be set for all others
	 * @param objectPtr Pointer to an object
	 * @return TRUE if indexable bit is set properly
	 */
	MMINLINE bool
	checkIndexableFlag(J9Object *objectPtr)
	{
		bool result = false;

		if (isObjectArray(objectPtr) || isPrimitiveArray(objectPtr)) {
			if (isIndexable(objectPtr)) {
				result = true;
			}
		} else {
			if (!isIndexable(objectPtr)) {
				result = true;
			}
		}
		return result;
	}
	
	/**
	 * Determine the basic hash code for the specified object. This may modify the object. For example, it may
	 * set the HAS_BEEN_HASHED bit in the object's header. Object must not be NULL.
	 * 
	 * @param object[in] the object to be hashed
	 * @return the persistent, basic hash code for the object 
	 */
	MMINLINE int32_t
	getObjectHashCode(J9JavaVM *vm, J9Object *object)
	{
		int32_t result = 0;
#if defined (OMR_GC_MODRON_COMPACTION) || defined (J9VM_GC_GENERATIONAL)
		if (hasBeenMoved(object)) {
			uintptr_t hashOffset = getHashcodeOffset(object);
			result = *(int32_t*)((uint8_t*)object + hashOffset);
		} else {
			atomicSetObjectFlags(object, 0, OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS);
			result = convertValueToHash(vm, (uintptr_t)object);
		}
#else /* defined (OMR_GC_MODRON_COMPACTION) || defined (J9VM_GC_GENERATIONAL) */
		result = computeObjectAddressToHash(vm, object);
#endif /* defined (OMR_GC_MODRON_COMPACTION) || defined (J9VM_GC_GENERATIONAL) */
		return result;
	}

	/**
	 * Initialize the basic hash code for the specified object.
	 * Space for the hash slot must already exist.
	 *
	 * @note Sets OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS | OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS flags
	 * @param object[in] the object to be initialized.
	 */
	MMINLINE void
	initializeHashSlot(J9JavaVM* vm, J9Object *objectPtr)
	{
#if defined (OMR_GC_MODRON_COMPACTION) || defined (J9VM_GC_GENERATIONAL)
		uintptr_t hashOffset = getHashcodeOffset(objectPtr);
		uint32_t *hashCodePointer = (uint32_t*)((uint8_t*)objectPtr + hashOffset);

		*hashCodePointer = convertValueToHash(vm, (uintptr_t)objectPtr);
		setObjectHasBeenMoved(objectPtr);
#endif /* defined (OMR_GC_MODRON_COMPACTION) || defined (J9VM_GC_GENERATIONAL) */
	}

	/**
	 * Returns TRUE if an object has been hashed or moved, FALSE otherwise.
	 * @param objectPtr Object to test
	 * @return TRUE if an object has been hashed or moved, FALSE otherwise
	 */
	MMINLINE bool
	hasBeenHashed(J9Object *objectPtr)
	{
		return hasBeenHashed(getObjectFlags(objectPtr));
	}

	MMINLINE bool
	hasBeenHashed(uintptr_t objectFlags)
	{
		return 0 != (objectFlags & (OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS | OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS));
	}

	/**
	 * Returns TRUE if an object has been hashed but not moved, FALSE otherwise.
	 * @param objectPtr Object to test
	 * @return TRUE if an object has been hashed but not moved, FALSE otherwise
	 */
	MMINLINE bool
	hasJustBeenHashed(J9Object *objectPtr)
	{
		return hasJustBeenHashed(getObjectFlags(objectPtr));
	}

	MMINLINE bool
	hasJustBeenHashed(uintptr_t objectFlags)
	{
		return OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS == (objectFlags & OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS);
	}

	/**
	 * Returns TRUE if an object has been moved, regardless of state of hashed bit, FALSE otherwise.
	 * @param objectPtr Object to test
	 * @return TRUE if an object has been moved, regardless of state of hashed bit, FALSE otherwise
	 */
	MMINLINE bool
	hasBeenMoved(J9Object *objectPtr)
	{
		return hasBeenMoved(getObjectFlags(objectPtr));
	}

	MMINLINE bool
	hasBeenMoved(uintptr_t objectFlags)
	{
		return OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS == (objectFlags & OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS);
	}

	/**
	 * Returns TRUE if an object has been moved and hashed bit cleared, FALSE otherwise.
	 * @param objectPtr Object to test
	 * @return TRUE if an object has been moved and hashed bit cleared, FALSE otherwise
	 */
	MMINLINE bool
	hasRecentlyBeenMoved(J9Object *objectPtr)
	{
		return hasRecentlyBeenMoved(getObjectFlags(objectPtr));
	}

	MMINLINE bool
	hasRecentlyBeenMoved(uintptr_t objectFlags)
	{
		return OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS == (objectFlags & (OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS | OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS));
	}

	/**
	 * Set OBJECT_HEADER_HAS_BEEN_MOVED and OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS flags
	 * @param objectPtr Pointer to an object
	 */
	MMINLINE void
	setObjectHasBeenMoved(omrobjectptr_t objectPtr)
	{
		setObjectFlags(objectPtr, 0, OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS | OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS);
	}

	/**
	 * Set OBJECT_HEADER_HAS_BEEN_MOVED flag / clear OBJECT_HEADER_HAS_BEEN_HASHED bit
	 * @param objectPtr Pointer to an object
	 */
	MMINLINE void
	setObjectJustHasBeenMoved(omrobjectptr_t objectPtr)
	{
		setObjectFlags(objectPtr, OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS, OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS);
	}

	MMINLINE int32_t
	computeObjectHash(MM_ForwardedHeader *forwardedHeader)
	{
		return convertValueToHash(_javaVM, (uintptr_t)forwardedHeader->getObject());
	}

	MMINLINE uintptr_t
	getHashcodeOffset(omrobjectptr_t objectPtr) {
		return getObjectModelDelegate()->getHashcodeOffset(objectPtr);
	}

	/**
	 * Same as getConsumedSizeInBytesWithHeader, except that it returns the size
	 * the object will consume if it is moved. i.e. it includes space for the
	 * hash code slot.
	 * @param objectPtr Pointer to an object
	 * @return The consumed heap size of an object, in bytes, including the header
	 */
	MMINLINE uintptr_t
	getConsumedSizeInBytesWithHeaderForMove(J9Object *objectPtr)
	{
		return adjustSizeInBytes(getObjectModelDelegate()->getObjectSizeInBytesWithHeader(objectPtr, hasBeenHashed(objectPtr)));
	}

	/**
	 * Same as getConsumedSizeInBytesWithHeader, except that it returns the size
	 * the object will consume if it is moved. i.e. it includes space for the
	 * hash code slot.
	 * @param objectPtr Pointer to an object
	 * @return The consumed heap size of an object, in bytes, including the header
	 */
	MMINLINE uintptr_t
	getConsumedSizeInBytesWithHeaderBeforeMove(J9Object *objectPtr)
	{
		return adjustSizeInBytes(getObjectModelDelegate()->getObjectSizeInBytesWithHeader(objectPtr, hasBeenMoved(objectPtr) && !hasRecentlyBeenMoved(objectPtr)));
	}

#if defined(J9VM_GC_MODRON_SCAVENGER)
	/**
	 * Extract the class pointer from an unforwarded object.
	 *
	 * This method will assert if the object has been marked as forwarded.
	 *
	 * @param[in] pointer to forwardedHeader the MM_ForwardedHeader instance encapsulating the object
	 * @return pointer to the J9Class from the object encapsulated by forwardedHeader
	 * @see MM_ForwardingHeader::isForwardedObject()
	 */
	MMINLINE J9Class *
	getPreservedClass(MM_ForwardedHeader *forwardedHeader)
	{
		return (J9Class *)((uintptr_t)(forwardedHeader->getPreservedSlot()) & J9GC_J9OBJECT_CLAZZ_ADDRESS_MASK);
	}

	/**
	 * Update the new version of this object after it has been copied. This undoes any damaged
	 * caused by installing the forwarding pointer into the original prior to the copy, and sets
	 * the object age.
	 *
	 * This will install the correct (i.e. unforwarded) class pointer, update the hashed/moved
	 * flags and install the hash code if the object has been hashed but not previously moved.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @param[in] destinationObjectPtr pointer to the copied object to be fixed up
	 * @param[in] objectAge the age to set in the copied object
	 */
	MMINLINE void
	fixupForwardedObject(MM_ForwardedHeader *forwardedHeader, omrobjectptr_t destinationObjectPtr, uintptr_t objectAge)
	{
		GC_ObjectModelBase::fixupForwardedObject(forwardedHeader, destinationObjectPtr, objectAge);

#if defined(J9VM_ENV_DATA64)
		if (_javaVM->isVirtualLargeObjectHeapEnabled) {
			if (isIndexable(forwardedHeader)) {
				_indexableObjectModel->fixupDataAddr(forwardedHeader, destinationObjectPtr);
			}
		}
#endif /* defined(J9VM_ENV_DATA64) */

		fixupHashFlagsAndSlot(forwardedHeader, destinationObjectPtr);
	}

	/**
	 * This will install the correct (i.e. unforwarded) class pointer, update the hashed/moved
	 * flags and install the hash code if the object has been hashed but not previously moved.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @param[in] destinationObjectPtr pointer to the copied object to be fixed up
	 */
	MMINLINE void
	fixupHashFlagsAndSlot(MM_ForwardedHeader *forwardedHeader, omrobjectptr_t destinationObjectPtr)
	{
		/*	To have ability to backout last scavenge we need to recognize objects just moved (moved first time) in current scavenge
		 *
		 *	Bits	State
		 *	--------------------------------
		 * 	m h		moved / hashed bits
		 * 	0 0		not moved / not hashed
		 * 	0 1		not moved / hashed
		 * 	1 0		just moved / hashed
		 * 	1 1		moved / hashed
		 *	--------------------------------
		 */
		if (hasBeenMoved(getPreservedFlags(forwardedHeader))) {
			if (hasRecentlyBeenMoved(getPreservedFlags(forwardedHeader))) {
				/* Moved bit set / hashed bit not set means "moved previous scavenge" so set moved/hashed */
				setObjectHasBeenMoved(destinationObjectPtr);
			}
		} else if (hasBeenHashed(getPreservedFlags(forwardedHeader))) {
			/* The object has been hashed and has not been moved so we must store the previous address into the hashcode slot at hashcode offset. */
			uintptr_t hashOffset;
			J9Class *clazz = getPreservedClass(forwardedHeader);
			if (isIndexable(clazz)) {
				hashOffset = _indexableObjectModel->getPreservedHashcodeOffset(forwardedHeader);
			} else {
				hashOffset = _mixedObjectModel->getHashcodeOffset(clazz);
			}

			uint32_t *hashCodePointer = (uint32_t*)((uint8_t*) destinationObjectPtr + hashOffset);
			*hashCodePointer = convertValueToHash(_javaVM, (uintptr_t)forwardedHeader->getObject());
			setObjectJustHasBeenMoved(destinationObjectPtr);
		}
	}

#endif /* defined(J9VM_GC_MODRON_SCAVENGER) */

#if defined(OMR_GC_REALTIME)
	/**
	 * Set GC_OVERFLOW bit atomically
	 * @param objectPtr Pointer to an object
	 * @return true, if GC_OVERFLOW bit has been set this call
	 */
	MMINLINE bool
	atomicSetOverflowBit(J9Object *objectPtr)
	{
		return atomicSetObjectFlags(objectPtr, 0, GC_OVERFLOW);
	}

	/**
	 * Clear GC_OVERFLOW bit atomically
	 * @param objectPtr Pointer to an object
	 * @return true, if GC_OVERFLOW bit has been cleared this call
	 */
	MMINLINE bool
	atomicClearOverflowBit(J9Object *objectPtr)
	{
		return atomicSetObjectFlags(objectPtr, GC_OVERFLOW, 0);
	}

	/**
	 * Return back true if GC_OVERFLOW bit is set
	 * @param objectPtr Pointer to an object
	 * @return true, if GC_OVERFLOW bit is set
	 */
	MMINLINE bool
	isOverflowBitSet(J9Object *objectPtr)
	{
		return (GC_OVERFLOW == (J9GC_J9OBJECT_FLAGS_FROM_CLAZZ(objectPtr, this) & GC_OVERFLOW));
	}
#endif /* defined(OMR_GC_REALTIME) */

	/**
	 * Set class in clazz slot in object header, with header flags.
	 * @param objectPtr Pointer to an object
	 * @param clazz class pointer to set
	 * @param flags flag bits to set
	 */
	MMINLINE void
	setObjectClassAndFlags(J9Object *objectPtr, J9Class* clazz, uintptr_t flags)
	{
		uintptr_t classBits = (uintptr_t)clazz;
		uintptr_t flagsBits = flags & (uintptr_t)OMR_OBJECT_METADATA_FLAGS_MASK;
		if (compressObjectReferences()) {
			*((uint32_t*)getObjectHeaderSlotAddress(objectPtr)) = (uint32_t)(classBits | flagsBits);
		} else {
			*((uintptr_t*)getObjectHeaderSlotAddress(objectPtr)) = (classBits | flagsBits);
		}
	}

	/**
	 * Set class in clazz slot in object header, preserving header flags
	 * @param objectPtr Pointer to an object
	 * @param clazz class pointer to set
	 */
	MMINLINE void
	setObjectClass(J9Object *objectPtr, J9Class* clazz)
	{
		setObjectClassAndFlags(objectPtr, clazz, getObjectFlags(objectPtr));
	}

	/**
	 * Initialize the receiver, a new instance of GC_ObjectModel
	 * 
	 * @return true on success, false on failure
	 */
	virtual bool initialize(MM_GCExtensionsBase *extensions);
	
	/**
	 * Tear down the receiver
	 */
	virtual void tearDown(MM_GCExtensionsBase *extensions);

	/**
	 * Constructor.
	 */
	GC_ObjectModel()
		: GC_ObjectModelBase()
	{}
};

#endif /* OBJECTMODEL_HPP_ */

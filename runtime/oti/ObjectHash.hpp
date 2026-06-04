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

#if !defined(OBJECTHASH_HPP_)
#define OBJECTHASH_HPP_

#include "j9cfg.h"

#if defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES)
#if OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES
#define VM_ObjectHash VM_ObjectHashCompressed
#else /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */
#define VM_ObjectHash VM_ObjectHashFull
#endif /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */
#endif /* OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES */

#include "j9.h"
#include "j9accessbarrier.h"
#include "j9consts.h"
#include "AtomicSupport.hpp"
#include "VMHelpers.hpp"

class VM_ObjectHash
{
/*
 * Data members
 */
private:
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	struct ValueTypeHashQueueEntry {
		j9object_t objectPointer;
		J9Class *clazz;
		UDATA startOffset;
	};

	struct ValueTypeHashQueue {
		J9JavaVM * const vm;
		ValueTypeHashQueueEntry *entries;
		UDATA capacity;
		UDATA head;
		UDATA tail;
		ValueTypeHashQueueEntry space[128];

		ValueTypeHashQueue(J9JavaVM *vm)
			: vm(vm)
			, entries(space)
			, capacity(sizeof(space) / sizeof(space[0]))
			, head(0)
			, tail(0)
		{
		}

		~ValueTypeHashQueue()
		{
			PORT_ACCESS_FROM_JAVAVM(vm);
			if (space != entries) {
				j9mem_free_memory(entries);
			}
		}

		/**
		 * Append an entry to this queue.
		 *
		 * Handles transition from internal space to heap space on overflow.
		 *
		 * @param objectPointer  a pointer to an object to be queued
		 * @param clazz          the class of that object
		 * @param startOffset    the starting offset in that object
		 * @return true if successful, false if additional space could not be allocated
		 */
		VMINLINE bool
		append(j9object_t objectPointer, J9Class *clazz, UDATA startOffset)
		{
			if (tail >= capacity) {
				PORT_ACCESS_FROM_JAVAVM(vm);
				UDATA newCapacity = capacity * 2;
				ValueTypeHashQueueEntry *newEntries = (ValueTypeHashQueueEntry *)j9mem_allocate_memory(newCapacity * sizeof(ValueTypeHashQueueEntry), OMRMEM_CATEGORY_VM);
				if (NULL == newEntries) {
					return false;
				}
				UDATA size = tail - head;
				memcpy(newEntries, &entries[head], size * sizeof(ValueTypeHashQueueEntry));
				if (space != entries) {
					j9mem_free_memory(entries);
				}
				entries = newEntries;
				capacity = newCapacity;
				tail = size;
				head = 0;
			}

			entries[tail].objectPointer = objectPointer;
			entries[tail].clazz = clazz;
			entries[tail].startOffset = startOffset;
			tail += 1;
			return true;
		}

		/**
		 * Copy the entry at the head of this queue.
		 *
		 * @param entry a pointer to space to receive the entry
		 * @return true if the queue was not empty
		 */
		VMINLINE bool
		remove(ValueTypeHashQueueEntry *entry)
		{
			if (head < tail) {
				*entry = entries[head];
				head += 1;
				return true;
			}
			return false;
		}
	};
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
protected:
public:

/*
 * Function members
 */
private:

	static VMINLINE void
	setHasBeenHashed(J9JavaVM *vm, j9object_t objectPtr)
	{
		if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
			VM_AtomicSupport::bitOrU32(
				(uint32_t*)&((J9Object*)objectPtr)->clazz,
				(uint32_t)OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS);
		} else {
			VM_AtomicSupport::bitOr(
				(uintptr_t*)&((J9Object*)objectPtr)->clazz,
				(uintptr_t)OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS);
		}
	}

	static VMINLINE U_32
	getSalt(J9JavaVM *vm, UDATA objectPointer
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
			, bool isValueType = false
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	)
	{
		/* set up the default salt */
		U_32 salt = 1421595292 ^ (U_32) (UDATA) vm;

		J9IdentityHashData *hashData = vm->identityHashData;
		UDATA saltPolicy = hashData->hashSaltPolicy;
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		if (isValueType) {
			saltPolicy = J9_IDENTITY_HASH_SALT_POLICY_NONE;
		}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		switch(saltPolicy) {
		case J9_IDENTITY_HASH_SALT_POLICY_STANDARD:
			/* Gencon/optavgpause/optthruput use the default salt for non heap and
			 * tenure space but they use a table salt for the nursery.
			 *
			 * hashData->hashData1 is nursery base
			 * hashData->hashData2 is nursery top
			 */
			if (objectPointer >= hashData->hashData1) {
				if (objectPointer < hashData->hashData2) {
					/* Object is in the nursery */
					salt = hashData->hashSaltTable[0];
				} else {
					/* not in the heap so use default salt */
				}
			} else {
				/* not in the nursery so use default salt */
			}
			break;
		case J9_IDENTITY_HASH_SALT_POLICY_REGION:
			/* Balanced uses the default heap for non heap a table salt based on the region
			 * for heap memory.
			 *
			 * hashData->hashData1 is heapBase
			 * hashData->hashData2 is heapTop
			 * hashData->hashData3 is log of regionSize
			 */
			if (objectPointer >= hashData->hashData1) {
				if (objectPointer < hashData->hashData2) {
					UDATA heapDelta = objectPointer - hashData->hashData1;
					UDATA index = heapDelta >> hashData->hashData3;
					salt = hashData->hashSaltTable[index];
				} else {
					/* not in the heap so use default salt */
				}
			} else {
				/* not in the heap so use default salt */
			}
			break;
		case J9_IDENTITY_HASH_SALT_POLICY_NONE:
			/* Use default salt */
			break;
		default:
			/* Unrecognized salt policy.  Should assert but we are in util */
			break;
		}

		return salt;
	}

	static VMINLINE U_32
	rotateLeft(U_32 value, U_32 count)
	{
		return (value << count) | (value >> (32 - count));
	}

	static VMINLINE U_32
	mix(U_32 hashValue, U_32 datum)
	{
		const U_32 MUL1 = 0xcc9e2d51;
		const U_32 MUL2 = 0x1b873593;
		const U_32 ADD1 = 0xe6546b64;

		datum *= MUL1;
		datum = rotateLeft(datum, 15);
		datum *= MUL2;
		hashValue ^= datum;
		hashValue = rotateLeft(hashValue, 13);
		hashValue *= 5;
		hashValue += ADD1;
		return hashValue;
	}

	/**
	 * Finalize a MurmurHash3 hash value.
	 *
	 * @param hashValue the hash value
	 * @param numBytesHashed the number of bytes hashed
	 * @return finalized hash value
	 */
	static VMINLINE U_32
	finalizeMurmur3Hash(U_32 hashValue, U_32 numBytesHashed)
	{
		const U_32 MUL1 = 0x85ebca6b;
		const U_32 MUL2 = 0xc2b2ae35;

		hashValue ^= numBytesHashed;
		hashValue ^= hashValue >> 16;
		hashValue *= MUL1;
		hashValue ^= hashValue >> 13;
		hashValue *= MUL2;
		hashValue ^= hashValue >> 16;

		return hashValue;
	}

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	/**
	 * Calculate hashcode for valuetypes by hashing all its fields iteratively.
	 * Use inlineConvertValueToHash for reference fields.
	 * Use a queue to iterate through the fields of a valuetype.
	 *
	 * @param vm                a java VM
	 * @param objectPointer     a valid valuetype object
	 * @param clazz             class of the valuetye object
	 * @param startOffset       offset to start reading fields from
	 * @param[out] oomOccurred  true if an OOM occurred during hash computation
	 * @return the calculated hash code or 0 if an OOM occurred
	 */
	static I_32
	convertValueObjectAtOffsetToHash(J9JavaVM *vm, J9VMThread *currentThread, j9object_t objectPointer, J9Class *clazz, UDATA startOffset, bool *oomOccurred)
	{
		MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
		I_32 hashValue = getSalt(vm, (UDATA)objectPointer, true);
		U_32 numBytesHashed = 0;
		ValueTypeHashQueue queue(vm);
		ValueTypeHashQueueEntry entry = {objectPointer, clazz, startOffset};
		bool oom = false;

		hashValue = mix(hashValue, (U_32)(UDATA)clazz);
		numBytesHashed = sizeof(UDATA);

		while (true) {
			J9ROMFieldOffsetWalkState state;
			J9ROMFieldOffsetWalkResult *result = vm->internalVMFunctions->fieldOffsetsStartDo(
					vm, entry.clazz->romClass, VM_VMHelpers::getSuperclass(entry.clazz), &state,
					J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE, entry.clazz->flattenedClassCache);
			while (NULL != result->field) {
				UDATA fieldOffset = entry.startOffset + result->offset;
				J9UTF8 *signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(&result->field->nameAndSignature);
				U_8 *sigChar = J9UTF8_DATA(signature);
				switch (*sigChar) {
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
				case 'Z': /* boolean */
				case 'B': /* byte */ {
					U_32 datum = (U_32)objectAccessBarrier.inlineMixedObjectReadU8(currentThread, entry.objectPointer, fieldOffset);
					hashValue = mix(hashValue, datum);
					numBytesHashed += 1;
					break;
				}
				case 'C': /* char */
				case 'S': /* short */ {
					U_32 datum = (U_32)objectAccessBarrier.inlineMixedObjectReadU16(currentThread, entry.objectPointer, fieldOffset);
					hashValue = mix(hashValue, datum);
					numBytesHashed += 2;
					break;
				}
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
				case 'Z': /* boolean */
				case 'B': /* byte */
				case 'C': /* char */
				case 'S': /* short */
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
				case 'I': /* int */
				case 'F': /* float */ {
					U_32 datum = objectAccessBarrier.inlineMixedObjectReadU32(currentThread, entry.objectPointer, fieldOffset);
					hashValue = mix(hashValue, datum);
					numBytesHashed += 4;
					break;
				}

				case 'J': /* long */
				case 'D': /* double */ {
					U_64 datum = objectAccessBarrier.inlineMixedObjectReadU64(currentThread, entry.objectPointer, fieldOffset);
					hashValue = mix(hashValue, (U_32)(datum & 0xffffffff));
					hashValue = mix(hashValue, (U_32)(datum >> 32));
					numBytesHashed += 8;
					break;
				}

				case '[':
				case 'L': {
					U_32 datum = 0;
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
					if (J9ROMFIELD_IS_NULL_RESTRICTED(result->field)) {
						if (vm->internalVMFunctions->isFlattenableFieldFlattened(entry.clazz, result->field)) {
							/* Null-restricted flattened field. */
							J9Class *flatClazz = vm->internalVMFunctions->getFlattenableFieldType(entry.clazz, result->field);
							if (!queue.append(entry.objectPointer, flatClazz, fieldOffset)) {
								hashValue = 0;
								oom = true;
								goto done;
							}
						} else {
							/* Null-restricted non-flattened field. */
							j9object_t fieldObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, entry.objectPointer, fieldOffset);
							if (NULL != fieldObject) {
								J9Class *fieldClazz = J9OBJECT_CLAZZ(currentThread, fieldObject);
								UDATA flags = J9OBJECT_FLAGS_FROM_CLAZZ(currentThread, fieldObject);
								bool addToQueue = true;
								if (J9_ARE_ANY_BITS_SET(flags, OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS)) {
									UDATA hashSlotOffset = fieldClazz->backfillOffset;
									I_32 storedHash = objectAccessBarrier.inlineMixedObjectReadI32(currentThread, fieldObject, hashSlotOffset);
									if (0 != storedHash) {
										datum = (U_32)storedHash;
										hashValue = mix(hashValue, datum);
										numBytesHashed += 4;
										addToQueue = false;
									}
								}
								if (addToQueue) {
									if (!queue.append(fieldObject, fieldClazz, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread))) {
										hashValue = 0;
										oom = true;
										goto done;
									}
								}
							}
						}
					} else
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
					{
						j9object_t fieldObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, entry.objectPointer, fieldOffset);
						if (NULL != fieldObject) {
							J9Class *fieldClazz = J9OBJECT_CLAZZ(currentThread, fieldObject);
							UDATA flags = J9OBJECT_FLAGS_FROM_CLAZZ(currentThread, fieldObject);
							if (J9_ARE_ANY_BITS_SET(flags, OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS)) {
								bool addToQueue = true;
								I_32 storedHash = 0;
								if (J9CLASS_IS_ARRAY(fieldClazz)) {
									if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
										storedHash = inlineIndexableObjectHashCodeCompressed(vm, fieldObject, fieldClazz);
									} else {
										storedHash = inlineIndexableObjectHashCodeFull(vm, fieldObject, fieldClazz);
									}
								} else {
									UDATA hashSlotOffset = fieldClazz->backfillOffset;
									storedHash = objectAccessBarrier.inlineMixedObjectReadI32(currentThread, fieldObject, hashSlotOffset);
								}
								if (0 != storedHash) {
									datum = (U_32)storedHash;
									hashValue = mix(hashValue, datum);
									numBytesHashed += 4;
									addToQueue = false;
								}
								if (addToQueue) {
									if (!queue.append(fieldObject, fieldClazz, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread))) {
										hashValue = 0;
										oom = true;
										goto done;
									}
								}
							} else if (J9_IS_J9CLASS_VALUETYPE(fieldClazz)) {
								if (!queue.append(fieldObject, fieldClazz, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread))) {
									hashValue = 0;
									oom = true;
									goto done;
								}
							} else {
								datum = (U_32)inlineObjectHashCode(vm, fieldObject, oomOccurred);
								if (*oomOccurred) {
									hashValue = 0;
									oom = true;
									goto done;
								}
								hashValue = mix(hashValue, datum);
								numBytesHashed += 4;
							}
						}
					}
					break;
				}
				default:
					/* Invalid field signature. Should assert but we are in util. */
					break;
				}
				result = vm->internalVMFunctions->fieldOffsetsNextDo(&state);
			}
			if (!queue.remove(&entry)) {
				break;
			}
		}

		hashValue = finalizeMurmur3Hash(hashValue, numBytesHashed);
		/* Hash code for value types should not be zero. */
		if (0 == hashValue) {
			hashValue = 1;
		}
done:
		if (oom) {
			*oomOccurred = oom;
		}
		return hashValue;
	}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
protected:

public:
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	/**
	 * Convert object to hash, handling both regular objects and ValueTypes.
	 *
	 * @param vm               a java VM
	 * @param currentThread    the current VM Thread
	 * @param objectPointer    a valid valuetype object
	 * @param[out] oomOccurred true if an OOM occurred
	 * @return the calculated hash code or 0 if oom occurred
	 */
	static VMINLINE I_32
	inlineConvertObjectToHash(J9JavaVM *vm, J9VMThread *currentThread, j9object_t objectPointer, bool *oomOccurred)
	{
		I_32 hashValue = 0;
		J9Class *clazz = J9OBJECT_CLAZZ(currentThread, objectPointer);
		if ((NULL != clazz) && J9_IS_J9CLASS_VALUETYPE(clazz)) {
			hashValue = convertValueObjectAtOffsetToHash(vm, currentThread, objectPointer, clazz, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread), oomOccurred);
		} else {
			hashValue = inlineConvertValueToHash(vm, (UDATA)objectPointer);
		}
		return hashValue;
	}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

	/**
	 * Hash an UDATA via murmur3 algorithm
	 *
	 * @param vmThread 	a java VM
	 * @param value 	an UDATA Value
	 */
	static VMINLINE I_32
	inlineConvertValueToHash(J9JavaVM *vm, UDATA value)
	{
		U_32 hashValue = getSalt(vm, value);
		UDATA shiftedAddress = value >>  vm->omrVM->_objectAlignmentShift;

		U_32 datum = (U_32) (shiftedAddress & 0xffffffff);
		hashValue = mix(hashValue, datum);

#if defined (J9VM_ENV_DATA64)
		datum = (U_32) (shiftedAddress >> 32);
		hashValue = mix(hashValue, datum);
#endif
		hashValue = finalizeMurmur3Hash(hashValue, sizeof(UDATA));

		/* If forcing positive hash codes, AND out the sign bit */
		if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_POSITIVE_HASHCODE)) {
			hashValue &= (U_32)0x7FFFFFFF;
		}

		return (I_32) hashValue;
	}

	/**
	 * Compute hashcode of an objectPointer via murmur3 algorithm
	 *
	 * @pre objectPointer must be a valid object reference
	 *
	 * @param vm			a java VM
	 * @param objectPointer 	a valid object reference.
	 */
	static VMINLINE I_32
	inlineComputeObjectAddressToHash(J9JavaVM *vm, j9object_t objectPointer)
	{
		return inlineConvertValueToHash(vm, (UDATA)objectPointer);
	}

	/**
	 * Fetch objectPointer's hashcode when objectPointer is a contiguous object.
	 *
	 * @pre objectPointer must be a valid object reference
	 *
	 * @param vm			a java VM
	 * @param objectPointer 	a valid object reference
	 * @param objectClass 	class of objectPointer
	 * @param offset 	objectPointer size offset
	 */
	static VMINLINE I_32
	inlineIndexableContiguousObjectHashCode(J9JavaVM *vm, j9object_t objectPointer, J9Class *objectClass, UDATA offset)
	{
		J9ROMArrayClass *romClass = (J9ROMArrayClass *)objectClass->romClass;
		offset = ROUND_UP_TO_POWEROF2((offset << (romClass->arrayShape & 0x0000FFFF)) + vm->contiguousIndexableHeaderSize, sizeof(I_32));
		return *(I_32 *)((UDATA)objectPointer + offset);
	}

	/**
	 * Fetch the objectPointer's hashcode when compressed references are enabled.
	 *
	 * @pre objectPointer must be a valid object reference
	 *
	 * @param vm			a java VM
	 * @param objectPointer 	a valid object reference
	 * @param objectClass 	class of objectPointer
	 */
	static VMINLINE I_32
	inlineIndexableObjectHashCodeCompressed(J9JavaVM *vm, j9object_t objectPointer, J9Class *objectClass)
	{
		I_32 hashValue = 0;
		UDATA offset = ((J9IndexableObjectContiguousCompressed *)objectPointer)->size;
		if (0 != offset) {
			hashValue = inlineIndexableContiguousObjectHashCode(vm, objectPointer, objectClass, offset);
		} else {
			if (0 == ((J9IndexableObjectDiscontiguousCompressed *)objectPointer)->size) {
				/* Zero-sized array */
				hashValue = *(I_32 *)((UDATA)objectPointer + vm->discontiguousIndexableHeaderSize);
			} else {
				/* Discontiguous array */
				hashValue = vm->memoryManagerFunctions->j9gc_objaccess_getObjectHashCode(vm, objectPointer);
			}
		}

		return hashValue;
	}

	/**
	 * Fetch the objectPointer's hashcode when compressed references are disabled.
	 *
	 * @pre objectPointer must be a valid object reference
	 *
	 * @param vm			a java VM
	 * @param objectPointer 	a valid object reference
	 * @param objectClass 	class of objectPointer
	 */
	static VMINLINE I_32
	inlineIndexableObjectHashCodeFull(J9JavaVM *vm, j9object_t objectPointer, J9Class *objectClass)
	{
		I_32 hashValue = 0;
		UDATA offset = ((J9IndexableObjectContiguousFull *)objectPointer)->size;
		if (0 != offset) {
			hashValue = inlineIndexableContiguousObjectHashCode(vm, objectPointer, objectClass, offset);
		} else {
			if (0 == ((J9IndexableObjectDiscontiguousFull *)objectPointer)->size) {
				/* Zero-sized array */
				hashValue = *(I_32 *)((UDATA)objectPointer + vm->discontiguousIndexableHeaderSize);
			} else {
				/* Discontiguous array */
				hashValue = vm->memoryManagerFunctions->j9gc_objaccess_getObjectHashCode(vm, objectPointer);
			}
		}

		return hashValue;
	}

	/**
	 * Fetch the objectPointer's hashcode.
	 *
	 * @pre objectPointer must be a valid object reference
	 *
	 * @param vm               a java VM
	 * @param objectPointer    a valid object reference
	 * @param[out] oomOccurred true if an OOM occurred (for value types)
	 * @return the calculated hash code
	 */
	static VMINLINE I_32
	inlineObjectHashCode(J9JavaVM *vm, j9object_t objectPointer
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
			, bool *oomOccurred
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	)
	{
		I_32 hashValue = 0;
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		J9VMThread *currentThread = vm->internalVMFunctions->currentVMThread(vm);
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		if (VM_VMHelpers::mustCallWriteAccessBarrier(vm)) {
			hashValue = vm->memoryManagerFunctions->j9gc_objaccess_getObjectHashCode(vm, objectPointer);
		} else {
#if defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_GENERATIONAL)
			J9Class *objectClass = J9OBJECT_CLAZZ_VM(vm, objectPointer);
			UDATA flags = J9OBJECT_FLAGS_FROM_CLAZZ_VM(vm, objectPointer);

			if (J9_ARE_ANY_BITS_SET(flags, OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS)) {
				if (J9CLASS_IS_ARRAY(objectClass)) {
					if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
						hashValue = inlineIndexableObjectHashCodeCompressed(vm, objectPointer, objectClass);
					} else {
						hashValue = inlineIndexableObjectHashCodeFull(vm, objectPointer, objectClass);
					}
				} else {
					hashValue = *(I_32 *)((UDATA)objectPointer + objectClass->backfillOffset);
				}
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
				if (0 == hashValue) {
					if (J9_IS_J9CLASS_VALUETYPE(objectClass)) {
						hashValue = convertValueObjectAtOffsetToHash(vm, currentThread, objectPointer, objectClass, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread), oomOccurred);
						if (!*oomOccurred) {
							/* hashValue cannot be zero. Should assert here but we are in util. */
							*(I_32 *)((UDATA)objectPointer + objectClass->backfillOffset) = hashValue;
						}
					}
				}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
			} else {
				if (J9_ARE_NO_BITS_SET(flags, OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS)) {
					setHasBeenHashed(vm, objectPointer);
				}
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
				hashValue = inlineConvertObjectToHash(vm, currentThread, objectPointer, oomOccurred);
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
				hashValue = inlineConvertValueToHash(vm, (UDATA)objectPointer);
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
			}
#else /* defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_GENERATIONAL) */
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
			hashValue = inlineConvertObjectToHash(vm, currentThread, objectPointer, oomOccurred);
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
			hashValue = inlineConvertValueToHash(vm, (UDATA)objectPointer);
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
#endif /* defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_GENERATIONAL) */
		}
		return hashValue;
	}
};

#endif /* OBJECTHASH_HPP_ */

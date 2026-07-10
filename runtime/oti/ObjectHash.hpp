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
	struct ValueTypeHashStackEntry {
		j9object_t objectPointer;
		J9Class *clazz;
		UDATA startOffset;

		 /* Murmur3 hash of the current entry. */
		U_32 hashValue;
		/* Number of bytes hashed while processing current entry. */
		U_32 numBytesHashed;

		/* Field iterator state of the current entry. */
		J9ROMFieldOffsetWalkState walkState;
		/* Next field to process wthin the current entry. */
		J9ROMFieldOffsetWalkResult *currentField;

		/* Index of the parent entry, -1 for the root entry. */
		IDATA parentIndex;
	};

	struct ValueTypeHashStack {
		J9JavaVM * const vm;
		ValueTypeHashStackEntry *entries;
		IDATA top;
		UDATA capacity;
		ValueTypeHashStackEntry space[128];

		ValueTypeHashStack(J9JavaVM *vm)
			: vm(vm)
			, entries(space)
			, top(-1)
			, capacity(sizeof(space) / sizeof(space[0]))
		{
		}

		~ValueTypeHashStack()
		{
			PORT_ACCESS_FROM_JAVAVM(vm);
			if (space != entries) {
				j9mem_free_memory(entries);
			}
		}

		/**
		 * Push a new entry to the stack.
		 * The stack follows a DFS traversal.
		 *
		 * Handles transition from internal space to heap space on overflow.
		 *
		 * @param objectPointer  a pointer to an object to be added to the stack
		 * @param clazz          the class of that object
		 * @param startOffset    the starting offset in that object
		 * @param parentIndex    index of the parent entry (-1 for root)
		 * @return true on success, false if heap allocation failed
		 */
		VMINLINE bool
		append(j9object_t objectPointer, J9Class *clazz, UDATA startOffset, IDATA parentIndex)
		{
			UDATA newTop = (UDATA)(top + 1);
			if (newTop == capacity) {
				PORT_ACCESS_FROM_JAVAVM(vm);
				UDATA newCapacity = capacity * 2;
				ValueTypeHashStackEntry *newEntries = (ValueTypeHashStackEntry *)j9mem_allocate_memory(newCapacity * sizeof(ValueTypeHashStackEntry), OMRMEM_CATEGORY_VM);
				if (NULL == newEntries) {
					return false;
				}
				memcpy(newEntries, entries, capacity * sizeof(ValueTypeHashStackEntry));
				if (space != entries) {
					j9mem_free_memory(entries);
				}
				entries = newEntries;
				capacity = newCapacity;
			}
			ValueTypeHashStackEntry *frame = &entries[newTop];
			frame->objectPointer = objectPointer;
			frame->clazz = clazz;
			frame->startOffset = startOffset;
			frame->hashValue = 0;
			frame->numBytesHashed = 0;
			frame->currentField = NULL;
			frame->parentIndex = parentIndex;
			top = (IDATA)newTop;
			return true;
		}

		/**
		 * @return true if the stack is empty.
		 */
		VMINLINE bool
		isEmpty(void)
		{
			return -1 == top;
		}

		/**
		 * @return a pointer to the top entry.
		 */
		VMINLINE ValueTypeHashStackEntry *
		peek(void)
		{
			return &entries[top];
		}

		/**
		 * Remove the top entry.
		 */
		VMINLINE void
		remove(void)
		{
			top -= 1;
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
	 * Initialize the hash value of the current entry.
	 *
	 * @param vm     a java VM
	 * @param frame  the entry to initialize
	 */
	static VMINLINE void
	initHashFrame(J9JavaVM *vm, ValueTypeHashStackEntry *frame)
	{
		frame->hashValue = getSalt(vm, (UDATA)frame->objectPointer, true);
		frame->hashValue = mix(frame->hashValue, (U_32)(UDATA)frame->clazz);
		frame->numBytesHashed = sizeof(UDATA);
		frame->currentField = vm->internalVMFunctions->fieldOffsetsStartDo(
				vm,
				frame->clazz->romClass,
				VM_VMHelpers::getSuperclass(frame->clazz),
				&frame->walkState,
				J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE,
				frame->clazz->flattenedClassCache);
	}

	/**
	 * Push an entry for value object field.
	 * Update the parent entry's iterator to the next field.
	 * Initialize the child entry's hash.
	 *
	 * @param vm            a java VM
	 * @param currentThread the current VM thread
	 * @param stack         the stack
	 * @param entry         the parent entry
	 * @param childObject   the child object to hash
	 * @param childClazz    class of child Object
	 * @param childStart    startOffset for the child object
	 * @return true on success, false on OOM
	 */
	static VMINLINE bool
	pushChildFrame(J9JavaVM *vm, J9VMThread *currentThread, ValueTypeHashStack *stack, ValueTypeHashStackEntry *entry, j9object_t childObject, J9Class *childClazz, UDATA childStart)
	{
		IDATA parentIndex = stack->top;
		/* Advance the parent's iterator. */
		entry->currentField = vm->internalVMFunctions->fieldOffsetsNextDo(&entry->walkState);
		if (!stack->append(childObject, childClazz, childStart, parentIndex)) {
			return false;
		}
		initHashFrame(vm, stack->peek());
		return true;
	}

	/**
	 * Fast-path recursive hash computation for value objects.
	 * Used if recursive depth is less than the default value.
	 *
	 * @param vm                 a java VM
	 * @param currentThread      the current VM thread
	 * @param objectPointer      the value object being hashed
	 * @param clazz              class of the value object
	 * @param startOffset        offset to start reading fields from
	 * @param depth              current recursion depth
	 * @param[out] depthExceeded set to true if depth limit is hit
	 * @param[out] oomOccurred   set to true if an OOM occurred
	 * @return finalized hash, or 0 if depthExceeded or oomOccurred
	 */
	static I_32
	recursiveValueObjectHash(J9JavaVM *vm, J9VMThread *currentThread, j9object_t objectPointer, J9Class *clazz, UDATA startOffset, UDATA depth, bool *depthExceeded, bool *oomOccurred)
	{
		if (depth >= vm->hashMaxRecDepth) {
			*depthExceeded = true;
			return 0;
		}

		MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
		I_32 hashValue = getSalt(vm, (UDATA)objectPointer, true);
		hashValue = mix(hashValue, (U_32)(UDATA)clazz);
		U_32 numBytesHashed = sizeof(UDATA);

		J9ROMFieldOffsetWalkState walkState;
		J9ROMFieldOffsetWalkResult *result = vm->internalVMFunctions->fieldOffsetsStartDo(
				vm,
				clazz->romClass,
				VM_VMHelpers::getSuperclass(clazz),
				&walkState,
				J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE,
				clazz->flattenedClassCache);

		while (NULL != result->field) {
			UDATA fieldOffset = startOffset + result->offset;
			U_8 sigChar = *J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(&result->field->nameAndSignature));

			switch (sigChar) {
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
			case 'Z': /* boolean */
			case 'B': /* byte */ {
				U_32 datum = (U_32)objectAccessBarrier.inlineMixedObjectReadU8(currentThread, objectPointer, fieldOffset);
				hashValue = mix(hashValue, datum);
				numBytesHashed += 1;
				break;
			}
			case 'C': /* char */
			case 'S': /* short */ {
				U_32 datum = (U_32)objectAccessBarrier.inlineMixedObjectReadU16(currentThread, objectPointer, fieldOffset);
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
				U_32 datum = objectAccessBarrier.inlineMixedObjectReadU32(currentThread, objectPointer, fieldOffset);
				hashValue = mix(hashValue, datum);
				numBytesHashed += 4;
				break;
			}
			case 'J': /* long */
			case 'D': /* double */ {
				U_64 datum = objectAccessBarrier.inlineMixedObjectReadU64(currentThread, objectPointer, fieldOffset);
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
					if (vm->internalVMFunctions->isFlattenableFieldFlattened(clazz, result->field)) {
						/* Null-restricted flattened field. */
						J9Class *flatClazz = vm->internalVMFunctions->getFlattenableFieldType(clazz, result->field);
						datum = (U_32)recursiveValueObjectHash(vm, currentThread, objectPointer, flatClazz, fieldOffset, depth + 1, depthExceeded, oomOccurred);
					} else {
						/* Non-flattened null-restricted value object field. */
						j9object_t fieldObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, objectPointer, fieldOffset);
						if (NULL != fieldObject) {
							J9Class *fieldClazz = J9OBJECT_CLAZZ(currentThread, fieldObject);
							if (J9_ARE_ANY_BITS_SET(J9OBJECT_FLAGS_FROM_CLAZZ(currentThread, fieldObject), OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS)) {
								I_32 storedHash = objectAccessBarrier.inlineMixedObjectReadI32(currentThread, fieldObject, fieldClazz->backfillOffset);
								if (0 != storedHash) {
									datum = (U_32)storedHash;
									goto mixHash;
								}
							}
							datum = (U_32)recursiveValueObjectHash(vm, currentThread, fieldObject, fieldClazz, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread), depth + 1, depthExceeded, oomOccurred);
						}
					}
				} else
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
				{
					j9object_t fieldObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, objectPointer, fieldOffset);
					if (NULL != fieldObject) {
						J9Class *fieldClazz = J9OBJECT_CLAZZ(currentThread, fieldObject);
						if (J9_ARE_ANY_BITS_SET(J9OBJECT_FLAGS_FROM_CLAZZ(currentThread, fieldObject), OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS)) {
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
								goto mixHash;
							}
							datum = (U_32)recursiveValueObjectHash(vm, currentThread, fieldObject, fieldClazz, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread), depth + 1, depthExceeded, oomOccurred);
						} else if (J9_IS_J9CLASS_VALUETYPE(fieldClazz)) {
							datum = (U_32)recursiveValueObjectHash(vm, currentThread, fieldObject, fieldClazz, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread), depth + 1, depthExceeded, oomOccurred);
						} else {
							datum = (U_32)inlineObjectHashCode(vm, fieldObject, oomOccurred);
						}
					}
				}
				if (*depthExceeded) {
					return 0;
				}
mixHash:
				hashValue = mix(hashValue, datum);
				numBytesHashed += 4;
				break;
			}
			default:
				/* Invalid field signature. Should assert but we are in util. */
				break;
			}

			result = vm->internalVMFunctions->fieldOffsetsNextDo(&walkState);
		}

		I_32 finalHash = (I_32)finalizeMurmur3Hash(hashValue, numBytesHashed);
		if (0 == finalHash) {
			/* Hash code for value types should not be zero. */
			return 1;
		}
		return finalHash;
	}

	/**
	 * Calculate the hash code for a value object.
	 * Use the recursive fast path first.
	 * If the recursion depth exceeds the default value.
	 * switch to the iterative DFS approach.
	 * The iterative approach uses a stack to traverse the fields.
	 * Requires VM access.
	 *
	 * @param vm                a java VM
	 * @param currentThread     the current VM thread
	 * @param objectPointer     a valid value object
	 * @param clazz             class of the value object
	 * @param startOffset       offset to start reading fields from
	 * @param[out] oomOccurred  set to true if an OOM occurred
	 * @return the finalized hash code, or 0 if an OOM occurred
	 */
	static I_32
	convertValueObjectAtOffsetToHash(J9JavaVM *vm, J9VMThread *currentThread, j9object_t objectPointer, J9Class *clazz, UDATA startOffset, bool *oomOccurred)
	{
		/* Fast path: recursive approach. */
		bool depthExceeded = false;
		I_32 hashValue = recursiveValueObjectHash(vm, currentThread, objectPointer, clazz, startOffset, 0, &depthExceeded, oomOccurred);
		if (false == depthExceeded) {
			return hashValue;
		}

		/* Slow path: iterative DFS traversal of the stack. */
		MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
		ValueTypeHashStack stack(vm);

		if (!stack.append(objectPointer, clazz, startOffset, -1)) {
			*oomOccurred = true;
			return 0;
		}
		initHashFrame(vm, stack.peek());

		while (!stack.isEmpty()) {
			ValueTypeHashStackEntry *entry = stack.peek();

			/* Once all the fields of the current entry are processed, finalize its hash and mix it into the parent entry's hash. */
			if (NULL == entry->currentField->field) {
				U_32 finalHash = finalizeMurmur3Hash(entry->hashValue, entry->numBytesHashed);
				if (0 == finalHash) {
					/* Hash code for value types should not be zero. */
					finalHash = 1;
				}
				IDATA parentIdx = entry->parentIndex;
				stack.remove();

				if (-1 == parentIdx) {
					/* Final hash value. */
					return (I_32)finalHash;
				}
				ValueTypeHashStackEntry *parent = &stack.entries[parentIdx];
				parent->hashValue = mix(parent->hashValue, finalHash);
				parent->numBytesHashed += 4;
				continue;
			}

			J9ROMFieldOffsetWalkResult *result = entry->currentField;
			UDATA fieldOffset = entry->startOffset + result->offset;
			U_8 sigChar = *J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(&result->field->nameAndSignature));

			switch (sigChar) {
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
			case 'Z': /* boolean */
			case 'B': /* byte */ {
				U_32 datum = (U_32)objectAccessBarrier.inlineMixedObjectReadU8(currentThread, entry->objectPointer, fieldOffset);
				entry->hashValue = mix(entry->hashValue, datum);
				entry->numBytesHashed += 1;
				break;
			}
			case 'C': /* char */
			case 'S': /* short */ {
				U_32 datum = (U_32)objectAccessBarrier.inlineMixedObjectReadU16(currentThread, entry->objectPointer, fieldOffset);
				entry->hashValue = mix(entry->hashValue, datum);
				entry->numBytesHashed += 2;
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
				U_32 datum = objectAccessBarrier.inlineMixedObjectReadU32(currentThread, entry->objectPointer, fieldOffset);
				entry->hashValue = mix(entry->hashValue, datum);
				entry->numBytesHashed += 4;
				break;
			}
			case 'J': /* long */
			case 'D': /* double */ {
				U_64 datum = objectAccessBarrier.inlineMixedObjectReadU64(currentThread, entry->objectPointer, fieldOffset);
				entry->hashValue = mix(entry->hashValue, (U_32)(datum & 0xffffffff));
				entry->hashValue = mix(entry->hashValue, (U_32)(datum >> 32));
				entry->numBytesHashed += 8;
				break;
			}
			case '[':
			case 'L': {
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
				if (J9ROMFIELD_IS_NULL_RESTRICTED(result->field)) {
					if (vm->internalVMFunctions->isFlattenableFieldFlattened(entry->clazz, result->field)) {
						/* Null-restricted non-flattened field. */
						J9Class *flatClazz = vm->internalVMFunctions->getFlattenableFieldType(entry->clazz, result->field);
						if (!pushChildFrame(vm, currentThread, &stack, entry, entry->objectPointer, flatClazz, fieldOffset)) {
							goto oom;
						}
						continue;
					}
					/* Non-flattened null-restricted value object field. */
					j9object_t fieldObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, entry->objectPointer, fieldOffset);
					if (NULL != fieldObject) {
						J9Class *fieldClazz = J9OBJECT_CLAZZ(currentThread, fieldObject);
						if (J9_ARE_ANY_BITS_SET(J9OBJECT_FLAGS_FROM_CLAZZ(currentThread, fieldObject), OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS)) {
							I_32 storedHash = objectAccessBarrier.inlineMixedObjectReadI32(currentThread, fieldObject, fieldClazz->backfillOffset);
							if (0 != storedHash) {
								entry->hashValue = mix(entry->hashValue, (U_32)storedHash);
								entry->numBytesHashed += 4;
								break;
							}
						}
						if (!pushChildFrame(vm, currentThread, &stack, entry, fieldObject, fieldClazz, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread))) {
							goto oom;
						}
						continue;
					}
				} else
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
				{
					j9object_t fieldObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, entry->objectPointer, fieldOffset);
					if (NULL != fieldObject) {
						J9Class *fieldClazz = J9OBJECT_CLAZZ(currentThread, fieldObject);
						if (J9_ARE_ANY_BITS_SET(J9OBJECT_FLAGS_FROM_CLAZZ(currentThread, fieldObject), OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS)) {
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
								entry->hashValue = mix(entry->hashValue, (U_32)storedHash);
								entry->numBytesHashed += 4;
								break;
							}
							if (!pushChildFrame(vm, currentThread, &stack, entry, fieldObject, fieldClazz, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread))) {
								goto oom;
							}
							continue;
						} else if (J9_IS_J9CLASS_VALUETYPE(fieldClazz)) {
							if (!pushChildFrame(vm, currentThread, &stack, entry, fieldObject, fieldClazz, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread))) {
								goto oom;
							}
							continue;
						} else {
							U_32 datum = (U_32)inlineObjectHashCode(vm, fieldObject, oomOccurred);
							if (*oomOccurred) {
								goto oom;
							}
							entry->hashValue = mix(entry->hashValue, datum);
							entry->numBytesHashed += 4;
						}
					}
				}
				break;
			}
			default:
				/* Invalid field signature. Should assert but we are in util. */
				break;
			}

			entry->currentField = vm->internalVMFunctions->fieldOffsetsNextDo(&entry->walkState);
		}

oom:
		*oomOccurred = true;
		return 0;
	}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
protected:

public:
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	/**
	 * Convert object to hash, handling both regular objects and ValueTypes.
	 * Requires VM access.
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
		uintptr_t dataSize = 0;
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		if (J9_IS_J9CLASS_FLATTENED(objectClass)) {
			dataSize = offset * J9ARRAYCLASS_GET_STRIDE(objectClass);
		} else
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
		{
			J9ROMArrayClass *romClass = (J9ROMArrayClass *)objectClass->romClass;
			dataSize = offset << (romClass->arrayShape & 0x0000FFFF);
		}
#if defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) && defined(J9VM_ENV_DATA64)
		if (vm->isIndexableDataAddrPresent) {
			void *dataAddr = NULL;
			if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
				dataAddr = (void *)((J9IndexableObjectWithDataAddressContiguousCompressed *)objectPointer)->dataAddr;
			} else {
				dataAddr = (void *)((J9IndexableObjectWithDataAddressContiguousFull *)objectPointer)->dataAddr;
			}
			void *inlineDataStart = (void *)((uintptr_t)objectPointer + vm->contiguousIndexableHeaderSize);
			if (dataAddr != inlineDataStart) {
				offset = ROUND_UP_TO_POWEROF2(vm->contiguousIndexableHeaderSize, sizeof(I_32));
				return *(I_32 *)((UDATA)objectPointer + offset);
			}
		}
#endif /* defined(J9VM_GC_SPARSE_HEAP_ALLOCATION) && defined(J9VM_ENV_DATA64) */
		offset = ROUND_UP_TO_POWEROF2(dataSize + vm->contiguousIndexableHeaderSize, sizeof(I_32));
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
	 * Requires VM access for the hash computation
	 * of value objects.
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

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

#if !defined(OBJECTHASH_HPP_)
#define OBJECTHASH_HPP_

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
	getSalt(J9JavaVM *vm, UDATA objectPointer)
	{
		/* set up the default salt */
		U_32 salt = 1421595292 ^ (U_32) (UDATA) vm;

		J9IdentityHashData *hashData = vm->identityHashData;
		UDATA saltPolicy = hashData->hashSaltPolicy;
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
protected:

public:
	/**
	 * Hash an UDATA via murmur3 algorithm
	 *
	 * @param vmThread 	a java VM
	 * @param value 	an UDATA Value
	 */
	static VMINLINE I_32
	inlineConvertValueToHash(J9JavaVM *vm, UDATA value)
	{
		const U_32 MUL1 = 0x85ebca6b;
		const U_32 MUL2 = 0xc2b2ae35;

		U_32 hashValue = getSalt(vm, value);
		UDATA shiftedAddress = value >>  vm->omrVM->_objectAlignmentShift;

		U_32 datum = (U_32) (shiftedAddress & 0xffffffff);
		hashValue = mix(hashValue, datum);

#if defined (J9VM_ENV_DATA64)
		datum = (U_32) (shiftedAddress >> 32);
		hashValue = mix(hashValue, datum);
#endif

		hashValue ^= sizeof(UDATA);

		hashValue ^= hashValue >> 16;
		hashValue *= MUL1;
		hashValue ^= hashValue >> 13;
		hashValue *= MUL2;
		hashValue ^= hashValue >> 16;

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
	 * Fetch objectPointer's hashcode
	 *
	 * @pre objectPointer must be a valid object reference
	 *
	 * @param vm			a java VM
	 * @param objectPointer 	a valid object reference.
	 */
	static VMINLINE I_32
	inlineObjectHashCode(J9JavaVM *vm, j9object_t objectPointer)
	{
		I_32 hashValue = 0;
		if (VM_VMHelpers::mustCallWriteAccessBarrier(vm)) {
			hashValue = vm->memoryManagerFunctions->j9gc_objaccess_getObjectHashCode(vm, objectPointer);
		} else {
#if defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_GENERATIONAL)
			J9Class *objectClass = J9OBJECT_CLAZZ_VM(vm, objectPointer);

			/* Technically, one should use J9OBJECT_FLAGS_FROM_CLAZZ macro to fetch the clazz flags.
			 * However, considering the need to optimize hashcode code path and how we actually use
			 * the flag, it is sufficient to fetch objectPointer->clazz.
			 */
			UDATA flags = (UDATA)(((J9Object*)objectPointer)->clazz);

			if (J9_ARE_ANY_BITS_SET(flags, OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS)) {
				if (J9CLASS_IS_ARRAY(objectClass)) {
					UDATA offset = ((J9IndexableObjectContiguous*)objectPointer)->size;
					if (0 != offset) {
						/* Contiguous array */
						J9ROMArrayClass *romClass = (J9ROMArrayClass*)objectClass->romClass;
						offset = ROUND_UP_TO_POWEROF2((offset << (romClass->arrayShape & 0x0000FFFF)) + sizeof(J9IndexableObjectContiguous), sizeof(I_32));
						hashValue = *(I_32*)((UDATA)objectPointer + offset);
					} else {
						if (0 == ((J9IndexableObjectDiscontiguous*)objectPointer)->size) {
							/* Zero-sized array */
							hashValue = *(I_32*)((J9IndexableObjectDiscontiguous*)objectPointer + 1);
						} else {
							/* Discontiguous array */
							hashValue = vm->memoryManagerFunctions->j9gc_objaccess_getObjectHashCode(vm, objectPointer);
						}
					}
				} else {
					hashValue = *(I_32*)((UDATA)objectPointer + objectClass->backfillOffset);
				}

			} else {
				if (J9_ARE_NO_BITS_SET(flags, OBJECT_HEADER_HAS_BEEN_HASHED_IN_CLASS)) {
					setHasBeenHashed(vm, objectPointer);
				}
				hashValue = inlineConvertValueToHash(vm, (UDATA)objectPointer);
			}
#else /* defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_GENERATIONAL) */
			hashValue = inlineConvertValueToHash(vm, (UDATA)objectPointer);
#endif /* defined(J9VM_GC_MODRON_COMPACTION) || defined(J9VM_GC_GENERATIONAL) */
		}
		return hashValue;
	}
};

#endif /* OBJECTHASH_HPP_ */

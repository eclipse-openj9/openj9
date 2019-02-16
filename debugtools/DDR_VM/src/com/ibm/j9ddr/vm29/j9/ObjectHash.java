/*******************************************************************************
 * Copyright (c) 2013, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.pointer.generated.J9IdentityHashDataPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9JavaVMHelper;
import com.ibm.j9ddr.vm29.structure.J9IdentityHashData;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

public class ObjectHash {

	private static final long J9_EXTENDED_RUNTIME_POSITIVE_HASHCODE =
			J9ConstantHelper.getLong(J9Consts.class, "J9_EXTENDED_RUNTIME_POSITIVE_HASHCODE", 0);

	private static U32 getSalt(J9JavaVMPointer vm, UDATA objectPointer) throws CorruptDataException
	{
		/* set up the default salt */
		UDATA salt = new U32(1421595292).bitXor(new U32(UDATA.cast(vm)));
		J9IdentityHashDataPointer hashData = vm.identityHashData();
		UDATA saltPolicy = hashData.hashSaltPolicy();
		
		/* Replacing case statement in C with an if-else if statements since case expressions must use constant */
		if (saltPolicy.eq(J9IdentityHashData.J9_IDENTITY_HASH_SALT_POLICY_STANDARD)) {
			/* Gencon/optavgpause/optthruput use the default salt for non heap and
			 * tenure space but they use a table salt for the nursery.
			 *
			 * hashData->hashData1 is nursery base
			 * hashData->hashData2 is nursery top
			 */
			if (objectPointer.gte(hashData.hashData1())) {
				if (objectPointer.lt(hashData.hashData2())) {
					/* Object is in the nursery */
					salt = hashData.hashSaltTableEA().at(0);
				} else {
					/* not in the heap so use default salt */
				}
			} else {
				/* not in the nursery so use default salt */
			}
		} else if (saltPolicy.eq(J9IdentityHashData.J9_IDENTITY_HASH_SALT_POLICY_REGION)) {
			/* Balanced uses the default heap for non heap a table salt based on the region
			 * for heap memory.
			 *
			 * hashData->hashData1 is heapBase
			 * hashData->hashData2 is heapTop
			 * hashData->hashData3 is log of regionSize
			 */
			
			if (objectPointer.gte(hashData.hashData1())) {
				if (objectPointer.lt(hashData.hashData2())) {
					UDATA heapDelta = objectPointer.sub(hashData.hashData1());
					UDATA index = heapDelta.rightShift(hashData.hashData3());
					salt = hashData.hashSaltTableEA().at(index);
				} else {
					/* not in the heap so use default salt */
				}
			} else {
				/* not in the heap so use default salt */
			}			
		} else if (saltPolicy.eq(J9IdentityHashData.J9_IDENTITY_HASH_SALT_POLICY_NONE)) {
			/* Use default salt */
		} else {
			/* Unrecognized salt policy.  Should assert but we are in util */
			throw new CorruptDataException("Invalid salt policy");
		}
		
		return new U32(salt);
	}
	
	static U32 rotateLeft(U32 value, int count)
	{
		return value.leftShift(count).bitXor(value.rightShift(32 - count));
	}

	static U32 mix(U32 hashValue, U32 datum)
	{
		final U32 MUL1 = new U32(0xcc9e2d51);
		final U32 MUL2 = new U32(0x1b873593);
		final U32 ADD1 = new U32(0xe6546b64);
		
		datum = datum.mult(MUL1);
		datum = rotateLeft(datum, 15);
		datum = datum.mult(MUL2);
		hashValue = hashValue.bitXor(datum);
		hashValue = rotateLeft(hashValue, 13);
		hashValue = hashValue.mult(5);
		hashValue = hashValue.add(ADD1);
		
		return hashValue;
	}
	
	private static I32 inlineConvertValueToHash(J9JavaVMPointer vm, UDATA objectPointer) throws CorruptDataException
	{
		final U32 MUL1 = new U32(0x85ebca6b);
		final U32 MUL2 = new U32(0xc2b2ae35);

		U32 hashValue = getSalt(vm, objectPointer);
		UDATA shiftedAddress = objectPointer.div(ObjectModel.getObjectAlignmentInBytes());
		
		U32 datum = new U32(shiftedAddress.bitAnd(0xffffffff));
		hashValue = mix(hashValue, datum);

		if (J9BuildFlags.env_data64) {
			datum = new U32(shiftedAddress.rightShift(32));
			hashValue = mix(hashValue, datum);
		}
		
		hashValue = hashValue.bitXor(UDATA.SIZEOF);

		hashValue = hashValue.bitXor(hashValue.rightShift(16));
		hashValue = hashValue.mult(MUL1);
		hashValue = hashValue.bitXor(hashValue.rightShift(13));
		hashValue = hashValue.mult(MUL2);
		hashValue = hashValue.bitXor(hashValue.rightShift(16));

		/* If forcing positive hash codes, AND out the sign bit */
		if (J9JavaVMHelper.extendedRuntimeFlagIsSet(vm, J9_EXTENDED_RUNTIME_POSITIVE_HASHCODE)) {
			hashValue = hashValue.bitAnd(0x7FFFFFFF);
		}

		return new I32(hashValue);
	}

	public static I32 convertObjectAddressToHash(J9JavaVMPointer vm, J9ObjectPointer object) throws CorruptDataException
	{
		return inlineConvertValueToHash(vm, UDATA.cast(object));
	}

	public static I32 convertValueToHash(J9JavaVMPointer vm, UDATA value) throws CorruptDataException
	{
		return inlineConvertValueToHash(vm, value);
	}
}

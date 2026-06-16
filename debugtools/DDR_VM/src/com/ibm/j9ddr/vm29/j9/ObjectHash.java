/*
 * Copyright IBM Corp. and others 2013
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
 */
package com.ibm.j9ddr.vm29.j9;

import java.util.Iterator;
import java.util.LinkedList;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U64Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IdentityHashDataPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9JavaVMHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMFieldShapeHelper;
import com.ibm.j9ddr.vm29.pointer.helper.ValueTypeHelper;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.structure.J9FieldFlags;
import com.ibm.j9ddr.vm29.structure.J9IdentityHashData;
import com.ibm.j9ddr.vm29.structure.J9Object;
import com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U64;
import com.ibm.j9ddr.vm29.types.UDATA;

public class ObjectHash {

	private static final U32 MIX_MUL1 = new U32(0xcc9e2d51);
	private static final U32 MIX_MUL2 = new U32(0x1b873593);
	private static final U32 MIX_ADD1 = new U32(0xe6546b64);
	private static final U32 FINALIZE_MUL1 = new U32(0x85ebca6b);
	private static final U32 FINALIZE_MUL2 = new U32(0xc2b2ae35);

	private static final class ValueTypeHashQueueEntry {
		final J9ObjectPointer objectPointer;
		final J9ClassPointer clazz;
		final UDATA startOffset;

		ValueTypeHashQueueEntry(J9ObjectPointer objectPointer, J9ClassPointer clazz, UDATA startOffset) {
			this.objectPointer = objectPointer;
			this.clazz = clazz;
			this.startOffset = startOffset;
		}
	}

	private static U32 getSalt(J9JavaVMPointer vm, UDATA objectPointer, boolean isValueType) throws CorruptDataException
	{
		/* set up the default salt */
		UDATA salt = new U32(1421595292).bitXor(new U32(UDATA.cast(vm)));
		J9IdentityHashDataPointer hashData = vm.identityHashData();
		UDATA saltPolicy = hashData.hashSaltPolicy();

		if (isValueType && ValueTypeHelper.getValueTypeHelper().areValueTypesSupported()) {
			saltPolicy = new UDATA(J9IdentityHashData.J9_IDENTITY_HASH_SALT_POLICY_NONE);
		}

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
		datum = datum.mult(MIX_MUL1);
		datum = rotateLeft(datum, 15);
		datum = datum.mult(MIX_MUL2);
		hashValue = hashValue.bitXor(datum);
		hashValue = rotateLeft(hashValue, 13);
		hashValue = hashValue.mult(5);
		hashValue = hashValue.add(MIX_ADD1);

		return hashValue;
	}

	private static U32 finalizeMurmur3Hash(U32 hashValue, U32 numBytesHashed)
	{
		hashValue = hashValue.bitXor(numBytesHashed);
		hashValue = hashValue.bitXor(hashValue.rightShift(16));
		hashValue = hashValue.mult(FINALIZE_MUL1);
		hashValue = hashValue.bitXor(hashValue.rightShift(13));
		hashValue = hashValue.mult(FINALIZE_MUL2);
		hashValue = hashValue.bitXor(hashValue.rightShift(16));

		return hashValue;
	}

	private static I32 convertValueObjectAtOffsetToHash(J9JavaVMPointer vm, J9ObjectPointer objectPointer, J9ClassPointer clazz, UDATA startOffset) throws CorruptDataException
	{
		U32 hashValue = getSalt(vm, UDATA.cast(objectPointer), true);
		U32 numBytesHashed = new U32(0);
		LinkedList<ValueTypeHashQueueEntry> queue = new LinkedList<>();
		ValueTypeHashQueueEntry entry = new ValueTypeHashQueueEntry(objectPointer, clazz, startOffset);
		ValueTypeHelper valueTypeHelper = ValueTypeHelper.getValueTypeHelper();

		hashValue = mix(hashValue, new U32(UDATA.cast(clazz)));
		numBytesHashed = numBytesHashed.add(UDATA.SIZEOF);

		while (true) {
			U32 walkFlags = new U32(J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE);
			J9ClassPointer superClass = J9ClassHelper.superclass(entry.clazz);
			Iterator<J9ObjectFieldOffset> fieldIterator = J9ObjectFieldOffsetIterator.J9ObjectFieldOffsetIteratorFor(entry.clazz, superClass, walkFlags);
			while (fieldIterator.hasNext()) {
				J9ObjectFieldOffset fieldOffset = fieldIterator.next();
				UDATA offset = entry.startOffset.add(fieldOffset.getOffsetOrAddress());
				String signature = J9ROMFieldShapeHelper.getSignature(fieldOffset.getField());
				char sigChar = signature.charAt(0);
				switch (sigChar) {
				case 'Z': /* boolean */
				case 'B': /* byte */ {
					if (J9BuildFlags.J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) {
						U8 datum8 = U8Pointer.cast(entry.objectPointer.addOffset(offset)).at(0);
						hashValue = mix(hashValue, new U32(datum8));
						numBytesHashed = numBytesHashed.add(1);
						break;
					}
					/* Fall through to 32-bit case if compact layouts are not enabled. */
				}
				case 'C': /* char */
				case 'S': /* short */ {
					if (J9BuildFlags.J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) {
						U16 datum16 = U16Pointer.cast(entry.objectPointer.addOffset(offset)).at(0);
						hashValue = mix(hashValue, new U32(datum16));
						numBytesHashed = numBytesHashed.add(2);
						break;
					}
					/* Fall through to 32-bit case if compact layouts are not enabled. */
				}
				case 'I': /* int */
				case 'F': /* float */ {
					U32 datum32 = U32Pointer.cast(entry.objectPointer.addOffset(offset)).at(0);
					hashValue = mix(hashValue, datum32);
					numBytesHashed = numBytesHashed.add(4);
					break;
				}

				case 'J': /* long */
				case 'D': /* double */ {
					U64 datum64 = U64Pointer.cast(entry.objectPointer.addOffset(offset)).at(0);
					hashValue = mix(hashValue, new U32(datum64.bitAnd(0xffffffffL)));
					hashValue = mix(hashValue, new U32(datum64.rightShift(32)));
					numBytesHashed = numBytesHashed.add(8);
					break;
				}

				case '[': /* array */
				case 'L': /* object */ {
					U32 datum = new U32(0);
					UDATA modifiers = fieldOffset.getField().modifiers();
					if (modifiers.anyBitsIn(J9FieldFlags.J9FieldFlagIsNullRestricted)) {
						/* Null-restricted field. */
						if (valueTypeHelper.isFieldInClassFlattened(entry.clazz, fieldOffset.getField())) {
							/* Null-restricted flattened field. */
							J9ClassPointer flatClazz = valueTypeHelper.findJ9ClassInFlattenedClassCacheWithFieldName(entry.clazz, J9ROMFieldShapeHelper.getName(fieldOffset.getField()));
							if (flatClazz.notNull()) {
								queue.add(new ValueTypeHashQueueEntry(entry.objectPointer, flatClazz, offset));
							}
						} else {
							/* Null-restricted non-flattened field. */
							J9ObjectPointer fieldObject = ObjectReferencePointer.cast(entry.objectPointer.addOffset(offset)).at(0);
							if (fieldObject.notNull()) {
								J9ClassPointer fieldClazz = J9ObjectHelper.clazz(fieldObject);
								UDATA flags = new UDATA(J9ObjectHelper.flags(fieldObject));
								boolean addToQueue = true;
								if (flags.anyBitsIn(J9Object.OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS)) {
									UDATA hashSlotOffset = ObjectModel.getHashcodeOffset(fieldObject);
									I32 storedHash = I32Pointer.cast(fieldObject.addOffset(hashSlotOffset)).at(0);
									if (!storedHash.eq(0)) {
										datum = new U32(storedHash);
										hashValue = mix(hashValue, datum);
										numBytesHashed = numBytesHashed.add(4);
										addToQueue = false;
									}
								}
								if (addToQueue) {
									UDATA objectHeaderSize = new UDATA(J9ObjectHelper.headerSize());
									queue.add(new ValueTypeHashQueueEntry(fieldObject, fieldClazz, objectHeaderSize));
								}
							}
						}
					} else {
						J9ObjectPointer fieldObject = ObjectReferencePointer.cast(entry.objectPointer.addOffset(offset)).at(0);
						if (fieldObject.notNull()) {
							J9ClassPointer fieldClazz = J9ObjectHelper.clazz(fieldObject);
							UDATA flags = new UDATA(J9ObjectHelper.flags(fieldObject));
							if (flags.anyBitsIn(J9Object.OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS)) {
								boolean addToQueue = true;
								UDATA hashSlotOffset = ObjectModel.getHashcodeOffset(fieldObject);
								I32 storedHash = I32Pointer.cast(fieldObject.addOffset(hashSlotOffset)).at(0);

								if (!storedHash.eq(0)) {
									datum = new U32(storedHash);
									hashValue = mix(hashValue, datum);
									numBytesHashed = numBytesHashed.add(4);
									addToQueue = false;
								}

								if (addToQueue) {
									UDATA objectHeaderSize = new UDATA(J9ObjectHelper.headerSize());
									queue.add(new ValueTypeHashQueueEntry(fieldObject, fieldClazz, objectHeaderSize));
								}
							} else if (valueTypeHelper.isJ9ClassAValueType(fieldClazz)) {
								UDATA objectHeaderSize = new UDATA(J9ObjectHelper.headerSize());
								queue.add(new ValueTypeHashQueueEntry(fieldObject, fieldClazz, objectHeaderSize));
							} else {
								datum = new U32(inlineConvertValueToHash(vm, UDATA.cast(fieldObject)));
								hashValue = mix(hashValue, datum);
								numBytesHashed = numBytesHashed.add(4);
							}
						}
					}
					break;
				}
				default:
					/* Invalid field signature. */
					break;
				}
			}
			if (queue.isEmpty()) {
				break;
			}
			entry = queue.removeFirst();
		}
		hashValue = finalizeMurmur3Hash(hashValue, numBytesHashed);
		if (hashValue.eq(0)) {
			hashValue = new U32(1);
		}
		return new I32(hashValue);
	}

	public static I32 inlineConvertObjectToHash(J9JavaVMPointer vm, J9ObjectPointer objectPointer) throws CorruptDataException
	{
		I32 hashValue;
		ValueTypeHelper valueTypeHelper = ValueTypeHelper.getValueTypeHelper();
		if (valueTypeHelper.areValueTypesSupported()) {
			J9ClassPointer clazz = J9ObjectHelper.clazz(objectPointer);
			if (clazz.notNull() && valueTypeHelper.isJ9ClassAValueType(clazz)) {
				UDATA objectHeaderSize = new UDATA(J9ObjectHelper.headerSize());
				hashValue = convertValueObjectAtOffsetToHash(vm, objectPointer, clazz, objectHeaderSize);
			} else {
				hashValue = inlineConvertValueToHash(vm, UDATA.cast(objectPointer));
			}
		} else {
			hashValue = inlineConvertValueToHash(vm, UDATA.cast(objectPointer));
		}
		return hashValue;
	}

	private static I32 inlineConvertValueToHash(J9JavaVMPointer vm, UDATA objectPointer) throws CorruptDataException
	{
		U32 hashValue = getSalt(vm, objectPointer, false);
		UDATA shiftedAddress = objectPointer.div(ObjectModel.getObjectAlignmentInBytes());

		U32 datum = new U32(shiftedAddress.bitAnd(0xffffffff));
		hashValue = mix(hashValue, datum);

		if (J9BuildFlags.J9VM_ENV_DATA64) {
			datum = new U32(shiftedAddress.rightShift(32));
			hashValue = mix(hashValue, datum);
		}

		hashValue = finalizeMurmur3Hash(hashValue, new U32(UDATA.SIZEOF));

		/* If forcing positive hash codes, AND out the sign bit */
		if (J9JavaVMHelper.extendedRuntimeFlagIsSet(vm, J9Consts.J9_EXTENDED_RUNTIME_POSITIVE_HASHCODE)) {
			hashValue = hashValue.bitAnd(0x7FFFFFFF);
		}

		return new I32(hashValue);
	}

	public static I32 convertObjectAddressToHash(J9JavaVMPointer vm, J9ObjectPointer object) throws CorruptDataException
	{
		if (ValueTypeHelper.getValueTypeHelper().areValueTypesSupported()) {
			return inlineConvertObjectToHash(vm, object);
		} else {
			return inlineConvertValueToHash(vm, UDATA.cast(object));
		}
	}

	public static I32 convertValueToHash(J9JavaVMPointer vm, UDATA value) throws CorruptDataException
	{
		return inlineConvertValueToHash(vm, value);
	}
}

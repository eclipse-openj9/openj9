/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_CLASS;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_FIELD;

import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMClassRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMConstantPoolItemPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMConstantPoolItemPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldRefPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.types.U32;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

/**
 * VMConstant pool value retrieval. The constant pool is referenced by constants
 * from com.ibm.j9ddr.vm29.structure.J9ConstantPool. To find out what type the
 * value is, refer to oti/vmconstantpool.xml, or look into the generated header
 * ./oti/j9vmconstantpool.h.
 */
public final class VMConstantPool {

	/* Use Object, since not everything in the array inherits from AbstractPointer */
	private static Object _constantPool [] = null;
	private static U32Pointer _cpShapeDescription;
	private static J9ROMConstantPoolItemPointer _romCPStart;
	private static J9RAMConstantPoolItemPointer _ramCPStart;

	static {
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9ConstantPoolPointer jclConstantPool = J9ConstantPoolPointer.cast(vm.jclConstantPoolEA());
			J9ClassPointer ramClass = jclConstantPool.ramClass();
			J9ROMClassPointer romClass = ramClass.romClass();

			if (romClass.romConstantPoolCount().gte(new U32(Integer.MAX_VALUE))) {
				throw new CorruptDataException("ROM constantpoolcount exceeds Integer.MAX_VALUE");
			}
			int constPoolCount = romClass.romConstantPoolCount().intValue();

			_constantPool = new Object[constPoolCount];
			_cpShapeDescription = J9ROMClassHelper.cpShapeDescription(romClass);
			_romCPStart = J9ROMClassHelper.constantPool(romClass);
			_ramCPStart = J9RAMConstantPoolItemPointer.cast(jclConstantPool);
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Problem reading the VMConstantPool", e, false);
		}
	}

	/**
	 * Get a class from the constant pool.
	 * @param index A J9VmconstantpoolConstants index into the constant pool.  Must be for a class reference.
	 * @return Either the loaded class, or if its not in the constant pool, J9ClassPointer.NULL.
	 * @throws CorruptDataException If the CPShape of the index is not a class.
	 */
	public static J9ClassPointer getClass (long index) throws CorruptDataException
	{
		if (_constantPool.length <= index || 0 > index) {
			throw new IndexOutOfBoundsException("Index outside of constant pool bounds");
		}
		int cpIndex = (int) index;

		long shapeDesc = ConstantPoolHelpers.J9_CP_TYPE(_cpShapeDescription, cpIndex);
		if (J9CPTYPE_CLASS != shapeDesc) {
			throw new CorruptDataException("VMConstantPool[" + index + "] CP_TYPE is not J9CPTYPE_CLASS");
		}

		J9ClassPointer classPointer = null;
		if (null != _constantPool[cpIndex] ) {
			classPointer = (J9ClassPointer) _constantPool[cpIndex];
		} else {
			J9RAMConstantPoolItemPointer ramEntry = _ramCPStart.add(index);
			classPointer = J9RAMClassRefPointer.cast(ramEntry).value();
			_constantPool[cpIndex] = classPointer;
		}
		return classPointer;
	}

	/**
	 * Get a field offset from the constant pool.
	 * @param index A J9VmconstantpoolConstants index into the constant pool.  Must be for a static or instance field reference.
	 * @return Either the offset to the object, or null if the field's class is not resolved in the constant pool.
	 * @throws CorruptDataException If the field cannot be found in the related class or the CP index is not a field reference.
	 */
	public static J9ObjectFieldOffset getFieldOffset(long index) throws CorruptDataException
	{
		if (_constantPool.length <= index || 0 > index) {
			throw new IndexOutOfBoundsException("Index outside of constant pool bounds");
		}

		int cpIndex = (int) index;
		long shapeDesc = ConstantPoolHelpers.J9_CP_TYPE(_cpShapeDescription, cpIndex);
		if (J9CPTYPE_FIELD != shapeDesc) {
			throw new CorruptDataException("VMConstantPool[" + index + "] CP_TYPE is not J9CPTYPE_FIELD");
		}

		/* The offset of the field, to be returned */
		J9ObjectFieldOffset offset = null;
		
		if (null != _constantPool[cpIndex]) {
			offset = (J9ObjectFieldOffset) _constantPool[cpIndex];
		} else {
			J9ROMFieldRefPointer romRef = J9ROMFieldRefPointer.cast(_romCPStart.add(cpIndex));
			J9ClassPointer refClass = getClass(romRef.classRefCPIndex().longValue());
			J9ClassPointer currentClass = refClass;
			if (currentClass.notNull()) {
				/* If the current class is J9ClassPointer.NULL, return null as the field offset */

				/* Resolve the fieldname, starting from the current class,
				 * and working up the class hierarchy towards the super classes. This should
				 * properly handle shadowed fields.
				 */
				String fieldName = J9UTF8Helper.stringValue(romRef.nameAndSignature().name());
				String signature = J9UTF8Helper.stringValue(romRef.nameAndSignature().signature());
				while (currentClass.notNull() && (null == offset)) {
					Iterator<J9ObjectFieldOffset> fields = J9ClassHelper.getFieldOffsets(currentClass);
					while (fields.hasNext()) {
						J9ObjectFieldOffset field = fields.next();
						if (field.getName().equals(fieldName) && field.getSignature().equals(signature)) {
							offset = field;
							break;
						}
					}
					currentClass = J9ClassHelper.superclass(currentClass);
				}

				if (null == offset) {
					/* The field should exist in the class it points to, unless the constant pool is corrupt or wrong */
					throw new CorruptDataException("VMConstantPool[" + index + "] field not found: " +  J9ClassHelper.getName(refClass) + "." + fieldName + " " + signature);
				} else {
					_constantPool[cpIndex] = offset;
				}
			}
		}
		return offset;
	}
}

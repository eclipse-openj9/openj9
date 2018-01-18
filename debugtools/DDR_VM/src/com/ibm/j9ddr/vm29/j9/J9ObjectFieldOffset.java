/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMFieldShapeHelper;
import com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Representation of the offset for instance fields or address for static fields in a J9Object
 */
 
public class J9ObjectFieldOffset {
	private final J9ROMFieldShapePointer field;
	private final UDATA offsetOrAddress;
	private final boolean isHidden;
	
	public J9ObjectFieldOffset(J9ROMFieldShapePointer field, UDATA offsetOrAddress, boolean isHidden) {
		this.field = field;
		this.offsetOrAddress = offsetOrAddress;
		this.isHidden = isHidden;
	}

	public boolean isStatic() {
		try {
			return field.modifiers().anyBitsIn(J9JavaAccessFlags.J9AccStatic);
		} catch (CorruptDataException e) {
			return false;
		}
	}
	
	public boolean isHidden() {
		return isHidden;
	}
	
	public J9ROMFieldShapePointer getField() {
		return field;
	}
	
	public UDATA getOffsetOrAddress() {
		return offsetOrAddress;
	}
	
	public String toString() {
		return String.format("%s: %s isStatic: %s", field, offsetOrAddress, isStatic());
	}
	
	public String getName() throws CorruptDataException {
		return J9ROMFieldShapeHelper.getName(field);
	}
	
	public String getSignature() throws CorruptDataException {
		return J9ROMFieldShapeHelper.getSignature(field);
	}
}

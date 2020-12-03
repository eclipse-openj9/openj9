/*******************************************************************************
 * Copyright (c) 2001, 2021 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9.walkers;

import com.ibm.j9ddr.vm29.pointer.SelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.types.U32;

public class LocalVariableTable {
	public LocalVariableTable(U32 slotNumber, U32 startVisibility, U32 visibilityLength,
			SelfRelativePointer genericSignatureSrp, J9UTF8Pointer genericSignature,
			SelfRelativePointer nameSrp, J9UTF8Pointer name,
			SelfRelativePointer signatureSrp, J9UTF8Pointer signature) {
		this.slotNumber = slotNumber;
		this.startVisibility = startVisibility;
		this.visibilityLength = visibilityLength;
		this.genericSignatureSrp = genericSignatureSrp;
		this.genericSignature = genericSignature;
		this.nameSrp = nameSrp;
		this.name = name;
		this.signatureSrp = signatureSrp;
		this.signature = signature;
	}

	private U32 slotNumber;
	private U32 startVisibility;
	private U32 visibilityLength;
	private SelfRelativePointer genericSignatureSrp;
	private J9UTF8Pointer genericSignature;
	private SelfRelativePointer nameSrp;
	private J9UTF8Pointer name;
	private SelfRelativePointer signatureSrp;
	private J9UTF8Pointer signature;

	public U32 getSlotNumber() {
		return slotNumber;
	}
	public U32 getStartVisibility() {
		return startVisibility;
	}
	public U32 getVisibilityLength() {
		return visibilityLength;
	}
	public SelfRelativePointer getGenericSignatureSrp() {
		return genericSignatureSrp;
	}
	public J9UTF8Pointer getGenericSignature() {
		return genericSignature;
	}
	public SelfRelativePointer getNameSrp() {
		return nameSrp;
	}
	public J9UTF8Pointer getName() {
		return name;
	}
	public SelfRelativePointer getSignatureSrp() {
		return signatureSrp;
	}
	public J9UTF8Pointer getSignature() {
		return signature;
	}
}

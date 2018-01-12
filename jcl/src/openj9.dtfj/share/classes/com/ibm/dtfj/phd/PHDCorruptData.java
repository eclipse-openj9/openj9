/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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
package com.ibm.dtfj.phd;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;

/**
 * @author ajohnson
 */
public class PHDCorruptData implements CorruptData {

	private final String reason;
	private final ImagePointer address;
	private final Exception cause;
	
	/**
	 * Build a CorruptData object from a CorruptDataException possibly from another dump type.
	 * @param space
	 * @param e
	 */
	PHDCorruptData(ImageAddressSpace space, CorruptDataException e) {
		this(e.getCorruptData().toString(), convPointer(space, e.getCorruptData().getAddress()), e);
	}
	
	PHDCorruptData(ImageAddressSpace space, CorruptData cd) {
		this(cd.toString(), convPointer(space, cd.getAddress()));
	}
	
	PHDCorruptData(String desc, ImagePointer addr) {
		this(desc, addr, null);
	}
	
	PHDCorruptData(String desc, ImagePointer addr, Exception exc) {
		reason = desc;
		address = addr;
		cause = exc;
	}
	
	public ImagePointer getAddress() {
		return address;
	}

	public String toString() {
		StringBuffer buf = new StringBuffer();
		if (reason != null) {
			buf.append(reason);
		}
		if (address != null) { 
			if (buf.length() > 0) buf.append(" at ");
			buf.append("0x").append(Long.toHexString(address.getAddress()));
		}
		if (cause != null) {
			if (buf.length() > 0) buf.append(": ");
			buf.append(cause);
		}
		return buf.toString();
	}
	
	static CorruptDataException newCorruptDataException(PHDCorruptData cd) {
		CorruptDataException e = new CorruptDataException(cd);
		cd.initCause(e);
		return e;
	}
	
	CorruptDataException initCause(CorruptDataException t) {
		if (cause != null) t.initCause(cause);
		return t;
	}
	
	private static ImagePointer convPointer(ImageAddressSpace space, ImagePointer p) {
		if (p != null) {
			// Convert the the image pointer to the current address space.
			return space.getPointer(p.getAddress());
		} else {
			return null;
		}
	}
}

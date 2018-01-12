/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2009, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.javacore;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageRegister;

public class JCImageRegister implements ImageRegister {

	private final String name;
	private final Number value;
	
	public JCImageRegister(String name, Number value) {
		this.name = name;
		this.value = value;
	}
	
	public String getName() {
		return name;
	}

	public Number getValue() throws CorruptDataException {
		if (value == null) throw new CorruptDataException(new JCCorruptData("No value", null));
		return value;
	}

	public String toString() {
		return name+"="+value;
	}
	
	/**
	 */
	public boolean equals(Object obj) {
		if (this == obj)
			return true;
		if (obj == null)
			return false;
		if (getClass() != obj.getClass())
			return false;
		JCImageRegister to = (JCImageRegister)obj;
		return name != null && name.equals(to.name) && value != null && value.equals(to.value);
	}
	
	/**
	 */
	public int hashCode() {
		return (name != null ? name.hashCode() : 0) ^ (value != null ? value.intValue() : 0);
	}
}

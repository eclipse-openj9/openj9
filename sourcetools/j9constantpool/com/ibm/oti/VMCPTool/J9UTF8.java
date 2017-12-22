/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.oti.VMCPTool;

public class J9UTF8 implements ConstantPoolItem {
	public final String data;

	public J9UTF8(String data) {
		this.data = data;
	}
	
	public boolean equals(Object utf) {
		return (utf instanceof J9UTF8) 
			&& data.equals(((J9UTF8)utf).data);
	}

	public int hashCode() {
		return data.hashCode();
	}
	
	public void write(ConstantPoolStream ds) {
		ds.alignTo(2);
		ds.setOffset(this);
		
		ds.writeShort(data.length());
		ds.writeUTF(data);
	}
}

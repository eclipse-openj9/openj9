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

public class NameAndSignature implements ConstantPoolItem {
	public final J9UTF8 name;
	public final J9UTF8 signature;

	public NameAndSignature(String name, String signature) {
		this.name = new J9UTF8(name);
		this.signature = new J9UTF8(signature);
	}
	
	public boolean equals(Object nas) {
		return (nas instanceof NameAndSignature) 
			&& name.equals(((NameAndSignature)nas).name) 
			&& signature.equals(((NameAndSignature)nas).signature);
	}

	public int hashCode() {
		return name.hashCode() ^ signature.hashCode();
	}
	
	public void write(ConstantPoolStream ds) {
		ds.writeSecondaryItem(name);
		ds.writeSecondaryItem(signature);

		ds.alignTo(4);
		ds.setOffset(this);
		ds.writeInt( ds.getOffset(name) - ds.getOffset() );
		ds.writeInt( ds.getOffset(signature) - ds.getOffset() );
	}
}

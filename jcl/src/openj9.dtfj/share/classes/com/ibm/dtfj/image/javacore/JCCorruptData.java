/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.ImagePointer;

public class JCCorruptData implements CorruptData{
	
	private ImagePointer fPointer;
	private String fMessage = null;

	public JCCorruptData(ImagePointer pointer) {
		fPointer = pointer;
	}
	
	public JCCorruptData(String message, ImagePointer pointer) {
		fMessage = message;
		fPointer = pointer;
	}
	
	public String toString() {
		if (fPointer != null) {
			if (fMessage != null) 
				return fMessage+" : 0x"+Long.toHexString(fPointer.getAddress());
			else
				return "0x"+Long.toHexString(fPointer.getAddress());
		}
		return fMessage;
	}

	public ImagePointer getAddress() {
		return fPointer;
	}

}

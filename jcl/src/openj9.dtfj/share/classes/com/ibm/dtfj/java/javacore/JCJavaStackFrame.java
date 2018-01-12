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
package com.ibm.dtfj.java.javacore;

import java.util.Collections;
import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.javacore.JCCorruptData;
import com.ibm.dtfj.java.JavaLocation;
import com.ibm.dtfj.java.JavaStackFrame;

public class JCJavaStackFrame implements JavaStackFrame {
	
	private ImagePointer fPointer;
	private final JavaLocation fLocation;
	private final JCJavaThread fThread;
	
	public JCJavaStackFrame(JCJavaThread thread, JavaLocation location) throws JCInvalidArgumentsException{
		if (location == null) {
			throw new JCInvalidArgumentsException("Must have a valid location.");
		}
		if (thread == null) {
			throw new JCInvalidArgumentsException("A java stack frame must be associated with a valid java thread");
		}
		fThread = thread;
		fLocation = location;
		fThread.addStackFrame(this);
	}
	
	
	/**
	 * 
	 * @param imagePointer
	 */
	public void setBasePointer(ImagePointer imagePointer) {
		fPointer = imagePointer;
	}

	
	/**
	 * 
	 */
	public ImagePointer getBasePointer() throws CorruptDataException {
		if (fPointer == null) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fPointer;
	}

	
	/**
	 * 
	 */
	public JavaLocation getLocation() throws CorruptDataException {
		return fLocation;
	}


	public Iterator getHeapRoots() {
		return Collections.EMPTY_LIST.iterator();
	}

}

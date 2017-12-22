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

import java.util.Collections;
import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;

/**
 * A JavaObject which could not be built properly.
 * @author ajohnson
 */
class PHDCorruptJavaObject extends PHDCorruptData implements JavaObject {
	
	PHDCorruptJavaObject(String des, ImagePointer where, Exception e) {
		super(des, where, e);
		// Check for a null pointer - getID cannot return null
		where.getAddress();
	}
	
	public void arraycopy(int srcStart, Object dst, int dstStart, int length)
			throws CorruptDataException, MemoryAccessException {
		throw initCause(new CorruptDataException(this));
	}

	public int getArraySize() throws CorruptDataException {
		throw initCause(new CorruptDataException(this));
	}

	public long getHashcode() throws DataUnavailable, CorruptDataException {
		throw initCause(new CorruptDataException(this));
	}

	public JavaHeap getHeap() throws CorruptDataException, DataUnavailable {
		throw initCause(new CorruptDataException(this));
	}

	public ImagePointer getID() {
		return getAddress();
	}

	public JavaClass getJavaClass() throws CorruptDataException {
		throw initCause(new CorruptDataException(this));
	}

	public long getPersistentHashcode() throws DataUnavailable, CorruptDataException {
		throw initCause(new CorruptDataException(this));
	}

	public Iterator<JavaReference> getReferences() {
		return Collections.<JavaReference>emptyList().iterator();
	}

	public Iterator<ImageSection> getSections() {
		return Collections.<ImageSection>emptyList().iterator();
	}

	public long getSize() throws CorruptDataException {
		throw initCause(new CorruptDataException(this));
	}

	public boolean isArray() throws CorruptDataException {
		throw initCause(new CorruptDataException(this));
	}
}

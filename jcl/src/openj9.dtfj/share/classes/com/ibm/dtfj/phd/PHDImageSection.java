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

import java.util.Properties;

import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;

/** 
 *  @author ajohnson
 */
class PHDImageSection implements ImageSection {
	
	private final ImagePointer base;
	private final long size;
	private final String name;
	PHDImageSection(String name, ImagePointer base, long size) {
		this.base = base;
		this.size = size;
		this.name = name;
	}

	public ImagePointer getBaseAddress() {
		return base;
	}

	public String getName() {
		return name;
	}

	public long getSize() {
		return size;
	}

	public boolean isExecutable() throws DataUnavailable {
		return base.isExecutable();
	}

	public boolean isReadOnly() throws DataUnavailable {
		return base.isReadOnly();
	}

	public boolean isShared() throws DataUnavailable {
		return base.isShared();
	}

	public Properties getProperties() {
		return base.getProperties();
	}
	
	public int hashCode() {
		return base.hashCode() ^ (int)size ^ (int)(size >>> 32);
	}
	
	public boolean equals(Object o) {
		if (!(o instanceof PHDImageSection)) return false;
		PHDImageSection to = (PHDImageSection)o;
		return base.equals(to.base) && size == to.size;
	}
	
	public String toString() {
		return this.getClass().getName()+" "+getName()+" 0x"+Long.toHexString(getBaseAddress().getAddress())+":0x"+Long.toHexString(getBaseAddress().getAddress()+getSize());
	}
}

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

import java.util.Properties;

import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;

public class JCImageSection implements ImageSection {

	private final String name;
	private final ImagePointer base;
	private final long size;
	
	/**
	  * Construct a new Image section with the given name, base address and length in bytes
	  */
	public JCImageSection(String name, ImagePointer base, long size) {
		this.name = name;
		this.base = base;
		this.size = size;
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
		throw new DataUnavailable();
	}

	public boolean isReadOnly() throws DataUnavailable {
		throw new DataUnavailable();
	}

	public boolean isShared() throws DataUnavailable {
		throw new DataUnavailable();
	}
	
	public Properties getProperties() {
		return base.getProperties();
	}

}

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
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.java.JavaLocation;
import com.ibm.dtfj.java.JavaMethod;

/**
 * @author ajohnson
 */
public class PHDCorruptJavaLocation extends PHDCorruptData implements JavaLocation {
	
	PHDCorruptJavaLocation(ImageAddressSpace space, CorruptData cd) {
		super(space, cd);
	}

	public int getCompilationLevel() throws CorruptDataException {
		throw new CorruptDataException(this);
	}

	public String getFilename() throws DataUnavailable, CorruptDataException {
		throw new CorruptDataException(this);
	}

	public int getLineNumber() throws DataUnavailable, CorruptDataException {
		throw new CorruptDataException(this);
	}

	public JavaMethod getMethod() throws CorruptDataException {
		throw new CorruptDataException(this);
	}

}

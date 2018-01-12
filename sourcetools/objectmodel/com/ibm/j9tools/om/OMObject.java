/*******************************************************************************
 * Copyright (c) 2007, 2011 IBM Corp. and others
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
package com.ibm.j9tools.om;

import java.io.File;

import org.xml.sax.Locator;

public abstract class OMObject {
	// Location where this Objects data was obtained from
	private SourceLocation _sourceLocation;

	public OMObject() {

	}
	
	/**
	 * Set the location where this BuildSpec's data was obtained from. The Column and Line
	 * number of the locator point at the <spec> element.
	 * 
	 * @param 	file		File this BuildSpec was obtained frmo
	 * @param 	locator		Locator reference
	 */
	public void setLocation(File file, Locator locator) {
		_sourceLocation = new SourceLocation(file, locator);
	}

	public SourceLocation getLocation() {
		return _sourceLocation;
	}

}

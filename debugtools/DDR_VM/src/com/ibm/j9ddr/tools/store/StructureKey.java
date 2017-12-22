/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.store;

public class StructureKey {
	private String platform;
	private String buildID;
	
	public StructureKey(String platform, String buildID) {
		super();
		this.platform = platform;
		this.buildID = buildID;
	}
	
	public StructureKey(String keyString) {
		super();
		int index = keyString.indexOf(".");
		this.platform = keyString.substring(0, index);
		this.buildID = keyString.substring(index + 1);
	}

	public String getPlatform() {
		return platform;
	}
	
	public String getBuildID() {
		return buildID;
	}
	
	public boolean equals(Object obj) {
		if (obj != null && obj instanceof StructureKey) {
			StructureKey keyObj = (StructureKey) obj;
			return platform.equals(keyObj.platform) && buildID.equals(keyObj.buildID);
		}
		return false;
	}
	
	public int hashCode() {
		return toString().hashCode();
	}
	
	public String toString() {
		return platform + "." + buildID;
	}
}

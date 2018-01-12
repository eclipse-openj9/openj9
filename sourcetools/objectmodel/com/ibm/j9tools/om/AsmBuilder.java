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

/**
 * A container class used to keep track of a {@link BuildSpec}'s assembly builder.  
 * A {@link BuildSpec} may contain only one {@link AsmBuilder}.
 * 
 * @author	Gabriel Castro
 * @since	v1.5.0
 */
public class AsmBuilder extends OMObject implements Comparable<AsmBuilder>{
	String id;

	/**
	 * Creates an {@link AsmBuilder} container with the given builder ID.
	 * 
	 * @param 	asmId	the assembly builder name
	 */
	public AsmBuilder(String asmId) {
		this.id = asmId;
	}

	/**
	 * Retrieves the name of the assembly builder.
	 * 
	 * @return	the assembly builder name
	 */
	public String getId() {
		return id;
	}

	/**
	 * Sets the name of the assembly builder.
	 * 
	 * @param 	id			the assembly builder name
	 */
	public void setId(String id) {
		this.id = id;
	}

	public int compareTo(AsmBuilder builder) {
		return id.compareToIgnoreCase(builder.id);
	}
}

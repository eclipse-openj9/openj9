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
 * A container class used to keep track of a {@link BuildSpec}'s default JCL configuration.  
 * A {@link BuildSpec} may contain only one {@link JclConfiguration}.
 * 
 * @author	Gabriel Castro
 * @since	v1.5.0	
 */
public class JclConfiguration extends OMObject implements Comparable<JclConfiguration>{
	String id;

	/**
	 * Creates a default JCL configuration with the given name.
	 *  
	 * @param 	jclId	the name of the JCL configuration
	 */
	public JclConfiguration(String jclId) {
		this.id = jclId;
	}

	/**
	 * Retrieves the name of this JCL configuration.
	 * 
	 * @return	the configuration name
	 */
	public String getId() {
		return id;
	}

	/**
	 * Sets the name of this JCL configuration.
	 * 
	 * @param 	id			the configuration name
	 */
	public void setId(String id) {
		this.id = id;
	}

	public int compareTo(JclConfiguration jcl) {
		return id.compareToIgnoreCase(jcl.id);
	}
}

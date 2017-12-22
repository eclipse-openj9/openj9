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
 * A container class used to keep track of valid {@link BuildSpec} source projects.
 * 
 * @author	Gabriel Castro
 */
public class Source extends OMObject implements Comparable<Source> {
	private String _projectId;

	/**
	 * Creates a J9 source project with the given name.
	 * 
	 * @param 	projectId	the project name
	 */
	public Source(String projectId) {
		this._projectId = projectId;
	}

	/**
	 * Retrieves the name of this J9 source project.
	 * 
	 * @return	the project name
	 */
	public String getId() {
		return _projectId;
	}

	/**
	 * Sets the name of this J9 source project.
	 * 
	 * @param 	id			the project name
	 */
	public void setId(String id) {
		_projectId = id;
	}

	/**
	 * @see java.lang.Comparable#compareTo(java.lang.Object)
	 */
	public int compareTo(Source o) {
		return _projectId.compareToIgnoreCase(o._projectId);
	}

	/**
	 * @see java.lang.Object#equals(java.lang.Object)
	 */
	@Override
	public boolean equals(Object o) {
		return (o instanceof Source) ? _projectId.equalsIgnoreCase(((Source) o)._projectId) : false;
	}

}

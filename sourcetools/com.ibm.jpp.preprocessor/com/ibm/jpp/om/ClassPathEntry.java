/*******************************************************************************
 * Copyright (c) 1999, 2019 IBM Corp. and others
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
package com.ibm.jpp.om;

/**
 * This class represents a classpath entry for use in the JPP Eclipse Plugin.
 */
public class ClassPathEntry {
	private final String type;
	private final String path;
	private final String sourcePath;
	private final boolean isExported;

	/**
	 * Main constructor for ClassPathEntry
	 *
	 * @param       path        the path associated with this entry
	 * @param       type        the type of classpath
	 */
	public ClassPathEntry(String path, String type) {
		this(path, type, null, false);
	}

	/**
	 * Main constructor for ClassPathEntry
	 *
	 * @param       path        the path associated with this entry
	 * @param       type        the type of classpath
	 * @param       isExported  <code>true</code> if this entry is contributed to dependent projects also, <code>false</code> otherwise
	 */
	public ClassPathEntry(String path, String type, boolean isExported) {
		this(path, type, null, isExported);
	}

	/**
	 * Main constructor for ClassPathEntry
	 *
	 * @param       path        the path associated with this entry
	 * @param       type        the type of classpath
	 * @param       sourcePath  the source path associated with this entry
	 */
	public ClassPathEntry(String path, String type, String sourcePath) {
		this(path, type, sourcePath, false);
	}

	/**
	 * Main constructor for ClassPathEntry
	 *
	 * @param       path        the path associated with this entry
	 * @param       type        the type of classpath
	 * @param       sourcePath  the source path associated with this entry
	 * @param       isExported  <code>true</code> if this entry is contributed to dependent projects also, <code>false</code> otherwise
	 */
	public ClassPathEntry(String path, String type, String sourcePath, boolean isExported) {
		this.path = path;
		this.type = type;
		this.sourcePath = sourcePath;
		this.isExported = isExported;
	}

	/**
	 * Returns this entry's path
	 *
	 * @return      the path.
	 */
	public String getPath() {
		return path;
	}

	/**
	 * Returns the short form of type of this classpath (one of: var, src, lib, pro, or con)
	 *
	 * @return the classpath type in short form.
	 *
	 * @see         #isVariable()
	 * @see         #isSource()
	 * @see         #isLibrary()
	 * @see         #isProject()
	 * @see         #isContainer()
	 *
	 */
	public String getType() {
		return type;
	}

	/**
	 * Returns the source path associated with this classpath
	 *
	 * @return      the classpath's source path
	 */
	public String getSourcePath() {
		return sourcePath;
	}

	/**
	 * Returns the classpath type
	 *
	 * @return      the classpath type
	 */
	public String getTypeString() {
		if (isVariable()) {
			return "variable";
		} else if (isSource()) {
			return "source";
		} else if (isLibrary()) {
			return "library";
		} else if (isProject()) {
			return "project";
		} else if (isContainer()) {
			return "container";
		} else {
			return "unknown";
		}
	}

	/**
	 * Returns whether or not this classpath sets up a variable or not
	 *
	 * @return      <code>true</code> if this classpath sets up a variable, <code>false</code> otherwise
	 */
	public boolean isVariable() {
		return (type.trim().equals("var"));
	}

	/**
	 * Returns whether or not this classpath points to a source folder or not
	 *
	 * @return      <code>true</code> if this classpath points to a source folder, <code>false</code> otherwise
	 */
	public boolean isSource() {
		return (type.trim().equals("src") && !(path.startsWith("/") || path.startsWith(":", 1)));
	}

	/**
	 * Returns whether or not this classpath points to a library or not
	 *
	 * @return      <code>true</code> if this classpath points to a library, <code>false</code> otherwise
	 */
	public boolean isLibrary() {
		return (type.trim().equals("lib"));
	}

	/**
	 * Returns whether or not this classpath points to a local library
	 *
	 * @return      <code>true</code> if this points to a local library, <code>false</code> otherwise
	 */
	public boolean isLocalLibrary() {
		return (type.trim().equals("lib") && !path.startsWith("/"));
	}

	/**
	 * Returns whether or not this classpath points to a project or not
	 *
	 * @return      <code>true</code> if this classpath points to a project, <code>false</code> otherwise
	 */
	public boolean isProject() {
		return (type.trim().equals("src") && path.startsWith("/"));
	}

	/**
	 * Returns whether or not this classpath points to a container or not
	 *
	 * @return      <code>true</code> if this classpath points to a container, <code>false</code> otherwise
	 */
	public boolean isContainer() {
		return (type.trim().equals("con"));
	}

	/**
	 * Returns whether or not this classpath is to be exported to dependent projects
	 *
	 * @return      <code>true</code> if this entry is contributed to dependent projects also, <code>false</code> otherwise
	 */
	public boolean isExported() {
		return isExported;
	}
}

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

import java.util.Map;
import java.util.Set;

/**
 * A container class used to keep track of a single Flag and its state
 *
 * @author	Maciek Klimkowski
 * @author	Gabriel Castro
 * @author	Han Xu
 */
public class Flag extends FlagDefinition {
	private Boolean state;
	private FlagDefinition flagDefinition;

	/**
	 * Creates a flag with the given ID and a default state of <code>false</code>.
	 *
	 * @param 	id			the ID of this flag
	 */
	public Flag(String id) {
		this(id, false);
	}

	/**
	 * Creates a flag with the given ID and state
	 *
	 * @param 	id			the ID of this flag
	 * @param 	state		the state of this flag
	 */
	public Flag(String id, Boolean state) {
		this.id = id;
		this.state = state;
	}

	/**
	 * Creates a flag from the given {@link FlagDefinition} with a default state
	 * of <code>false</code>.  The flag ID will be the same as the given {@link FlagDefinition}.
	 *
	 * @param 	flagDef		the parent flag definition
	 */
	public Flag(FlagDefinition flagDef) {
		this(flagDef, false);
	}

	/**
	 * Creates a flag from the given {@link FlagDefinition} with the given state.
	 * The flag ID will be the same as the given {@link FlagDefinition}.
	 *
	 * @param 	flagDef		the parent flag definition
	 * @param 	state		the state of this flag
	 */
	public Flag(FlagDefinition flagDef, boolean state) {
		this(flagDef.getId(), state);
		this.setDefinition(flagDef);
	}

	/**
	 * Sets the {@link FlagDefinitions} associated with this flag.
	 *
	 * @param 	flagDef		the flag definition for this flag
	 */
	public void setDefinition(FlagDefinition flagDef) {
		this.flagDefinition = flagDef;
		this.complete = true;
	}

	/**
	 * Removes the {@link FlagDefinition} associated with this flag.
	 */
	public void removeDefinition() {
		this.flagDefinition = null;
		complete = false;
	}

	/**
	 * Retrieves the state of this flag.
	 *
	 * @return	<code>true</code> if this flag is enabled, <code>false</code> otherwise
	 */
	public Boolean getState() {
		return state;
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#getDescription()
	 */
	@Override
	public String getDescription() {
		return (complete) ? flagDefinition.getDescription() : super.getDescription();
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#getIfRemoved()
	 */
	@Override
	public String getIfRemoved() {
		return (complete) ? flagDefinition.getIfRemoved() : super.getIfRemoved();
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#getPrecludes()
	 */
	@Override
	public Map<String, FlagDefinition> getPrecludes() {
		return (complete) ? flagDefinition.getPrecludes() : super.getPrecludes();
	}

	@Override
	public Map<String, FlagDefinition> getLocalPrecludes() {
		return (complete) ? flagDefinition.getLocalPrecludes() : super.getLocalPrecludes();
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#getRequires()
	 */
	@Override
	public Map<String, FlagDefinition> getRequires() {
		return (complete) ? flagDefinition.getRequires() : super.getRequires();
	}

	@Override
	public Map<String, FlagDefinition> getLocalRequires() {
		return (complete) ? flagDefinition.getLocalRequires() : super.getLocalRequires();
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#setPrecludes(java.util.Set)
	 */
	@Override
	public void setPrecludes(Set<FlagDefinition> precludes) {
		// Not allowed for Flag
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#addPrecludes(com.ibm.j9tools.om.FlagDefinition)
	 */
	@Override
	public void addPrecludes(FlagDefinition precludeFlag) {
		// Not allowed for Flag
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#removePrecludes(java.lang.String)
	 */
	@Override
	public void removePrecludes(String precludeFlagId) {
		// Not allowed for Flag
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#setRequires(java.util.Set)
	 */
	@Override
	public void setRequires(Set<FlagDefinition> requires) {
		// Not allowed for Flag
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#addRequires(com.ibm.j9tools.om.FlagDefinition)
	 */
	@Override
	public void addRequires(FlagDefinition requiredFlag) {
		// Not allowed for Flag
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#removeRequires(java.lang.String)
	 */
	@Override
	public void removeRequires(String requireFlagId) {
		// Not allowed for Flag
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#setDescription(java.lang.String)
	 */
	@Override
	public void setDescription(String description) {
		// Not allowed for Flag
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#setIfRemoved(java.lang.String)
	 */
	@Override
	public void setIfRemoved(String ifRemoved) {
		// Not allowed for Flag
	}

	/**
	 * Sets the state of this flag.
	 *
	 * @param 	state		the flag state
	 */
	public void setState(Boolean state) {
		this.state = state;
	}

	/**
	 * @see com.ibm.j9tools.om.FlagDefinition#equals(java.lang.Object)
	 */
	@Override
	public boolean equals(Object o) {
		return (o instanceof Flag) ? (id.equalsIgnoreCase(((Flag) o).id) && state.equals(((Flag) o).state)) : false;
	}

}

/*******************************************************************************
 * Copyright (c) 2007, 2019 IBM Corp. and others
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

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.StringTokenizer;
import java.util.TreeMap;

/**
 * Describes flag details but not a Flag state. Its principal use is to provide means
 * of defining Flag Requirements and Precludes chains. It will also verbosely describe the
 * function of any given Flag along with reprecisions involved in modifying or removing 
 * that Flag.
 * 
 * @author 	Maciek Klimkowski
 * @author	Gabriel Castro
 * @author	Han Xu
 */
public class FlagDefinition extends OMObject implements Comparable<FlagDefinition> {
	protected boolean complete = false;

	/**
	 * An unique flag identifier.
	 */
	protected String id;

	/**
	 * A free text field that describes the purpose of the flag, and specifically
	 * indicates what will happen if the flag is activated. This field is intended
	 * to help end users configure the virtual machine, so please be as verbose as
	 * necessary 	when describing the impact of enabling the flag.
	 */
	protected String description;

	/**
	 * A free text field that describes what will happen if the flag is removed. 
	 * This field is intended to help end users configure the virtual machine, 
	 * so please be as verbose as necessary when describing the impact of removing 
	 * the flag. 
	 */
	protected String ifRemoved;

	/**
	 * A set of requires flags. 
	 * Represents a depends-on linkage between flags. Indicates that the flag requires
	 * that another flag be enabled. The flag which is dependent upon is identified 
	 * by name using the flag attribute.
	 */
	protected Map<String, FlagDefinition> requires = new TreeMap<String, FlagDefinition>();

	/**
	 * A set of elements which identify flags that are incompatible with the 
	 * containing flag definition.
	 * Represents an either-or linkage between flags. Indicates that the flag 
	 * cannot function properly in the presence	of another enabled flag. The incompatible 
	 * flags is identified by name using the flag attribute.
	 */
	protected Map<String, FlagDefinition> precludes = new TreeMap<String, FlagDefinition>();

	/**
	 * Default FlagDefinition constructor
	 */
	public FlagDefinition() {
	}

	/**
	 * Default FlagDefinition constructor
	 * 
	 * @param 	id				Flag id
	 * @param 	description		Flag free text description
	 * @param 	ifRemoved		Flag free text description of flag removal repercussions 
	 */
	public FlagDefinition(String id, String description, String ifRemoved) {
		this.id = id;

		this.description = description;
		this.ifRemoved = ifRemoved;
	}

	public String getId() {
		return id;
	}

	public void setId(String id) {
		this.id = id;
	}

	public String getDescription() {
		return description;
	}

	public void setDescription(String description) {
		this.description = description;
	}

	public String getIfRemoved() {
		return ifRemoved;
	}

	public void setIfRemoved(String ifRemoved) {
		this.ifRemoved = ifRemoved;
	}

	/**
	 * Add a single requires flag to this flags required flags list
	 * 
	 * @param 	requiredFlag		the required flag
	 */
	public void addRequires(FlagDefinition requiredFlag) {
		requires.put(requiredFlag.getId(), requiredFlag);
	}

	public Map<String, FlagDefinition> getLocalRequires() {
		return Collections.unmodifiableMap(requires);
	}

	public Map<String, FlagDefinition> getRequires() {
		Map<String, FlagDefinition> allRequires = new HashMap<String, FlagDefinition>();
		addAllRequires(allRequires, this);

		return Collections.unmodifiableMap(allRequires);
	}

	private void addAllRequires(Map<String, FlagDefinition> allRequires, FlagDefinition definition) {
		for (FlagDefinition require : definition.getLocalRequires().values()) {
			if (!allRequires.containsKey(require.getId()) && !require.equals(definition)) {
				allRequires.put(require.getId(), require);
				addAllRequires(allRequires, require);
			}
		}
	}

	public void setRequires(Set<FlagDefinition> requires) {
		this.requires.clear();

		for (FlagDefinition flagDefinition : requires) {
			this.requires.put(flagDefinition.getId(), flagDefinition);
		}
	}

	public void removeRequires(String requireFlagId) {
		requires.remove(requireFlagId);
	}

	/**
	 * Add a single precludes flag to this flags precluded flags list
	 * 
	 * @param 	precludeFlag		the precluded flag
	 */
	public void addPrecludes(FlagDefinition precludeFlag) {
		precludes.put(precludeFlag.getId(), precludeFlag);
	}

	public Map<String, FlagDefinition> getLocalPrecludes() {
		return Collections.unmodifiableMap(precludes);
	}

	public Map<String, FlagDefinition> getPrecludes() {
		Map<String, FlagDefinition> allPrecludes = new HashMap<String, FlagDefinition>();
		allPrecludes.putAll(precludes);

		for (FlagDefinition requirement : getRequires().values()) {
			allPrecludes.putAll(requirement.getLocalPrecludes());
		}

		return Collections.unmodifiableMap(allPrecludes);
	}

	public void setPrecludes(Set<FlagDefinition> precludes) {
		this.precludes.clear();

		for (FlagDefinition flagDefinition : precludes) {
			this.precludes.put(flagDefinition.getId(), flagDefinition);
		}
	}

	public void removePrecludes(String precludeFlagId) {
		precludes.remove(precludeFlagId);
	}

	/**
	 * Retrieves the category (i.e. prefix) of the flag definition.
	 * 
	 * @return 	the category name
	 */
	public String getCategory() {
		StringTokenizer st = new StringTokenizer(id, "_"); //$NON-NLS-1$

		return (st.countTokens() == 1) ? "default" : st.nextToken(); //$NON-NLS-1$
	}

	/**
	 * Retrieves the component (ie. the postfix) of a flag definition.
	 * 
	 * @return	the component name
	 */
	public String getComponent() {
		StringTokenizer st = new StringTokenizer(id, "_"); //$NON-NLS-1$

		return (st.countTokens() == 1) ? st.nextToken() : id.substring(getCategory().length() + 1);
	}

	/**
	 * Defines this flag definition as fully parsed.
	 * 
	 * @param 	complete	<code>true</code> if definition fully parse, <code>false</code> otherwise
	 */
	public void setComplete(boolean complete) {
		this.complete = complete;
	}

	/**
	 * Determines if this flag definition has been fully parsed.
	 * 
	 * @return	<code>true</code> if definition fully parse, <code>false</code> otherwise
	 */
	public boolean isComplete() {
		return complete;
	}

	/**
	 * @see java.lang.Comparable#compareTo(java.lang.Object)
	 */
	public int compareTo(FlagDefinition flagDef) {
		return getComponent().compareTo(flagDef.getComponent());
	}

	/**
	 * @see java.lang.Object#equals(java.lang.Object)
	 */
	@Override
	public boolean equals(Object o) {
		if (o instanceof FlagDefinition) {
			FlagDefinition def = (FlagDefinition) o;

			return (id.equalsIgnoreCase(def.id) && description.equals(def.description) && requires.equals(def.requires) && precludes.equals(def.precludes));
		}
		return false;
	}

}

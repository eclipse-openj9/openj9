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
package com.ibm.dtfj.javacore.builder.javacore;

import java.util.HashMap;
import java.util.Iterator;

/**
 * Contains a collection of factories. Each factory is identified by a unique
 * id. Factories with the same ID are not allowed.
 * 
 * @see com.ibm.dtfj.javacore.builder
 *
 */
public class BuilderContainer {
	
	private HashMap fList;
	private AbstractBuilderComponent fCurrentComponent;
	
	public BuilderContainer() {
		fList = new HashMap();
		fCurrentComponent = null;
	}
	
	/**
	 * 
	 * @return iterator to the entire list of factories. Is never null.
	 */
	public Iterator getList()	{
		return fList.values().iterator();
	}
	
	/**
	 * 
	 * @param id unique id to lookup a particular factory in the container.
	 * @return found factory, or null if not found
	 */
	public AbstractBuilderComponent findComponent(String id) {
		AbstractBuilderComponent component = null;
		if (id != null) {
			component = (AbstractBuilderComponent) fList.get(id);
		}
		return component;
	}
	
	/**
	 * If factory component already exists, it will overwrite the existing entry in the container.
	 * @param component to be added.
	 * @return true if added/overwritten, false otherwise
	 */
	public boolean add(AbstractBuilderComponent component) {
		boolean added = false;
		if (added = (component != null)) {
			fList.put(component.getID(), component);
			fCurrentComponent = component;
		}
		return added;
	}
	
	/**
	 * 
	 * @return the last added factory component. May be null if no factory components have been added yet.
	 */
	public AbstractBuilderComponent getLastAdded() {
		return fCurrentComponent;
	}
}

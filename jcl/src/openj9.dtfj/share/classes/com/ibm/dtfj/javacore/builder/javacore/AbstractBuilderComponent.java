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

/**
 * Factories that build image level components like:
 * <br>
 * com.ibm.dtfj.image.Image
 * <br>
 * com.ibm.dtfj.image.ImageAddressSpace
 * <br>
 * com.ibm.dtfj.image.ImageProcess
 * <br>
 * share similar structure in the sense that each contains a collection of children factories.
 * For example, an Image factory will contain various ImageAddressSpace factories, each associated
 * with a unique ImageAddressSpace
 * an ImageAddressSpace factory will contain various ImageProcess factories, each associated with a 
 * unique ImageProcess.
 * 
 * Such image-level components have a similar internal structure, and thus inherit such structure
 * from this parent class.
 *
 */
public abstract class AbstractBuilderComponent {
	
	private final String fID;
	private final BuilderContainer fSubComponentContainer;
	
	/**
	 * @param id unique for this factory.
	 */
	public AbstractBuilderComponent(String id) {
		if (id == null) {
			throw new IllegalArgumentException("Must pass non-null ID");
		}
		fID = id;
		fSubComponentContainer = new BuilderContainer();
	}
	
	
	/**
	 * @return container with list of all children factories.
	 */
	protected BuilderContainer getBuilderContainer() {
		return fSubComponentContainer;
	}
	
	
	/**
	 * 
	 * @return unique id of this factory.
	 */
	public String getID() {
		return fID;
	}
}

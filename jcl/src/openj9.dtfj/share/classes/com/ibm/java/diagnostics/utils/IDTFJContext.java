/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2019 IBM Corp. and others
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
package com.ibm.java.diagnostics.utils;

import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;

public interface IDTFJContext extends IContext {
	/**
	 * The major version of the DTFJ API for this context.
	 * 
	 * @return major version number
	 */
	public int getMajorVersion();
	
	/**
	 * The minor version of the DTFJ API for this context.
	 * 
	 * @return minor version number
	 */
	public int getMinorVersion();
	
	/**
	 * The address space for this context. This may or may not
	 * contain any processes.
	 * 
	 * @return the image address space
	 */
	public ImageAddressSpace getAddressSpace();
	
	/**
	 * The process for this context. A process may or may not
	 * contain a Java runtime.
	 * @return
	 */
	public ImageProcess getProcess();
	
	/**
	 * The Java runtime for this context.
	 * 
	 * @return the runtime or null if there is no runtime available
	 */
	public JavaRuntime getRuntime();

	/**
	 * A simple Java bean which allows access to the data on DTFJ image interface.
	 * This removes the requirement to provide a command/plugin with a direct
	 * reference to the Image.
	 * 
	 * @return the bean
	 */
	public DTFJImageBean getImage();
	
	/**
	 * Used to determine if a property has been set in the context property
	 * bag, and that the value of that property is TRUE
	 * @param name property name to check
	 * @return true if the property exists and Boolean.parseValue() returns true
	 */
	public boolean hasPropertyBeenSet(String name);
}

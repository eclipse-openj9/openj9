/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.spi;

import com.ibm.java.diagnostics.utils.IDTFJContext;

/**
 * A combined context is an extended DTFJ context which also contains a DDR context.
 * This is to allow commands to be passed down the stack starting with DTFJ and then 
 * moving to DDR if necessary.
 * 
 * @author adam
 *
 */
public interface ICombinedContext extends IDTFJContext {

	/**
	 * Contexts have a unique positive numeric identifier within a session. This number always
	 * increases if new contexts are created via the open command rather than attempting to re-use
	 * any already used IDs.
	 *  
	 * @return the ID for this context
	 */
	public int getID();

	/**
	 * Sets the numeric ID for this context. This setter is provided so as to allow context creation
	 * before assigning the ID. However once set it should not be changed. Later implementations may
	 * enforce this.
	 * 
	 * @param id the context ID
	 */
	public void setID(int id);

	/**
	 * A handle to the underlying DDR interactive session
	 * 
	 * @return a valid session or null if the core file is not DDR enabled
	 */
	public Object getDDRIObject();

	/**
	 * Get the exception which was thrown when trying to start a DDR interactive session.
	 * 
	 * @return the exception or null if the session started without error.
	 */
	public Throwable getDDRStartupException();

	/**
	 * Display the contexts which are present in the core file currently being analysed.
	 * 
	 * @param shortFormat true for short format which just displays the first line of the java -version string
	 */
	public void displayContext(IOutputManager out, boolean shortFormat);

	/**
	 * Called when this context becomes the current context to allow
	 * any initialisation to take place. 
	 */
	public void setAsCurrent();
	
	/**
	 * Flag to indicate if an associated DDR context is available as well as the DTFJ one.
	 * @return true if DDR is available
	 */
	public boolean isDDRAvailable();

}

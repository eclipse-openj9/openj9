/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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
package com.ibm.dtfj.runtime;

import com.ibm.dtfj.image.CorruptDataException;

/**
 * <p>Represents a generic managed runtime instance.</p>
 * 
 * <p>No class should implement this interface directly. Instead, they should implement
 * another interface which extends this interface and provides APIs for a specific
 * language runtime environment.</p>
 *
 */
public interface ManagedRuntime {

	/**
	 * Get the 'full' version information for this runtime.
	 * @return a string representation of the version information for this runtime instance
	 * @throws CorruptDataException If the runtime presents no understandable version data
	 * @see #getVersion()
	 * 
	 * @deprecated Use "getVersion()" instead
	 */
	@Deprecated
	public String getFullVersion() throws CorruptDataException;

	/**
	 * Get the version data available for this runtime instance.
	 * @return a string representing all the version data available for this runtime instance.
	 * 
	 * @throws CorruptDataException If the runtime presents no understandable version data
	 */
	public String getVersion() throws CorruptDataException;
}

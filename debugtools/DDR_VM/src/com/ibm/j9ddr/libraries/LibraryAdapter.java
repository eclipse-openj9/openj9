/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

package com.ibm.j9ddr.libraries;

import java.io.File;
import java.util.ArrayList;

//defines the operations to be supported by a library adapter which handles requests for both DTFJ and DDR.

public interface LibraryAdapter {
	/**
	 * Return a list of libraries which should be collected for the specified core file
	 * @param coreFile core file to collect the libraries for
	 * @return a list of paths, each of which point to a library entry
	 */
	public ArrayList<String> getLibraryList(final File coreFile);
	
	/**
	 * Retrieve a list of error messages which have been produced during the library collection
	 * @return a list of error messages as strings
	 */
	public ArrayList<String> getErrorMessages();
	
	/**
	 * Determines if library collection is required for the specified core file
	 * @param coreFile core file to analyse
	 * @return true if the libraries need to be collected (Linux/AIX), false if not (Windows/z/OS)
	 */
	public boolean isLibraryCollectionRequired(final File coreFile);
}

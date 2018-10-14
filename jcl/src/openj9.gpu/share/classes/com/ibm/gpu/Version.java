/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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
package com.ibm.gpu;

/**
 * Acts as an accessor to the current build level of the ibmgpu library.
 * The main method can be launched from the command line, which will print
 * the current level to stdout.
 */
public class Version {

	/**
	 * The current build level of this package.
	 */
	protected static final String VERSION = "0.95"; //$NON-NLS-1$

	/**
	 * Prints the current level of ibmgpu to stdout.
	 *
	 * @param args unused
	 */
	public static void main(String[] args) {
		System.out.println("Using IBM GPU release: " + VERSION); //$NON-NLS-1$
	}

}

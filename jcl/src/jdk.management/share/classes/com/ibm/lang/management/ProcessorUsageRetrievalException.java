/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2013, 2014 IBM Corp. and others
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

package com.ibm.lang.management;

/**
 * Exception class whose instance is thrown when Processor usage sampling fails. For exact
 * cause one needs to inspect the exception string message capturing the details by calling
 * the toString() method.
 * 
 * @author sonchakr
 * @since 1.7.1
 */
public class ProcessorUsageRetrievalException extends Exception {
	private static final long serialVersionUID = 7905403037592303981L;

	/**
	 * Constructor for the exception class that takes a message string describing
	 * the exact nature of the exception.
	 *
	 * @param msg The string that describes the exact nature of the exception that
	 * occured while sampling processor usage statistics.
	 */
	public ProcessorUsageRetrievalException(String msg)
	{
		super(msg);
	}
}

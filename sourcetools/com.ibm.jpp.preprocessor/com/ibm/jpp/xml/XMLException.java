/*******************************************************************************
 * Copyright (c) 1999, 2017 IBM Corp. and others
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
package com.ibm.jpp.xml;

/**
 * XML handler exception.
 */
public class XMLException extends Exception {
	/**
	 * serialVersionUID
	 */
	private static final long serialVersionUID = 3257572818962298678L;

	/**
	 * Class constructor.
	 */
	public XMLException() {
		super();
	}

	/**
	 * Class constructor.
	 *
	 * @param detail exception details
	 */
	public XMLException(String detail) {
		super(detail);
	}

	/**
	 * Class constructor.
	 *
	 * @param cause the cause of the exception
	 */
	public XMLException(Exception cause) {
		super(cause);
	}

}

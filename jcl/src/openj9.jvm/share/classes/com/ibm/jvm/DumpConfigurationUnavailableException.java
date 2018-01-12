/*[INCLUDE-IF Sidecar17]*/
package com.ibm.jvm;

/*******************************************************************************
 * Copyright (c) 2012, 2014 IBM Corp. and others
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

/**
 * This exception is thrown when the dump configuration cannot be
 * updated through the methods on com.ibm.jvm.Dump because it is
 * in use. This is usually because a dump is in progress and the
 * configuration is locked. A dump may take some time to complete
 * so this exception is thrown instead of blocking the calling
 * thread indefinitely.
 */
public class DumpConfigurationUnavailableException extends Exception {

	private static final long serialVersionUID = -5355657048034576194L;

	public DumpConfigurationUnavailableException(String message) {
		super(message);
	}
	
}

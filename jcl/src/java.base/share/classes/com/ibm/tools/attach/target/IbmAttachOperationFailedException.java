/*[INCLUDE-IF Sidecar18-SE]*/
package com.ibm.tools.attach.target;

import java.io.IOException;
/**
 * Indicates failure in Attach API operation after target attached.
 * Need a private copy since the public version is in a different module.
 */
@SuppressWarnings("serial")
/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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


public class IbmAttachOperationFailedException extends IOException {

	/**
	 * Constructs a new instance of this class with its 
	 * walkback filled in.
	 */
	public IbmAttachOperationFailedException() {
		super("IbmAttachOperationFailedException"); //$NON-NLS-1$
	}
	
	/**
	 * Constructs a new instance of this class with its 
	 * walkback and message filled in.
	 * @param message
	 *            details of exception
	 */
	public IbmAttachOperationFailedException(String message) {
		super(message);
	}

	/**
	 * Constructs the exception with a message and a nested Throwable.
	 * @param message text of the message
	 * @param cause underlying Throwable
	 */
	public IbmAttachOperationFailedException(String message, Throwable cause) {
		super(message, cause);
	}

	@Override
	public String toString() {
		String result;
		Throwable myCause = getCause();
		if (null != myCause) {
			result = String.format("\"%s\"; Caused by: \"%s\"", super.toString(), myCause.toString()); //$NON-NLS-1$
		} else {
			result = super.toString();
		}
		return result;
	}
	
}

/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target;
/*******************************************************************************
 * Copyright (c) 2009, 2015 IBM Corp. and others
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
 * Strings for communication from target to attacher.
 *
 */
public interface Response {

	/**
	 * responses from target after a connection is established.
	 */
	/**
	 * Request
	 */
	static final String ACK = "ATTACH_ACK"; //$NON-NLS-1$
	static final String ATTACH_RESULT = "ATTACH_RESULT="; //$NON-NLS-1$
	static final String EXCEPTION_IOEXCEPTION = "IOException"; //$NON-NLS-1$
	static final String EXCEPTION_AGENT_INITIALIZATION_EXCEPTION = "AgentInitializationException"; //$NON-NLS-1$
	static final String EXCEPTION_AGENT_LOAD_EXCEPTION = "AgentLoadException"; //$NON-NLS-1$
	static final String EXCEPTION_ATTACH_OPERATION_FAILED_EXCEPTION = "AttachOperationFailedException"; //$NON-NLS-1$
	static final String EXCEPTION_NULL_POINTER_EXCEPTION = "NullPointerException"; //$NON-NLS-1$
	static final String EXCEPTION_ILLEGAL_ARGUMENT_EXCEPTION = "IllegalArgumentException"; //$NON-NLS-1$
	static final String EXCEPTION_NO_SUCH_METHOD_EXCEPTION = "NoSuchMethodException"; //$NON-NLS-1$
	static final String EXCEPTION_SECURITY_EXCEPTION = "SecurityException"; //$NON-NLS-1$
	static final String EXCEPTION_ILLEGAL_ACCESS_EXCEPTION = "IllegalAccessException"; //$NON-NLS-1$
	static final String EXCEPTION_INVOCATION_TARGET_EXCEPTION = "InvocationTargetException"; //$NON-NLS-1$
	static final String EXCEPTION_CLASS_NOT_FOUND_EXCEPTION = "ClassNotFoundException"; //$NON-NLS-1$
	static final String EXCEPTION_WRONG_METHOD_TYPE_EXCEPTION = "WrongMethodTypeException"; //$NON-NLS-1$
	static final String CONNECTED = "ATTACH_CONNECTED"; //$NON-NLS-1$
	static final String DETACHED = "ATTACH_DETACHED"; //$NON-NLS-1$
	static final String ERROR = "ATTACH_ERR"; //$NON-NLS-1$

}

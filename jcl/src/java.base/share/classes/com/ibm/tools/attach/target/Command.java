/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target;

/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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
 * Communication with the Attach API server uses textual verbs to request
 * actions and information. Constants for supported operations are defined here.
 */
public interface Command {

	/**
	 * Request the load of an agent library by name. - parameters are:
	 */
	static final String LOADAGENT = "ATTACH_LOADAGENT"; //$NON-NLS-1$

	/**
	 * Request the load of an agent library at a particular path. - parameters
	 * are:
	 */
	static final String LOADAGENTPATH = (LOADAGENT + "PATH"); //$NON-NLS-1$

	/**
	 * Request the load of an agent library at a particular path.
	 */
	static final String LOADAGENTLIBRARY = (LOADAGENT + "LIBRARY"); //$NON-NLS-1$

	/**
	 * Request a close of the connection.
	 */
	static final String DETACH = "ATTACH_DETACH"; //$NON-NLS-1$

	static final String GET_SYSTEM_PROPERTIES = "ATTACH_GETSYSTEMPROPERTIES"; //$NON-NLS-1$
	static final String GET_AGENT_PROPERTIES = "ATTACH_GETAGENTPROPERTIES"; //$NON-NLS-1$
	static final String START_MANAGEMENT_AGENT = "ATTACH_START_MANAGEMENT_AGENT"; //$NON-NLS-1$
	static final String START_LOCAL_MANAGEMENT_AGENT = "ATTACH_START_LOCAL_MANAGEMENT_AGENT"; //$NON-NLS-1$
	static final String GET_THREAD_GROUP_INFO = "ATTACH_GET_THREAD_INFO"; //$NON-NLS-1$

}

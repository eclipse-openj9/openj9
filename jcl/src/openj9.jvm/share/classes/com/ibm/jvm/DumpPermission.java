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

import java.security.BasicPermission;

/**
 * The permission class for operations on the com.ibm.jvm.Dump class.
 * Allowing code access to this permission will allow changes to be made
 * to system wide dump settings controlling which events cause dumps to be
 * and allow dumps to be triggered directly.
 * 
 * Triggering dumps may pause the application while the dump is taken. This pause
 * can potentially be minutes depending on the size of the application.
 * A dump file may be a complete image of the applications memory. Code with read
 * access to dump files produced by com.ibm.jvm.Dump should be considered as having
 * access to any information that was within the application at the time the dump
 * was taken. 
 */
public class DumpPermission extends BasicPermission {

	private static final long serialVersionUID = -7467700398466970030L;

	public DumpPermission() {
		super("DumpPermission"); //$NON-NLS-1$
	}
}

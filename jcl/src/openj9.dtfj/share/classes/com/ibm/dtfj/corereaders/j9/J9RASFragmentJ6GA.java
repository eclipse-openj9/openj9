/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
package com.ibm.dtfj.corereaders.j9;

/**
 * Represents the J9RAS structure prior to SR5.
 * Does not support TID and PID
 * 
 * @author Adam Pilkington
 *
 */
public class J9RASFragmentJ6GA implements J9RASFragment {

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.j9.J9RASFragment#currentThreadSupported()
	 */
	public boolean currentThreadSupported() {
		return false;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.j9.J9RASFragment#getPID()
	 */
	public long getPID() {
		throw new UnsupportedOperationException("Get PID() is not supported for this version of J9RAS");
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.j9.J9RASFragment#getTID()
	 */
	public long getTID() {
		throw new UnsupportedOperationException("Get TID() is not supported for this version of J9RAS");
	}

}

/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.j9;

import com.ibm.dtfj.image.DataUnavailable;

/**
 * @author jmdisher
 * This class exists to be thrown in the cases where data might be available after future support is added.  It
 * allows us to correctly implement the spec but still keep a sort of eye-catching data in the code which we can 
 * use to evaluate what still needs to be done in the future.  Most likely, occurrences of this class will be 
 * replaced by DataUnavailable or will be implemented in terms of new core file data or improved JExtract support.
 */
public class FutureImplementation extends DataUnavailable
{

	public FutureImplementation(String description)
	{
		super(description);
	}

	/**
	 * 
	 */
	private static final long serialVersionUID = 1L;
}

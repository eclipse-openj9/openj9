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
package com.ibm.dtfj.java;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;

/**
 * <p>Represents the JavaVMOption C structures passed to JNI_CreateJavaVM
 * to create the VM.</p>
 * 
 * <p>Each JavaVMOption consists of two components:</p>
 * <ol>
 *     <li>an optionString string, used to identify the option.</li>
 *     <li>an extraInfo pointer, used to pass additional information. This component is usually NULL.</li>
 * </ol>
 */
public interface JavaVMOption {

	/**
	 * Fetch the optionString component of the option.
	 * @return a string representing the optionString. This is never null.
	 * @throws DataUnavailable
	 * @throws CorruptDataException
	 */
	public String getOptionString() throws DataUnavailable, CorruptDataException;
	
	/**
	 * Fetch the extraInfo component of this option.
	 * @return the pointer value from the extraInfo (usually null).
	 * @throws DataUnavailable
	 * @throws CorruptDataException
	 */
	public ImagePointer getExtraInfo() throws DataUnavailable, CorruptDataException;
}

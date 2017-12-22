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
package com.ibm.dtfj.image;

/**
 * This class is used to indicate that corruption has been detected in the image.
 * It may indicate corruption of the image file, or it may indicate that 
 * inconsistencies have been detected within the image file, perhaps caused by
 * a bug in the runtime or application.
 * 
 * It may be encountered in two scenarios:
 * <ul>
 * <li>within a CorruptDataException</li>
 * <li>returned as an element from an Iterator</li> 
 * </ul>
 * 
 * Any iterator in DTFJ may implicitly include one or more CorruptData objects
 * within the list of objects it provides. Normal data may be found after the
 * CorruptData object if the DTFJ implementation is able to recover from the
 * corruption.
 * 
 * @see CorruptDataException
 */
public interface CorruptData {

	/**
	 * Provides a string which describes the corruption.
	 * 
	 * @return a descriptive string.
	 */
	public String toString();
	
	/**
	 * Return an address associated with the corruption.
	 * 
	 * If the corruption is not associated with an address, return null.
	 * 
	 * If the corruption is associated with more than one address, return
	 * the one which best identifies the corruption.
	 * 
	 * @return the address of the corrupted data.
	 */
	public ImagePointer getAddress();
	
}

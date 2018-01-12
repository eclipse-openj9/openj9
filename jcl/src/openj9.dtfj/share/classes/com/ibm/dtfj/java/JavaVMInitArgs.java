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

import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;

/**
 * Represents the JavaVMInitArgs C structure passed to JNI_CreateJavaVM
 * to create the Java runtime.
 */
public interface JavaVMInitArgs {

	/**
	 * The JNI specified version constant for the Java 1.1 version of JNI
	 */
	public static final int JNI_VERSION_1_1 = 0x00010001;

	/**
	 * The JNI specified version constant for the Java 1.2 version of JNI
	 */
	public static final int JNI_VERSION_1_2 = 0x00010002;

	/**
	 * The JNI specified version constant for the Java 1.4 version of JNI
	 */
	public static final int JNI_VERSION_1_4 = 0x00010004;
	
	/**
	 * The JNI specified version constant for the Java 1.6 version of JNI
	 */
	public static final int JNI_VERSION_1_6 = 0x00010006;
	
	/**
	 * Fetch the JNI version from the JavaVMInitArgs structure used to create this VM.
	 * See the JNI specification for the meaning for this field.
	 *  
	 * @return the JNI version
	 * @throws DataUnavailable
	 * @throws CorruptDataException
	 */
	public int getVersion() throws DataUnavailable, CorruptDataException;
	
	/**
	 * Fetch the ignoreUnrecognized field from the JavaVMInitArgs structure used to create this VM.
	 * See the JNI specification for the meaning of this field.
	 * 
	 * @return true if ignoreUnrecognized was set to a non-zero value with the VM was invoked
	 * @throws DataUnavailable
	 * @throws CorruptDataException
	 */
	public boolean getIgnoreUnrecognized() throws DataUnavailable, CorruptDataException;
	
	/**
	 * Fetch the options used to start this VM, in the order they were originally specified.
	 * 
	 * @return an Iterator over the collection of JavaVMOptions
	 * @throws DataUnavailable
	 * 
	 * @see JavaVMOption
	 */
	public Iterator getOptions() throws DataUnavailable;

}

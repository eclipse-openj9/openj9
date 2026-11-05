/*[INCLUDE-IF Sidecar18-SE]*/
/*
 * Copyright IBM Corp. and others 2004
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
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
	 * The JNI specified version constant for the Java 1.1 version of JNI.
	 */
	public static final int JNI_VERSION_1_1 = 0x00010001;

	/**
	 * The JNI specified version constant for the Java 1.2 version of JNI.
	 */
	public static final int JNI_VERSION_1_2 = 0x00010002;

	/**
	 * The JNI specified version constant for the Java 1.4 version of JNI.
	 */
	public static final int JNI_VERSION_1_4 = 0x00010004;

	/**
	 * The JNI specified version constant for the Java 1.6 version of JNI.
	 */
	public static final int JNI_VERSION_1_6 = 0x00010006;

	/**
	 * The JNI specified version constant for the Java 1.8 version of JNI.
	 */
	public static final int JNI_VERSION_1_8 = 0x00010008;

	/*[IF JAVA_SPEC_VERSION >= 9]*/
	/**
	 * The JNI specified version constant for the Java 9 version of JNI.
	 */
	public static final int JNI_VERSION_9 = 0x00090000;
	/*[ENDIF] JAVA_SPEC_VERSION >= 9 */

	/*[IF JAVA_SPEC_VERSION >= 10]*/
	/**
	 * The JNI specified version constant for the Java 10 version of JNI.
	 */
	public static final int JNI_VERSION_10 = 0x000A0000;
	/*[ENDIF] JAVA_SPEC_VERSION >= 10 */

	/*[IF JAVA_SPEC_VERSION >= 19]*/
	/**
	 * The JNI specified version constant for the Java 19 version of JNI.
	 */
	public static final int JNI_VERSION_19 = 0x00130000;
	/*[ENDIF] JAVA_SPEC_VERSION >= 19 */

	/*[IF JAVA_SPEC_VERSION >= 20]*/
	/**
	 * The JNI specified version constant for the Java 20 version of JNI.
	 */
	public static final int JNI_VERSION_20 = 0x00140000;
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

	/*[IF JAVA_SPEC_VERSION >= 21]*/
	/**
	 * The JNI specified version constant for the Java 21 version of JNI.
	 */
	public static final int JNI_VERSION_21 = 0x00150000;
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

	/*[IF JAVA_SPEC_VERSION >= 24]*/
	/**
	 * The JNI specified version constant for the Java 24 version of JNI.
	 */
	public static final int JNI_VERSION_24 = 0x00180000;
	/*[ENDIF] JAVA_SPEC_VERSION >= 24 */

	/*[IF JAVA_SPEC_VERSION >= 28]*/
	/**
	 * The JNI specified version constant for the Java 28 version of JNI.
	 */
	public static final int JNI_VERSION_28 = 0x001C0000;
	/*[ENDIF] JAVA_SPEC_VERSION >= 28 */

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

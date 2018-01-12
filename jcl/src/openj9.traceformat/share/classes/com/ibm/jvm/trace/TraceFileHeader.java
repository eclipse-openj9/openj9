/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2016 IBM Corp. and others
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
package com.ibm.jvm.trace;

/**
 * A class to format and report a binary TraceFileHeader produced by the IBM JVM's trace
 * engine.
 * @author Simon Rowland
 *
 */
public interface TraceFileHeader {
	/**
	 * @return a String representation of the JVM that produced the current TraceFile, null
	 * if no valid data.
	 */
	public String getVMVersion();
	/**
	 * @return a String array representation of the startup parameters that were used to start
	 * the JVM that produced the current TraceFile, null if no valid data.
	 */
	public String[] getVMStartUpParameters();
	/**
	 * @return a String array representation of the Trace startup parameters that were used to start
	 * the JVM that produced the current TraceFile, null if no valid data.
	 */
	public String[] getTraceParameters();
	/**
	 * @return a String array representation of the system on which the JVM that produced 
	 * the current TraceFile was run, null if no valid data.
	 */
	public String[] getSysProcessorInfo();
	/**
	 * @return the millisecond time at which the JVM that produced the current TraceFile was
	 * started. Returns -1 if no valid data.
	 */
	public long getJVMStartedMillis();
	/**
	 * @return the millisecond time at which the JVM that produced the current TraceFile wrote 
	 * its most recent TracePoint before the production of the current TraceFile. Returns -1 if 
	 * no valid data.
	 */
	public long getLastBufferWriteMillis();
	/**
	 * @return the millisecond time of the first TracePoint in the current TraceFile. This may
	 * not be the first TracePoint that the JVM produced. Returns -1 if no valid data.
	 */
	public long getFirstTracePointMillis();
	/**
	 * @return the millisecond time of the last TracePoint in the current TraceFile. This may
	 * not be the last TracePoint that the JVM produced. Returns -1 if no valid data.
	 */
	public long getLastTracePointMillis();
}


/*[INCLUDE-IF Sidecar16]*/

/*******************************************************************************
 * Copyright (c) 2009, 2010 IBM Corp. and others
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

package com.ibm.le.conditionhandling;

/**
 * A ConditionException is used to represent a z/OS Language Environment condition in Java.
 * <p>
 * References 
 * <ul>
 * <li>z/OS Language Environment Programming Reference: CEEDCOD-Decompose a condition token
 * <li>z/OS XL C/C++ Run-Time Library Reference: __le_condition_token_build()
 * <li>The AMODE31 header file leawi.h
 */

public class ConditionException extends RuntimeException {
	
	static final long serialVersionUID = -6740638630352857974L;
	private String failingRoutine;
	private long offset;
	private byte[] rawTokenBytes;
	private int c1;
	private int c2;
	private int caze;
	private int severity;
	private int control;
	private String facilityID;
	private long iSInfo;

	/**
	 * 
	 * This constructor is intentionally not public so that only the JVM can instantiate a ConditionException
	 * 
	 * @param failingRoutine	The routine that triggered the condition
	 * @param offset			This offset into the routine where the condition was triggered
	 * @param rawTokenBytes		The raw bytes representing the condition token
	 * @param facilityID		The condition's facility ID
	 * @param c1				c_1, as stored in the condition token 
	 * @param c2				c_2, as stored in the condition token
	 * @param caze				The condition's case
	 * @param severity			The condition's severity
	 * @param control			The condition's control field 
	 * @param iSInfo			The condition's ISI
	 * 
	 */
	ConditionException(String failingRoutine, long offset, byte []rawTokenBytes, String facilityID, int c1, int c2, int caze, int severity, int control, long iSInfo) {

		this.failingRoutine = failingRoutine;
		this.offset = offset;
		this.rawTokenBytes = rawTokenBytes.clone();
		this.facilityID = facilityID;
		this.c1 = c1;
		this.c2 = c2;
		this.caze = caze;
		this.severity = severity;
		this.control = control;
		this.iSInfo = iSInfo;		
	}

	/**
	 * Returns the condition's facility ID.
	 * 
	 * @return the condition's facility ID
	 */
	public String getFacilityID() {
		return facilityID;
	}
	
	/**
	 * Returns c_1, as stored in the condition token.
	 * 
	 * @return c_1, as stored in the condition token
	 */
	public int getC1() {
		return c1;
	}
	
	/**
	 * Returns c_2, as stored in the condition token.
	 * 
	 * @return c_2, as stored in the condition token
	 */
	public int getC2() {
		return c2;
	}
	
	/**
	 * Returns the format of c_1 and c_2.
	 * 
	 * @return the format of c_1 and c_2 
	 */
	public int getCase() {
		return caze;
	}
	
	/**
	 * Returns the condition's control field.
	 * 
	 * @return the condition's control field
	 */
	public int getControl() {
		return control;
	}
	
	/**
	 * Returns the condition's severity.
	 * 
	 * @return the condition's severity
	 */
	public int getSeverity() {
		return severity;
	}
	
	/**
	 * Returns the condition's ISI.
	 * 
	 * @return the condition's ISI
	 */
	public long getISInfo() {
		return iSInfo;
	}
	
	/**
	 * Returns the raw 12-byte representation of the condition.
	 * 
	 * @return the raw 12-byte representation of the condition
	 */
	public byte[] getToken() {
		return rawTokenBytes.clone();
	}

	/**
	 * Returns the condition's message number.
	 * 
	 * @return the condition's message number
	 */
	public int getMessageNumber() {
		return getC2();
	}

	/**
	 * Returns the routine that triggered the condition.
	 * 
	 * @return the routine that triggered the condition
	 */
	public String getRoutine() {
		return failingRoutine;
	}
	
	/**
	 * Returns the offset into the routine that triggered the condition.
	 * 
	 * @return the offset into the routine that triggered the condition
	 */
	public long getOffsetInRoutine() {
		return offset;
	}
}

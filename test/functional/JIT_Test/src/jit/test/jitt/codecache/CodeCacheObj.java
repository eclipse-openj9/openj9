/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package jit.test.jitt.codecache;

import java.util.ArrayList;

/**
 * Represents a code cache in Code Cache Test Harness runtime
 *
 * @author mesbah
 */
public class CodeCacheObj {
	private String mccAddress;
	private String startAddress;
	private String endAddress;
	private int reclamationCount = 0 ;
	private ArrayList<MethodObj> methodList = null;

	public CodeCacheObj ( String mccAddress, String startAddress, String endAddress ) {
		this.mccAddress = mccAddress;
		this.startAddress = startAddress;
		this.endAddress = endAddress;
		this.methodList = new ArrayList<MethodObj>();
	}

	public String getStartAddress() {
		return startAddress;
	}

	public String getEndAddress() {
		return endAddress;
	}

	public int getAllocCount() {
		return methodList.size();
	}

	public void incrementReclaimedCount () {
		reclamationCount++;
	}

	public int getReclamationCount () {
		return reclamationCount;
	}

	/**
	 * Given a verbose log method entry represented by a Method object, figures out whether or not the method
	 * belongs to this code cache by comparing the method's address range with the code cache's address range.
	 *
	 * Since the values�0..9�and�A..F�are in hex-digit order in the ASCII character set, address comparison is
	 * done by simply using the String.compareTo(String) method.
	 *
	 * @param method - the Method object representing a verbose log entry
	 * @return True if the given method belongs to this code cache, false otherwise
	 */
	public boolean addressRangeIncludes(MethodObj method){
		return addressRangeIncludes( method.getCcAddressRange() );
	}

	public void addMethod ( MethodObj method ) {
		this.methodList.add(method);
	}

	/**
	 * Given a method signature string, searches for the method in the current code cache, if found, returns true, else, returns false.
	 * @param signature - Signature or path of signature of the method being looked up.
	 * @return
	 */
	public boolean hasMethod ( String signature ) {
		for ( int i = 0 ; i < this.methodList.size() ; i++ ) {
			MethodObj method = this.methodList.get(i);
			if ( method.getMethodSignature().equals(signature) ) {
				return true;
			}
		}
		return false;
	}

	public boolean addressRangeIncludes ( String range ) {
		if ( range.contains("-") == false ) {
			return false;
		}

		String tokens [] = range.split("-");

		if ( tokens.length != 2 ) {
			return false;
		}

		String methodStartAddr = tokens[0].toUpperCase();
		String methodEndAddr = tokens[1].toUpperCase();

		if ( methodStartAddr.length() != methodEndAddr.length() ) {
			return false;
		}

		if ( methodStartAddr.startsWith("0X") ) {
			methodStartAddr = methodStartAddr.substring(2);
		}

		if ( methodEndAddr.startsWith("0X")) {
			methodEndAddr = methodEndAddr.substring(2);
		}

		if ( this.startAddress.compareTo(methodStartAddr) < 0 ) { // if the code cache's start address comes before the method's start address
			if ( this.endAddress.compareTo(methodEndAddr) > 0 ) { // if the code cache's end address comes after the method's end address
				return true;
			}
		}

		return false;
	}

	public void clearMethodList() {
		this.methodList.clear();
		this.methodList = null;
	}

	@Override
	public String toString() {
		return "CodeCacheObj [mccAddress=" + mccAddress + ", startAddress="
				+ startAddress + ", endAddress=" + endAddress + "]";
	}

	public boolean removeMethod( MethodObj method ) {
		for ( int i = 0 ; i < this.methodList.size() ; i++ ) {
			MethodObj m = this.methodList.get(i);
			if ( m.getMethodSignature().equalsIgnoreCase(method.getMethodSignature()) ) {
				return this.methodList.remove(m);
			}
		}
		return false;
	}
}

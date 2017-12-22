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
package com.ibm.dtfj.corereaders;

public class StackFrame {
	
	long position;
	long retAddr;
	
	
	public StackFrame(long stackPosition, long retAddr)  {
		this.retAddr = retAddr;
		position = stackPosition;
	}
	 

 

	/**
	 * @return Returns the position.
	 */
	public long getPosition() {
		return position;
	}
	/**
	 * @return Returns the retAddr.
	 */
	public long getRetAddr() {
		return retAddr;
	}
	
	public String toString() {
		StringBuffer sb= new StringBuffer( "(Bp: 0x");
	 
		String ra = Long.toHexString(position);
//		ra = DumpUtils.padToPtrSize(ra);
		sb.append( ra + ")");
//		sb.append("  0x" + DumpUtils.padToPtrSize(Long.toHexString(retAddr)));
		sb.append("  0x" + Long.toHexString(retAddr));
		String location = Symbol.getSymbolForAddress(retAddr);
		if (null != location) {
			sb.append( "  <==");
			sb.append(location);
		}
			 
		return sb.toString();
			
		 
		
		
	}	
}

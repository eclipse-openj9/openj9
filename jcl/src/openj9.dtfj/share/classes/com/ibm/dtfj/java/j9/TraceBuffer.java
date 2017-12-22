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
package com.ibm.dtfj.java.j9;

/**
 * @author jmdisher
 * A container for the information that is described by the &lt;trace&gt; data in the XML index
 * 
 * Data looks like this:
 * <trace>
 	<header address="0x10a63008" size="1484"/>
 	<buffers address="0x10a5ef00" size="8192" offset="40" link="24" terminator="0x0"/>
 * </trace>
 * 
 * We will have to know more about what is desired from this component before implementing it
 */
public class TraceBuffer
{
	public void setHeader(long address, long size)
	{
		//we will need the capability to turn these addresses into ImagePointer instances but there is no 
		// API to read this type, yet
	}
	
	public void setBuffer(long address, long size, int offset, int link, int terminator)
	{
		//we will need the capability to turn these addresses into ImagePointer instances but there is no 
		// API to read this type, yet
	}
}

/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.heapdump.portable;

/**
 * @author schan
 * 
 * Object representing the PHD class cache. A compression mechanism
 * based around assigning numbers to the last 4 class addresses
 * written out in full.
 */
public class PortableHeapDumpClassCache
{
	public static final int CLASSCACHE_SIZE = 4;
	
	private final long _classCache[] = new long[CLASSCACHE_SIZE];
	private byte _classIndex = 0;

	byte getClassCacheIndex(long address)
	{
		if (_classCache[0] == address)
			return 0;
		if (_classCache[1] == address)
			return 1;
		if (_classCache[2] == address)
			return 2;
		if (_classCache[3] == address)
			return 3;

		return -1;
	}

	byte setClassCacheIndex(long address)
	{
		byte result = _classIndex;
		_classCache[_classIndex] = address;
		_classIndex = (byte) ((_classIndex + 1) % CLASSCACHE_SIZE);
		return result;
	}

}
